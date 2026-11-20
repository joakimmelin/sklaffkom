/*
  lrz - receive files with x/y/zmodem
  Copyright (C) until 1988 Chuck Forsberg (Omen Technology INC)
  Copyright (C) 1994 Matt Porter, Michael D. Black
  Copyright (C) 1996, 1997 Uwe Ohse

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
  02111-1307, USA.

  originally written by Chuck Forsberg
*/

#include "zglobal.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <utime.h>

#ifndef HAVE_GETOPT_LONG
#define getopt_long(argc,argv,str,longopts,longind) getopt(argc,argv,str)
#endif

#include "timing.h"

#ifndef HAVE_O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

unsigned Baudrate = 2400;

static FILE *fout;


static int Lastrx;
static int Crcflg;
static int Firstsec;
int errors;
static int Restricted=1;	/* restricted; no /.. or ../ in filenames */
static int skip_if_not_found;
static unsigned int Segmentsize = 0;

static char *Pathname;

static int MakeLCPathname=true;	/* make received pathname lower case */
int Verbose=0;
static int Quiet=0;		/* overrides logic that would otherwise set verbose */
static int Nflag = 0;		/* Don't really transfer files */
static int Rxclob=false;	/* Clobber existing file */
static int Rxbinary=false;	/* receive all files in bin mode */
static int Rxascii=false;	/* receive files in ascii (translate) mode */
static int Thisbinary;		/* current file is to be received in bin mode */
static bool try_resume=false;
static bool keep_incomplete=false;
static bool junk_path=false;
bool no_timeout=false;
enum zm_type_enum protocol;
bool under_rsh=false;
bool zmodem_requested=false;

static char *secbuf;

#ifdef O_SYNC
static int o_sync = 0;
#endif
static int rzfiles (struct zm_fileinfo *);
static int tryz (void);
static void checkpath (const char *name);
static void report (int sct);
static void uncaps (char *s);
static int IsAnyLower (const char *s);
static int putsec (struct zm_fileinfo *zi, char *buf, size_t n);
enum safe_mkdir { MKDIR_NO, MKDIR_OK };
static FILE *safe_open (const char *path, const char *mode, enum safe_mkdir mkdir_ok);
static int procheader (char *name, struct zm_fileinfo *);
static int setup_output_buffer (struct zm_fileinfo *zi, FILE *fout);
static int wcgetsec (size_t *Blklen, char *rxbuf, unsigned int maxtime);
static int wcrx (struct zm_fileinfo *);
static int wcrxpn (struct zm_fileinfo *, char *rpn);
static int wcreceive (int argc, char **argp);
static int rzfile (struct zm_fileinfo *);
static void usage (int exitcode, const char *what);
static int closeit (struct zm_fileinfo *);
static void ackbibi (void);
static void zmputs (const char *s);
static mode_t lrz_umask = 0077;

static long buffersize=32768;
static unsigned long min_bps=0;
static long min_bps_time=120;

static char Lzmanag;		/* Local file management request */
static char zconv;		/* ZMODEM file conversion request */
static char zmanag;		/* ZMODEM file management request */
static char ztrans;		/* ZMODEM file transport request */
bool Zctlesc;		/* Encode control characters */
unsigned int Zrwindow = 1400;	/* RX window size (controls garbage count) */

static int tryzhdrtype=ZRINIT;	/* Header type to send corresponding to Last rx close */
static time_t stop_time;

int enable_syslog=true;

/* DO_SYSLOG/DO_SYSLOG_FNAME are replaced by the shared variadic DO_SYSLOG
 * macro (zglobal.h) plus lrzsz_basename() (util.c) - same logged values,
 * no variable declarations or `zi` scope coupling inside a macro. */


/* Signal handler for SIGINT/SIGTERM/SIGPIPE.
 * This is not strictly async-signal safe, because of the tcsetattr and tcflush
 * calls in io_mode(), but these functions are, under linux at least, simple 
 * syscall wrappers.
 * Leaving the terminal in raw mode would be worse. So we rely on undefined 
 * behaviour here. Oh well.
 * _exit() skips atexit/stdio on purpose, exit() would risk stdio/malloc 
 * re-entrancy from the handler. */
LRZSZ_NORETURN static void
bibi_sighandler(int n)
{
	int saved_errno = errno;

	if (zmodem_requested)
		zmputs(Attn);
	canit(STDOUT_FILENO);
	io_mode(0,0);
	errno = saved_errno;
	_exit(128+n);
}

/* called from normal control flow (checkpath, rbsb) to clean things up */
LRZSZ_NORETURN void
bibi(int n)
{
	/* The refusal (or session error) that leads here is fully
	 * communicated: the cancel sequence was sent, the message printed.
	 * The cancel burst inside this function is sent just in case; if the
	 * sender already left because it saw the first burst and cancelled,
         * that write hits EPIPE and dying of SIGPIPE there would turn the
	 * designed exit (127 for the refusals) into 141. Ignore SIGPIPE from
	 * now on. write_all already ignores write errors, so the exit code 
         * is deterministic.
	 * Same behaviour and reason as ackbibi() below. */
	signal(SIGPIPE, SIG_IGN);
	if (zmodem_requested)
		zmputs(Attn);
	canit(STDOUT_FILENO);
	io_mode(0,0);
	fatal_error(128+n,0,_("caught signal %d; exiting"), n);
}

/* the cancel-handler installer is shared: see lrzsz_install_signal() in
 * util.c; lrz installs its own bibi_sighandler with it. */

#ifdef HAVE_GETOPT_LONG
static struct option const long_options[] =
{
	{"append", no_argument, NULL, '+'},
	{"ascii", no_argument, NULL, 'a'},
	{"segmentsize", no_argument, NULL, 'A'},
	{"binary", no_argument, NULL, 'b'},
	{"bufsize", required_argument, NULL, 'B'},
	{"allow-commands", no_argument, NULL, 'C'},
	{"escape", no_argument, NULL, 'e'},
	{"rename", no_argument, NULL, 'E'},
	{"help", no_argument, NULL, 'h'},
	{"crc-check", no_argument, NULL, 'H'},
	{"junk-path", no_argument, NULL, 'j'},
	{"errors", required_argument, NULL, 'F'},
	{"disable-timeouts", no_argument, NULL, 'O'},
	{"disable-timeout", no_argument, NULL, 'O'}, /* i can't get it right */
	{"min-bps", required_argument, NULL, 'm'},
	{"min-bps-time", required_argument, NULL, 'M'},
	{"newer", no_argument, NULL, 'n'},
	{"newer-or-longer", no_argument, NULL, 'N'},
	{"protect", no_argument, NULL, 'p'},
	{"resume", no_argument, NULL, 'r'},
	{"restricted", no_argument, NULL, 'R'},
	{"quiet", no_argument, NULL, 'q'},
	{"stop-at", required_argument, NULL, 's'},
	{"timesync", no_argument, NULL, 'S'},
	{"timeout", required_argument, NULL, 't'},
	{"keep-uppercase", no_argument, NULL, 'u'},
	{"unrestrict", no_argument, NULL, 'U'}, /* removed, kept for the error message */
	{"verbose", no_argument, NULL, 'v'},
	{"windowsize", required_argument, NULL, 'w'},
	{"with-crc", no_argument, NULL, 'c'},
	{"xmodem", no_argument, NULL, 'X'},
	{"ymodem", no_argument, NULL, 'g'},
	{"zmodem", no_argument, NULL, 'Z'},
	{"overwrite", no_argument, NULL, 'y'},
	{"null", no_argument, NULL, 'D'},
	{"version", no_argument, NULL, 'V'},
	{"journal", required_argument, NULL, 'J'},
	{"syslog", optional_argument, NULL , 2},
	{"delay-startup", required_argument, NULL, 4},
	{"o-sync", no_argument, NULL, 5},
	{"o_sync", no_argument, NULL, 5},
	{"keep-incomplete", no_argument, NULL, 6},
	{"debug", required_argument, NULL, 7},
	{NULL,0,NULL,0}
};
#endif

LRZSZ_NORETURN static void
show_version(void)
{
	printf ("%s (GNU %s) %s\n", lrzsz_progname, PACKAGE, VERSION);
        exit(0);
}

int
main(int argc, char *argv[])
{
	char *cp;
	int npats;
	char **patts=NULL; /* keep compiler quiet */
	int exitcode=0;
	int c;
	unsigned int startup_delay=0;
        const char *journal_filename;
    
        lrzsz_set_progname (argv[0], "lrz"); /* needed for the error handling => early */

	Rxtimeout = 100;
	setbuf(stderr, NULL);
	if ((cp=getenv("SHELL")) && cp[0]) {
		const char *base = strrchr(cp, '/');
		base = base ? base + 1 : cp;
		if (strcmp(base, "rsh") == 0 || strcmp(base, "rksh") == 0
			|| strcmp(base, "rbash") == 0 || strcmp(base, "rshell") == 0)
			under_rsh=true;
	}
	if ((getenv("ZMODEM_RESTRICTED"))!=NULL)
		Restricted=2;

	/* rights for temporary and unfinished files  */
	umask(lrz_umask);

	from_cu();
	chkinvok(argv[0], 'r');	/* if called as [-]rzCOMMAND set flag */

	journal_filename=getenv("LRZ_JOURNAL");
        if (!journal_filename || !*journal_filename) {
	    journal_filename=getenv("LRZSZ_JOURNAL");
        }
        if (journal_filename && *journal_filename) {
            stats_journal_open(journal_filename);
        }

	openlog(lrzsz_progname,LOG_PID,SYSLOG_FACILITY);

	setlocale (LC_ALL, "");
        setlocale (LC_TIME, "C"); /* do not translate just the timestamp of a syslog message. uh. */
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);
	while ((c = getopt_long (argc, argv, 
		"A:a+bB:cCDeEgh7HJ:m:M:nNOprRqs:St:uUvVw:XZy",
		long_options, (int *) 0)) != EOF)
	{
		switch (c)
		{
		case 0:
			break;
		case '+': Lzmanag = ZF1_ZMAPND; break;
		case 'a': Rxascii=true;  break;
                case 'A':
                        Segmentsize = (unsigned int) lrzsz_strtoul( "-A / --segmentsize", optarg, NULL, 0, 65535);
                        break;
		case 'b': Rxbinary=true; break;
		case 'B': 
			if (strcmp(optarg,"auto")==0)
				fatal_error(1,0,_("the auto buffersize option has been removed; see the manual"));
                        buffersize = lrzsz_strtoul( _("-B / --bufsize"), optarg, "km", 0, 0);
			break;
		case 'c': Crcflg=true; break;
		case 'D': Nflag = true; break;
		case 'E': Lzmanag = ZF1_ZMCHNG; break;
 		case 'e': Zctlesc = 1; break;
 	case 'h': usage(0,NULL); break;
	case 'J':
		stats_journal_open(optarg);
		break;
	case 'H': Lzmanag= ZF1_ZMCRC; break;
		case 'j': junk_path=true; break;
		case 'm':
			min_bps = lrzsz_strtoul( _("-m / --min-bps"), optarg, "km", 0, 0);
			break;
		case 'M':
 			min_bps_time = lrzsz_strtoul( _("-M / --min-bps-time"), optarg, "k", 1, LONG_MAX);
			break;
		case 'N': Lzmanag = ZF1_ZMNEWL;  break;
		case 'n': Lzmanag = ZF1_ZMNEW;  break;
		case 'O': no_timeout=true; break;
		case 'p': Lzmanag = ZF1_ZMPROT;  break;
		case 'q': Quiet=true; Verbose=0; break;
		case 's':
			stop_time = lrzsz_parse_stop_time(optarg, usage);
			break;


		case 'r': 
			if (try_resume) 
				Lzmanag= ZF1_ZMCRC;
			else
				try_resume=true;  
			break;
		case 'R':
			if (Restricted < 2)
				Restricted=2;
			break;
		case 'C':
		case 'S':
		case 'U':
			fprintf(stderr,
				_("this option has been removed. See the manual page for more information.\n"));
			exit(1);
		case 't':
                        Rxtimeout = (unsigned int) lrzsz_strtoul( "-t / --timeout", optarg, NULL, 10, 1000);
			break;
		case 'w':
                        Zrwindow = (unsigned int) lrzsz_strtoul( "-w / --windowsize", optarg, NULL, 1, 8192);
			break;
		case 'u':
			MakeLCPathname=false; break;
		case 'v':
			++Verbose; break;
		case 'V':
			show_version();  break;
		case 'X': protocol=ZM_XMODEM; break;
		case 'g':   protocol=ZM_YMODEM; break;
		case 'Z': protocol=ZM_ZMODEM; break;
		case 'y':
			Rxclob=true; break;
		case 2:
			if (optarg && (!strcmp(optarg,"off") || !strcmp(optarg,"no"))) {
				if (under_rsh)
					lrzsz_warning(0, _("cannot turnoff syslog"));
				else
					enable_syslog=false;
			}
			else
				enable_syslog=true;
			break;
                case 4:
                        startup_delay = (unsigned int) lrzsz_strtoul( "--delay-startup", optarg, NULL, 0, 0);
			break;
		case 5:
#ifdef O_SYNC
			o_sync=1;
#else
			lrzsz_warning(0, _("O_SYNC not supported by the kernel"));
#endif
			break;
		case 6:
			keep_incomplete=true;
			break;
		case 7:
			if (!lrzsz_parse_debug(optarg))
				usage(2, _("bad --debug argument"));
			break;
		default:
			usage(2,NULL);
		}

	}

	if (getuid()!=geteuid()) {
		fatal_error(1,0,
		_("this program was never intended to be used setuid\n"));
	}

	secbuf=xmalloc(1+LRZSZ_MAX_SUBPACKET_LEN);

	/* initialize zsendline tab */
	zsendline_init();
	if (startup_delay)
		sleep(startup_delay);

	npats = argc - optind;
	patts=&argv[optind];

	if (npats > 1)
		usage(2,_("garbage on commandline"));
	if (protocol!=ZM_XMODEM && npats)
		usage(2, _("garbage on commandline"));
	if (Fromcu && !Quiet) {
		if (Verbose == 0)
			Verbose = 2;
	}

	DPRINTF(DEBUG_TRANSFER, "%s %s\n", lrzsz_progname, VERSION);

        {
            struct timespec start_timespec;
            int ret;
            ret=clock_gettime(CLOCK_MONOTONIC, &start_timespec);
            if (ret) {
                stat_for_all.start_time=0;
            } else {
                stat_for_all.start_time=start_timespec.tv_sec+((double)start_timespec.tv_nsec/1e9);
            }
        }
        stat_for_all.direction = 'r';
        stat_for_file.direction = 'r';

	if (io_mode(0,1))
		lrzsz_warning(0, _("cannot set transfer line mode; continuing"));
	readline_setup(0, LRZSZ_MAX_SUBPACKET_LEN, LRZSZ_MAX_SUBPACKET_LEN*2);
	lrzsz_install_signal(SIGINT, bibi_sighandler);
	lrzsz_install_signal(SIGTERM, bibi_sighandler);
	lrzsz_install_signal(SIGPIPE, bibi_sighandler);
	if (wcreceive(npats, patts)==ERROR) {
		exitcode=0x80;
		canit(STDOUT_FILENO);
	}
	io_mode(0,0);
	if (exitcode && !zmodem_requested)	/* bellow again with all thy might. */
		canit(STDOUT_FILENO);
	if (Verbose)
	{
		fputs("\r\n",stderr);
		if (exitcode)
			fputs(_("Transfer incomplete\n"),stderr);
		else
			fputs(_("Transfer complete\n"),stderr);
	}
        stats_account_finalize_file();
        stats_log(&stat_for_all,false);
        stats_journal_close();
	exit(exitcode);
}

LRZSZ_NORETURN static void
usage(int exitcode, const char *what)
{
	FILE *f=stdout;

	if (exitcode)
	{
		if (what)
			fprintf(stderr, "%s: %s\n",lrzsz_progname,what);
		fprintf (stderr, _("Try `%s -h' for more information.\n"),
			lrzsz_progname);
		exit(exitcode);
	}

	fprintf(f, _("%s version %s\n"), lrzsz_progname,
		VERSION);

	fprintf(f,_("Usage: %s [options] [filename.if.xmodem]\n"), lrzsz_progname);
	fputs(_("Receive files with ZMODEM/YMODEM/XMODEM protocol\n"),f);
	fputs(_(
		"    (X) = option applies to XMODEM only\n"
		"    (Y) = option applies to YMODEM only\n"
		"    (Z) = option applies to ZMODEM only\n"
		),f);
	fputs(_(
"  -+, --append                append to existing files\n"
"  -a, --ascii                 ASCII transfer (change CR/LF to LF)\n"
"  -A, --segmentsize           ask for an ACK every ... bytes\n"
"  -b, --binary                binary transfer\n"
"  -B, --bufsize N             buffer N bytes before writing to disk\n"
"  -c, --with-crc              Use 16 bit CRC (X)\n"
"  -D, --null                  write all received data to /dev/null\n"
"      --delay-startup N       sleep N seconds before doing anything\n"
"  -e, --escape                Escape control characters (Z)\n"
"  -E, --rename                rename any files already existing\n"
"  -h, --help                  Help, print this usage message\n"
"  -H, --crc-check             accept file if lengths and/or CRCs differ (Z)\n"
"  -J, --journal FILE          append transfer statistics to FILE\n"
"      --keep-incomplete       keep partially received files on error (default: delete)\n"
"  -m, --min-bps N             stop transmission if BPS below N\n"
"  -M, --min-bps-time N          for at least N seconds (default: 120)\n"
"  -n, --newer                 receive file if source newer (Z)\n"
"  -N, --newer-or-longer       receive file if source newer or longer (Z)\n"
"  -O, --disable-timeouts      disable timeout code, wait forever for data\n"
"      --o-sync                open output file(s) in synchronous write mode\n"
"  -p, --protect               protect existing files\n"
"  -q, --quiet                 quiet, no progress reports\n"
"  -r, --resume                try to resume interrupted file transfer (Z)\n"
"  -R, --restricted            restricted, more secure mode\n"
"  -s, --stop-at {HH:MM|+N}    stop transmission at HH:MM or in N seconds\n"
/* -S was timesync */
"      --syslog[=off]          turn syslog on or off, if possible\n"
"  -t, --timeout N             set timeout to N tenths of a second\n"
"  -u, --keep-uppercase        keep upper case filenames\n"
"  -v, --verbose               be verbose (twice: also show progress)\n"
"  --debug=MODULES             comma separated list: protocol, readline,\n"
"                              transfer, windowhandling, tty, all\n"
"  -V, --version               show version\n"
"  -w, --windowsize N          Window is N bytes (Z)\n"
"  -X  --xmodem                use XMODEM protocol\n"
"  -y, --overwrite             Yes, clobber existing file if any\n"
"  -g, --ymodem                use YMODEM protocol\n"
"  -Z, --zmodem                use ZMODEM protocol\n"
"\n"
"short options use the same arguments as the long ones\n"
	),f);
#ifndef HAVE_GETOPT_LONG
        fputs(_("The long options (--...) are not available on your system (getopt_long is not available)\n"), f);
#endif
	exit(exitcode);
}

/*
 * Let's receive something already.
 */

static int 
wcreceive(int argc, char **argp)
{
	int c;
	struct zm_fileinfo zi;
	const char *shortname=NULL;
	zi_init(&zi);

	/* forward decl: fubar is reached before Pathname's declaration line
	 * is ever executed (e.g. when the very first tryz() fails). */
	Pathname=NULL;

	if (protocol!=ZM_XMODEM || argc==0) {
		Crcflg=1;
		if ( !Quiet)
			vstringf(_("%s waiting to receive."), lrzsz_progname);
		if ((c=tryz())!=0) {
			if (c == ZCOMPL)
				return OK;
			if (c == ERROR)
				goto fubar;
			c = rzfiles(&zi);

			shortname=NULL;
			if (c)
				goto fubar;
		} else {
                        /* YMODEM path. */
			for (;;) {
			if (Verbose > 1 || enable_syslog)
				timing_reset();
			shortname=NULL;
				if (wcrxpn(&zi,secbuf)== ERROR)
					goto fubar;
				if (secbuf[0]==0)
					return OK;
				if (procheader(secbuf, &zi) == ERROR)
					goto fubar;
                                stats_set_filename(zi.fname);
				shortname=strrchr(zi.fname,'/');
				if (shortname)
					shortname++;
				else
					shortname=zi.fname;
				if (wcrx(&zi)==ERROR)
					goto fubar;

				report_transfer_result(0, shortname, zi.fname,
					(long) zi.bytes_received, (long) zi.bytes_total,
					(long) zi.bytes_skipped);
				/* stats_file_ok() already ran in wcrx(); don't count twice */
                                stats_account_one();
				/* the file is complete; a later fubar must not delete it */
				free(Pathname);
				Pathname=NULL;
			}
		}
	} else {
		char dummy[128] = ""; /* empty filename; procheader uses name + 1 + strlen(name) */
		zi.bytes_total = DEFBYTL;

		if (Verbose > 1 || enable_syslog)
		timing_reset();
		procheader(dummy, &zi);

		if (Pathname)
			free(Pathname);
		errno=0;
		/* argc may be 0 here (XMODEM with a bare -D; no filename on
		 * the command line). *argp would be NULL - the transfer target
		 * name is a mere placeholder in that case, and -D replaces it
		 * with /dev/null right below anyway. */
		Pathname=xstrdup(argc ? *argp : "stdin");

                checkpath(Pathname);
                if (Nflag) {
                        /* -D: write to /dev/null instead (as procheader()
                         * does for ZMODEM/YMODEM) */
                        free(Pathname);
                        Pathname=xstrdup("/dev/null");
                }
                stats_set_filename(Pathname);
                shortname=strrchr(Pathname,'/');
                if (shortname)
                    shortname++;
                else
                    shortname=Pathname;
                if (Verbose>1) {
                    vchar('\n');
                    vstringf(_("%s: ready to receive %s"), lrzsz_progname, Pathname);
                    vstring("\r\n");
                }

                if ((fout=safe_open(Pathname, "w", MKDIR_NO)) == NULL) {
                    DO_SYSLOG(LOG_ERR, "%s/%s: cannot open: %s",
                            lrzsz_basename(shortname), protname(), strerror(errno));
                    return ERROR;
                }
                if (wcrx(&zi)==ERROR) {
                    goto fubar;
                }
                /*  stat_for_file.files_ok++; done by wcrx. */
                stats_account_one();
                report_transfer_result(0, shortname, zi.fname,
                    (long) zi.bytes_received, (long) zi.bytes_total,
                    (long) zi.bytes_skipped);
		/* the file is complete */
		free(Pathname);
		Pathname=NULL;
        }
        return OK;
fubar:
        DO_SYSLOG(LOG_ERR, "%s/%s: got error",
                lrzsz_basename(shortname), protname());
        canit(STDOUT_FILENO);
        if (fout) {
            fclose(fout);
            fout = NULL;
        }

        if (Pathname && keep_incomplete) {
            /* the partial file is kept for later --resume; the message may
             * name a file never created (error before open) - harmless */
            vstringf(_("\r\n%s: %s kept (incomplete).\r\n"),
                lrzsz_progname, Pathname);
        } else if (Pathname) {
            struct stat st;
            /* Never unlink anything but a regular file: -D sets the
             * pathname to /dev/null, and any similar special case added
             * later must not turn a failed transfer into the deletion of
             * a device, socket, or symlink. */
            if (lstat(Pathname, &st) == 0 && S_ISREG(st.st_mode)) {
                unlink(Pathname);
                vstringf(_("\r\n%s: %s removed.\r\n"), lrzsz_progname, Pathname);
            }
        }
        stats_file_error();
        stats_account_one();
        return ERROR;
}


/*
 * this receives the YMODEM header.
 *
 * Fetch a pathname from the other end as a C ctyle ASCIZ string.
 * Length is indeterminate as long as less than Blklen
 * A null string represents no more files (YMODEM)
 */
static int
wcrxpn(struct zm_fileinfo *zi, char *rpn)
{
    int c;
    size_t Blklen=0;		/* record length of received packets */

    READLINE_PF(1);

    for (;;) {
        bool got_wceot;
        Firstsec=true;
        zi->eof_seen=false;
        sendline(Crcflg?WANTCRC:NAK);
        flushmo();
        purgeline(0); /* Do read next time ... */
        got_wceot = false;
        while ((c = wcgetsec(&Blklen, rpn, 100)) != 0) {
            if (c != WCEOT)
                return ERROR;
            zperr( _("Pathname fetch returned EOT"));
            stats_msg_out_of_sync();
            sendline(ACK);
            flushmo();
            purgeline(0);	/* Do read next time ... */
            READLINE_PF(1);
            got_wceot = true;
            break;
        }
        if (got_wceot)
            continue;		/* retry the pathname fetch */
        sendline(ACK);
        flushmo();
        return OK;
    }
}

/*
 * this is the XMODEM receiver.
 * this is the YMODEM body receiver.
 *
 * Historical note:
 * Adapted from CMODEM13.C, written by
 * Jack M. Wierda and Roderick W. Hart
 */
static int 
wcrx(struct zm_fileinfo *zi)
{
    int sectnum, sectcurr;
    char sendchar;
    size_t Blklen;

    Firstsec=true;sectnum=0; 
    zi->eof_seen=false;
    sendchar=Crcflg?WANTCRC:NAK;

    for (;;) {
        sendline(sendchar);	/* send it now, we're ready! */
        flushmo();
        purgeline(0);	/* Do read next time ... */
        sectcurr=wcgetsec(&Blklen, secbuf, 
                (unsigned int) ((sectnum&0x7f) ? 50 : 130));
        if (sectcurr==((sectnum+1) &0xff)) {
            sectnum++;
            /* report the real section/block number, not sectcurr
             * this is (a) more correct, and
             * (b) avoid overwriting 255 with 0 to create 055…
             */
            report(sectnum);
            /* if using xmodem we don't know how long a file is */
            if (zi->bytes_total && R_BYTESLEFT(zi) < Blklen)
                Blklen=R_BYTESLEFT(zi);
            zi->bytes_received+=Blklen;
            if (putsec(zi, secbuf, Blklen)==ERROR)
                return ERROR;
            sendchar=ACK;
        }
        else if (sectcurr==(sectnum&0xff)) {
            zperr( _("Received dup Sector"));
            sendchar=ACK;
            stats_msg_out_of_sync();
        }
        else if (sectcurr==WCEOT) {
            if (closeit(zi)) {
                stats_file_local_error();
                return ERROR;
            }
            stats_file_ok();
            sendline(ACK);
            flushmo();
            purgeline(0);	/* Do read next time ... */
            return OK;
        }
        else if (sectcurr==ERROR) /* wcgetsec accounted for stats. */
            return ERROR;
        else {
            zperr( _("Sync Error"));
            stats_msg_out_of_sync();
            return ERROR;
        }
    }
}

/*
 * Wcgetsec fetches a Ward Christensen type sector.
 * Returns sector number encountered or ERROR if valid sector not received,
 * or CAN CAN received
 * or WCEOT if eot sector
 * time is timeout for first char, set to 4 seconds thereafter
 ***************** NO ACK IS SENT IF SECTOR IS RECEIVED OK **************
 *    (Caller must do that when he is good and ready to get next sector)
 */
static int
wcgetsec(size_t *Blklen, char *rxbuf, unsigned int maxtime)
{
    int checksum, wcj, firstch, c;
    char *p;
    int sectcurr;

    for (Lastrx=errors=0; errors<RETRYMAX; errors++) {
        bool timed_out = false;

        if ((firstch=READLINE_PF(maxtime))==STX || firstch==SOH) {
            *Blklen = (firstch==STX) ? 1024 : 128;
            sectcurr=READLINE_PF(1);
            if ((sectcurr+READLINE_PF(1))==0xff) {
                unsigned short oldcrc;
                oldcrc=checksum=0;
                for (p=rxbuf,wcj=*Blklen; --wcj>=0; ) {
                    if ((firstch=READLINE_PF(1)) < 0) {
                        timed_out = true;
                        break;
                    }
                    oldcrc=updcrc(firstch, oldcrc);
                    checksum += (*p++ = firstch);
                }
                if (!timed_out) {
                    if ((firstch=READLINE_PF(1)) < 0)
                        timed_out = true;
                    else if (Crcflg) {
                        oldcrc=updcrc(firstch, oldcrc);
                        stats_msg_bytes(*Blklen);
                        if ((firstch=READLINE_PF(1)) < 0)
                            timed_out = true;
                        else {
                            oldcrc=updcrc(firstch, oldcrc);
                            if (oldcrc != 0) {
                                stats_msg_invalid();
                                zperr( _("CRC"));
                            } else {
                                Firstsec=false;
                                stats_msg_valid(0);
                                return sectcurr;
                            }
                        }
                    } else {
                        stats_msg_bytes(*Blklen);
                        if (((checksum-firstch)&0xff)==0) {
                            Firstsec=false;
                            stats_msg_valid(0);
                            return sectcurr;
                        }
                        else {
                            stats_msg_invalid();
                            zperr( _("Checksum"));
                        }
                    }
                }
            }
            else {
                stats_msg_format_error();
                zperr(_("Sector number garbled"));
            }
        }
        /* make sure eot really is eot and not just mixmash */
        else if (firstch==EOT
                 && ((c = READLINE_PF(1)) == TIMEOUT || c == RCDO))
            return WCEOT;
        else if (firstch==CAN) {
            if (Lastrx==CAN) {
                zperr( _("Sender Cancelled"));
                stats_cancel();
                return ERROR;
            } else {
                Lastrx=CAN;
                continue;
            }
        }
        else if (firstch==TIMEOUT) {
            if (!Firstsec)
                timed_out = true;   /* a timeout in the first sector is not counted */
        } else if (firstch==RCDO) {
            /* the line died (peer closed, carrier lost): do not burn the
             * retry budget NAKing into the void */
            zperr( _("Line dropped"));
            return ERROR;
        } else {
            zperr( _("Got 0x%x sector header"), (unsigned int) firstch);
        }

        if (timed_out) {
            stats_timeout();
            zperr( _("TIMEOUT"));
        }
        Lastrx=0;
        {
            int cnt=1000;
            int dc;
            /* drain garbage, but give up immediately if the line is gone */
            while(cnt-- && (dc=READLINE_PF(1))!=TIMEOUT && dc!=ERROR && dc!=RCDO)
                ;
        }
        if (Firstsec) {
            sendline(Crcflg?WANTCRC:NAK);
            flushmo();
            purgeline(0);	/* Do read next time ... */
        } else {
            maxtime=40;
            sendline(NAK);
            flushmo();
            purgeline(0);	/* Do read next time ... */
        }
    }
    /* try to stop the bubble machine. */
    canit(STDOUT_FILENO);
    stats_too_many_errors();
    return ERROR;
}

#define ZCRC_DIFFERS (ERROR+1)
#define ZCRC_EQUAL (ERROR+2)
/*
 * do ZCRC-Check for open file f.
 * check at most check_bytes bytes (crash recovery). if 0 -> whole file.
 * remote file size is remote_bytes.
 */
static int 
do_crc_check(FILE *f, size_t remote_bytes, size_t check_bytes) 
{
    struct stat st;
    unsigned long crc;
    unsigned long rcrc;
    size_t n;
    int c;
    int t1=0,t2=0;
    if (-1==fstat(fileno(f),&st)) {
        stats_file_local_error();
        DO_SYSLOG(LOG_ERR,"cannot fstat open file: %s",strerror(errno));
        return ERROR;
    }
    if (check_bytes==0 && ((size_t) st.st_size)!=remote_bytes)
        return ZCRC_DIFFERS; /* shortcut */

    crc=0xFFFFFFFFL;
    n=check_bytes;
    if (n==0)
        n=st.st_size;
    while (n-- && ((c = getc(f)) != EOF))
        crc = UPDC32(c, crc);
    crc = ~crc;
    clearerr(f);  /* Clear EOF */
    fseeko(f, 0, 0);

    while (t1<3) {
        stohdr(check_bytes);
        zshhdr(ZCRC, Txhdr);
        t2=0;
        while(t2<3) {
            size_t tmp;
            c = zgethdr(Rxhdr, 0, &tmp);
            rcrc=(unsigned long) tmp;
            switch (c) {
                case TIMEOUT:
                    stats_timeout();
                    break;
                case ERROR:
                default: /* ignore */
                    break;
                case ZFIN:
                    stats_msg_out_of_sync();
                    return ERROR;
                case ZRINIT:
                    stats_msg_out_of_sync();
                    return ERROR;
                case ZCAN:
                    if (Verbose)
                        vstringf(_("got ZCAN"));
                    stats_cancel();
                    return ERROR;
                    break;
                case ZCRC:
                    if (crc!=rcrc)
                        return ZCRC_DIFFERS;
                    return ZCRC_EQUAL;
                    break;
            }
            t2++;
        }
        t1++;
    }
    stats_too_many_errors();
    return ERROR;
}

/*
 * Open a file w/o following symlinks, resolving relative to the CWD.
 * - O_NOFOLLOW prevents the final path component from being a symlink;
 * - every intermediate directory component is opened with O_DIRECTORY|O_NOFOLLOW,
 *   so a symlinked directory component fails instead of being followed
 * - with mkdir_ok, missing intermediate directories are created via mkdirat().
 * Maps fopen-style mode strings to open() flags.
 * This is complicated, but race free.
 *
 * @return FILE *|NULL
 */
/*
 * Map fopen-style mode strings to open() flags.
 */
static int
fopen_mode_to_flags(const char *mode)
{
    int flags = 0;
    const char *p;

    for (p = mode; *p; p++) {
        switch (*p) {
            case 'r':
                flags |= O_RDONLY;
                break;
            case 'w':
                flags |= O_WRONLY | O_CREAT | O_TRUNC;
                break;
            case 'a':
                flags |= O_WRONLY | O_CREAT | O_APPEND;
                break;
            case '+':
                flags &= ~(O_RDONLY | O_WRONLY);
                flags |= O_RDWR;
                break;
            default:
                break;
        }
    }
    return flags;
}

/*
 * Open path (relative to the CWD) without following symlinks.
 * Walks intermediate components with openat(); the final component
 * is opened with extra_flags ored in (caller supplies O_CREAT etc.).
 * Returns an fd, or -1 with errno set.
 * - O_NOFOLLOW prevents the final path component from being a symlink;
 * - every intermediate directory component is opened with O_DIRECTORY|O_NOFOLLOW,
 *   so a symlinked directory component fails instead of being followed
 * - with mkdir_ok, missing intermediate directories are created via mkdirat().
 * This is complicated, but race free.
 *
 * @return fd, -1 on error
 */
static int
safe_open_fd(const char *path, int open_flags, enum safe_mkdir mkdir_ok)
{
    int dirfd;
    int fd;
    int saved_errno;

    open_flags |= O_NOFOLLOW;

    if (path[0] == '/') {
        /* -D: /dev/null is the one absolute path we deliberately open
         * (a device node; open_flags already carry O_NOFOLLOW). Anything
         * else absolute is rejected by checkpath(). */
        if (0 == strcmp(path, "/dev/null"))
            return open(path, open_flags, 0666);
        errno = EACCES;
        return -1;
    }

    dirfd = open(".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (dirfd < 0)
        return -1;

    {
        /* walk intermediate components with openat(), creating them if asked */
        char *tmp = xstrdup(path);
        char *start = tmp;
        char *end = tmp + strlen(tmp);
        char *slash;
        const char *last;

        while ((slash = memchr(start, '/', (size_t) (end - start))) != NULL) {
            if (slash == start) { /* leading or doubled '/' */
                start = slash + 1;
                continue;
            }
            *slash = '\0';
            fd = openat(dirfd, start, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
            if (fd < 0 && errno == ENOENT && mkdir_ok == MKDIR_OK) {
                if (mkdirat(dirfd, start, 0700) == 0 || errno == EEXIST)
                    fd = openat(dirfd, start, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
            }
            if (fd < 0) {
                close(dirfd);
                free(tmp);
                return -1; /* ELOOP: symlinked component; ENOENT; ... */
            }
            close(dirfd);
            dirfd = fd;
            start = slash + 1;
        }
        last = start; /* final component, relative to dirfd */
        if (*last == '\0') { /* path ended in '/', e.g. "sub/" */
            close(dirfd);
            free(tmp);
            errno = EISDIR;
            return -1;
        }
        if (last[0] == '.' && (last[1] == '\0' ||
                               (last[1] == '.' && last[2] == '\0'))) {
            close(dirfd);
            free(tmp);
            errno = EACCES;
            return -1;
        }
        fd = openat(dirfd, last, open_flags, 0666);
        saved_errno = errno;
        close(dirfd);
        free(tmp);
        errno = saved_errno;
        return fd;
    }
}

static FILE *
safe_open(const char *path, const char *mode, enum safe_mkdir mkdir_ok)
{
    int fd;
    FILE *fp;

    fd = safe_open_fd(path, fopen_mode_to_flags(mode), mkdir_ok);
    if (fd < 0)
        return NULL;
    fp = fdopen(fd, mode);
    if (!fp)
        close(fd);
    return fp;
}

/*
 * Atomically claim a free "name.N" (N = 0..999) for ZMCHNG rename,
 * creating it with O_CREAT|O_EXCL (race free: no stat-then-use).
 * Returns a FILE* open for writing, and the claimed name in *claimed
 * (xmalloc'ed). On failure returns NULL: errno==EEXIST means all 1000
 * names are taken (policy reject), anything else is a local error.
 */
static FILE *
claim_rename_target(const char *name, char **claimed)
{
    size_t newnamelen = strlen(name) + 5;
    char *tmpname = xmalloc(newnamelen);
    int i;
    int fd = -1;
    FILE *fp;

    for (i = 0; i < 1000; i++) {
        snprintf(tmpname, newnamelen, "%s.%d", name, i);
        fd = safe_open_fd(tmpname, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, MKDIR_NO);
        if (fd >= 0)
            break;
        if (errno != EEXIST)
            break; /* EACCES, ELOOP (symlink game), ...: give up */
        /* name.N taken, try the next suffix */
    }
    if (i == 1000) {
        errno = EEXIST; /* exhausted all suffixes */
        free(tmpname);
        return NULL;
    }
    if (fd < 0) { /* non-EEXIST error, errno preserved */
        free(tmpname);
        return NULL;
    }
    fp = fdopen(fd, "w");
    if (!fp) {
        int saved_errno = errno;
        close(fd);
        unlink(tmpname);
        errno = saved_errno;
        free(tmpname);
        return NULL;
    }
    *claimed = tmpname;
    return fp;
}

/*
 * Process incoming file information header
 * returns OK or ERROR.
 */
static int
procheader(char *name, struct zm_fileinfo *zi)
{
    const char *openmode;
    char *p;
    static char *name_static=NULL;
    char *nameend;
    stats_start_one();

    if (name_static)
        free(name_static);
    if (junk_path) {
        p=strrchr(name,'/');
        if (p) {
            p++;
            if (!*p) {
                /* alert - file name ended in with a / */
                if (Verbose)
                    vstringf(_("file name ends with a /, skipped: %s\n"),name);
                DO_SYSLOG(LOG_ERR,"file name ends with a /, skipped: %s", name);
                stats_file_policy_reject();
                return ERROR;
            }
            name=p;
        }
    }
    name_static=xstrdup(name);
    zi->fname=name_static;

    DPRINTF(DEBUG_TRANSFER, "zmanag=%d, Lzmanag=%d\n", zmanag, Lzmanag);
    DPRINTF(DEBUG_TRANSFER, "zconv=%d\n", zconv);

    /* set default parameters and overrides */
    openmode = "w";
    Thisbinary = (!Rxascii) || Rxbinary;
    if (Lzmanag)
        zmanag = Lzmanag;

    /*
     *  Process ZMODEM remote file management requests
     */
    /* NOTE: zconv == ZCNL (2) is honored below for compatibility with
     * 40 year old peers, although the current ZMODEM documentation says
     * a receiver MUST assume ZCBIN for anything but ZCBIN/ZCRESUM.
     * See man page -a/--ascii: deprecated. */
    if (!Rxbinary && zconv == ZCNL)	/* Remote ASCII override */
        Thisbinary = 0;
    if (zconv == ZCBIN)	/* Remote Binary override */
        Thisbinary = true;
    if (Thisbinary && zconv == ZCBIN && try_resume)
        zconv=ZCRESUM;
    if (zmanag == ZF1_ZMAPND && zconv!=ZCRESUM)
        openmode = "a";
    if (skip_if_not_found)
        openmode="r+";

    zi->bytes_total = DEFBYTL;
    zi->mode = 0; 
    zi->eof_seen = 0; 
    zi->modtime = 0;

    nameend = name + 1 + strlen(name);
    if (*nameend) {	/* file coming from Unix or DOS system */
        unsigned long modtime = 0;
        long bytes_total = 0;
        unsigned long mode = 0;
        if (sscanf(nameend, "%ld%lo%lo", &bytes_total, &modtime, &mode) >= 1) {
            zi->modtime = (time_t) modtime;
            zi->bytes_total = (size_t) bytes_total;
            zi->mode = (mode_t) mode;
            /* NOTE: acting on the historical full-st_mode marker below is
             * a SHOULD NOT per doc/zmodem-wip.txt §4 ("file access
             * rights"), kept for compatibility with old peers. See also
             * the mode checks in closeit()/S1. */
            if (zi->mode & S_IFMT)
                ++Thisbinary;
        }
    }

	/* Security: validate the sender-chosen name BEFORE any open of it.
	 * safe_open_fd() deliberately allows interior ".." components, so a
	 * pre-check open or the -H CRC probe below would otherwise probe and
	 * read files outside the receive directory, leaking existence and
	 * (via do_crc_check) content fingerprints to the peer. checkpath()
	 * must run first. No-op for the empty XMODEM placeholder name. */
	if (*name_static)
		checkpath(name_static);

	/* Check for existing file */
	if (zconv != ZCRESUM && !Rxclob && (zmanag&ZF1_ZMMASK) != ZF1_ZMCLOB
            && (zmanag&ZF1_ZMMASK) != ZF1_ZMAPND
            && (fout=safe_open(name, "r", MKDIR_NO))) {
        struct stat sta;
        char *tmpname;
        if (zmanag == ZF1_ZMNEW || zmanag==ZF1_ZMNEWL) {
            if (-1==fstat(fileno(fout),&sta)) {
                int e=errno;
                if (Verbose)
                    vstringf(_("file exists, skipped: %s\n"),name);
                DO_SYSLOG(LOG_ERR,"cannot fstat open file %s: %s",
                            name,strerror(e));
                stats_file_local_error();
                fclose(fout);
                fout = NULL;
                return ERROR;
            }
            if (zmanag == ZF1_ZMNEW) {
                if (sta.st_mtime > zi->modtime) {
                    DO_SYSLOG(LOG_INFO,"skipping %s: newer file exists", name);
                    fclose(fout);
                    fout = NULL;
                    stats_file_skipped();
                    return ERROR; /* skips file */
                }
            } else {
                /* newer-or-longer */
                if (((size_t) sta.st_size) >= zi->bytes_total
                        && sta.st_mtime > zi->modtime) {
                    DO_SYSLOG(LOG_INFO,"skipping %s: longer+newer file exists", name);
                    fclose(fout);
                    fout = NULL;
                    stats_file_skipped();
                    return ERROR; /* skips file */
                }
            }
            fclose(fout);
            fout = NULL;
        } else if (zmanag==ZF1_ZMCRC) {
            int r=do_crc_check(fout,zi->bytes_total,0);
            if (r==ERROR) {
                fclose(fout);
                fout = NULL;
                return ERROR;
            }
            if (r!=ZCRC_DIFFERS) {
                fclose(fout);
                fout = NULL;
                stats_file_skipped();
                return ERROR; /* skips */
            }
            fclose(fout);
            fout = NULL;
        } else {
            fclose(fout);
            fout = NULL;
            if ((zmanag & ZF1_ZMMASK)!=ZF1_ZMCHNG) {
                stats_file_skipped();
                if (Verbose)
                    vstringf(_("file exists, skipped: %s\n"),name);
                return ERROR;
            }
            /* try to rename: atomically claim "name.N" via O_CREAT|O_EXCL
             * (no stat-then-use race), keeping the claimed file open */
            fout = claim_rename_target(name, &tmpname);
            if (!fout) {
                if (errno == EEXIST) {
                    /* all 1000 names taken */
                    stats_file_policy_reject();
                } else {
                    stats_file_local_error();
                    if (Verbose)
                        vstringf(_("cannot claim %s.N: %s\n"),name,strerror(errno));
                }
                return ERROR;
            }
            free(name_static);
            name_static=tmpname;
            zi->fname=name_static;
        }
    }

    /* if a rename target was claimed, the final name is already fixed;
     * don't run the CP/M and lower-case transformations on it */
    if (!fout) {
    if (!*nameend) {		/* File coming from CP/M system */
        for (p=name_static; *p; ++p)		/* change / to _ */
            if ( *p == '/')
                *p = '_';

        if (p > name_static && *--p == '.')		/* zap trailing period */
            *p = 0;
    }

    /* NOTE: the S_IFMT check is the same legacy marker as above (V4):
     * a full st_mode from an old sender means "Unix peer, keep case".
     * Deprecated per doc/zmodem-wip.txt §4, kept for old peers. */
    if (!zmodem_requested && MakeLCPathname && !IsAnyLower(name_static)
            && !(zi->mode&S_IFMT))
        uncaps(name_static);
    }

    /* the received name is about to be replaced (or discarded for the
     * XMODEM dummy case); Pathname must not point at the previous file
     * from here on - a fubar between here and the new assignment below
     * would otherwise act on that older file's path. */
    free(Pathname);
    Pathname=NULL;

    if (protocol==ZM_XMODEM)
        /* we don't have the filename yet */
        return OK; /* dummy */
    Pathname=xstrdup(name_static);
    if (Verbose) {
        /* overwrite the "waiting to receive" line */
        vstring("\r                                                                     \r");
        vstringf(_("Receiving: %s\n"), name_static);
    }

	/* checkpath() already ran before any open (see above). */
        if (Nflag)
        {
                /* write to /dev/null instead; if a rename target was claimed
                 * already, discard it */
                if (fout) {
                        fclose(fout);
                        fout=NULL;
                }
                free(name_static);
                name_static=xstrdup("/dev/null");
                zi->fname=name_static; /* was left dangling at the freed name */
        }

        if (!fout && Thisbinary && zconv==ZCRESUM) {
                struct stat st;
                fout = safe_open(name_static, "r+", MKDIR_NO);
                if (fout && 0==fstat(fileno(fout),&st))
                {
                        int can_resume=true;
                        if (zmanag==ZF1_ZMCRC) {
                                int r=do_crc_check(fout,zi->bytes_total,st.st_size);
                                if (r==ERROR) {
                                        fclose(fout);
                                        fout=NULL;
                                        return ERROR;
                                }
                                if (r==ZCRC_DIFFERS) {
                                        can_resume=false;
                                }
                        }
                        if ((unsigned long)st.st_size > zi->bytes_total) {
                                can_resume=false;
                        }
                        /* retransfer whole blocks */
                        zi->bytes_skipped = st.st_size & ~(1023);
                        if (can_resume) {
                                if (fseeko(fout, (off_t) zi->bytes_skipped, SEEK_SET)) {
                                        stats_file_local_error();
                                        fclose(fout);
                                        fout=NULL;
                                        return ERROR;
                                }
                                /* resume from the recorded offset; must not
                                 * run the bytes_skipped=0 reset below */
                                return setup_output_buffer(zi, fout);
                        }
                        else {
                                /* resume impossible, file has changed:
                                 * fall through to the normal open below, which
                                 * truncates (openmode "w" -> O_TRUNC).
                                 * Overwriting the "r+" handle without
                                 * truncation would leave a stale tail of
                                 * the old file if the new one is shorter. */
                                fclose(fout);
                                fout=NULL;
                        }
                }
                zi->bytes_skipped=0;
                if (fout) {
                        fclose(fout);
                        fout=NULL;
                }
        }
        /* fout can already be open here: claim_rename_target() claimed
         * and created "name.N" atomically; reuse that file. */
#ifdef ENABLE_MKDIR
        if (!fout && Restricted < 2)
                fout = safe_open(name_static, openmode, MKDIR_OK);
        else
#endif
        if (!fout)
                fout = safe_open(name_static, openmode, MKDIR_NO);
        if ( !fout)
        {
                int e=errno;
                stats_file_local_error();
                zpfatal(_("cannot open %s"), name_static);
                DO_SYSLOG(LOG_ERR,"%s: cannot open: %s",
                        protname(),strerror(e));
                return ERROR;
        }

        return setup_output_buffer(zi, fout);
}

/* Apply o_sync and the (once-allocated) large stdio buffer to the output
 * file, and record the receive start offset. Called from both the normal
 * open path and the ZCRESUM resume path of procheader(); the statics
 * deliberately persist across files. The global fout is passed explicitly
 * (the new stream, already assigned) to keep this helper self-contained. */
static int
setup_output_buffer(struct zm_fileinfo *zi, FILE *fp)
{
        static char *s=NULL;
        static size_t last_length=0;
#ifdef O_SYNC
        if (o_sync) {
                int oldflags;
                oldflags = fcntl (fileno(fp), F_GETFL, 0);
                if (oldflags>=0 && !(oldflags & O_SYNC)) {
                        oldflags|=O_SYNC;
                        fcntl (fileno(fp), F_SETFL, oldflags); /* errors don't matter */
                }
        }
#endif

        if (!s && buffersize) {
                last_length=buffersize;
                /* buffer `4096' bytes pages */
                last_length=(last_length+4095)&0xfffff000;
                s=xmalloc(last_length);
        }
        if (s) {
                setvbuf(fp,s,_IOFBF,last_length);
        }
        zi->bytes_received=zi->bytes_skipped;

        return OK;
}

/*
 * Putsec writes the n characters of buf to receive file fout.
 *  If not in binary mode, carriage returns, and all characters
 *  starting with CPMEOF are discarded.
 */
static int 
putsec(struct zm_fileinfo *zi, char *buf, size_t n)
{
	char *p;

	if (n == 0)
		return OK;
	if (Thisbinary) {
		if (fwrite(buf,n,1,fout)!=1)
			return ERROR;
	}
	else {
		if (zi->eof_seen)
			return OK;
		for (p=buf; n>0; ++p,n-- ) {
			if ( *p == '\r')
				continue;
			if (*p == CPMEOF) {
				zi->eof_seen=true;
				return OK;
			}
			putc(*p ,fout);
		}
	}
	return OK;
}

/* make string s lower case */
static void
uncaps(char *s)
{
	for ( ; *s; ++s)
		if (isupper((unsigned char)(*s)))
			*s = tolower((unsigned char)(*s));
}
/*
 * IsAnyLower returns true if string s has lower case letters.
 */
static int 
IsAnyLower(const char *s)
{
	for ( ; *s; ++s)
		if (islower((unsigned char)(*s)))
			return true;
	return false;
}

static void
report(int sct)
{
	if (Verbose>1)
	{
		vstringf(_("Blocks received: %d"),sct);
		vchar('\r');
	}
}

/* chkinvok() lives in util.c (shared with lsz); called with 'r' here. */

static void
checkpath(const char *name)
{
	if (!Restricted)
		return;

	/* don't overwrite any file in very restricted mode.
	 * don't overwrite hidden files in restricted mode */
	{
		FILE *noleak = NULL;
		const char *bn;
		bn = strrchr(name, '/');
		bn = bn ? bn + 1 : name;
		if ((Restricted == 2 || *bn == '.') && (noleak = safe_open(name, "r", MKDIR_NO)) != NULL) {
			fclose(noleak);
			canit(STDOUT_FILENO);
			vstring("\r\n");
			vstringf(_("%s: %s exists\n"),
				lrzsz_progname, name);
			bibi(-1);
		}
	}

	if (lrzsz_path_component_violation(name, Restricted))
		goto violation;

	return;

violation:
	canit(STDOUT_FILENO);
	vstring("\r\n");
	vstringf(_("%s:\tSecurity Violation"), lrzsz_progname);
	vstring("\r\n");
	bibi(-1);
}

/*
 * Initialize for Zmodem receive attempt, try to activate Zmodem sender
 *  Handles ZSINIT frame
 *  Return ZFILE if Zmodem filename received, -1 on error,
 *   ZCOMPL if transaction finished,  else 0
 */
static int
tryz(void)
{
	int c, n;
	int zrqinits_received=0;
	size_t bytes_in_block=0;

	if (protocol!=ZM_ZMODEM)		/* Check for "rb" program name */
		return 0;

	for (n=zmodem_requested?15:5; 
		 (--n + zrqinits_received) >=0 && zrqinits_received<10; ) {
		/* Set buffer length and capability flags */
                stohdr(0L);
                if (tryzhdrtype==ZRINIT) {
                    /* special short number */
		    Txhdr[ZP0] = Segmentsize & 0xff;
		    Txhdr[ZP1] = (Segmentsize / 256) & 0xff;
                }
		/* advertise CANBRK only if the transfer line is a terminal:
		 * a break is a termios concept, unavailable on pipes/sockets
		 * (doc/zmodem-wip.txt §10.6: "if you have tcsendbreak and
		 * send via a terminal, set this") */
		if (isatty(0))
			Txhdr[ZF0] = CANFC32|CANFDX|CANOVIO|CANBRK;
		else
			Txhdr[ZF0] = CANFC32|CANFDX|CANOVIO;
 		if (Zctlesc)
 			Txhdr[ZF0] |= TESCCTL; /* TESCCTL == ESCCTL */
 		zshhdr(tryzhdrtype, Txhdr);

		if (tryzhdrtype == ZSKIP)	/* Don't skip too far */
			tryzhdrtype = ZRINIT;	/* CAF 8-21-87 */
again:
		switch (zgethdr(Rxhdr, 0, NULL)) {
		case ZRQINIT:
			/* getting one ZRQINIT is totally ok. Normally a ZFILE follows 
			 * (and might be in our buffer, so don't purge it). But if we
			 * get more ZRQINITs the sender has started before us
			 * and sent ZRQINITs while waiting. 
			 */
			zrqinits_received++;
			continue;
		
		case ZEOF:
			/* A ZEOF between files can only be a duplicate of the
			 * just-completed file: the sender never saw our ZRINIT.
			 * Answer at once instead of silently waiting for the
			 * next header, so a sender stuck in a retransmit loop
			 * (e.g. zmtx 1.02, which retransmits without pacing
			 * while its input buffer holds queued frames) gets out
			 * of its ZEOF wait one round earlier. */
			zshhdr(ZRINIT, Txhdr);
			continue;
		case TIMEOUT:
			continue;
		case RCDO:
			/* the line is gone (peer closed, carrier lost).
			 * Retrying into the void would burn the whole
			 * n-budget and then return 0, which sends the caller
			 * down the YMODEM path against a dead line. Also:
			 * re-handshaking after a reconnect would be wrong -
			 * the calling program (bbs, cu, ...) may have
			 * authenticated the original peer, and whoever is on
			 * the reconnected line may be someone different. */
			DO_SYSLOG(LOG_INFO, "%s: RCDO during ZMODEM handshake",
					   protname());
			return ERROR;
		case ZFILE:
			zconv = Rxhdr[ZF0];
			if (!zconv)
				/* resume with sz -r is impossible (at least with unix sz)
				 * if this is not set */
				zconv=ZCBIN;
			if (Rxhdr[ZF1] & ZF1_ZMSKNOLOC) {
				Rxhdr[ZF1] &= ~(ZF1_ZMSKNOLOC);
				skip_if_not_found=true;
			}
			zmanag = Rxhdr[ZF1];
			ztrans = Rxhdr[ZF2];
			tryzhdrtype = ZRINIT;
			c = zrdata(secbuf, LRZSZ_MAX_SUBPACKET_LEN, &bytes_in_block);
			io_mode(0,3);
			if (c == GOTCRCW)
				return ZFILE;
			zshhdr(ZNAK, Txhdr);
			goto again;
		case ZSINIT:
			/* this once was:
			 * Zctlesc = TESCCTL & Rxhdr[ZF0];
			 * trouble: if rz get --escape flag:
			 * - it sends TESCCTL to sz, 
			 *   get a ZSINIT _without_ TESCCTL (yeah - sender didn't know), 
			 *   overwrites Zctlesc flag ...
			 * - sender receives TESCCTL and uses "|=..."
			 * so: sz escapes, but rz doesn't unescape ... not good.
			 */
 			Zctlesc |= TESCCTL & Rxhdr[ZF0];
 			/* rebuild the send table on any flag change - lrz
 			 * sends headers/ACKs through zsendline, and a
 			 * stale table would send the bytes wrong. This
 			 * also fixes the long-standing staleness for
 			 * late-arriving ESCCTL (the table was only
 			 * initialized once at startup). */
 			zsendline_init();
  			if (zrdata(Attn, ZATTNLEN,&bytes_in_block) == GOTCRCW) {
 				stohdr(0L); /* ZACK to a ZSINIT must carry 0 (see zmodem-wip.txt §4) */
 				zshhdr(ZACK, Txhdr);
 				goto again;
 			}
			zshhdr(ZNAK, Txhdr);
			goto again;
		case ZFREECNT:
			stohdr(0);
			zshhdr(ZACK, Txhdr);
			goto again;
		case ZCOMMAND:
			if (zrdata(secbuf, LRZSZ_MAX_SUBPACKET_LEN, &bytes_in_block) == GOTCRCW) {
				if (Verbose) {
					vstringf("%s: %s\n", lrzsz_progname,
						_("remote command execution denied"));
					vstringf("%s: %s\n", lrzsz_progname, secbuf);
				}
                                stohdr(1L); /* be nice and tell the other side that the execution failed. */
                                zshhdr(ZCOMPL, Txhdr);
                                DO_SYSLOG(LOG_INFO,"rexec denied: %s",secbuf);
                                return ZCOMPL;
			}
			zshhdr(ZNAK, Txhdr);
			goto again;
		case ZCOMPL:
			goto again;
		default:
			continue;
		case ZFIN:
			ackbibi();
			return ZCOMPL;
		case ZRINIT:
			if (Verbose)
				vstringf(_("got ZRINIT"));
			return ERROR;
		case ZCAN:
			if (Verbose)
				vstringf(_("got ZCAN"));
			return ERROR;
		}
	}
	return 0;
}


/*
 * Receive 1 or more files with ZMODEM protocol
 */
static int
rzfiles(struct zm_fileinfo *zi)
{
	int c;

	for (;;) {
		timing_reset();
		c = rzfile(zi);
		switch (c) {
		case ZEOF:
			report_transfer_result(0, NULL, zi->fname,
				(long) zi->bytes_received, (long) zi->bytes_total,
				(long) zi->bytes_skipped);
                        stats_file_ok();
                        stats_account_one();
			/* the file is complete; a later fubar must not delete it */
			free(Pathname);
			Pathname=NULL;
			break;
		case ZSKIP:
                        stats_file_skipped();
                        stats_account_one();
                        if (Verbose) 
                                vstringf(_("Skipped"));
                        DO_SYSLOG(LOG_INFO, "%s/%s: skipped",lrzsz_basename(zi->fname),protname());
			/* the file was skipped, not written; a later fubar must
			 * not delete a pre-existing file */
			free(Pathname);
			Pathname=NULL;
			break;
		default:
		case ERROR:
                        stats_file_error();
                        stats_account_one();
			DO_SYSLOG(LOG_INFO, "%s/%s: error",lrzsz_basename(zi->fname),protname());
			return ERROR;
		}
                switch (tryz()) {
                case ZCOMPL:
                        return OK;
                default:
                        return ERROR;
                case ZFILE:
                        break;
                }
	}
}

/*
 * Receive a file with ZMODEM protocol
 *  Assumes file name frame is in secbuf
 * returns: ERROR ZSIKP ZEOF
 *
 * The control flow is an explicit three-state machine:
 *  ST_ZRPOS - send ZRPOS (on entry and after every retry)
 *  ST_HDR   - wait for a header (former nxthdr label)
 *  ST_DATA  - receive data subpackets (former moredata label)
 */
enum rzfile_state { ST_ZRPOS, ST_HDR, ST_DATA };

static int
rzfile(struct zm_fileinfo *zi)
{
	int c, n;
	long last_rxbytes=0;
	unsigned long last_bps=0;
	long not_printed=0;
	double low_bps=0;
	size_t bytes_in_block=0;
	int state = ST_ZRPOS;
	unsigned int saved_rxtimeout = Rxtimeout;
	int pos_waits = 0;
        time_t now;

	zi->eof_seen=false;

	n = 20;

	if (procheader(secbuf,zi) == ERROR) {
		DO_SYSLOG(LOG_INFO, "%s/%s: procheader error",
				   lrzsz_basename(zi->fname),protname());
		return (tryzhdrtype = ZSKIP);
	}
        stats_set_filename(zi->fname);

	for (;;) {
		switch (state) {
		case ST_ZRPOS:
			stohdr(zi->bytes_received);
			zshhdr(ZRPOS, Txhdr);
			/* The ZDATA answering the ZRPOS is either already in
			 * flight or was lost on the line. Waiting the full
			 * Rxtimeout (by default 100 tenths of a second) for
                         * a lost frame costs 10s per resync, during which the
                         * the sender may wait. Therefore we shorten the first
                         * few waits after a ZRPOS, with a fallback to the
			 * full timeout for slow peers. pos_waits counts
			 * consecutive short waits. */
			pos_waits = 0;
			Rxtimeout = 10;
			/* fall through */
		case ST_HDR:
			c = zgethdr(Rxhdr, 0, NULL);
			if (pos_waits < 4)
				pos_waits++;
			else
				Rxtimeout = saved_rxtimeout;
			switch (c) {
			default:
				Rxtimeout = saved_rxtimeout;
				DO_SYSLOG(LOG_INFO, "%s/%s: error: zgethdr returned %d", lrzsz_basename(zi->fname),
					   protname(), c);
				DPRINTF(DEBUG_TRANSFER, "rzfile: zgethdr returned %d\n", c);
				return ERROR;
			case ZNAK:
			case TIMEOUT:
				if ( --n < 0) {
					Rxtimeout = saved_rxtimeout;
					DO_SYSLOG(LOG_INFO, "%s/%s: error: zgethdr returned %s", lrzsz_basename(zi->fname),
						   protname(), c == ZNAK ? "ZNAK" : "TIMEOUT");
					DPRINTF(DEBUG_TRANSFER, "rzfile: zgethdr returned %d\n", c);
					return ERROR;
				}
                                /* traditionally we fell through to the ZFILE block and did an ZRDATA,
                                 * but that is wrong.
                                 */
				state = ST_ZRPOS;
				continue;
			case ZFILE:
				Rxtimeout = saved_rxtimeout;
				zrdata(secbuf, LRZSZ_MAX_SUBPACKET_LEN, &bytes_in_block);
				state = ST_ZRPOS;
				continue;
			case ZEOF:
				Rxtimeout = saved_rxtimeout;
				if (rclhdr(Rxhdr) != (unsigned long) zi->bytes_received) {
					/*
					 * Ignore eof if it's at wrong place - force
					 *  a timeout because the eof might have gone
					 *  out before we sent our zrpos.
					 */
					errors = 0;
					state = ST_HDR;
					continue;
				}
				if (closeit(zi)) {
					tryzhdrtype = ZFERR;
					DO_SYSLOG(LOG_INFO, "%s/%s: error: closeit return <>0",
							   lrzsz_basename(zi->fname), protname());
					DPRINTF(DEBUG_TRANSFER, "rzfile: closeit returned <> 0\n");
					return ERROR;
				}
				DPRINTF(DEBUG_TRANSFER, "rzfile: normal EOF\n");
				return c;
			case ERROR:	/* Too much garbage in header search error */
				Rxtimeout = saved_rxtimeout;
				if ( --n < 0) {
					DO_SYSLOG(LOG_INFO, "%s/%s: error: zgethdr returned %d",
						   lrzsz_basename(zi->fname), protname(), c);
					DPRINTF(DEBUG_TRANSFER, "rzfile: zgethdr returned %d\n", c);
					return ERROR;
				}
				zmputs(Attn);
				state = ST_ZRPOS;
				continue;
			case ZSKIP:
				Rxtimeout = saved_rxtimeout;
				closeit(zi);
				DO_SYSLOG(LOG_INFO, "%s/%s: error: sender skipped",
						   lrzsz_basename(zi->fname), protname());
				DPRINTF(DEBUG_TRANSFER, "rzfile: Sender SKIPPED file\n");
				return c;
			case ZDATA:
				Rxtimeout = saved_rxtimeout;
				if (rclhdr(Rxhdr) != (unsigned long) zi->bytes_received) {
					if ( --n < 0) {
						DPRINTF(DEBUG_TRANSFER, "rzfile: out of sync\n");
						DO_SYSLOG(LOG_INFO, "%s/%s: error: out of sync",
						   lrzsz_basename(zi->fname), protname());
						return ERROR;
					}
                                /* this is a data block with subframes. if the sender streams
                                 * fast, this is a mass of data, which can easily overflow
                                 * the garbage max or the cancel max.
                                 * so we read and forget it.
                                 * But this *may* be a stale ZDATA (with a position behind
                                 * what we already have, an answer to a duplicate ZRPOS).
                                 * Reading it with the full Rxtimeout stalls the resync for
                                 * 10 seconds while the sender is waiting for us.
                                 * It may even be a series of subpackets missing a final
                                 * ZCRCx (garbled or dropped on the line).
                                 * We shorten our wait drastically here.
                                 * 0.1s per read still drains what is already in
                                 * flight, and the ZRPOS below goes out right away. */
					Rxtimeout = 1;
					zrdata(secbuf, LRZSZ_MAX_SUBPACKET_LEN, &bytes_in_block);
					Rxtimeout = saved_rxtimeout;
					zmputs(Attn);
					state = ST_ZRPOS;
					continue;
				}
				state = ST_DATA;
				continue;
			}
			continue;
		case ST_DATA:
                        now=time(NULL); /* wall clock: -s stop deadline */
                        if (stop_time) {
				if (lrzsz_stop_time_reached("rzfile", now,
							    stop_time, zi->fname))
					return ERROR;
                        }
			if (Verbose>1 || min_bps) {
				int minleft =  0;
				int secleft =  0;
				double d;
				d=timing_elapsed();
				if (d==0)
					d=0.5; /* first call: zero elapsed */
				last_bps=(unsigned long) (zi->bytes_received/d);
                                if (min_bps) {
                                        if (lrzsz_bps_watchdog("rzfile", (long) last_bps, d,
                                                               min_bps, min_bps_time,
                                                               &low_bps, zi->fname))
                                                return ERROR;
                                }

				if (Verbose > 1) {
				        if (not_printed > (min_bps ? 3 : 7)
                                            || zi->bytes_received > last_bps / 2 + last_rxbytes) {
				            lrzsz_eta(last_bps, (size_t) R_BYTESLEFT(zi), &minleft, &secleft);
                                            vstringf(_("\rBytes received: %7ld/%7ld   BPS:%-6lu ETA %02d:%02d  "),
                                                    (long) zi->bytes_received, (long) zi->bytes_total,
                                                    last_bps, minleft, secleft);
                                            last_rxbytes=zi->bytes_received;
                                            not_printed=0;
                                        } else {
				            not_printed++;
                                        }
				}
                        }
			switch (c = zrdata(secbuf, LRZSZ_MAX_SUBPACKET_LEN, &bytes_in_block))
			{
			case ZCAN:
				DPRINTF(DEBUG_TRANSFER, "rzfile: zrdata returned %d\n", c);
				DO_SYSLOG(LOG_INFO, "%s/%s: zrdata returned ZCAN",
						   lrzsz_basename(zi->fname), protname());
				return ERROR;
			case ERROR:	/* CRC error */
				if ( --n < 0) {
					DPRINTF(DEBUG_TRANSFER, "rzfile: zgethdr returned %d\n", c);
					DO_SYSLOG(LOG_INFO, "%s/%s: zrdata returned ERROR",
							   lrzsz_basename(zi->fname), protname());
					return ERROR;
				}
				zmputs(Attn);
				state = ST_ZRPOS;
				continue;
			case TIMEOUT:
				if ( --n < 0) {
					DO_SYSLOG(LOG_INFO, "%s/%s: zrdata returned TIMEOUT",
							   lrzsz_basename(zi->fname), protname());
					DPRINTF(DEBUG_TRANSFER, "rzfile: zgethdr returned %d\n", c);
					return ERROR;
				}
				state = ST_ZRPOS;
				continue;
			case RCDO:
				/* the line died mid-file: no ZRPOS retry into
				 * the void (see tryz() for why no reconnect
				 * handshake either) */
				DO_SYSLOG(LOG_INFO, "%s/%s: zrdata returned RCDO",
						   lrzsz_basename(zi->fname), protname());
				return ERROR;
			case GOTCRCW:
				n = 20;
				putsec(zi, secbuf, bytes_in_block);
				zi->bytes_received += bytes_in_block;
				DPRINTF(DEBUG_TRANSFER, "GOTCRCW: bytes_received=%ld, block=%zu, readline_left=%d\n",
				      (long)zi->bytes_received, bytes_in_block, readline_left);
				stohdr(zi->bytes_received);
				zshhdr(ZACK, Txhdr);
				state = ST_HDR;
				continue;
			case GOTCRCQ:
				n = 20;
				putsec(zi, secbuf, bytes_in_block);
				zi->bytes_received += bytes_in_block;
				DPRINTF(DEBUG_TRANSFER, "GOTCRCQ: bytes_received=%ld, block=%zu, readline_left=%d\n",
				      (long)zi->bytes_received, bytes_in_block, readline_left);
				stohdr(zi->bytes_received);
				zshhdr(ZACK, Txhdr);
				state = ST_DATA;
				continue;
			case GOTCRCG:
				n = 20;
				putsec(zi, secbuf, bytes_in_block);
				zi->bytes_received += bytes_in_block;
				state = ST_DATA;
				continue;
			case GOTCRCE:
				n = 20;
				putsec(zi, secbuf, bytes_in_block);
				zi->bytes_received += bytes_in_block;
				DPRINTF(DEBUG_TRANSFER, "GOTCRCE: bytes_received=%ld, block=%zu, readline_left=%d\n",
				      (long)zi->bytes_received, bytes_in_block, readline_left);
				state = ST_HDR;
				continue;
			default:
				;   /* zrdata's remaining returns are handled
				       above (GOT*, ERROR, TIMEOUT, RCDO,
				       ZCAN) - anything else mirrors the
				       original's fall out of the loop body:
				       re-send ZRPOS */
				state = ST_ZRPOS;
				continue;
			}
			continue;
		default:
			/* unreachable: state always holds one of the three enum
			 * values; maps to the loop head like any retry */
			state = ST_ZRPOS;
			continue;
		}
	}
}

/*
 * Send a string to the modem, processing for \336 (sleep 1 sec)
 *   and \335 (break signal)
 */
static void
zmputs(const char *s)
{
	const char *p;

	while (s && *s)
	{
		p=strpbrk(s,"\335\336");
		if (!p)
		{
			ssize_t off=0;
			size_t len=strlen(s);
			while ((size_t)off < len) {
				ssize_t n=write(1,s+off,len-off);
				if (n < 0) {
					if (errno == EINTR)
						continue;
					return;
				}
				off += n;
			}
			return;
		}
		if (p!=s)
		{
			ssize_t off=0;
			size_t len=(size_t) (p-s);
			while ((size_t)off < len) {
				ssize_t n=write(1,s+off,len-off);
				if (n < 0) {
					if (errno == EINTR)
						continue;
					return;
				}
				off += n;
			}
			s=p;
		}
		if (*p=='\336')
			sleep(1);
		else
			sendbrk(0);
		p++;
	}
}

/*
 * Close the receive dataset, return OK or ERROR
 */
static int
closeit(struct zm_fileinfo *zi)
{
	int ret;
	ret=fclose(fout);
        fout = NULL;
	if (ret) {
		zpfatal(_("file close error"));
		/* this may be any sort of error, including random data corruption */

		if (!keep_incomplete)
			unlink(Pathname);
		return ERROR;
	}
	if (zi->modtime) {
		struct utimbuf timep;
		timep.actime = time(NULL);
		timep.modtime = zi->modtime;
		utime(Pathname, &timep);
	}
	if (S_ISREG(zi->mode)) {
		/* we must not make this program executable if running
		 * under rsh, because the user might have uploaded an
		 * unrestricted shell.
		 */
		/* sanity check: a sender-chosen mode without owner read/
		 * write (e.g. 0100006 = S_IFREG|0006) would leave the owner
		 * locked out of their own uploaded file once the umask is
		 * applied. Owner rw is therefore granted unconditionally; 
                 * the sender keeps influence over exec and group/other bits.
                 */
		if (under_rsh)
			chmod(Pathname, ((0666 & zi->mode) | 0600) & ~lrz_umask);
		else
			chmod(Pathname, ((0777 & zi->mode) | 0600) & ~lrz_umask);
	}
	return OK;
}

/*
 * Ack a ZFIN packet, let byegones be byegones
 */
static void
ackbibi(void)
{
	int n;
	struct sigaction sa;

	DPRINTF(DEBUG_TRANSFER, "ackbibi:");
	/* Now the session is complete, every file has been received and 
         * this ZFIN exchange is only the sender's farewell.
	 * If the line destroys our ZFIN or the sender's "OO", we retransmit
	 * - and if the sender already left (its OO was lost, it exited
	 * after writing it), out retransmit would fail with EPIPE. Dying of
	 * SIGPIPE there would turn the clean end into exit 141 even
	 * though nothing was lost. The retransmit loop below handles
	 * the silence (RCDO/TIMEOUT) and just gives up. So ignore
	 * SIGPIPE for the remainder of the session. */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);
	stohdr(0L);
	for (n=3; --n>=0; ) {
		purgeline(0);
		zshhdr(ZFIN, Txhdr);
		switch (READLINE_PF(100)) {
		case 'O':
			READLINE_PF(1);	/* Discard 2nd 'O' */
			DPRINTF(DEBUG_TRANSFER, "ackbibi complete\n");
			return;
		case RCDO:
			return;
		case TIMEOUT:
		default:
			break;
		}
	}
}


/* End of lrz.c */

/*
  lsz - send files with x/y/zmodem
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
#include <stdint.h>

#include "timing.h"

#ifndef HAVE_GETOPT_LONG
#define getopt_long(argc,argv,str,longopts,longind) getopt(argc,argv,str)
#endif


unsigned int Baudrate=2400;	/* Default, should be set by first mode() call */
static unsigned int Txwindow;	/* Control the size of the transmitted window */
static unsigned int Txwspac;	/* Spacing between zcrcq requests */
static unsigned int Txwcnt;	/* Counter used to space ack requests */
static size_t Lrxpos;		/* Receiver's last reported offset */
int errors;
enum zm_type_enum protocol;
bool under_rsh=false;
static int no_unixmode;

static int Canseek=1; /* 1: can; 0: only rewind, -1: neither */

static int zsendfile (struct zm_fileinfo *zi, const char *buf, size_t blen);
static int getnak (void);
static int wctxpn (struct zm_fileinfo *);
static int wcs (const char *oname, const char *remotename);
static size_t zfilbuf (struct zm_fileinfo *zi);
static size_t filbuf (char *buf, size_t count);
static int getzrxinit (void);
static int calc_blklen (long total_sent);
static int sendzsinit (void);
static int wctx (struct zm_fileinfo *);
static int zsendfdata (struct zm_fileinfo *);
static int ack_resync (struct zm_fileinfo *, int, bool, int *, int *);
static int sigint_resync (struct zm_fileinfo *, int *, int *);
static int getinsync (struct zm_fileinfo *, int flag);
static void countem (int argc, char **argv);
static void usage (int exitcode, const char *what);
static int zsendcmd (const char *buf, size_t blen);
static void saybibi (void);
static int wcsend (int argc, char *argp[]);
static int wcputsec (char *buf, int sectnum, size_t cseclen);

/* DO_SYSLOG is replaced by the shared variadic macro (zglobal.h) plus
 * lrzsz_basename() (util.c) - same logged values, no `zi` coupling. */

#define ZSDATA(x,y,z) \
	do { if (Crc32t) {zsda32(x,y,z); } else {zsdata(x,y,z);}} while(0)

static int Filesleft;
static long Totalleft;
static size_t buffersize=16384;

/*
 * Attention string to be executed by receiver to interrupt streaming data
 *  when an error is detected.  A pause (0336) may be needed before the
 *  ^C (03) or after it.
 */
static char Myattn[] = { 0 };
/* alternative: char Myattn[] = { 03, 0336, 0 }; when READCHECK was not defined (but it always was under linux). */

static FILE *input_f;

static char *txbuf;

static long vpos = 0;			/* Number of bytes read from file */

static char Lastrx;
static char Crcflg;
int Verbose=0;
static int Restricted=0;	/* restricted; no /.. or ../ in filenames */
static int Quiet=0;		/* overrides logic that would otherwise set verbose */
static int Ascii=0;		/* Add CR's for brain damaged programs */
static int Fullname=0;		/* transmit full pathname */
static int Unlinkafter=0;	/* Unlink file after it is sent */
static int firstsec;
static int errcnt=0;		/* number of files unreadable */
static size_t blklen=128;		/* length of transmitted records */
static int Optiong;		/* Let it rip no wait for sector ACK's */
static int Totsecs;		/* total number of sectors this file */
static int Filcnt=0;		/* count of number of files opened */
static int Lfseen=0;
#define SAFETY_RXBUFLEN (128*1024) /* will be set one first error */
#define DEFAULT_RXBUFLEN (16*1024)
static unsigned int Rxbuflen = DEFAULT_RXBUFLEN;	/* Receiver's max buffer length */
static unsigned int Tframlen = 0;	/* Override for tx frame length */
static unsigned int blkopt=0;		/* Override value for zmodem blklen */
static int sync_transfer;		/* ZCRCW after every data subpacket */
static int Rxflags = 0;
static size_t bytcnt;
static int Wantfcs32 = true;	/* want to send 32 bit FCS */
static char Lzconv;	/* Local ZMODEM file conversion request */
static char Lzmanag;	/* Local ZMODEM file management request */
static int Lskipnocor;
static char Lztrans;
static int command_mode;		/* Send a command, then exit. */
static int Cmdtries = 11;
static int Cmdack1;		/* Rx ACKs command, then do it */
static int Exitcode;
static size_t Lastsync;		/* Last offset to which we got a ZRPOS */
static int Beenhereb4;		/* How many times we've been ZRPOS'd same place */
/* set when an abort arm has answered a receiver's ZABORT/ZFERR with the
 * spec-required ZFIN sequence (zmodem-wip.txt §4): the session is then
 * already ended cleanly, and main() must not append a cancel sequence */
static bool replied_abort=false;

bool no_timeout=false;
static size_t max_blklen=1024;
static size_t start_blklen=0;
bool zmodem_requested;
static time_t stop_time=0;

static int error_count;
#define OVERHEAD 18
#define OVER_ERR 20

/* garbage tolerance for input that never becomes a recognizable prompt or
 * header. getnak() has no protocol-state retry budget of its own (only
 * TIMEOUT is bounded), so a line feeding continuous junk must give up
 * instead of waiting forever. */
#define MAX_GARBAGE 100

int enable_syslog=true;

/* set by the SIGINT handler; zsendfdata() checks it at its loop
 * checkpoints and attempts a receiver resync. sig_atomic_t + volatile:
 * the only async-signal-safe way to communicate with the handler. */
static volatile sig_atomic_t sigint_seen = 0;

static long min_bps;
static long min_bps_time;

static int io_mode_fd=0;
static int zrqinits_sent=0;
static int play_with_sigint=0;

/* Signal handler for SIGINT/SIGTERM/SIGPIPE/SIGHUP. 
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

	canit(STDOUT_FILENO);
	io_mode(io_mode_fd, 0);
	errno = saved_errno;
	_exit(128 + n);
}

/* called from normal control flow (rbsb.c) to clean things up */
LRZSZ_NORETURN void
bibi(int n)
{
	canit(STDOUT_FILENO);
	fflush(stdout);
	io_mode(io_mode_fd, 0);
        lrzsz_warning(0, _("caught signal %d; exiting"), n);
	if (n == SIGQUIT)
		abort();
	exit(128 + n);
}

/* the cancel-handler installer is shared: see lrzsz_install_signal() in
 * util.c; lsz installs its own bibi_sighandler with it. */

/* Called when ZMODEM gets an interrupt (^C): record the request; the
 * transfer loop checks sigint_seen at its (timeout-bounded) checkpoints
 * and resyncs with the receiver there. No longjmp: the handler only
 * touches a sig_atomic_t, so it is strictly async-signal-safe. */
static void
onintr(int n)
{
	(void) n; /* use it */
	sigint_seen = 1;
	signal(SIGINT, onintr);
}

bool Zctlesc;	/* Encode control characters */
unsigned int Zrwindow = 1400;	/* RX window size (controls garbage count) */

#ifdef HAVE_GETOPT_LONG
static struct option const long_options[] =
{
  {"append", no_argument, NULL, '+'},
  {"twostop", no_argument, NULL, '2'},
  {"try-8k", no_argument, NULL, '8'},
  {"start-8k", no_argument, NULL, '9'},
  {"try-4k", no_argument, NULL, '4'},
  {"start-4k", no_argument, NULL, '5'},
  {"ascii", no_argument, NULL, 'a'},
  {"binary", no_argument, NULL, 'b'},
  {"bufsize", required_argument, NULL, 'B'},
  {"cmdtries", required_argument, NULL, 'C'},
  {"command-tries", required_argument, NULL, 'C'},
  {"command", required_argument, NULL, 'c'},
  {"immediate-command", required_argument, NULL, 'i'},
  {"dot-to-slash", no_argument, NULL, 'd'},
  {"full-path", no_argument, NULL, 'f'},
  {"escape", no_argument, NULL, 'e'},
  {"rename", no_argument, NULL, 'E'},
  {"help", no_argument, NULL, 'h'},
  {"crc-check", no_argument, NULL, 'H'},
  {"1024", no_argument, NULL, 'k'},
  {"1k", no_argument, NULL, 'k'},
  {"packetlen", required_argument, NULL, 'L'},
  {"framelen", required_argument, NULL, 'l'},
  {"min-bps", required_argument, NULL, 'm'},
  {"min-bps-time", required_argument, NULL, 'M'},
  {"newer", no_argument, NULL, 'n'},
  {"newer-or-longer", no_argument, NULL, 'N'},
  {"16-bit-crc", no_argument, NULL, 'o'},
  {"disable-timeouts", no_argument, NULL, 'O'},
  {"disable-timeout", no_argument, NULL, 'O'}, /* i can't get it right */
  {"protect", no_argument, NULL, 'p'},
  {"resume", no_argument, NULL, 'r'},
  {"restricted", no_argument, NULL, 'R'},
  {"quiet", no_argument, NULL, 'q'},
  {"stop-at", required_argument, NULL, 's'},
  {"syslog", optional_argument, NULL , 2},
  {"timesync", no_argument, NULL, 'S'},
  {"timeout", required_argument, NULL, 't'},
  {"turbo", no_argument, NULL, 'T'},
  {"unlink", no_argument, NULL, 'u'},
  {"unrestrict", no_argument, NULL, 'U'}, /* removed, kept for the error message */
  {"verbose", no_argument, NULL, 'v'},
  {"version", no_argument, 0, 'V'},
  {"windowsize", required_argument, NULL, 'w'},
  {"xmodem", no_argument, NULL, 'X'},
  {"ymodem", no_argument, NULL, 'g'},
  {"zmodem", no_argument, NULL, 'Z'},
  {"overwrite", no_argument, NULL, 'y'},
  {"overwrite-or-skip", no_argument, NULL, 'Y'},

  {"journal", required_argument, NULL, 'J'},
  {"delay-startup", required_argument, NULL, 4},
  {"no-unixmode", no_argument, NULL, 8},
  {"debug", required_argument, NULL, 9},
  {"sync-transfer", no_argument, NULL, 10},
  {NULL, 0, NULL, 0}
};
#endif

LRZSZ_NORETURN static void
show_version(void)
{
	printf ("%s (%s) %s\n", lrzsz_progname, PACKAGE, VERSION);
        exit(0);
}


int
main(int argc, char **argv)
{
	char *cp;
	int npats;
	int dm;
	int i;
	int stdin_files;
	char **patts;
	int c;
	const char *Cmdstr=NULL;		/* Pointer to the command string */
	unsigned int startup_delay=0;
	const char *journal_filename;

        lrzsz_set_progname (argv[0], "lsz"); /* needed for the error handling => early */

	if (((cp = getenv("ZNULLS")) != NULL) && *cp)
		Znulls = (int) lrzsz_strtoul("ZNULLS", cp, NULL, 0, 65535);
	if (((cp=getenv("SHELL"))!=NULL) && cp[0]) {
		const char *base = strrchr(cp, '/');
		base = base ? base + 1 : cp;
		if (strcmp(base, "rsh") == 0 || strcmp(base, "rksh") == 0
			|| strcmp(base, "rbash") == 0 || strcmp(base, "rshell") == 0)
		{
			under_rsh=true;
			Restricted=1;
		}
	}
	if (getenv("ZMODEM_RESTRICTED")!=NULL)
		Restricted=1;
	from_cu();
	chkinvok(argv[0], 's');

        journal_filename=getenv("LSZ_JOURNAL");
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

	Rxtimeout = 600;

	while ((c = getopt_long (argc, argv,
		"2+4589aA:bB:C:c:dfeEghHi:J:kL:l:m:M:NnOop7Rrqs:St:TUuvVw:XYyZ",
		long_options, (int *) 0))!=EOF)
	{
		switch (c)
		{
		case 0:
			break;
		case '+': Lzmanag = ZF1_ZMAPND; break;
		case '2': Twostop = true; break;
		case '8':
			if (max_blklen==8192)
				start_blklen=8192;
			else
				max_blklen=8192;
			break;
		case '9': /* this is a longopt .. */
			start_blklen=8192;
			max_blklen=8192;
			break;
		case '4':
			if (max_blklen==4096)
				start_blklen=4096;
			else
				max_blklen=4096;
			break;
		case '5': /* this is a longopt .. */
			start_blklen=4096;
			max_blklen=4096;
			break;
		case 'a': /* deprecated: ZCNL is marked historic by the current
			   * ZMODEM documentation; kept for old peers */
			Lzconv = ZCNL; Ascii = true; break;
		case 'b': Lzconv = ZCBIN; break;
		case 'B':
			if (0==strcmp(optarg,"auto"))
				buffersize= (size_t) -1;
			else
				buffersize=(size_t) lrzsz_strtoul( "-B / --bufsize", optarg, "km", 0, 0);
			break;
		case 'C':
                        Cmdtries = lrzsz_strtoul( "-C / --command-tries", optarg, NULL, 0, 0);
			break;
		case 'i':
			Cmdack1 = ZCACK1;
			command_mode = true;
			Cmdstr = optarg;
			break;
		case 'c':
			command_mode = true;
			Cmdstr = optarg;
			break;
		case 'd':
			fatal_error(1,0,_("this option has been removed; see the manual"));
			break;
		case 'f': Fullname=true; break;
 		case 'e': Zctlesc = 1; break;
 		case 'E': Lzmanag = ZF1_ZMCHNG; break;
		case 'h': usage(0,NULL); break;
		case 'H': Lzmanag = ZF1_ZMCRC; break;
		case 'J':
			stats_journal_open(optarg);
			break;
		case 'k': start_blklen=1024; break;
		case 'L':
                        blkopt = (unsigned int) lrzsz_strtoul( "-L / --packetlen", optarg, "ck", 24, LRZSZ_MAX_SUBPACKET_LEN);
			break;
		case 'l':
                        Tframlen = (unsigned int) lrzsz_strtoul( "-l / --framelen", optarg, "ck", 32, LRZSZ_MAX_SUBPACKET_LEN);
			break;
                case 'm':
                        min_bps = (unsigned int) lrzsz_strtoul( "-m / --min-bps", optarg, "km", 0, 0);
			break;
                case 'M':
                        min_bps_time = (unsigned int) lrzsz_strtoul( "-M / --min-bps-time", optarg, "k", 1, 0);
			break;
		case 'N': Lzmanag = ZF1_ZMNEWL;  break;
		case 'n': Lzmanag = ZF1_ZMNEW;  break;
		case 'o': Wantfcs32 = false; break;
		case 'O': no_timeout = true; break;
		case 'p': Lzmanag = ZF1_ZMPROT;  break;
		case 'r':
			if (Lzconv == ZCRESUM)
				Lzmanag = ZF1_ZMCRC;
			else
				Lzconv = ZCRESUM;
			break;
		case 'R': Restricted = true; break;
		case 'q': Quiet=true; Verbose=0; break;
		case 's':
			stop_time = lrzsz_parse_stop_time(optarg, usage);
			break;
		case 'S':
			fprintf(stderr,
				_("this option has been removed. See the manual page for more information.\n"));
			exit(1);
		case 'T': turbo_escape=1; break;
		case 't':
                        Rxtimeout = (unsigned int) lrzsz_strtoul( "-t / --timeout", optarg, NULL, 10, 1000);
			break;
		case 'u': ++Unlinkafter; break;
		case 'U':
			fprintf(stderr,
				_("this option has been removed. See the manual page for more information.\n"));
			exit(1);
		case 'v': ++Verbose; break;
                case 'V': show_version(); break;
		case 'w':
                        Txwindow = (unsigned int) lrzsz_strtoul( "-w / --windowsize", optarg, NULL, 256, 0);
			Txwindow = (Txwindow/64) * 64;
			Txwspac = Txwindow/4;
			if (blkopt > Txwspac
			 || (!blkopt && Txwspac < LRZSZ_MAX_SUBPACKET_LEN))
				blkopt = Txwspac;
			break;
		case 'X': protocol=ZM_XMODEM; break;
		case 'g': protocol=ZM_YMODEM; break;
		case 'Z': protocol=ZM_ZMODEM; break;
		case 'Y':
			Lskipnocor = true;
			Lzmanag = ZF1_ZMCLOB;
                        break;
		case 'y':
			Lzmanag = ZF1_ZMCLOB; break;
		case 2:
			if (optarg && (!strcmp(optarg,"off") || !strcmp(optarg,"no")))
			{
				if (under_rsh)
					lrzsz_warning(0, _("cannot turnoff syslog"));
				else
					enable_syslog=false;
			}
			else
				enable_syslog=true;
			break;
		case 4:
                        startup_delay = (unsigned int) lrzsz_strtoul( "--delay-startup", optarg, NULL, 1, 0);
			break;
		case 8: no_unixmode=1; break;
		case 9:
			if (!lrzsz_parse_debug(optarg))
				usage(2, _("bad --debug argument"));
			break;
		case 10: sync_transfer = 1; break;
		default:
			usage (2,NULL);
			break;
		}
	}

	if (getuid()!=geteuid()) {
		fatal_error(1,0,
		_("this program was never intended to be used setuid\n"));
	}

	txbuf=xmalloc(LRZSZ_MAX_SUBPACKET_LEN);
        memset(txbuf,0, LRZSZ_MAX_SUBPACKET_LEN);

	zsendline_init();

	if (start_blklen==0) {
		if (protocol == ZM_ZMODEM) {
			start_blklen=1024;
			if (Tframlen) {
				start_blklen=max_blklen=Tframlen;
			}
		}
		else
			start_blklen=128;
	}

	if (argc<2)
		usage(2,_("need at least one file to send"));

	if (startup_delay)
		sleep(startup_delay);



	npats = argc - optind;
	patts=&argv[optind];

	if (npats < 1 && !command_mode)
		usage(2,_("need at least one file to send"));
	if (command_mode && Restricted) {
		/* stderr, not stdout: stdout is the protocol stream. */
		lrzsz_warning(0, _("can't send command in restricted mode"));
		exit(1);
	}

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

	stat_for_all.direction = 's';
	stat_for_file.direction = 's';

	{
		/* we write max_blocklen (data) + 18 (ZModem protocol overhead)
		 * + escape overhead (about 4 %), so buffer has to be
		 * somewhat larger than max_blklen
		 */
		char *s=malloc(max_blklen+1024);
		if (!s)
		{
			zperr(_("out of memory"));
			exit(1);
		}
		setvbuf(stdout,s,_IOFBF,max_blklen+1024);
	}
	blklen=start_blklen;

	for (i=optind,stdin_files=0;i<argc;i++) {
		if (0==strcmp(argv[i],"-"))
			stdin_files++;
	}

	if (stdin_files>1) {
		usage(1,_("can read only one file from stdin"));
	} else if (stdin_files==1) {
		io_mode_fd=1;
	}
	if (io_mode(io_mode_fd,1))
		lrzsz_warning(0, _("cannot set transfer line mode; continuing"));
	readline_setup(io_mode_fd, 128, 256);

	if (lrzsz_install_signal(SIGINT, bibi_sighandler))
		play_with_sigint=1;
	lrzsz_install_signal(SIGTERM, bibi_sighandler);
	lrzsz_install_signal(SIGPIPE, bibi_sighandler);
	lrzsz_install_signal(SIGHUP, bibi_sighandler);

	if ( protocol!=ZM_XMODEM) {
		countem(npats, patts);
		if (protocol == ZM_ZMODEM) {
			/* throw away any input already received. This doesn't harm
			 * as we invite the receiver to send it's data again, and
			 * might be useful if the receiver has already died or
			 * if there is dirt left if the line
			 */
			struct timeval t;
			unsigned char throwaway;
			fd_set f;

			purgeline(io_mode_fd);

			t.tv_sec = 0;
			t.tv_usec = 0;

			FD_ZERO(&f);
			FD_SET(io_mode_fd,&f);

			while (select(1,&f,NULL,NULL,&t)) {
				if (0==read(io_mode_fd,&throwaway,1)) /* EOF ... */
					break;
			}

			purgeline(io_mode_fd);

			stohdr(0L);
			if (command_mode)
				Txhdr[ZF0] = ZCOMMAND;
			zshhdr(ZRQINIT, Txhdr);
			zrqinits_sent++;
		}
	}
	fflush(stdout);

	/* the session is already over when a receiver's ZABORT/ZFERR was
	 * answered with the ZFIN sequence (see replied_abort): appending a
	 * cancel sequence there would contradict that reply */
#define END_WITH_CANCEL() do { Exitcode=0x80; \
		if (!replied_abort) canit(STDOUT_FILENO); } while (0)

	if (Cmdstr) {
		if (getzrxinit()) {
			END_WITH_CANCEL();
		}
		else if (zsendcmd(Cmdstr, strlen(Cmdstr)+1)) {
			END_WITH_CANCEL();
		}
	} else if (wcsend(npats, patts)==ERROR) {
		END_WITH_CANCEL();
	}
#undef END_WITH_CANCEL
	fflush(stdout);
	io_mode(io_mode_fd,0);
	if (Exitcode)
		dm=Exitcode;
	else if (errcnt)
		dm=1;
	else
		dm=0;
	if (Verbose)
	{
		fputs("\r\n",stderr);
		if (dm)
			fputs(_("Transfer incomplete\n"),stderr);
		else
			fputs(_("Transfer complete\n"),stderr);
	}
        stats_account_finalize_file();
        stats_log(&stat_for_all,false);
	stats_journal_close();
	exit(dm);
}

static int
wcsend (int argc, char *argp[])
{
	int n;

	Crcflg = false;
	firstsec = true;
	bytcnt = (size_t) -1;

	for (n = 0; n < argc; ++n) {
		Totsecs = 0;
		if (wcs (argp[n],NULL) == ERROR) {
                        stats_account_one();
			return ERROR;
                }
                stats_account_one();
	}
	Totsecs = 0;
	if (Filcnt == 0) {			/* bitch if we couldn't open ANY files */
		canit(STDOUT_FILENO);
		vstring ("\r\n");
		vstringf (_ ("Can't open any requested files."));
		vstring ("\r\n");
		return ERROR;
	}
	if (zmodem_requested)
		saybibi ();
	else if (protocol == ZM_YMODEM) {
		struct zm_fileinfo zi;
		zi_init(&zi);
		zi.fname = xstrdup(""); /* empty filename packet signals end of batch */
		wctxpn (&zi);
                free(zi.fname);
	}
	return OK;
}

static int
wcs(const char *oname, const char *remotename)
{
	struct stat f;
	static char *name=NULL;
	struct zm_fileinfo zi;
	const char *shortname;
	zi_init(&zi);
	shortname=strrchr(oname,'/');
	if (shortname)
		shortname++;
	else
		shortname=oname;

	free(name);
	name=NULL;

	if (Restricted) {
		/* restrict pathnames to current tree */
		if (lrzsz_path_component_violation(oname, Restricted))
			goto violation;
	}

	if (0==strcmp(oname,"-")) {
		char *p=getenv("ONAME");
		if (p) {
			name=xstrdup(p);
		} else {
			char tmp[64];
			snprintf(tmp, sizeof tmp, "s%lu.lsz", (unsigned long) getpid());
			name=xstrdup(tmp);
		}
		input_f=stdin;
	} else if ((input_f=fopen(oname, "r"))==NULL) {
		int e=errno;
		lrzsz_warning(e, _("cannot open %s"),oname);
		++errcnt;
                stats_file_local_error();
		return OK;	/* pass over it, there may be others */
	} else {
		name=xstrdup(oname);
	}
	{
		static char *s=NULL;
		static size_t last_length=0;
		struct stat st;
		if (fstat(fileno(input_f),&st)==-1)
			st.st_size=1024*1024;
		if (buffersize==(size_t) -1 && s) {
			if ((size_t) st.st_size > last_length) {
				free(s);
				s=NULL;
				last_length=0;
			}
		}
		if (!s && buffersize) {
			last_length=16384;
			if (buffersize==(size_t) -1) {
				if (st.st_size>0)
					last_length=st.st_size;
			} else
				last_length=buffersize;
			/* buffer whole pages */
			last_length=(last_length+4095)&0xfffff000;
			s=xmalloc(last_length);
		}
		if (s) {
			setvbuf(input_f,s,_IOFBF,last_length);
		}
	}
	vpos = 0;
	/* fstat must succeed: its fields are transmitted below as the file's
	 * metadata (mode/mtime/size). A failure used to send uninitialized
	 * memory - a garbage size in particular can make the receiver
	 * truncate the incoming stream to R_BYTESLEFT. Skip the file. */
	if (fstat(fileno(input_f), &f) == -1) {
		int e = errno;
		lrzsz_warning(e, _("cannot stat %s"), name);
		fclose(input_f);
		++errcnt;
                stats_file_local_error();
		return OK;
	}
	/* Check for directory or block special files */
	if (S_ISDIR(f.st_mode) || S_ISBLK(f.st_mode)) {
		lrzsz_warning(0, _("is not a file: %s"),name);
		fclose(input_f);
                stats_file_local_error();
		return OK;
	}

	if (remotename) {
		/* disqualify const */
		union {
			const char *c;
			char *s;
		} cheat;
		cheat.c=remotename;
		zi.fname=cheat.s;
	} else
		zi.fname=name;
	zi.modtime=f.st_mtime;
	zi.mode=f.st_mode;
	zi.bytes_total= (S_ISFIFO(f.st_mode)) ? DEFBYTL : f.st_size;
	timing_reset();
	stats_set_filename(shortname);
	stats_start_one();

	if (!Quiet && zi.bytes_total > 0x7FFFFFFFUL)
		lrzsz_warning(0, _("file %s is larger than 2 GiB; "
			"transfers may fail with older ZMODEM implementations"),
			name);

	++Filcnt;
	switch (wctxpn(&zi)) {
	case ERROR:
		if (enable_syslog)
			lsyslog(LOG_INFO, _("%s/%s: error occured"),protname(),shortname);
                stats_file_error();
		return ERROR;
	case ZSKIP:
		lrzsz_warning(0, _("skipped: %s"),name);
                stats_file_skipped();
		if (enable_syslog)
			lsyslog(LOG_INFO, _("%s/%s: skipped"),protname(),shortname);
		return OK;
        default:
                ;
	}
	if (!zmodem_requested && wctx(&zi)==ERROR)
	{
		if (enable_syslog)
			lsyslog(LOG_INFO, _("%s/%s: error occured"),protname(),shortname);
                stats_file_error();
		return ERROR;
	}
	if (Unlinkafter)
		unlink(oname);

	report_transfer_result(1, shortname, zi.fname,
		(long) zi.bytes_sent, 0, 0);
        stats_file_ok();
	return 0;

violation:
        stats_file_policy_reject();
        stats_account_one();
        stats_log(&stat_for_all, false);
	canit(STDOUT_FILENO);
	vchar('\r');
	fatal_error(1,0,
		_("security violation: not allowed to upload from %s"),oname);
}

/*
 * generate and transmit pathname block consisting of
 *  pathname (null terminated),
 *  file length, mode time and file mode in octal
 *  as provided by the Unix fstat call.
 *  N.B.: modifies the passed name, may extend it!
 */
static int
wctxpn(struct zm_fileinfo *zi)
{
	char *p, *q;
	struct stat f;

	if (protocol==ZM_XMODEM) {
		if (Verbose && *zi->fname && fstat(fileno(input_f), &f)!= -1) {
			vstringf(_("Sending %s, %ld blocks: "),
			  zi->fname, (long) (f.st_size>>7));
		}
                if (Verbose) {
                    vstringf(_("Give your local XMODEM receive command now."));
                    vstring("\r\n");
                }
		return OK;
	}
	if (!zmodem_requested)
		if (getnak()) {
			DPRINTF(DEBUG_TRANSFER, "getnak failed\n");
			DO_SYSLOG(LOG_INFO, "%s/%s: getnak failed",
					   lrzsz_basename(zi->fname), protname());
			return ERROR;
		}

	/* ZMODEM spec: file information subpacket must not exceed 1024 bytes.
	 * YMODEM block 0 is 128 bytes. Both carry the same filename + metadata.
	 * Reserve room for the metadata suffix (max ~64 bytes) and null. */
	{
		size_t maxlen = zmodem_requested ? (1024 - 64) : (128 - 64);
		const char *base = zi->fname;
		if (!Fullname) {
			const char *slash = strrchr(zi->fname, '/');
			if (slash)
				base = slash + 1;
		}
		if (strlen(base) >= maxlen) {
			lrzsz_warning(0, _("filename too long for %s subpacket: %s"),
				      protname(), zi->fname);
			fclose(input_f);
			return OK;
		}
		for (p=zi->fname, q=txbuf ; *p; )
			if ((*q++ = *p++) == '/' && !Fullname)
				q = txbuf;
		*q++ = 0;
		p=q;
	}

	/* zero the size in case the fstat below is skipped (stdin, ascii,
	 * empty name) or fails: the batch-estimate arithmetic below
	 * (Totalleft) uses f.st_size unconditionally. */
	f.st_size=0;

	/* note that we may lose some information here in case mode_t is wider than an
	 * int. But i believe sending %lo instead of %o _could_ break compatability
	 */
	if (!Ascii && (input_f!=stdin) && *zi->fname && fstat(fileno(input_f), &f)!= -1) {
		/* cap the ZFILE data subpacket at 1024 bytes (zmodem-wip.txt
		 * §4). snprintf truncates cleanly; a truncated metadata suffix
		 * only loses trailing batch-estimate fields. */
		size_t metasize = (size_t)(txbuf + 1024 - 1 - p);
		snprintf(p, metasize, "%lu %llo %o 0 %d %ld",
              (unsigned long) f.st_size,
              (unsigned long long) f.st_mtime,
              (unsigned int)((no_unixmode) ? 0 : f.st_mode),
              Filesleft, Totalleft);
	}
	if (Verbose)
		vstringf(_("Sending: %s\n"),txbuf);
	Totalleft -= f.st_size;
	if (--Filesleft <= 0)
		Totalleft = 0;
	if (Totalleft < 0)
		Totalleft = 0;

	if (zmodem_requested)
		return zsendfile(zi,txbuf, 1+strlen(p)+(p-txbuf));
	if (wcputsec(txbuf, 0, 128)==ERROR) {
		DPRINTF(DEBUG_TRANSFER, "wcputsec failed\n");
		DO_SYSLOG(LOG_INFO, "%s/%s: wcputsec failed",
				   lrzsz_basename(zi->fname), protname());
		return ERROR;
	}
	return OK;
}

static int
getnak(void)
{
	int firstch;
	int tries=0;
	int garbage=0;

	Lastrx = 0;
	for (;;) {
		tries++;
		switch (firstch = READLINE_PF(100)) {
		case ZPAD:
			if (getzrxinit())
				return ERROR;
			Ascii = 0;	/* Receiver does the conversion */
			return false;
		case TIMEOUT:
                        stats_timeout();
			/* 30 seconds are enough */
			if (tries==3) {
				zperr(_("Timeout on pathname"));
				return ERROR;
			}
			/* don't send a second ZRQINIT _directly_ after the
			 * first one. Never send more then 4 ZRQINIT, because
			 * omen rz stops if it saw 5 of them */
			if ((zrqinits_sent>1 || tries>1) && zrqinits_sent<4) {
				/* if we already sent a ZRQINIT we are using zmodem
				 * protocol and may send further ZRQINITs
				 */
				stohdr(0L);
				zshhdr(ZRQINIT, Txhdr);
				zrqinits_sent++;
			}
			continue;
		case WANTG:
			io_mode(io_mode_fd,2);	/* Set cbreak, XON/XOFF, etc. */
			Optiong = true;
			blklen=1024;
			Crcflg = true;
                        return false;
		case WANTCRC:
			Crcflg = true;
			return false;
		case NAK:
			return false;
		case RCDO:
			/* line dead (peer closed): the receiver will never
			 * answer - getnak's loop has no retry budget of its
			 * own, so RCDO must terminate it (like CAN CAN). */
			zperr(_("Line dropped"));
			return ERROR;
		case CAN:
			if ((firstch = READLINE_PF(20)) == CAN && Lastrx == CAN)
				return ERROR;
			if (++garbage > MAX_GARBAGE) {
				zperr(_("Too much garbage on line"));
				return ERROR;
			}
			break;
		default:
			/* unbounded if garbage never stops arriving: the TIMEOUT
			 * budget above only runs while the line is quiet. */
			if (++garbage > MAX_GARBAGE) {
				zperr(_("Too much garbage on line"));
				return ERROR;
			}
			break;
		}
		Lastrx = firstch;
	}
}


static int
wctx(struct zm_fileinfo *zi)
{
	size_t thisblklen;
	int sectnum, attempts, firstch;

	firstsec=true;  thisblklen = blklen;
	DPRINTF(DEBUG_TRANSFER, "wctx:file length=%ld\n", (long) zi->bytes_total);

	while ((firstch=READLINE_PF(Rxtimeout))!=NAK && firstch != WANTCRC
	  && firstch != WANTG && firstch!=TIMEOUT && firstch!=CAN
	  && firstch!=ERROR && firstch!=RCDO)
		;
	if (firstch==CAN) {
		zperr(_("Receiver Cancelled"));
		return ERROR;
	}
	if (firstch==ERROR || firstch==RCDO) {
		zperr(_("Line dropped"));
		return ERROR;
	}
	if (firstch==WANTCRC)
		Crcflg=true;
	if (firstch==WANTG)
		Crcflg=true;
	sectnum=0;
	for (;;) {
		if (zi->bytes_total <= (zi->bytes_sent + 896L))
			thisblklen = 128;
		if ( !filbuf(txbuf, thisblklen))
			break;
		if (wcputsec(txbuf, ++sectnum, thisblklen)==ERROR)
			return ERROR;
		zi->bytes_sent += thisblklen;
	}
	fclose(input_f);
	attempts=0;
	for (;;) {
		purgeline(io_mode_fd);
		sendline(EOT);
		flushmo();
		++attempts;
		firstch = READLINE_PF(Rxtimeout);
		if (firstch == ACK)
			return OK;
		if (firstch == ERROR || firstch == RCDO) {
			zperr(_("Line dropped"));
			return ERROR;
		}
		if (attempts >= RETRYMAX)
			break;
	}
	zperr(_("No ACK on EOT"));
	stats_too_many_errors();
	return ERROR;
}

static int
wcputsec(char *buf, int sectnum, size_t cseclen)
{
	int checksum, wcj;
	char *cp;
	unsigned oldcrc;
	int firstch;
	int attempts;

	firstch=0;	/* part of logic to detect CAN CAN */

	if (Verbose>1) {
		vchar('\r');
		if (protocol==ZM_XMODEM) {
			vstringf(_("Xmodem sectors/kbytes sent: %3d/%2dk"), Totsecs, Totsecs/8 );
		} else {
			vstringf(_("Ymodem sectors/kbytes sent: %3d/%2dk"), Totsecs, Totsecs/8 );
		}
	}
	for (attempts=0; attempts <= RETRYMAX; attempts++) {
		Lastrx= firstch;
		sendline(cseclen==1024?STX:SOH);
		sendline(sectnum);
		sendline(-sectnum -1);
		oldcrc=checksum=0;
		for (wcj=cseclen,cp=buf; --wcj>=0; ) {
			sendline(*cp);
			oldcrc=updcrc((0xff& *cp), oldcrc);
			checksum += *cp++;
		}
                stats_msg_bytes(cseclen);
		if (Crcflg) {
			oldcrc=updcrc(0,updcrc(0,oldcrc));
			sendline((int)oldcrc>>8);
			sendline((int)oldcrc);
		}
		else
			sendline(checksum);

		flushmo();
		if (Optiong) {
			firstsec = false; return OK;
		}
		firstch = READLINE_PF(Rxtimeout);
gotnak:
		switch (firstch) {
		case CAN:
			if(Lastrx == CAN) {
cancan:
                                stats_cancel();
				zperr(_("Cancelled"));  return ERROR;
			}
			break;
		case TIMEOUT:
                        stats_timeout();
			zperr(_("Timeout on sector ACK")); continue;
		case WANTCRC:
			if (firstsec)
				Crcflg = true;
                        continue;
		case NAK:
                        stats_msg_invalid();
			zperr(_("NAK on sector")); continue;
		case ACK:
			firstsec=false;
			Totsecs += (cseclen>>7);
                        stats_msg_valid(0);
			return OK;
		case ERROR:
			zperr(_("Got burst for sector ACK")); break;
		default:
			zperr(_("Got %02x for sector ACK"), (unsigned int) firstch); break;
		}
		for (;;) {
			Lastrx = firstch;
			if ((firstch = READLINE_PF(Rxtimeout)) == TIMEOUT)
				break;
			if (firstch == ERROR || firstch == RCDO) {
				zperr(_("Line dropped"));
				return ERROR;
			}
			if (firstch == NAK || firstch == WANTCRC)
				goto gotnak;
			if (firstch == CAN && Lastrx == CAN) {
				goto cancan;
                        }
		}
	}
	zperr(_("Retry Count Exceeded"));
        stats_too_many_errors();
	return ERROR;
}

/* fill buf with count chars padding with ^Z for CPM */
static size_t
filbuf(char *buf, size_t count)
{
	int c;
	size_t m;

	if ( !Ascii) {
		ssize_t r = read(fileno(input_f), buf, count);
		if (r < 0) {
			int er = errno;
			lrzsz_warning(0, _("read error: %s"), strerror(er));
                        stats_file_local_error();
			return 0;
		}
		if (r == 0)
			return 0;
		m = (size_t)r;
		while (m < count)
			buf[m++] = CPMEOF;
		return count;
	}
	m=count;
	if (Lfseen) {
		*buf++ = '\n'; --m; Lfseen = 0;
	}
	while ((c=getc(input_f))!=EOF) {
		if (c == '\n') {
			*buf++ = '\r';
			if (--m == 0) {
				Lfseen = true; break;
			}
		}
		*buf++ =c;
		if (--m == 0)
			break;
	}
        if (ferror(input_f)) {
            lrzsz_warning(0, _("read error: %s"), strerror(errno));
            stats_file_local_error();
            return 0;
        }
	if (m==count)
		return 0;
	else
		while (m--!=0)
			*buf++ = CPMEOF;
	return count;
}

/* Fill buffer with blklen chars */
static size_t
zfilbuf (struct zm_fileinfo *zi)
{
	size_t n;

	n = fread (txbuf, 1, blklen, input_f);
	if (n < blklen) {
		if (ferror(input_f)) {
			int er = errno;
			lrzsz_warning(0, _("read error: %s"), strerror(er));
		}
		zi->eof_seen = 1;
	}
	else {
		/* save one empty paket in case file ends ob blklen boundary */
		int c = getc(input_f);

		if (c != EOF || !feof(input_f))
			ungetc(c, input_f);
		else
			zi->eof_seen = 1;
	}
	return n;
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

	fprintf(f,_("Usage: %s [options] file ...\n"),
		lrzsz_progname);
	fprintf(f,_("   or: %s [options] -{c|i} COMMAND\n"),lrzsz_progname);
	fputs(_("Send file(s) with ZMODEM/YMODEM/XMODEM protocol\n"),f);
	fputs(_(
		"    (X) = option applies to XMODEM only\n"
		"    (Y) = option applies to YMODEM only\n"
		"    (Z) = option applies to ZMODEM only\n"
		),f);
	/* splitted into two halves for really bad compilers */
	fputs(_(
"  -+, --append                append to existing destination file (Z)\n"
"  -2, --twostop               use 2 stop bits\n"
"  -4, --try-4k                go up to 4K blocksize\n"
"      --start-4k              start with 4K blocksize (doesn't try 8)\n"
"  -8, --try-8k                go up to 8K blocksize\n"
"      --start-8k              start with 8K blocksize\n"
"  -a, --ascii                 ASCII transfer (change CR/LF to LF)\n"
"  -b, --binary                binary transfer (Z)\n"
"  -B, --bufsize N             buffer N bytes (N==auto: buffer whole file)\n"
"  -c, --command COMMAND       execute remote command COMMAND (Z)\n"
"  -C, --command-tries N       try N times to execute a command (Z)\n"
"  -d, --dot-to-slash          (removed; use -f for full pathnames)\n"
"      --delay-startup N       sleep N seconds before doing anything\n"
"  -e, --escape                escape all control characters (Z)\n"
"  -E, --rename                force receiver to rename files it already has\n"
"  -f, --full-path             send full pathname (Y/Z)\n"
"  -g, --ymodem                use YMODEM protocol (Y)\n"
"  -h, --help                  print this usage message\n"
"  -H, --crc-check             accept and transfer files if lengths/CRCs differ (Z)\n"
"  -i, --immediate-command CMD send remote CMD, return immediately (Z)\n"
"  -J, --journal FILE          append transfer statistics to FILE\n"
"      --no-unixmode           send mode 0 instead of Unix permission bits\n"
"  -k, --1k                    send 1024 byte packets (X)\n"
"  -L, --packetlen N           limit subpacket length to N bytes (Z)\n"
"  -l, --framelen N            limit frame length to N bytes (l>=L) (Z)\n"
"  -m, --min-bps N             stop transmission if BPS below N\n"
"  -M, --min-bps-time N          for at least N seconds (default: 120)\n"
		),f);
	fputs(_(
"  -n, --newer                 send file if source newer (Z)\n"
"  -N, --newer-or-longer       send file if source newer or longer (Z)\n"
"  -o, --16-bit-crc            use 16 bit CRC instead of 32 bit CRC (Z)\n"
"  -O, --disable-timeouts      disable timeout code, wait forever\n"
"  -p, --protect               protect existing destination file (Z)\n"
"  -r, --resume                resume interrupted file transfer (Z)\n"
"  -R, --restricted            restricted, more secure mode\n"
"  -q, --quiet                 quiet (no progress reports)\n"
"  -s, --stop-at {HH:MM|+N}    stop transmission at HH:MM or in N seconds\n"
/* -S was timesync */
"  -u, --unlink                unlink file after transmission\n"
"  -v, --verbose               be verbose (twice: also show progress)\n"
"  --debug=MODULES             comma separated list: protocol, readline,\n"
"                              transfer, windowhandling, tty, all\n"
"  --sync-transfer             force a ZCRCW after every data subpacket\n"
"                              (integrity mode: slower and immune to loss of\n"
"                              whole subframes)\n"
"  -V, --version               show version information\n"
"  -w, --windowsize N          Window is N bytes (Z)\n"
"  -X, --xmodem                use XMODEM protocol\n"
"  -y, --overwrite             overwrite existing files\n"
"  -Y, --overwrite-or-skip     overwrite existing files, else skip\n"
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
 * Get the receiver's init parameters
 */
static int
getzrxinit(void)
{
	static int dont_send_zrqinit=1;
	int old_timeout=Rxtimeout;
	int n;
	struct stat f;
	size_t rxpos;
	int timeouts=0;
        int gotblock;

	Rxtimeout=100; /* 10 seconds */
	/* XXX purgeline(io_mode_fd); this makes _real_ trouble. why? -- uwe */

	for (n=10; --n>=0; ) {
		/* we might need to send another zrqinit in case the first is
		 * lost. But *not* if getting here for the first time - in
		 * this case we might just get a ZRINIT for our first ZRQINIT.
		 * Never send more then 4 ZRQINIT, because
		 * omen rz stops if it saw 5 of them.
		 */
		if (zrqinits_sent<4 && n!=10 && !dont_send_zrqinit) {
			zrqinits_sent++;
			stohdr(0L);
			zshhdr(ZRQINIT, Txhdr);
		}
		dont_send_zrqinit=0;

		gotblock = zgethdr(Rxhdr, 1,&rxpos);
		switch (gotblock) {
		case ZCHALLENGE:	/* Echo receiver's challenge numbr */
			stohdr(rxpos);
			zshhdr(ZACK, Txhdr);
			continue;
		case ZCOMMAND:		/* They didn't see our ZRQINIT */
			/* ??? Since when does a receiver send ZCOMMAND?  -- uwe */
			continue;
 		case ZRINIT:
 			Rxflags = 0xff & Rxhdr[ZF0];
 			Txfcs32 = (Wantfcs32 && (Rxflags & CANFC32));
 			{
 				int old=Zctlesc;
 				Zctlesc |= Rxflags & TESCCTL;
 				/* update table - was initialised to not escape */
 				if (Zctlesc && !old)
 					zsendline_init();
 			}
			Rxbuflen = (0xff & Rxhdr[ZP0])+((0xff & Rxhdr[ZP1])<<8);
			if ( !(Rxflags & CANFDX))
				Txwindow = 0;
			DPRINTF(DEBUG_WINDOWHANDLING, "Rxbuflen=%u Tframlen=%u\n", Rxbuflen, Tframlen);
			if ( play_with_sigint)
				signal(SIGINT, SIG_IGN);

			io_mode(io_mode_fd,2);	/* Set cbreak, XON/XOFF, etc. */

			/* Override to force shorter frame length */
			if (Tframlen && Rxbuflen > Tframlen)
				Rxbuflen = Tframlen;
			DPRINTF(DEBUG_WINDOWHANDLING, "Rxbuflen=%u\n", Rxbuflen);

			/*
			 * If input is not a regular file, force ACK's to
			 *  prevent running beyond the buffer limits
			 */
			if ( !command_mode) {
				fstat(fileno(input_f), &f);
				if (!(S_ISREG(f.st_mode))) {
					Canseek = -1;
					/* return ERROR; */
				}
			}

                        /* for segmented streaming: */                        
                        if (start_blklen > Rxbuflen && Rxbuflen) {
                            start_blklen = Rxbuflen;
                        }
                        if (max_blklen > Rxbuflen && Rxbuflen) {
                            max_blklen = Rxbuflen;
                        }

			/* Set initial subpacket length */
			if (blklen < 1024) {	/* Command line override? */
				if (Baudrate > 300)
					blklen = 256;
				if (Baudrate > 1200)
					blklen = 512;
				if (Baudrate > 2400)
					blklen = 1024;
			}
			if (Rxbuflen && blklen>Rxbuflen)
				blklen = Rxbuflen;
			if (blkopt && blklen > blkopt)
				blklen = blkopt;
			DPRINTF(DEBUG_WINDOWHANDLING, "Rxbuflen=%u blklen=%lu\n", Rxbuflen, blklen);
			DPRINTF(DEBUG_WINDOWHANDLING, "Txwindow = %u Txwspac = %u\n", Txwindow, Txwspac);
			Rxtimeout=old_timeout;
                        if (debugmode & DEBUG_WINDOWHANDLING) {
                            fprintf(stderr,"after ZRINIT: Rxbuflen=%u,Txwindow=%u,Txwspac=%u\n",Rxbuflen,Txwindow,Txwspac);
                        }
			return (sendzsinit());
		case ZCAN:
		case TIMEOUT:
			if (timeouts++==0)
				continue; /* force one other ZRQINIT to be sent */
			return ERROR;
		case ZABORT:
		case ZFERR:
			/* the receiver aborted during the handshake; ZFERR is
			 * spec'd as equivalent to ZABORT (§4). NAKing an
			 * aborting receiver would loop forever here - reply
			 * with the ZFIN sequence the spec requires (§4). */
			if (gotblock == ZABORT)
				stats_cancel();
			else
				stats_file_error();
			replied_abort = true;
			saybibi();
			return ERROR;
		case ZRQINIT:
			if (Rxhdr[ZF0] == ZCOMMAND)
				continue;
                        stohdr(0);
			zshhdr(ZNAK, Txhdr);
			continue;
		default:
                        stohdr(0);
			zshhdr(ZNAK, Txhdr);
			continue;
		}
	}
	return ERROR;
}

/* Send send-init information */
static int
sendzsinit(void)
{
	int c;

	if (Myattn[0] == '\0' && (!Zctlesc || (Rxflags & TESCCTL)))
		return OK;
	errors = 0;
	for (;;) {
		stohdr(0L);
		/* send ZSINIT as a BINARY header, because it carries a data subpacket
		 * (the attention string), and data subpackets after a hex header would 
                 * use a 16-bit CRC, which the current ZMODEM * documentation forbids 
                 * (zmodem-wip.txt §2.2). zsbhdr sets Crc32t per negotiation, so 
                 * the subpacket goes out with the negotiated CRC (normally CRC-32).
		 */
 		if (Zctlesc)
 			Txhdr[ZF0] |= TESCCTL;
 		zsbhdr(ZSINIT, Txhdr);
		ZSDATA(Myattn, 1+strlen(Myattn), ZCRCW);
		c = zgethdr(Rxhdr, 1,NULL);
		switch (c) {
		case ZCAN:
			return ERROR;
		case ZACK:
			return OK;
		default:
			if (++errors > 19)
				return ERROR;
			continue;
		}
	}
}

/* Send file name and related info */
static int
zsendfile(struct zm_fileinfo *zi, const char *buf, size_t blen)
{
	unsigned long crc;
	size_t rxpos;
	int bad_acks = 0;

	/* we are going to send a ZFILE. There cannot be much useful
	 * stuff in the line right now (*except* ZCAN?).
	 */

	for (;;) {
		int gotblock;
		int gotchar;
		Txhdr[ZF0] = Lzconv;	/* file conversion request */
		Txhdr[ZF1] = Lzmanag;	/* file management request */
		if (Lskipnocor)
			Txhdr[ZF1] |= ZF1_ZMSKNOLOC;
		Txhdr[ZF2] = Lztrans;	/* file transport request */
		Txhdr[ZF3] = 0;
		zsbhdr(ZFILE, Txhdr);
		ZSDATA(buf, blen, ZCRCW);
again:
		gotblock = zgethdr(Rxhdr, 1, &rxpos);
		switch (gotblock) {
		case ZRINIT:
			while ((gotchar = READLINE_PF(50)) > 0)
				if (gotchar == ZPAD) {
					goto again;
				}
			/* **** FALL THRU TO **** */
			/* FALLTHROUGH */
		default:
			/* e.g. ZNAK from a receiver that cannot cope with the
			 * ZFILE subpacket. Resending identically a few times is
			 * fine, but don't loop forever (zmodem-wip.txt §4:
			 * the sender must be able to fall back - our subpacket
			 * never exceeds 1024 bytes, so a receiver that keeps
			 * ZNAKing won't be satisfied by resending). */
			if (++bad_acks > 3) {
				zperr(_("receiver keeps rejecting the file header"));
				return ERROR;
			}
			continue;
		case ZACK:
                        /* zmrx answers the ZCRCW for the ZFILE (filename etc)
                         * with a ZACK; omen never did - it used the ZRPOS as
                         * the ACK. A receiver that keeps ACKing must not make
                         * us resend the ZFILE forever, so ZACK shares the
                         * default branch's retry budget. */
                        if (++bad_acks > 3) {
                            zperr(_("receiver keeps rejecting the file header"));
                            return ERROR;
                        }
                        goto again;
		case ZRQINIT:  /* remote site is sender! */
			if (Verbose)
				vstringf(_("got ZRQINIT"));
			DO_SYSLOG(LOG_INFO, "%s/%s: got ZRQINIT - sz talks to sz",
					   lrzsz_basename(zi->fname), protname());
			return ERROR;
		case ZCAN:
			if (Verbose)
				vstringf(_("got ZCAN"));
			DO_SYSLOG(LOG_INFO, "%s/%s: got ZCAN - receiver canceled",
					   lrzsz_basename(zi->fname), protname());
			return ERROR;
		case TIMEOUT:
			DO_SYSLOG(LOG_INFO, "%s/%s: got TIMEOUT",
					   lrzsz_basename(zi->fname), protname());
			return ERROR;
		case RCDO:
			/* line dead (peer closed): retrying against it is
			 * pointless - getinsync() treats RCDO like TIMEOUT,
			 * and so must this phase. */
			DO_SYSLOG(LOG_INFO, "%s/%s: got RCDO",
					   lrzsz_basename(zi->fname), protname());
			return ERROR;
		case ZABORT:
		case ZFERR:
			/* the receiver aborted during the file-header phase;
			 * ZFERR is spec'd as equivalent to ZABORT
			 * (zmodem-wip.txt §4). Don't resend the ZFILE
			 * against a receiver that just gave up - reply with
			 * the ZFIN sequence the spec requires (§4). */
			if (gotblock == ZABORT)
				stats_cancel();
			else
				stats_file_error();
			DO_SYSLOG(LOG_INFO, "%s/%s: got %s - receiver aborted",
				   lrzsz_basename(zi->fname), protname(),
				   gotblock == ZABORT ? "ZABORT" : "ZFERR");
			replied_abort = true;
			if (input_f) {
				fclose(input_f);
				input_f=NULL;
			}
			saybibi();
			return ERROR;
		case ZFIN:
			DO_SYSLOG(LOG_INFO, "%s/%s: got ZFIN",
				   lrzsz_basename(zi->fname), protname());
			return ERROR;
		case ZCRC:
			crc = 0xFFFFFFFFL;
			if (Canseek >= 0) {
                                /* the input_f check is for the static analyzer clang-tidy,
                                 * which doesn't catch the logic around input_f. */
                                if (input_f) {
                                    if (rxpos==0) {
                                            struct stat st;
                                            if (0==fstat(fileno(input_f),&st)) {
                                                    rxpos=st.st_size;
                                            } else {
                                                    int er=errno;
                                                    lrzsz_warning(0, _("fstat failed: %s"), strerror(er));
                                                    rxpos=SIZE_MAX;
                                            }
                                    }
                                    while (rxpos-- && ((gotchar = getc(input_f)) != EOF))
                                            crc = UPDC32(gotchar, crc);
                                    crc = ~crc;
                                    clearerr(input_f);	/* Clear EOF */
                                    fseeko(input_f, 0, 0);
                                }
			}
			stohdr(crc);
			zsbhdr(ZCRC, Txhdr);
			goto again;
		case ZSKIP:
			if (input_f) {
				fclose(input_f);
				input_f=NULL;
			}

			DPRINTF(DEBUG_TRANSFER, "receiver skipped\n");
			DO_SYSLOG(LOG_INFO, "%s/%s: receiver skipped",
					   lrzsz_basename(zi->fname), protname());
			return ZSKIP;
		case ZRPOS:
			/*
			 * Suppress zcrcw request otherwise triggered by
			 * lastsync==bytcnt
			 */
			if (rxpos && fseeko(input_f, (off_t) rxpos, 0)) {
				int er=errno;
				DPRINTF(DEBUG_TRANSFER, "fseeko failed: %s\n", strerror(er));
				DO_SYSLOG(LOG_INFO, "%s/%s: fseeko failed: %s",
						   lrzsz_basename(zi->fname), protname(), strerror(er));
				return ERROR;
			}
			if (rxpos)
				zi->bytes_skipped=rxpos;
			bytcnt = zi->bytes_sent = rxpos;
			Lastsync = rxpos -1;
	 		return zsendfdata(zi);
		}
	}
}

/* The ACK/resync machinery of zsendfdata():
 * - the decision (former gotack) to return ERROR, ZSKIP or 0/OK.
 * - the pending-input drain (the former waitack/gotack block).
 * sync_first: run getinsync(zi,0) first (the former waitack entry, used
 * after a ZCRCW subpacket and for the SIGINT resync);
 * c: caller's preset verdict byte (the former goto gotack sites).
 * Returns ERROR or ZSKIP for the caller to propagate, or 0/OK for
 * "synced, keep going". in the later case the caller re-sends the ZDATA
 * header (the old waitack fall-through to normal flow did exactly that).
 * Not a piece of beauty, but so much better than before.
 */
static int
ack_resync (struct zm_fileinfo *zi, int c, bool sync_first,
             int *junkcountp, int *need_waitp)
{
	for (;;) {
		if (sync_first) {
			/* the former waitack head: the receiver is asked for
			 * its state before the verdict runs */
			*junkcountp = 0;
			c = getinsync (zi, 0);
			sync_first = false;
		}
		switch (c) {
		default:
			if (input_f) {
				fclose (input_f);
				input_f=NULL;
			}
			DO_SYSLOG(LOG_INFO, "%s/%s: got %d",
					   lrzsz_basename(zi->fname), protname(), c);
			return ERROR;
		case ZCAN:
			if (input_f) {
				fclose (input_f);
				input_f=NULL;
			}
			DO_SYSLOG(LOG_INFO, "%s/%s: got ZCAN",
					   lrzsz_basename(zi->fname), protname());
			return ERROR;
		case ZSKIP:
			if (input_f) {
				fclose (input_f);
				input_f=NULL;
			}
			DO_SYSLOG(LOG_INFO, "%s/%s: got ZSKIP",
					   lrzsz_basename(zi->fname), protname());
			return ZSKIP;
		case ZACK:
			break;
		case ZRPOS:
			/* a ZRPOS restart must resume with the first data
			 * subpacket being ZCRCW (zmodem-wip.txt section 5).
			 * need_wait was previously armed only on the CAN/ZPAD
			 * drain path below; a verdict-here ZRPOS (the normal
			 * error-recovery path) would otherwise resume with
			 * ZCRCG. */
			*need_waitp = 1;
			break;
		case ZRINIT:
			/* the receiver finished the file and closed our input
			 * (getinsync): the data phase must terminate, never
			 * re-enter it - zfilbuf() would fread() from NULL.
			 * (OK would mean "resend ZDATA" to the callers.) */
			return ZRINIT;
		}

		/* drain pending receiver input; CAN/ZPAD and read errors
		 * escalate back to the verdict switch (the former goto
		 * gotack), junk counts the rest */
		{
			int reswitch = 0;
			while (fd_readable (io_mode_fd)) {
				c = READLINE_PF(1);
				if (TIMEOUT == c || ERROR == c || RCDO == c) {
					reswitch = 1;
					break;
				}
				switch (c) {
				case CAN:
				case ZPAD:
					c = getinsync (zi, 1);
					*need_waitp = 1;
					reswitch = 1;
					break;
				case XOFF:			/* Wait a while for an XON */
				case XOFF | 0x80:
					READLINE_PF (100);
					/* FALLTHROUGH */
				default:
					++*junkcountp;
				}
				if (reswitch)
					break;
			}
			if (reswitch)
				continue;
			return 0;
		}
	}
}

/* The SIGINT checkpoint (former onintr/intrjmp longjmp return path): the
 * receiver is asked for its state and the verdict handled. Called at the
 * data-loop checkpoints; every read in between is timeout-bounded, so the
 * resync happens within at most one read timeout of the ^C. */
static int
sigint_resync (struct zm_fileinfo *zi, int *junkcountp, int *need_waitp)
{
	return ack_resync (zi, 0, true, junkcountp, need_waitp);
}

/* Send the data in the file */
static int
zsendfdata (struct zm_fileinfo *zi)
{
	/* total_sent is the only state here that must persist across files:
	 * it feeds calc_blklen()'s adaptive blocksize for the whole batch
	 * (and must never shrink back for file 2). Everything below it is
	 * per-file state.
	 */
	static long total_sent = 0;
	int c;
	int junkcount;				/* Counts garbage chars received by TX */
	size_t last_txpos = 0;
	long not_printed = 0;
	double low_bps = 0;
        int newcnt;
	int rc;
	time_t now = 0;		/* wall clock: -s stop deadline */
	/* the first data subpacket after a ZRPOS must be ZCRCW
	 * (zmodem-wip.txt §5, sender-side error recovery). Without this
	 * reset only the FIRST file of a batch started with ZCRCW (need_wait
	 * was static and consumed by file 1); later files started with ZCRCG. */
        int need_wait = 1;

	Lrxpos = 0;
	junkcount = 0;
	Beenhereb4 = 0;
	last_txpos = 0;
	low_bps = 0;
	not_printed = 0;

	/* the SIGINT handler is installed once, before the first data phase
	 * (zsendfdata runs once per file); it only sets sigint_seen, and the
	 * loop checkpoints below attempt the receiver resync when it is set.
	 * The old longjmp-based version re-executed setjmp on the ZRPOS
	 * restart and jumped out of stdio mid-read; both are gone. */
	if (play_with_sigint)
		signal (SIGINT, onintr);

	/* the ZRPOS restart re-enters here (the former somemore label): the
	 * ZDATA header is sent again, per-file counters keep their values
	 * (resume-in-place). No setjmp remains - the SIGINT checkpoint in
	 * the loop below handles the interrupt flag. */
restart_data:
        newcnt = Rxbuflen;
	Txwcnt = 0;
	stohdr (zi->bytes_sent);
	zsbhdr (ZDATA, Txhdr);

	do {
		size_t n;
		int e=0;
		unsigned long old = blklen;
		/* the SIGINT checkpoint (former longjmp return path): the
		 * flag is set by the handler during any of the timeout-
		 * bounded reads below; the resync attempt happens here */
		if (sigint_seen) {
			sigint_seen = 0;
			rc = sigint_resync (zi, &junkcount, &need_wait);
			if (rc)
				return rc;
			goto restart_data;	/* re-send ZDATA like the old
						   waitack fall-through */
		}
		blklen = calc_blklen (total_sent);
		total_sent += blklen + OVERHEAD;
		if (blklen != old)
			DPRINTF(DEBUG_WINDOWHANDLING, "zsendfdata: blklen changed %lu -> %lu\n",
				(unsigned long) old, (unsigned long) blklen);
		{
			n = zfilbuf (zi);
                }
		if (zi->eof_seen) {
			e = ZCRCE;
			DPRINTF(DEBUG_WINDOWHANDLING, "e=ZCRCE/eof seen\n");
		} else if (sync_transfer) {
			/* integrity mode: wait for an ACK after every subpacket.
			 * This detects the loss of complete subframes on unreliable 
			 * transport layers, which would be undetectable in the
			 * streaming mode.
			 */
			e = ZCRCW;
			DPRINTF(DEBUG_WINDOWHANDLING, "e=ZCRCW/sync-transfer\n");
		} else if (junkcount > 3) {
			e = ZCRCW;
			DPRINTF(DEBUG_WINDOWHANDLING, "e=ZCRCW/junkcount > 3\n");
		} else if (bytcnt == Lastsync) {
			e = ZCRCW;
			DPRINTF(DEBUG_WINDOWHANDLING,
				"e=ZCRCW/bytcnt == Lastsync == %lu\n",
				(unsigned long) Lastsync);
                /* segmented streaming. note: newcnt is reset to Rxbuflen in the goto jungle. */
                } else if (Rxbuflen && (newcnt -= n) <= 0) {
                        e = ZCRCW;
                        DPRINTF(DEBUG_WINDOWHANDLING,
                                "e=ZCRCW/Rxbuflen(newcnt=%lu,n=%lu, Rxbuflen=%lu)\n",
                                (unsigned long) newcnt,(unsigned long) n, (unsigned long) Rxbuflen);
		} else if (Txwindow && (Txwcnt += n) >= Txwspac) {
			Txwcnt = 0;
			e = ZCRCQ;
			DPRINTF(DEBUG_WINDOWHANDLING, "e=ZCRCQ/Window\n");
		} else if (need_wait) {
			e = ZCRCW;
                        need_wait = 0;
			DPRINTF(DEBUG_WINDOWHANDLING, "e=ZCRCW/need_wait\n");
		} else {
			e = ZCRCG;
			DPRINTF(DEBUG_WINDOWHANDLING, "e=ZCRCG\n");
		}
		now = time (NULL);	/* wall clock: -s stop deadline */
		if (stop_time
			&& lrzsz_stop_time_reached ("zsendfdata", now,
						    stop_time, zi->fname))
			return ERROR;
		if (Verbose > 1 || min_bps) {
	                long last_bps = 0;
			int minleft = 0;
			int secleft = 0;
			double d;
			d=timing_elapsed();
			if (d == 0)
				d = 0.5; /* first call: zero elapsed */
			last_bps = (long) ((zi->bytes_sent / d));
			/* the bps watchdog needs regular sampling: it arms at
			 * the first violation and fires after min_bps_time of
			 * cumulative elapsed time below the limit. */
			if (min_bps
				&& lrzsz_bps_watchdog ("zsendfdata", last_bps, d,
						       min_bps, min_bps_time,
						       &low_bps, zi->fname))
				return ERROR;

			if (Verbose > 1) {
				/* print throttling: only refresh the line at
				 * most every (min_bps ? 3 : 7) unprinted
				 * subpackets, or after half a cumulative-bps
				 * worth of data */
				if (not_printed > (min_bps ? 3 : 7)
					|| zi->bytes_sent > last_bps / 2 + last_txpos) {
					lrzsz_eta (last_bps,
						   zi->bytes_total - zi->bytes_sent,
						   &minleft, &secleft);
					vchar ('\r');
					vstringf (_("Bytes Sent:%7ld/%7ld   BPS:%-8ld ETA %02d:%02d  "),
						 (long) zi->bytes_sent, (long) zi->bytes_total,
						last_bps, minleft, secleft);
					last_txpos = zi->bytes_sent;
					not_printed = 0;
				} else
					not_printed++;
			}
		}
		ZSDATA (txbuf, n, e);
		bytcnt = zi->bytes_sent += n;
		if (e == ZCRCW) {
			/* the mandatory ACK wait (the former waitack entry) */
			rc = ack_resync (zi, 0, true, &junkcount, &need_wait);
			if (rc)
				return rc;
			goto restart_data;
		}

		fflush (stdout);
		while (fd_readable (io_mode_fd)) {
                        c=READLINE_PF(1);
                        if (TIMEOUT == c || ERROR == c || RCDO == c) {
                            /* escalate to the verdict switch; after the
                             * verdict the ZDATA header is re-sent (the old
                             * goto gotack fell through to Normal flow) */
                            rc = ack_resync (zi, c, false, &junkcount,
                                             &need_wait);
                            if (rc)
                                return rc;
                            goto restart_data;
                        }
			switch (c) {
			case CAN:
			case ZPAD:
				c = getinsync (zi, 1);
				if (c == ZACK)
					break;
				/* zcrce - dinna wanna starta ping-pong game */
				ZSDATA (txbuf, 0, ZCRCE);
				{
					/* escalate to the verdict switch */
					rc = ack_resync (zi, c, false,
							 &junkcount,
							 &need_wait);
					if (rc)
						return rc;
					goto restart_data;
				}
				break;
			case XOFF:			/* Wait a while for an XON */
			case XOFF | 0x80:
				READLINE_PF (100);
                                /* FALLTHROUGH */
			default:
				++junkcount;
			}
		}

                /* the window might be full, but ZCRCE might have been sent because of EOF. Do not try to eat the ACK */
		if (Txwindow && e != ZCRCE && e != ZCRCG) {
			size_t tcount = 0;
			int stuck_acks = 0;
			size_t prev_lrxpos = Lrxpos;
			while (zi->bytes_sent >= Lrxpos &&
			       (tcount = zi->bytes_sent - Lrxpos) >= Txwindow) {
				DPRINTF(DEBUG_WINDOWHANDLING,
					"%zu (%ld,%ld) window >= %u\n", tcount,
					(long) zi->bytes_sent, (long) Lrxpos,
					Txwindow);
				if (e != ZCRCQ) {
					ZSDATA (txbuf, 0, e = ZCRCQ);
                                }
				c = getinsync (zi, 1);
				if (c != ZACK) {
					ZSDATA (txbuf, 0, ZCRCE);
					rc = ack_resync (zi, c, false,
							 &junkcount,
							 &need_wait);
					if (rc)
						return rc;
					goto restart_data;
				}
				if (Lrxpos == prev_lrxpos) {
					if (++stuck_acks > RETRYMAX) {
						lrzsz_warning(0, _("receiver not advancing after %d ACKs"), stuck_acks);
						ZSDATA (txbuf, 0, ZCRCE);
						c = ERROR;
						rc = ack_resync (zi, c, false,
								 &junkcount,
								 &need_wait);
						if (rc)
							return rc;
						goto restart_data;
					}
				} else {
					stuck_acks = 0;
					prev_lrxpos = Lrxpos;
				}
			}
			DPRINTF(DEBUG_WINDOWHANDLING, "window = %zu\n",
				tcount);
		}
	} while (!zi->eof_seen);


	if (play_with_sigint)
		signal (SIGINT, SIG_IGN);

	for (;;) {
		int got;
		stohdr (zi->bytes_sent);
		zsbhdr (ZEOF, Txhdr);
		got = getinsync (zi, 0);
		switch (got) {
		case ZACK:
			continue;
		case ZRPOS:
			goto restart_data;
		case ZRINIT:
			return OK;
		case ZSKIP:
			if (input_f) {
				fclose (input_f);
				input_f=NULL;
			}
			DO_SYSLOG(LOG_INFO, "%s/%s: got ZSKIP",
					   lrzsz_basename(zi->fname), protname());
			return ZSKIP;
		default:
			if (input_f) {
				fclose (input_f);
				input_f=NULL;
			}
			DO_SYSLOG(LOG_INFO, "%s/%s: got %d",
					   lrzsz_basename(zi->fname), protname(), got);
			return ERROR;
		}
	}
}

static int
calc_blklen_worker(long total_sent)
{
	static long total_bytes=0;
	static int calcs_done=0;
	static long last_error_count=0;
	static int last_blklen=0;
	static long last_bytes_per_error=0;
	unsigned long best_bytes=0;
	long best_size=0;
	long this_bytes_per_error;
	long d;
        static unsigned original_rxbuflen=0;
	unsigned int i;
	if (total_bytes==0)
	{
		/* called from countem */
		total_bytes=total_sent;
		return 0;
	}

        /*
         * to avoid synchronization problems on faulty lines we limit the Rxbuflen and the window.
         * The problem to solve:
         * - on a high bandwidth pipe a mass of ZDATE data subpackets will be in flight.
         * - the receiver sends back an ZRPOS (#1)
         * - still datapackets will come in
         * - the receiver sends back an ZRPOS (#2)
         * - [still datapackets will come in, and new ZRPOS will be created] (#3)
         * - now the receiver sees a new ZDATA for ZPOS #1
         * - and many data subpackets
         * - now the receiver sees a new ZDATA for ZPOS #2
         * - and the many data subpackets again.
         * the critical point is #3. Repeat that often enough, and synchronization
         * will not happen.
         * 
         * This will not happen with a "low" Rxbuflen, because then the sender waits
         * for a receivers ACK. 
         *
         * Fun fact: this brute force solution is good for the performance.
         */
        if (error_count) {
            unsigned int last_rxbuflen=Rxbuflen;
            if (!Rxbuflen) {
                Rxbuflen=SAFETY_RXBUFLEN;
            }
            if (!original_rxbuflen) {
                original_rxbuflen=Rxbuflen;
            }
            Rxbuflen=original_rxbuflen;
            if (last_bytes_per_error <=    512000 && Rxbuflen>65536) Rxbuflen=65536;
            if (last_bytes_per_error <=    256000 && Rxbuflen>32768) Rxbuflen=32768;
            if (last_bytes_per_error <=    128000 && Rxbuflen>16384) Rxbuflen=16384;
            if (last_bytes_per_error <=     64000 && Rxbuflen> 8192) Rxbuflen= 8192;
            if (last_bytes_per_error <=     32000 && Rxbuflen> 4096) Rxbuflen= 4096;
            Txwindow=(Rxbuflen)/2;
            Txwindow = (Rxbuflen/2/64) * 64;
            Txwspac = Txwindow/4;
            if (Rxbuflen != last_rxbuflen) {
                DPRINTF(DEBUG_WINDOWHANDLING,
                    "calc_blklen: LPBE %lu => RXB %lu Txwindow %lu\n", 
                last_bytes_per_error, (unsigned long) Rxbuflen, (unsigned long) Txwindow);
            }
        }

	/* it's not good to calc blklen too early */
	if (calcs_done++ < 5) {
		if (error_count && start_blklen >1024)
			return last_blklen=1024;
		return last_blklen=start_blklen;
	}

	if (!error_count) {
		/* that's fine */
		if (start_blklen==max_blklen)
			return start_blklen;
		this_bytes_per_error=LONG_MAX;
		goto calcit;
	}

	if (error_count!=last_error_count) {
		/* the last block was bad. shorten blocks until one block is
		 * ok. this is because very often many errors come in an
		 * short period */
		if (error_count & 2)
		{
			last_blklen/=2;
			if (last_blklen < 32)
				last_blklen = 32;
			DPRINTF(DEBUG_WINDOWHANDLING,
				"calc_blklen: reduced to %d due to error\n",
				last_blklen);
		}
		last_error_count=error_count;
		last_bytes_per_error=0; /* force recalc */
		return last_blklen;
	}

	this_bytes_per_error=total_sent / error_count;
		/* we do not get told about every error, because
		 * there may be more than one error per failed block.
		 * but one the other hand some errors are reported more
		 * than once: If a modem buffers more than one block we
		 * get at least two ZRPOS for the same position in case
		 * *one* block has to be resent.
		 * so don't do this:
		 * this_bytes_per_error/=2;
		 */
	/* there has to be a margin */
	if (this_bytes_per_error<100)
		this_bytes_per_error=100;

	/* be nice to the poor machine and do the complicated things not
	 * too often
	 */
	if (last_bytes_per_error>this_bytes_per_error)
		d=last_bytes_per_error-this_bytes_per_error;
	else
		d=this_bytes_per_error-last_bytes_per_error;
	if (d<4)
	{
		DPRINTF(DEBUG_WINDOWHANDLING,
			"calc_blklen: returned old value %d due to low bpe diff\n",
			last_blklen);
		DPRINTF(DEBUG_WINDOWHANDLING,
			"calc_blklen: old %ld, new %ld, d %ld\n",
			last_bytes_per_error, this_bytes_per_error, d);
		return last_blklen;
	}
	last_bytes_per_error=this_bytes_per_error;

calcit:
	DPRINTF(DEBUG_WINDOWHANDLING,
		"calc_blklen: calc total_bytes=%ld, bpe=%ld, ec=%ld\n",
		total_bytes, this_bytes_per_error, (long) error_count);
	for (i=32;i<=max_blklen;i*=2) {
		long ok; /* some many ok blocks do we need */
		long failed; /* and that's the number of blocks not transmitted ok */
		unsigned long transmitted;
		ok=total_bytes / i + 1;
		failed=((long) i + OVERHEAD) * ok / this_bytes_per_error;
		transmitted=total_bytes + ok * OVERHEAD
			+ failed * ((long) i+OVERHEAD+OVER_ERR);
		DPRINTF(DEBUG_WINDOWHANDLING,
			"calc_blklen: blklen %u, ok %ld, failed %ld -> %lu\n",
			i, ok, failed, transmitted);
		if (transmitted < best_bytes || !best_bytes)
		{
			best_bytes=transmitted;
			best_size=i;
		}
	}
	if (best_size > 2*last_blklen)
		best_size=2*last_blklen;
	last_blklen=best_size;
	DPRINTF(DEBUG_WINDOWHANDLING, "calc_blklen: returned %d as best\n",
		last_blklen);
	return last_blklen;
}

static int
calc_blklen(long total_sent) {
    unsigned int old_Rxbuflen=Rxbuflen;
    unsigned int old_Txwindow=Txwindow;
    static int last_blklen;
    int ret=calc_blklen_worker(total_sent);
    if (debugmode & DEBUG_WINDOWHANDLING) {
        if (old_Rxbuflen!=Rxbuflen) {
            fprintf(stderr,"calc_blklen changed Rxbuflen from %u to %u\n",old_Rxbuflen,Rxbuflen);
        }
        if (old_Txwindow!=Txwindow) {
            fprintf(stderr,"calc_blklen changed Txwindow from %u to %u\n",old_Txwindow,Txwindow);
        }
        if (ret!=last_blklen && last_blklen) {
            fprintf(stderr,"calc_blklen changed blklen from %u to %u\n",last_blklen,ret);
        }
    }
    last_blklen=ret;
    return ret;
}

/*
 * Respond to receiver's complaint, get back in sync with receiver
 */
static int
getinsync(struct zm_fileinfo *zi, int flag)
{
	size_t rxpos;
	int gotblock;
	int garbage=0; /* consecutive garbled headers; bounds the ZNAK retry */

	for (;;) {
		/* another stop_time checkpoint: a ZCRCW ack wait
		 * blocks here up to the receiver's activity level, and the
		 * per-subpacket check in zsendfdata does not run while we
		 * are waiting here. So check. */
		if (stop_time
			&& lrzsz_stop_time_reached ("getinsync", time (NULL),
						    stop_time, zi->fname))
			return ERROR;
		gotblock = zgethdr(Rxhdr, 0, &rxpos);
		switch (gotblock) {
		case ZCAN:
		case ZFIN:
		case TIMEOUT:
		case RCDO:
			/* RCDO: the line is gone (peer closed, carrier lost).
			 * Retrying (ZNAK etc.) is futile and unbounded against
			 * a dead line - treat like TIMEOUT. */
			return ERROR;
		case ZABORT:
		case ZFERR:
			/* the receiver reported a fatal file error (ZFERR is
			 * spec'd as equivalent to ZABORT, zmodem-wip.txt
			 * §4) and wants the transfer ended - reply with the
			 * ZFIN sequence the spec requires (§4), not with
			 * retries or a cancel sequence. ZABORT: user cancel;
			 * ZFERR: receiver file error. */
			if (gotblock == ZABORT)
				stats_cancel();
			else
				stats_file_error();
			replied_abort = true;
			if (input_f) {
				fclose (input_f);
				input_f=NULL;
			}
			saybibi();
			return ERROR;
		case ZRPOS:
			/*  If sending to a buffered modem, you
			 *   might send a break at this point to
			 *   dump the modem's buffer.		 */
			/* purge pending output: bytes already written but not
			 * yet transmitted would only be stale data the receiver
			 * must discard (zmodem-wip.txt §5, sender-side
			 * error recovery). No-op on pipes/ptys. */
			tcflush(io_mode_fd, TCOFLUSH);
			if (input_f)
				clearerr(input_f);	/* In case file EOF seen */

			/* A ZRPOS is always honored as "resume at this offset",
			 * including a same-position one (rxpos == bytes_sent): §5
			 * grants the receiver the right to force a fresh sync
			 * point that way, and requires the first data subpacket
			 * after a ZRPOS to be ZCRCW (ack_resync() re-arms
			 * need_wait on this path). An earlier version used to
			 * translate a same-position ZRPOS into a ZACK here - a
			 * speed workaround for zmrx 1.02, which answers every
			 * ZCRCW with exactly such a ZRPOS - deviating from §5 by
			 * silently reinterpreting the frame type; removed. */

			/* the input_f check is for the static analyzer clang-tidy,
			 * which doesn't catch the logic around input_f. */
			if (input_f) {
				/* §5: a ZRPOS with rxpos > 0 into a non-seekable input
				 * (pipe, socket, char device) must not abort the
				 * transfer; the spec lets the sender ignore it (or
				 * restart - which on such input can only ever mean
				 * restarting from 0, since that is the only position a
				 * stream can still deliver). Ignore. */
				struct stat fst;
				if (fstat(fileno(input_f), &fst) == 0) {
					if (!S_ISREG(fst.st_mode)) {
						if (rxpos > 0)
							continue;
					} else if ((unsigned long long)rxpos > (unsigned long long)fst.st_size) {
						/* resume beyond our EOF: attack,
						 * receiver bug, or our file was
						 * shortened since the receiver's
						 * partial was made. No wire answer
						 * serves the receiver (clamp ->
						 * ZRPOS/ZEOF loop; ignore -> offset
						 * desync loop; ZSKIP is sender-side
						 * forbidden). Reject the frame (§5
						 * "implementation-defined") and end
						 * the file locally, like fseeko
						 * failure. Receiver keeps partial. */
						DO_SYSLOG(LOG_INFO,
							"%s/%s: resume beyond EOF (%llu > %llu)",
							lrzsz_basename(zi->fname),
							protname(),
							(unsigned long long)rxpos,
							(unsigned long long)fst.st_size);
						return ERROR;
					}
				}
				if (fseeko(input_f, (off_t) rxpos, 0))
					return ERROR;
			}
			zi->eof_seen = 0;
			bytcnt = Lrxpos = zi->bytes_sent = rxpos;
			/* every ZRPOS here is an error event: the receiver only
			 * re-requests a position after a subpacket failed CRC or
			 * was lost on the line (the initial ZRPOS after ZFILE is
			 * handled in zsendfile(), not here). The old same-position
			 * test undercounted on lines where every resync succeeded
			 * first-try: error_count stayed 0, the adaptive blklen and
			 * the Rxbuflen safety ladder never engaged, and the sender
			 * kept 8k subpackets whose terminators sat behind holes -
			 * each terminator-eating drop then cost a full 10s
			 * Rxtimeout instead of a fast CRC verdict. */
			error_count++;
			Lastsync = rxpos;
			return ZRPOS;
		case ZACK:
			Lrxpos = rxpos;
			if (flag || zi->bytes_sent == rxpos)
				return ZACK;
			garbage = 0; /* a parsed frame: not the loop we're bounding */
			continue;
		case ZRINIT:
			/* The receiver got the file and is ready for the next.
                         * return ZRINIT, not ZSKIP, for correct accounting.
                         * It must be ZRINIT, not OK: ack_resync()'s verdict
                         * switches on this value, and OK would mean "resend
                         * ZDATA" there - with input_f closed, zfilbuf()
                         * would fread() from NULL.
                         * One day i'll possibly clean up the function exit 
                         * codes.
                         */
			if (input_f) {
				fclose (input_f);
				input_f=NULL;
			}
			return ZRINIT;
		case ZSKIP:
			if (input_f) {
				fclose (input_f);
				input_f=NULL;
			}
			return ZSKIP;
		case ERROR:
		default:
			error_count++;
			/* was unbounded: a peer (or a noisy line) that keeps
			 * delivering garbled headers got a ZNAK for each, forever.
			 * error_count only feeds the adaptive blocksize; nothing
			 * ever stopped retrying. Give up after a bounded run. */
			if (++garbage > 10)
				return ERROR;
                        stohdr(0);
			zsbhdr(ZNAK, Txhdr);
			continue;
		}
	}
}


/* Say "bibi" to the receiver, try to do it cleanly. Two roles: the normal
 * end-of-batch close (zmodem_requested path), and the reply to a receiver
 * that aborted with ZABORT/ZFERR - the spec requires the sender to respond
 * with a ZFIN sequence there (zmodem-wip.txt §4, "The sender responds
 * with a ZFIN sequence"), the historical OMEN cancel reply deviated from
 * that. A receiver that has died answers with RCDO/nothing: bounded, so we
 * don't loop forever on a dead line. */
static void
saybibi(void)
{
	for (;;) {
		stohdr(0L);		/* CAF Was zsbhdr - minor change */
		zshhdr(ZFIN, Txhdr);	/*  to make debugging easier */
		switch (zgethdr(Rxhdr, 0,NULL)) {
		case ZFIN:
			sendline('O');
			sendline('O');
			flushmo();
		case ZCAN:
		case RCDO:
		case TIMEOUT:
			return;
                default:
                        ;
		}
	}
}

/* Send command and related info */
static int
zsendcmd(const char *buf, size_t blen)
{
	int c;
	pid_t cmdnum;
	size_t rxpos;

	cmdnum = getpid();
	errors = 0;
	for (;;) {
		stohdr((size_t) cmdnum);
		Txhdr[ZF0] = Cmdack1;
		zsbhdr(ZCOMMAND, Txhdr);
		ZSDATA(buf, blen, ZCRCW);
listen:
		Rxtimeout = 100;		/* Ten second wait for resp. */
		c = zgethdr(Rxhdr, 1, &rxpos);

		switch (c) {
		case ZRINIT:
			goto listen;	/* CAF 8-21-87 */
		case ERROR:
		case TIMEOUT:
			if (++errors > Cmdtries)
				return ERROR;
			continue;
		case ZCAN:
			return ERROR;
		case ZABORT:
		case ZFERR:
			/* ZFERR is specified as equivalent to ZABORT
			 * (zmodem-wip.txt §4): the receiver reported a
			 * fatal file error. Do not retry the command against
			 * a dead filesystem. Reply with the ZFIN sequence the
			 * spec requires for ZABORT (§4). */
			stats_cancel();
			replied_abort = true;
			saybibi();
			return ERROR;
		case ZFIN:
		case ZSKIP:
		case ZRPOS:
			return ERROR;
		default:
			if (++errors > 20)
				return ERROR;
			continue;
		case ZCOMPL:
			Exitcode = rxpos;
			saybibi();
			return OK;
		case ZRQINIT:
			DPRINTF(DEBUG_TRANSFER, _("got a ZRQINIT packet: the other side is trying to send, too\n"));
			if (++errors > 20)
				return ERROR;
		}
	}
}

/* chkinvok() lives in util.c (shared with lrz); called with 's' here. */

static void
countem (int argc, char **argv)
{
	struct stat f;

	for (Totalleft = 0, Filesleft = 0; --argc >= 0; ++argv) {
		f.st_size = -1;
		DPRINTF(DEBUG_TRANSFER, "\nCountem: %03d %s ", argc, *argv);
		if (access (*argv, R_OK) >= 0 && stat (*argv, &f) >= 0) {
			if (!S_ISDIR(f.st_mode) && !S_ISBLK(f.st_mode)) {
				++Filesleft;
				Totalleft += f.st_size;
			}
		} else if (strcmp (*argv, "-") == 0) {
			++Filesleft;
			Totalleft += DEFBYTL;
		}
		DPRINTF(DEBUG_TRANSFER, " %ld", (long) f.st_size);
	}
	DPRINTF(DEBUG_TRANSFER, "\ncountem: Total %d %ld\n",
			 Filesleft, Totalleft);
	calc_blklen (Totalleft);
}

/* End of lsz.c */



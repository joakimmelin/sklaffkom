#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include <sys/select.h>
#include "zglobal.h"
#include "timing.h"

#ifndef _
# define _(String) String
#endif

const char * lrzsz_progname;

unsigned int debugmode=0;

struct debug_name {
	const char *name;
	unsigned int mask;
};

bool
lrzsz_parse_debug (const char *spec)
{
	static const struct debug_name names[] = {
		{ "protocol",       DEBUG_PROTOCOL },
		{ "readline",       DEBUG_READLINE },
		{ "transfer",       DEBUG_TRANSFER },
		{ "window",         DEBUG_WINDOWHANDLING },
		{ "tty",            DEBUG_TTY },
		{ "all",            DEBUG_ALL },
	};
	const char *p = spec;

	if (!spec || !*spec)
		return false;
	while (*p) {
		const char *end = strchr(p, ',');
		size_t len = end ? (size_t)(end - p) : strlen(p);
		unsigned int i;
		while (*p == ' ')
			p++, len--;
		for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
			if (strlen(names[i].name) == len
			    && strncmp(p, names[i].name, len) == 0)
				break;
		}
		if (i == sizeof(names) / sizeof(names[0])) {
			lrzsz_warning(0, _("unknown debug module '%.*s'"),
				(int) len, p);
			return false;
		}
		debugmode |= names[i].mask;
		if (!end)
			break;
		p = end + 1;
	}
	return true;
}

void
lrzsz_set_progname (const char * str, const char *who)
{
    const char *known[]={"lrz","lsz","lrb","lsb","lrx","lsx"};
    const char * p = strrchr(str,'/');
    char *tmp;
    size_t len;
    if (!str) {
        lrzsz_progname=xstrdup(who);
        return;
    }
    if (p && p[1])
        p=p+1;
    else if (!p)
        p=str;
    for (unsigned int i=0;i< sizeof(known) / sizeof (const char *);i++) {
        if (0==strcmp(known[i],p)) {
            lrzsz_progname=known[i];
            return;
        }
    }
    /* asprintf is C11, over our baseline. Well. */
    len=strlen(p)+strlen(who)+4; /* 4: space ( ) null */
    tmp=malloc (len);
    if (!tmp) {
        fatal_error(1,0,"out of memory");
    }
    (void) snprintf(tmp,len,"%s (%s)",p, who);
    lrzsz_progname = tmp;
    return;
}

/* Print the program name and error message MESSAGE, which is a printf-style
 * format string with optional args.
 * If ERRNUM is nonzero, print its corresponding system error message.
 */
LRZSZ_FORMAT_PRINTF(2,0)
static void
lrzsz_vwarning (int errnum, const char *format, va_list ap)
{
    fflush(stdout);

    /* Print program name (or fallback) and colon-space prefix */
    fprintf(stderr, "%s: ", lrzsz_progname && *lrzsz_progname? lrzsz_progname : "lrzsz");

    /* Format the error message with optional arguments */
    vfprintf(stderr, format, ap);

    /* Add system error if errnum is nonzero */
    if (errnum)
        fprintf(stderr, ": %s", strerror(errnum));

    /* Print newline and flush */
    putc ('\n', stderr);
    fflush(stderr);
}

void
lrzsz_warning (int errnum, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    lrzsz_vwarning(errnum, format, args);
    va_end(args);
}

/* like lrzsz_warning(), but exit with status.  */
LRZSZ_NORETURN void
fatal_error (int status, int errnum, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    lrzsz_vwarning(errnum, message, args);
    va_end(args);
    exit(status);
}

/* End-of-transfer BPS summary shared by lrz and lsz. BYTES is the number
 * of bytes actually transferred; SKIPPED bytes (resume) are excluded
 * from the bps and syslog byte count. TOTAL is only printed for
 * receives (pass 0 when sending). */
void
report_transfer_result (int sending, const char *shortname,
    const char *fname, long bytes, long total, long skipped)
{
    long bps;

    if (!(Verbose > 1 || enable_syslog))
        return;
    bps = timing_bps(sending ? bytes : bytes - skipped);
    if (Verbose) {
        if (sending)
            vchar('\r');
        if (Verbose > 1) {
            if (sending)
                vstringf(_("Bytes Sent:%7ld   BPS:%-8ld                        \n"),
                    bytes, bps);
            else
                vstringf(_("\rBytes received: %7ld/%7ld   BPS:%-6ld                \r\n"),
                    bytes, total, bps);
        }
    }
    if (enable_syslog) {
        const char *s = fname ? strrchr(fname, '/') : NULL;
        s = s ? s + 1 : (fname ? fname : shortname);
        lsyslog(LOG_INFO, "%s/%s: %ld Bytes, %ld BPS", s, protname(),
            sending ? bytes : bytes - skipped, bps);
    }
}

unsigned long
lrzsz_strtoul(const char *name, const char *value, const char *valid_suffixes, unsigned long int min, unsigned long int max)
{
  char *p;
  unsigned long int tmp;
  unsigned int factor;

  errno = 0;
  tmp = strtoul (value, &p, 10);
  if (errno != 0)
    fatal_error(2,0, _("%s `%s' is larger than maximum unsigned long int"), name,value);
  if (value[0] == '-')
    fatal_error(2,0, _("%s `%s' is negative"), name,value);
  if (p == value)
    fatal_error(2,0, _("%s `%s' is invalid"), name,value);
  if (*p) {
      unsigned char c;
      if (!valid_suffixes || p[1] || !strchr (valid_suffixes, *p)) {
        fatal_error (2,0, _("%s `%s' is followed by an invalid suffix"), name,value);
      }
      c=*p;
      switch (c) {
        case 'c': factor=1; break;
        case 'k': factor=1024; break;
        case 'm': factor=1024*1024; break;
        case 'g': factor=1024*1024*1024; break;
        default:
        fatal_error (2,0, _("%s `%s' is followed by an invalid suffix"), name,value);
      }
      if (tmp > ULONG_MAX / (unsigned long) factor) {
        fatal_error(2,0, _("%s `%s' is larger than maximum unsigned long int"), name,value);
      }
      tmp *= factor;
  }
  if (tmp < (unsigned long int) min) {
    fatal_error(2,0, _("%s `%s' is smaller than the allowed minimum of `%lu'"), name,value, min);
  }
  if (max && tmp > (unsigned long int) max) {
    fatal_error(2,0, _("%s `%s' is larger than the allowed maximum of `%lu'"), name,value, max);
  }

  return tmp;
}

char *
xstrdup(const char *s)
{
    char *tmp = strdup(s);
    if (!tmp)
        fatal_error(1,errno,"strdup");
    return tmp;
}

void *
xmalloc(size_t size)
{
    char *tmp = malloc(size);
    if (!tmp)
        fatal_error(1,errno,"malloc");
    return tmp;
}

int
fd_readable(int fd)
{
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    return select(fd + 1, &fds, NULL, NULL, &tv);
}

/* Log-forging protection for syslog and the transfer journal: escape C0
 * control characters (and DEL) as \xNN; printable ASCII and bytes >=0x80
 * (multi-byte UTF-8 sequences, which live entirely in 0x80-0xff) pass
 * through verbatim. Stops cleanly at an escape boundary when dst runs
 * out; dst is always NUL-terminated. Returns the written length. */
size_t
sanitize_bytes(char *dst, size_t dstcap, const char *src)
{
    size_t o = 0, i;

    for (i = 0; src[i] && o + 4 < dstcap; i++) {
        unsigned char c = (unsigned char) src[i];
        if (c >= 0x20 && c != 0x7f) {
            dst[o++] = (char) c;
        } else {
            snprintf(dst + o, 5, "\\x%02x", c);
            o += 4;
        }
    }
    dst[o] = '\0';
    return o;
}

/* Install a "cancel" handler for SIG unless it is being ignored; see
 * util.h. The identical lrz/lsz logic, moved verbatim. */
int
lrzsz_install_signal (int sig, void (*handler)(int))
{
	struct sigaction sa, old;
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(sig, &sa, &old) == 0) {
		if (old.sa_handler == SIG_IGN) {
			sigaction(sig, &old, NULL);
			return 0;
		}
		return 1;
	}
	return 0;
}

/* Derive protocol and initial Verbose from the invoked name; see util.h.
 * From lrz.c/lsz.c, parameterized by the direction letter. */
void
chkinvok (const char *s, char dir)
{
	const char *p;

	p = s;
	while (*p == '-')
		s = ++p;
	while (*p)
		if (*p++ == '/')
			s = p;
	if (*s == 'v') {
		Verbose = 1;
		++s;
	}
	if (*s == 'l')
		s++; /* lrz/lsz -> rz/sz */
	protocol = ZM_ZMODEM;
	if (s[0] == dir && s[1] == 'x')
		protocol = ZM_XMODEM;
	if (s[0] == dir && (s[1] == 'b' || s[1] == 'y'))
		protocol = ZM_YMODEM;
}

/* Parse the -s/--stop-at argument; see util.h. Both programs' copies were
 * identical except the +N upper bound (lrz capped at LONG_MAX, lsz was
 * unbounded); the cap wins - it protects the time_t cast below, and
 * LONG_MAX seconds is beyond any practical stop time. */
time_t
lrzsz_parse_stop_time (const char *myarg, void (*die)(int, const char *))
{
	struct tm *tm;
	time_t t;
	int hh, mm;
	char *nex, *mstart;
	time_t stop_time;

	if (isdigit((unsigned char) (*myarg))) {
		hh = strtoul (myarg, &nex, 10);
		if (nex == myarg)
			die(2, _("unparsable stop time: empty hour"));
		if (hh > 23)
			die(2, _("hour too large (0..23)"));
		if (*nex != ':')
			die(2, _("unparsable stop time"));
		nex++;
		mstart = nex;
		mm = strtoul (nex, &nex, 10);
		if (nex == mstart)
			die(2, _("unparsable stop time: empty minute"));
		if (mm > 59)
			die(2, _("minute too large (0..59)"));

		t = time(NULL);
		tm = localtime(&t);
		tm->tm_hour = hh;
		tm->tm_min = mm;
		stop_time = mktime(tm);
		if (stop_time < t)
			stop_time += 86400L; /* one day more */
		if (stop_time - t < 10)
			die(2, _("stop time too small"));
	} else {
		stop_time = time(0) + (time_t) lrzsz_strtoul(_("-s / --stop-at"),
				myarg, NULL, 1, LONG_MAX);
	}
	return stop_time;
}

/* Pathname traversal check; see util.h. The lrz checkpath() and lsz wcs()
 * walks, identical except lrz's very-restricted dotfile ban, which the
 * RESTRICTED_LEVEL parameter expresses. */
bool
lrzsz_path_component_violation (const char *name, int restricted_level)
{
	const char *cp;

	/* deny absolute paths */
	if (name[0] == '/')
		return true;

	/* walk path components */
	for (cp = name; *cp; ) {
		const char *slash = strchr(cp, '/');
		size_t len = slash ? (size_t)(slash - cp) : strlen(cp);
		if (len > 0) {
			if (restricted_level > 1 && cp[0] == '.')
				return true;
			if (len == 2 && strncmp(cp, "..", 2) == 0)
				return true;
		}
		if (!slash)
			break;
		cp = slash + 1;
	}
	return false;
}

/* ETA arithmetic for the progress displays; see util.h. */
void
lrzsz_eta (long bps, size_t left, int *minleft, int *secleft)
{
	*minleft = 0;
	*secleft = 0;
	if (bps > 0) {
		*minleft = (int) (left / (unsigned long) bps / 60);
		*secleft = (int) ((left / (unsigned long) bps) % 60);
	}
}

/* Last path component of PATH, or "no.name" when PATH is NULL; see util.h.
 * This is the derivation the old DO_SYSLOG_FNAME/DO_SYSLOG macros performed
 * inline (including the "no.name" fallback for a missing filename). */
const char *
lrzsz_basename (const char *path)
{
	const char *s;

	if (!path)
		return "no.name";
	s = strrchr(path, '/');
	return s ? s + 1 : path;
}

/* min_bps watchdog; see util.h. The lrz/lsz copies were identical except
 * the debug name and cosmetic format drift (%lu vs %ld) - both unified
 * here: one display threshold (Verbose>=1), one type (long). */
bool
lrzsz_bps_watchdog (const char *dbgname, long last_bps, double d,
                    long min_bps, double min_bps_time, double *low_bpsp,
                    const char *fname)
{
	if (last_bps >= min_bps) {
		*low_bpsp = 0;	/* recovered */
		return false;
	}
	if (*low_bpsp == 0) {
		*low_bpsp = d;	/* arm: first violation */
		return false;
	}
	if (d - *low_bpsp < min_bps_time)
		return false;	/* below min, but not yet for long enough */

	if (Verbose)
		vstringf(_("%s: bps rate %ld below min %ld\n"),
			dbgname, last_bps, min_bps);
	DO_SYSLOG(LOG_INFO, "%s/%s: bps rate low: %ld < %ld",
		   lrzsz_basename(fname), protname(), last_bps, min_bps);
	return true;
}

/* -s/--stop-at deadline check; see util.h. */
bool
lrzsz_stop_time_reached (const char *dbgname, time_t now, time_t stop_time,
                         const char *fname)
{
        /* stop_time+1 because that might be based on a measurement at 0.99s */
	if (!stop_time || now < stop_time+1)
		return false;
	if (Verbose)
		vstringf(_("%s: reached stop time\n"), dbgname);
	DO_SYSLOG(LOG_INFO, "%s/%s: reached stop time",
		   lrzsz_basename(fname), protname());
	return true;
}

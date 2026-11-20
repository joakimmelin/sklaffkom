#ifndef LRZSZ_UTIL_H
#define LRZSZ_UTIL_H

#include "config.h" /* noreturn and format_printf */

#include <time.h> /* time_t */
#include <sys/types.h> /* size_t */
#include <stdbool.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L
#  define LRZSZ_NORETURN [[noreturn]]
#elif defined(__GNUC__) || defined(__clang__)
#  define LRZSZ_NORETURN __attribute__((__noreturn__))
#else
#  define LRZSZ_NORETURN
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L
#  define LRZSZ_FORMAT_PRINTF(fmtidx, argidx) [[gnu::format(printf, fmtidx, argidx)]]
#elif defined(__GNUC__) || defined(__clang__)
#  define LRZSZ_FORMAT_PRINTF(fmtidx, argidx) __attribute__((format(printf, fmtidx, argidx)))
#else
#  define LRZSZ_FORMAT_PRINTF(fmt, arg)
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define LRZSZ_ALWAYS_INLINE __attribute__((always_inline))
#else
#  define LRZSZ_ALWAYS_INLINE
#endif

extern const char * lrzsz_progname;
void lrzsz_set_progname (const char * str, const char *who);

LRZSZ_FORMAT_PRINTF(2,3)
void lrzsz_warning(int errnum, const char *format, ...);

LRZSZ_NORETURN LRZSZ_FORMAT_PRINTF(3,4)
void fatal_error (int status, int errnum, const char *message, ...);

unsigned long lrzsz_strtoul(const char *name, const char *value, const char *valid_suffixes, unsigned long int min, unsigned long int max);

char * xstrdup(const char *s);
void * xmalloc(size_t size);

size_t sanitize_bytes(char *dst, size_t dstcap, const char *src);

void report_transfer_result (int sending, const char *shortname,
    const char *fname, long bytes, long total, long skipped);

int fd_readable(int fd);

/* Install a "cancel" handler for sig unless it is being ignored; returns 1
 * if the handler was installed, 0 if the old disposition was SIG_IGN (and
 * was left alone). Shared by lrz.c and lsz.c; the handler itself is
 * program-specific. */
int lrzsz_install_signal (int sig, void (*handler)(int));

/* Derive the protocol (ZM_XMODEM/YMODEM/ZMODEM) and the initial Verbose
 * from the invoked name; DIR is 'r' (lrz: rx/rb/rz prefixes) or 's'
 * (lsz: sx/sb/sz). */
void chkinvok (const char *s, char dir);

/* Parse the -s/--stop-at argument: "HH:MM" (wall clock, today or tomorrow,
 * must be at least 10 s away) or "+N" seconds. Calls DIE (a program's
 * usage(int, const char*)) on unparsable or too-small values. Returns the
 * absolute stop time, or 0 for the "no stop time" sentinel use of -s? no:
 * always a valid time. */
time_t lrzsz_parse_stop_time (const char *optarg, void (*die)(int, const char *));

/* Check a received/sent pathname for traversal components. Returns true if
 * NAME is absolute, contains a ".." component, or (when RESTRICTED_LEVEL > 1)
 * contains a component starting with '.'. The caller owns the reaction. */
bool lrzsz_path_component_violation (const char *name, int restricted_level);

/* ETA arithmetic for the progress displays: BPS bytes/second, LEFT bytes
 * remaining. */
void lrzsz_eta (long bps, size_t left, int *minleft, int *secleft);

/* Last path component of PATH, or "no.name" when PATH is NULL. The returned
 * pointer refers into PATH (no copy). Used for syslog lines, which lsyslog()
 * sanitizes. */
const char * lrzsz_basename (const char *path);

/* min_bps watchdog returns true, if the transfer should be aborted (last bps 
 * was below min for at least the min seconds).
 * returns false to keep going (and set or clear the low bps tracker).
 * dbgname is the callers name.
 * fname is the transferred file's name (both used in messages).
 * Displays at Verbose>=1. */
bool lrzsz_bps_watchdog (const char *dbgname, long last_bps, double d,
                         long min_bps, double min_bps_time, double *low_bpsp,
                         const char *fname);

/* -s/--stop-at deadline check: true = abort the transfer (now has passed
 * stop_time; the deadline is specified in wall-clock terms).
 * dbgname / fname: see the watchdog above.
 * Displays at Verbose>=1. */
bool lrzsz_stop_time_reached (const char *dbgname, time_t now,
                              time_t stop_time, const char *fname);

/* Parse a --debug argument: a comma separated list of module names
 * (protocol, readline, transfer, windowhandling, tty) or "all"; the bits
 * are ORed into the global debugmode. Returns false for an unknown name
 * (after warning to stderr); the caller should exit with a usage error. */
bool lrzsz_parse_debug (const char *spec);

#endif

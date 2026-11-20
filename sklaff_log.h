/* sklaff_log.h
 *
 * Logging helpers for SklaffKOM.
 */

#ifndef SKLAFF_LOG_H
#define SKLAFF_LOG_H

#include <errno.h>

/*
 * Log level
 *
 * 0:    No logs
 * 1-2:  Errors only
 * 3-4:  Warnings
 * 5-6:  Most actions (command start/stop)
 * 7-8:  Inputs/outputs (sanitized)
 * 9-10: Verbose (command lines, environment)
 * 12+:  Very verbose
 */
#ifndef LOGLEVEL
#define LOGLEVEL 6
#endif

#ifndef LOGTAG
#define LOGTAG "core"
#endif

#ifndef DLOG_BUFSZ
#define DLOG_BUFSZ 2048
#endif

/*
 * Low-level logger currently implemented elsewhere in SklaffKOM.
 * Keeping this declaration here removes logging details from sklaff.h.
 */
void debuglog(char *, int);

/* Logging implementation used by the macros below. */
void dlog_impl(const char *tag, int level, const char *fmt, ...);
void dlog_errno_impl(const char *tag, int level,
                     const char *ctx, int errnum);

/* Use the compile-site LOGTAG unless an explicit tag is supplied. */
#define dlog(level, ...) \
    dlog_impl(LOGTAG, (level), __VA_ARGS__)

#define dlog_with(tag, level, ...) \
    dlog_impl((tag), (level), __VA_ARGS__)

/*
 * Capture errno at the call site before any logging function gets a chance
 * to alter it.
 */
#define dlog_errno(level, ctx) \
    dlog_errno_impl(LOGTAG, (level), (ctx), errno)

#define dlog_errno_with(tag, level, ctx) \
    dlog_errno_impl((tag), (level), (ctx), errno)

#endif /* SKLAFF_LOG_H */

/* log.c
 *
 * Logging helpers for SklaffKOM.
 */

#include "sklaff_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void
dlog_impl(const char *tag, int level, const char *fmt, ...)
{
    char buf[DLOG_BUFSZ];
    int prefix_len;
    va_list ap;

    if (tag == NULL)
        tag = "core";
    if (fmt == NULL)
        fmt = "";

    prefix_len = snprintf(buf, sizeof(buf), "[%s] ", tag);
    if (prefix_len < 0)
        return;

    /*
     * An absurdly long tag can consume the whole buffer. Keep the string
     * terminated and pass on what fits rather than writing past the end.
     */
    if ((size_t)prefix_len >= sizeof(buf)) {
        buf[sizeof(buf) - 1] = '\0';
        debuglog(buf, level);
        return;
    }

    va_start(ap, fmt);
    vsnprintf(buf + prefix_len, sizeof(buf) - (size_t)prefix_len, fmt, ap);
    va_end(ap);

    debuglog(buf, level);
}

void
dlog_errno_impl(const char *tag, int level, const char *ctx, int errnum)
{
    if (ctx == NULL)
        ctx = "(unknown)";

    dlog_impl(tag, level, "%s: errno=%d (%s)",
              ctx, errnum, strerror(errnum));
}

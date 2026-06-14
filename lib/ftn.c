#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../sklaff.h"

#define FTNQUEUE_DIR     SKLAFFDIR "/ftnqueue"         /* modified on 2026-06-14, PL */
#define FTNQUEUE_TMP     SKLAFFDIR "/ftnqueue/tmp"     /* modified on 2026-06-14, PL */
#define FTNQUEUE_PENDING SKLAFFDIR "/ftnqueue/pending" /* modified on 2026-06-14, PL */

static int
is_safe_ftn_area_name(const char *area)
{
    const unsigned char *p;

    if (area == NULL || *area == '\0')
        return 0;

    for (p = (const unsigned char *)area; *p != '\0'; p++) {
        if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.')
            return 0;
    }

    return 1;
}

static int
queue_ftn_export(const char *area, long textnum)
{
    char tmpfile[4096];
    char pendingfile[4096];
    long now;
    FILE *fp;
    int n;

    if (area == NULL || *area == '\0' || textnum <= 0)
        return -1;

    now = (long)time(NULL);

    /*
     * Queue format follows the traditional SklaffKOM style:
     *
     *   AREA:TEXTNUM:TIMESTAMP
     *
     * Write to tmp first and then rename into pending.  This makes queue
     * creation atomic, so the cron runner never sees a half-written job.
     *
     * modified on 2026-06-14, PL
     */
    n = snprintf(tmpfile, sizeof(tmpfile), "%s/%s.%ld.%ld.tmp",
        FTNQUEUE_TMP, area, textnum, (long)getpid());

    if (n < 0 || (size_t)n >= sizeof(tmpfile)) {
        dlog(2, "queue_ftn_export: tmp filename too long");
        return -1;
    }

    n = snprintf(pendingfile, sizeof(pendingfile), "%s/%s.%ld.%ld",
        FTNQUEUE_PENDING, area, textnum, now);

    if (n < 0 || (size_t)n >= sizeof(pendingfile)) {
        dlog(2, "queue_ftn_export: pending filename too long");
        return -1;
    }

    fp = fopen(tmpfile, "w");
    if (fp == NULL) {
        dlog(2, "queue_ftn_export: fopen failed for area [%s] text %ld, errno=%d",
            area, textnum, errno);
        return -1;
    }

    if (fprintf(fp, "%s:%ld:%ld\n", area, textnum, now) < 0) {
		dlog(2, "queue_ftn_export: fprintf failed for area [%s] text %ld, errno=%d",
            area, textnum, errno);
        fclose(fp);
        unlink(tmpfile);
        return -1;
    }

    if (fclose(fp) != 0) {
        dlog(2, "queue_ftn_export: fclose failed for area [%s] text %ld, errno=%d",
            area, textnum, errno);
        unlink(tmpfile);
        return -1;
    }

    if (rename(tmpfile, pendingfile) != 0) {
        dlog(2, "queue_ftn_export: rename failed for area [%s] text %ld, errno=%d",
            area, textnum, errno);
        unlink(tmpfile);
        return -1;
    }

    dlog(6, "queue_ftn_export: queued FTN export [%s:%ld:%ld]",
        area, textnum, now);

    return 0;
}

void
export_ftn_post_if_needed(struct CONF_ENTRY *ce, long textnum)
{
    if (ce == NULL || textnum <= 0)
        return;

    if (ce->type != FTN_CONF)
        return;

    if (!is_safe_ftn_area_name(ce->name)) {
        dlog(2, "export_ftn_post_if_needed: unsafe FTN area name [%s]",
            ce->name ? ce->name : "(null)");
        return;
    }

    /*
     * Do not run ftntoss directly from the interactive SklaffKOM process.
     * Ordinary telnet users should not need permission to read the SklaffKOM
     * database or write to the FTN spool.  Instead, queue the export and let
     * cron run the actual ftntoss command as the sklaff user.
     *
     * modified on 2026-06-14, PL
     */
    if (queue_ftn_export(ce->name, textnum) != 0) {
        dlog(2, "export_ftn_post_if_needed: failed to queue text %ld for area [%s]",
            textnum, ce->name);
    }
}

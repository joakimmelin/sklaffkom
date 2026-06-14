#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../sklaff.h"

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

void
export_ftn_post_if_needed(struct CONF_ENTRY *ce, long textnum)
{
    char cmdline[4096]; /* modified on 2026-06-14, PL */
	int n;
    int status;

    if (ce == NULL || textnum <= 0)
        return;

    if (ce->type != FTN_CONF)
        return;

    if (!is_safe_ftn_area_name(ce->name)) {
        dlog(2, "export_ftn_post_if_needed: unsafe FTN area name [%s]",
            ce->name ? ce->name : "(null)");
        return;
    }

	n = snprintf(cmdline, sizeof(cmdline),
		"%s/ftntoss --export-one %s %ld >>%s/log/ftntoss.log 2>&1",
		SKLAFFBIN, ce->name, textnum, SKLAFFDIR);

	if (n < 0 || (size_t)n >= sizeof(cmdline)) {
		dlog(2, "export_ftn_post_if_needed: command line too long");
		return;
	}   

	/*
     * Export newly posted SklaffKOM text to FTN echomail after the local
     * text has been saved. Keep cmd_post_text() itself free from FTN
     * details; ftntoss owns the actual .MSG generation.
     *
     * modified on 2026-06-14, PL
     */
	
	dlog(6, "export_ftn_post_if_needed: exporting text %ld to FTN area [%s]",
		textnum, ce->name);

    status = system(cmdline);
    if (status != 0) {
        dlog(2, "export_ftn_post_if_needed: ftntoss failed, status=%d",
            status);
    } else {
        dlog(6, "export_ftn_post_if_needed: exported text %ld to area [%s]",
            textnum, ce->name);
    }
}

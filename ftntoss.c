/*
 * ftntoss.c - FTN .MSG dry-run tosser for SklaffKOM
 *
 * Current version:
 *   - does NOT import anything
 *   - finds SklaffKOM conference by area name
 *   - verifies conference type == FTN_CONF
 *   - scans FTN_SPOOL/<area> for *.msg
 *   - parses each .MSG using ftnmsg.c
 *   - builds an in-memory MSGID -> filename map
 *   - scans existing SklaffKOM texts for ^AMSGID
 *   - creates a multi-pass dry-run import plan
 *   - prints verbose debug output, including reply/thread resolution
 *
 * modified on 2026-06-09, PL
 */

#include "sklaff.h"
#include "ftnmsg.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ftn_conf_info {
    int num;
    long last_text;
    int creator;
    long time;
    int type;
    int life;
    int comconf;
    char name[LINE_LEN];
};

struct msgref {
    char msgid[256];
    char filename[PATH_MAX];
    struct msgref *next;
};

struct skref {
    char msgid[256];
    long textnum;
    struct skref *next;
};

struct planref {
    char filename[PATH_MAX];
    char msgid[256];
    long planned_textnum;
    struct planref *next;
};

struct msgitem {
    char filename[PATH_MAX];
    char path[PATH_MAX];
    char msgid[256];
    char reply[256];
    int planned;
    int orphan;
    long planned_textnum;
    long parent_textnum;
    struct msgitem *next;
};

static int find_ftn_conf(const char *name, struct ftn_conf_info *out_ce);
static int is_msg_file(const char *name);
static int scan_ftn_area(const char *area, const struct ftn_conf_info *ce);

static void add_msgref(struct msgref **list, const char *msgid, const char *filename);
static const char *find_msgref(struct msgref *list, const char *msgid);
static void free_msgrefs(struct msgref *list);

static void add_skref(struct skref **list, const char *msgid, long textnum);
static long find_skref(struct skref *list, const char *msgid);
static void free_skrefs(struct skref *list);

static void add_planref(struct planref **list, const char *filename,
    const char *msgid, long planned_textnum);
static long find_planref_by_msgid(struct planref *list, const char *msgid);
static void free_planrefs(struct planref *list);

static int add_msgitem(struct msgitem **list, struct msgitem **tail,
    const char *filename, const char *path, const char *msgid, const char *reply);
static struct msgitem *find_msgitem_by_filename(struct msgitem *list, const char *filename);
static void free_msgitems(struct msgitem *list);

static int plan_imports(struct msgitem *items, struct skref *skrefs,
    struct planref **plans, long first_textnum, long *out_next_textnum,
    long *out_planned, long *out_top_level, long *out_reply_existing,
    long *out_reply_planned, long *out_orphan);

static int scan_existing_skl_msgids(const struct ftn_conf_info *ce,
    struct skref **out_refs, long *out_indexed);
static int extract_ftn_msgid_from_line(const char *line, char *out, size_t outsz);

static int
parse_conf_line(const char *line, struct ftn_conf_info *ce)
{
    LONG_LINE tmp;
    char *p;
    char *fields[8];
    int i;

    if (line == NULL || ce == NULL)
        return -1;

    strlcpy(tmp, line, sizeof(tmp)); /* modified on 2026-06-09, PL */

    p = tmp;
    for (i = 0; i < 7; i++) {
        fields[i] = p;
        p = strchr(p, ':');
        if (p == NULL)
            return -1;
        *p++ = '\0';
    }

    fields[7] = p;
    fields[7][strcspn(fields[7], "\r\n")] = '\0';

    ce->num       = atoi(fields[0]);
    ce->last_text = atol(fields[1]);
    ce->creator   = atoi(fields[2]);
    ce->time      = atol(fields[3]);
    ce->type      = atoi(fields[4]);
    ce->life      = atoi(fields[5]);
    ce->comconf   = atoi(fields[6]);
    strlcpy(ce->name, fields[7], sizeof(ce->name)); /* modified on 2026-06-09, PL */

    return 0;
}

static int
find_ftn_conf(const char *name, struct ftn_conf_info *out_ce)
{
    FILE *fp;
    LONG_LINE line;
    struct ftn_conf_info ce;

    printf("Checking the SklaffKOM CONF_FILE: %s... ", CONF_FILE);
    fflush(stdout);

    fp = fopen(CONF_FILE, "r");
    if (fp == NULL) {
        fprintf(stderr, "\n[ERROR] Could not open file '%s'\n", CONF_FILE);
        perror("fopen");
        return -1;
    }

    printf("OK!\n");
    printf("Checking conference: %s... ", name);
    fflush(stdout);

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (parse_conf_line(line, &ce) != 0)
            continue;

        if (strcmp(name, ce.name) == 0) {
            *out_ce = ce;
            fclose(fp);
            printf("OK!\n");
            return 0;
        }
    }

    fclose(fp);
    fprintf(stderr, "\n[ERROR] Conference '%s' not found in %s\n", name, CONF_FILE);
    return -1;
}

static int
is_msg_file(const char *name)
{
    const char *dot;

    if (name == NULL || name[0] == '.')
        return 0;

    dot = strrchr(name, '.');
    if (dot == NULL)
        return 0;

    return strcasecmp(dot, ".msg") == 0;
}

static void
add_msgref(struct msgref **list, const char *msgid, const char *filename)
{
    struct msgref *n;

    if (list == NULL || msgid == NULL || filename == NULL || *msgid == '\0')
        return;

    n = (struct msgref *)calloc(1, sizeof(*n));
    if (n == NULL)
        return;

    strlcpy(n->msgid, msgid, sizeof(n->msgid)); /* modified on 2026-06-09, PL */
    strlcpy(n->filename, filename, sizeof(n->filename)); /* modified on 2026-06-09, PL */

    n->next = *list;
    *list = n;
}

static const char *
find_msgref(struct msgref *list, const char *msgid)
{
    struct msgref *p;

    if (msgid == NULL || *msgid == '\0')
        return NULL;

    for (p = list; p != NULL; p = p->next) {
        if (strcmp(p->msgid, msgid) == 0)
            return p->filename;
    }

    return NULL;
}

static void
free_msgrefs(struct msgref *list)
{
    while (list) {
        struct msgref *t = list->next;
        free(list);
        list = t;
    }
}

static void
add_skref(struct skref **list, const char *msgid, long textnum)
{
    struct skref *n;

    if (list == NULL || msgid == NULL || *msgid == '\0' || textnum <= 0)
        return;

    n = (struct skref *)calloc(1, sizeof(*n));
    if (n == NULL)
        return;

    strlcpy(n->msgid, msgid, sizeof(n->msgid)); /* modified on 2026-06-09, PL */
    n->textnum = textnum;

    n->next = *list;
    *list = n;
}

static long
find_skref(struct skref *list, const char *msgid)
{
    struct skref *p;

    if (msgid == NULL || *msgid == '\0')
        return 0;

    for (p = list; p != NULL; p = p->next) {
        if (strcmp(p->msgid, msgid) == 0)
            return p->textnum;
    }

    return 0;
}

static void
free_skrefs(struct skref *list)
{
    while (list) {
        struct skref *t = list->next;
        free(list);
        list = t;
    }
}

static void
add_planref(struct planref **list, const char *filename,
    const char *msgid, long planned_textnum)
{
    struct planref *n;

    if (list == NULL || filename == NULL || *filename == '\0' ||
        msgid == NULL || *msgid == '\0' || planned_textnum <= 0)
        return;

    n = (struct planref *)calloc(1, sizeof(*n));
    if (n == NULL)
        return;

    strlcpy(n->filename, filename, sizeof(n->filename)); /* modified on 2026-06-09, PL */
    strlcpy(n->msgid, msgid, sizeof(n->msgid)); /* modified on 2026-06-09, PL */
    n->planned_textnum = planned_textnum;

    n->next = *list;
    *list = n;
}

static long
find_planref_by_msgid(struct planref *list, const char *msgid)
{
    struct planref *p;

    if (msgid == NULL || *msgid == '\0')
        return 0;

    for (p = list; p != NULL; p = p->next) {
        if (strcmp(p->msgid, msgid) == 0)
            return p->planned_textnum;
    }

    return 0;
}

static void
free_planrefs(struct planref *list)
{
    while (list) {
        struct planref *t = list->next;
        free(list);
        list = t;
    }
}

static int
add_msgitem(struct msgitem **list, struct msgitem **tail,
    const char *filename, const char *path, const char *msgid, const char *reply)
{
    struct msgitem *n;

    if (list == NULL || tail == NULL || filename == NULL || path == NULL)
        return -1;

    n = (struct msgitem *)calloc(1, sizeof(*n));
    if (n == NULL)
        return -1;

    strlcpy(n->filename, filename, sizeof(n->filename)); /* modified on 2026-06-09, PL */
    strlcpy(n->path, path, sizeof(n->path)); /* modified on 2026-06-09, PL */

    if (msgid != NULL)
        strlcpy(n->msgid, msgid, sizeof(n->msgid)); /* modified on 2026-06-09, PL */
    if (reply != NULL)
        strlcpy(n->reply, reply, sizeof(n->reply)); /* modified on 2026-06-09, PL */

    if (*tail == NULL) {
        *list = n;
        *tail = n;
    } else {
        (*tail)->next = n;
        *tail = n;
    }

    return 0;
}

static struct msgitem *
find_msgitem_by_filename(struct msgitem *list, const char *filename)
{
    struct msgitem *p;

    if (filename == NULL || *filename == '\0')
        return NULL;

    for (p = list; p != NULL; p = p->next) {
        if (strcmp(p->filename, filename) == 0)
            return p;
    }

    return NULL;
}

static void
free_msgitems(struct msgitem *list)
{
    while (list) {
        struct msgitem *t = list->next;
        free(list);
        list = t;
    }
}

static int
plan_imports(struct msgitem *items, struct skref *skrefs,
    struct planref **plans, long first_textnum, long *out_next_textnum,
    long *out_planned, long *out_top_level, long *out_reply_existing,
    long *out_reply_planned, long *out_orphan)
{
    int changed;
    long next_textnum = first_textnum;
    long planned = 0;
    long top_level = 0;
    long reply_existing = 0;
    long reply_planned = 0;
    long orphan = 0;

    if (plans == NULL || out_next_textnum == NULL || out_planned == NULL ||
        out_top_level == NULL || out_reply_existing == NULL ||
        out_reply_planned == NULL || out_orphan == NULL)
        return -1;

    *plans = NULL;

    /*
     * Multi-pass planning:
     * - top-level messages can always be planned
     * - replies to existing SklaffKOM texts can always be planned
     * - replies to already-planned messages can be planned on later passes
     */
    do {
        struct msgitem *m;

        changed = 0;

        for (m = items; m != NULL; m = m->next) {
            long parent_text = 0;
            long planned_parent = 0;

            if (m->planned)
                continue;

            if (m->reply[0] == '\0') {
                m->planned = 1;
                m->planned_textnum = next_textnum++;
                add_planref(plans, m->filename, m->msgid, m->planned_textnum);
                planned++;
                top_level++;
                changed = 1;
                continue;
            }

            parent_text = find_skref(skrefs, m->reply);
            if (parent_text > 0) {
                m->planned = 1;
                m->parent_textnum = parent_text;
                m->planned_textnum = next_textnum++;
                add_planref(plans, m->filename, m->msgid, m->planned_textnum);
                planned++;
                reply_existing++;
                changed = 1;
                continue;
            }

            planned_parent = find_planref_by_msgid(*plans, m->reply);
            if (planned_parent > 0) {
                m->planned = 1;
                m->parent_textnum = planned_parent;
                m->planned_textnum = next_textnum++;
                add_planref(plans, m->filename, m->msgid, m->planned_textnum);
                planned++;
                reply_planned++;
                changed = 1;
                continue;
            }
        }
    } while (changed);

    /*
     * Anything left is a true orphan for this dry-run:
     * no parent in existing SklaffKOM texts and no resolvable parent in spool.
     */
    {
        struct msgitem *m;

        for (m = items; m != NULL; m = m->next) {
            if (m->planned)
                continue;

            m->planned = 1;
            m->orphan = 1;
            m->planned_textnum = next_textnum++;
            add_planref(plans, m->filename, m->msgid, m->planned_textnum);
            planned++;
            orphan++;
        }
    }

    *out_next_textnum = next_textnum;
    *out_planned = planned;
    *out_top_level = top_level;
    *out_reply_existing = reply_existing;
    *out_reply_planned = reply_planned;
    *out_orphan = orphan;

    return 0;
}

static void
trim_left(char *s)
{
    char *p;

    if (s == NULL)
        return;

    p = s;
    while (*p && isspace((unsigned char)*p))
        p++;

    if (p != s)
        memmove(s, p, strlen(p) + 1);
}

static int
extract_ftn_msgid_from_line(const char *line, char *out, size_t outsz)
{
    const char *p = NULL;

    if (line == NULL || out == NULL || outsz == 0)
        return 0;

    /*
     * Support both real FTN kludge byte and visible debug-style ^A.
     *
     * Real stored FTN:
     *   \001MSGID: ...
     *
     * Possible visible/debug form:
     *   ^AMSGID: ...
     */
    if (line[0] == '\001' && strncmp(line + 1, "MSGID:", 6) == 0)
        p = line + 7;
    else if (strncmp(line, "^AMSGID:", 8) == 0)
        p = line + 8;
    else
        return 0;

    while (*p == ' ' || *p == '\t')
        p++;

    strlcpy(out, p, outsz); /* modified on 2026-06-09, PL */
    out[strcspn(out, "\r\n")] = '\0';
    trim_left(out);

    return out[0] != '\0';
}

static int
scan_existing_skl_msgids(const struct ftn_conf_info *ce,
    struct skref **out_refs, long *out_indexed)
{
    long i;
    long indexed = 0;
    char path[PATH_MAX];

    if (ce == NULL || out_refs == NULL || out_indexed == NULL)
        return -1;

    *out_refs = NULL;
    *out_indexed = 0;

    printf("Scanning existing SklaffKOM texts for ^AMSGID...\n");

    if (ce->last_text <= 0) {
        printf("Existing:    no texts yet, skipping SklaffKOM MSGID scan\n\n");
        return 0;
    }

    for (i = 1; i <= ce->last_text; i++) {
        FILE *fp;
        LONG_LINE line;
        char msgid[256];

        if (snprintf(path, sizeof(path), "%s/%d/%ld", SKLAFF_DB, ce->num, i) >= (int)sizeof(path)) {
            fprintf(stderr, "[ERROR] SklaffKOM text path too long: %s/%d/%ld\n",
                SKLAFF_DB, ce->num, i);
            return -1;
        }

        fp = fopen(path, "r");
        if (fp == NULL) {
            /*
             * Gaps/deleted texts are normal enough. Do not treat missing
             * numbered files as fatal in this dry-run scanner.
             */
            continue;
        }

        while (fgets(line, sizeof(line), fp) != NULL) {
            if (extract_ftn_msgid_from_line(line, msgid, sizeof(msgid))) {
                add_skref(out_refs, msgid, i);
                indexed++;
                break;
            }
        }

        fclose(fp);
    }

    *out_indexed = indexed;

    printf("Existing:    %ld FTN MSGID value(s) found in SklaffKOM texts\n\n",
        indexed);

    return 0;
}

static int
scan_ftn_area(const char *area, const struct ftn_conf_info *ce)
{
    DIR *dir;
    struct dirent *de;
    struct msgref *refs = NULL;
    struct skref *skrefs = NULL;
    struct planref *plans = NULL;
    struct msgitem *items = NULL;
    struct msgitem *items_tail = NULL;
    char spooldir[PATH_MAX];
    char path[PATH_MAX];
    long existing_indexed = 0;
    long seen = 0;
    long parsed = 0;
    long failed = 0;
    long indexed = 0;
    long top_level = 0;
    long reply_existing = 0;
    long reply_planned = 0;
    long reply_orphan = 0;
    long next_textnum = 0;
    long planned = 0;

    if (snprintf(spooldir, sizeof(spooldir), "%s/%s", FTN_SPOOL, area) >= (int)sizeof(spooldir)) {
        fprintf(stderr, "[ERROR] Spool path too long: %s/%s\n", FTN_SPOOL, area);
        return -1;
    }

    printf("\n");
    printf("FTN dry-run setup\n");
    printf("-----------------\n");
    printf("Area:        %s\n", area);
    printf("Spool:       %s\n", spooldir);
    printf("Conf name:   %s\n", ce->name);
    printf("Conf num:    %d\n", ce->num);
    printf("Conf type:   %d", ce->type);
    if (ce->type == FTN_CONF)
        printf(" (FTN_CONF)");
    printf("\n");
    printf("Last text:   %ld\n", ce->last_text);
    printf("\n");

    if (ce->type != FTN_CONF) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce->name, ce->type);
        return -1;
    }

    if (scan_existing_skl_msgids(ce, &skrefs, &existing_indexed) != 0)
        return -1;

    dir = opendir(spooldir);
    if (dir == NULL) {
        fprintf(stderr, "[ERROR] Could not open FTN spool directory '%s'\n", spooldir);
        perror("opendir");
        free_skrefs(skrefs);
        return -1;
    }

    /*
     * Pass 1:
     * Build in-memory metadata lists for all parseable .MSG files.
     */
    printf("Indexing .MSG files by MSGID...\n");

    while ((de = readdir(dir)) != NULL) {
        struct fido_msg msg;

        if (!is_msg_file(de->d_name))
            continue;

        if (snprintf(path, sizeof(path), "%s/%s", spooldir, de->d_name) >= (int)sizeof(path)) {
            fprintf(stderr, "[ERROR] Path too long while indexing: %s/%s\n",
                spooldir, de->d_name);
            failed++;
            continue;
        }

        if (read_fido_msg(path, &msg) != 0) {
            fprintf(stderr, "[ERROR] Could not parse .MSG while indexing: %s\n", path);
            failed++;
            continue;
        }

        if (msg.msgid[0] != '\0') {
            add_msgref(&refs, msg.msgid, de->d_name);
            indexed++;
        }

        if (add_msgitem(&items, &items_tail, de->d_name, path, msg.msgid, msg.reply) != 0) {
            fprintf(stderr, "[ERROR] Could not allocate message item for %s\n", path);
            failed++;
        }

        free_fido_msg(&msg);
    }

    printf("Indexed:     %ld MSGID value(s)\n\n", indexed);

    if (plan_imports(items, skrefs, &plans, ce->last_text + 1, &next_textnum,
        &planned, &top_level, &reply_existing, &reply_planned, &reply_orphan) != 0) {
        closedir(dir);
        free_msgrefs(refs);
        free_skrefs(skrefs);
        free_msgitems(items);
        return -1;
    }

    printf("Planning dry-run import order...\n");
    printf("Planned:     %ld simulated import(s)\n", planned);
    printf("Next text:   %ld\n\n", next_textnum);

    rewinddir(dir);

    printf("Scanning for .MSG files...\n\n");

    /*
     * Pass 2:
     * Parse each .MSG again, print debug output, and show the dry-run plan.
     */
    while ((de = readdir(dir)) != NULL) {
        struct fido_msg msg;
        struct msgitem *item;
        const char *parent_file = NULL;
        long parent_text = 0;
        long planned_parent_text = 0;

        if (!is_msg_file(de->d_name))
            continue;

        seen++;

        if (snprintf(path, sizeof(path), "%s/%s", spooldir, de->d_name) >= (int)sizeof(path)) {
            fprintf(stderr, "[ERROR] Path too long: %s/%s\n", spooldir, de->d_name);
            failed++;
            continue;
        }

        item = find_msgitem_by_filename(items, de->d_name);

        printf("============================================================\n");
        printf("File:    %s\n", path);

        if (read_fido_msg(path, &msg) != 0) {
            printf("[ERROR] Could not parse .MSG file\n");
            failed++;
            continue;
        }

        parsed++;

        printf("From:    %s\n", msg.from);
        printf("To:      %s\n", msg.to);
        printf("Subject: %s\n", msg.subject);
        printf("Date:    %s\n", msg.date);

        if (msg.chrs[0] != '\0')
            printf("CHRS:    %s\n", msg.chrs);
        else
            printf("CHRS:    (missing)\n");

        if (msg.msgid[0] != '\0')
            printf("MSGID:   %s\n", msg.msgid);
        else
            printf("MSGID:   (missing)\n");

        if (item == NULL) {
            printf("Plan:    internal planning error for %s\n", de->d_name);
        } else if (msg.reply[0] != '\0') {
            printf("REPLY:   %s\n", msg.reply);

            parent_text = find_skref(skrefs, msg.reply);
            if (parent_text > 0) {
                printf("Thread:  reply to existing SklaffKOM text %ld\n", parent_text);
                printf("Plan:    would import %s as SklaffKOM text %ld, comment to existing text %ld\n",
                    de->d_name, item->planned_textnum, parent_text);
            } else {
                planned_parent_text = find_planref_by_msgid(plans, msg.reply);
                if (planned_parent_text > 0 && !item->orphan) {
                    printf("Thread:  reply to newly planned SklaffKOM text %ld\n",
                        planned_parent_text);
                    printf("Plan:    would import %s as SklaffKOM text %ld, comment to newly planned text %ld\n",
                        de->d_name, item->planned_textnum, planned_parent_text);
                } else {
                    parent_file = find_msgref(refs, msg.reply);
                    if (parent_file != NULL) {
                        printf("Thread:  parent exists in current spool as %s, but could not be resolved by planner\n",
                            parent_file);
                    } else {
                        printf("Thread:  orphan reply, parent not found in SklaffKOM or current spool\n");
                    }
                    printf("Plan:    would import %s as SklaffKOM text %ld as top-level for now\n",
                        de->d_name, item->planned_textnum);
                }
            }
        } else {
            printf("REPLY:   (missing)\n");
            printf("Thread:  new top-level text\n");
            printf("Plan:    would import %s as SklaffKOM text %ld as top-level\n",
                de->d_name, item->planned_textnum);
        }

        /*
         * Later:
         *   - create real SklaffKOM texts in the planned order
         *   - use item->parent_textnum / planned parent lookup to set com
         *   - preserve ^AMSGID / ^AREPLY in stored body
         */

        printf("\n--- CLEAN BODY PREVIEW ---\n");
        if (msg.clean_body != NULL) {
            const char *p = msg.clean_body;
            int lines = 0;

            while (*p && lines < 12) {
                putchar(*p);
                if (*p == '\n')
                    lines++;
                p++;
            }

            if (*p)
                printf("[...]\n");
        }

        printf("\n");

        free_fido_msg(&msg);
    }

    closedir(dir);

    printf("============================================================\n");
    printf("FTN dry-run summary\n");
    printf("-------------------\n");
    printf("Area:          %s\n", area);
    printf("Seen:          %ld .MSG file(s)\n", seen);
    printf("Indexed:       %ld MSGID value(s)\n", indexed);
    printf("Existing IDs:  %ld SklaffKOM MSGID value(s)\n", existing_indexed);
    printf("Parsed:        %ld\n", parsed);
    printf("Failed:        %ld\n", failed);
    printf("Top-level:     %ld\n", top_level);
    printf("Reply existing:%ld\n", reply_existing);
    printf("Reply planned: %ld\n", reply_planned);
    printf("Orphan reply:  %ld\n", reply_orphan);
    printf("Planned:       %ld simulated import(s)\n", planned);
    printf("Next text no:  %ld\n", next_textnum);
    printf("Imported:      0 (dry-run)\n");

    free_msgrefs(refs);
    free_skrefs(skrefs);
    free_planrefs(plans);
    free_msgitems(items);

    return failed ? -1 : 0;
}

int
main(int argc, char **argv)
{
    struct ftn_conf_info ce;

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s <FTN-area / SklaffKOM conference>\n\n", argv[0]);
        fprintf(stderr, "Example:\n");
        fprintf(stderr, "  %s FSX_GEN\n\n", argv[0]);
        return 1;
    }

    printf("ftntoss dry-run starting\n");
    printf("========================\n\n");

    if (find_ftn_conf(argv[1], &ce) != 0)
        return 1;

    if (scan_ftn_area(argv[1], &ce) != 0)
        return 1;

    printf("\nftntoss dry-run done\n");

    return 0;
}

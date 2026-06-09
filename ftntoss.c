/*
 * ftntoss.c - FTN .MSG dry-run tosser for SklaffKOM
 *
 * First version:
 *   - does NOT import anything
 *   - finds SklaffKOM conference by area name
 *   - verifies conference type == FTN_CONF
 *   - scans FTN_SPOOL/<area> for *.msg
 *   - parses each .MSG using ftnmsg.c
 *   - builds an in-memory MSGID -> filename map
 *   - prints verbose debug output, including reply/thread resolution
 *
 * modified on 2026-06-09, PL
 */

#include "sklaff.h"
#include "ftnmsg.h"

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

static int find_ftn_conf(const char *name, struct ftn_conf_info *out_ce);
static int is_msg_file(const char *name);
static int scan_ftn_area(const char *area, const struct ftn_conf_info *ce);

static void add_msgref(struct msgref **list, const char *msgid, const char *filename);
static const char *find_msgref(struct msgref *list, const char *msgid);
static void free_msgrefs(struct msgref *list);

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

    /* Trim trailing newline from name */
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

static int
scan_ftn_area(const char *area, const struct ftn_conf_info *ce)
{
    DIR *dir;
    struct dirent *de;
    char spooldir[PATH_MAX];
    char path[PATH_MAX];
    struct msgref *refs = NULL;
    long seen = 0;
    long parsed = 0;
    long failed = 0;
    long indexed = 0;
    long top_level = 0;
    long reply_found = 0;
    long reply_orphan = 0;

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

    dir = opendir(spooldir);
    if (dir == NULL) {
        fprintf(stderr, "[ERROR] Could not open FTN spool directory '%s'\n", spooldir);
        perror("opendir");
        return -1;
    }

    /*
     * Pass 1:
     * Build an in-memory MSGID -> filename map for all parseable .MSG files
     * in this spool directory.
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

        free_fido_msg(&msg);
    }

    printf("Indexed:     %ld MSGID value(s)\n\n", indexed);

    rewinddir(dir);

    printf("Scanning for .MSG files...\n\n");

    /*
     * Pass 2:
     * Parse each .MSG again, print debug output, and resolve REPLY against
     * the MSGID map built above.
     */
    while ((de = readdir(dir)) != NULL) {
        struct fido_msg msg;
        const char *parent_file = NULL;

        if (!is_msg_file(de->d_name))
            continue;

        seen++;

        if (snprintf(path, sizeof(path), "%s/%s", spooldir, de->d_name) >= (int)sizeof(path)) {
            fprintf(stderr, "[ERROR] Path too long: %s/%s\n", spooldir, de->d_name);
            failed++;
            continue;
        }

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

        if (msg.reply[0] != '\0') {
            printf("REPLY:   %s\n", msg.reply);

            parent_file = find_msgref(refs, msg.reply);
            if (parent_file != NULL) {
                printf("Thread:  reply to %s\n", parent_file);
                printf("Would import as: reply/comment, dry-run only\n");
                reply_found++;
            } else {
                printf("Thread:  orphan reply, parent not found in current spool\n");
                printf("Would import as: new top-level text for now, dry-run only\n");
                reply_orphan++;
            }
        } else {
            printf("REPLY:   (missing)\n");
            printf("Thread:  new top-level text\n");
            printf("Would import as: new top-level text, dry-run only\n");
            top_level++;
        }

        /*
         * Later:
         *   - scan existing SklaffKOM texts for ^AMSGID
         *   - find msg.reply in REFLIST
         *   - set com = parent text number if found
         *   - call send_ftn(...)
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
    printf("Parsed:        %ld\n", parsed);
    printf("Failed:        %ld\n", failed);
    printf("Top-level:     %ld\n", top_level);
    printf("Reply found:   %ld\n", reply_found);
    printf("Orphan reply:  %ld\n", reply_orphan);
    printf("Imported:      0 (dry-run)\n");

    free_msgrefs(refs);

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

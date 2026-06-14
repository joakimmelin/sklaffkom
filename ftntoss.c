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
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define FTNTOSS_LOCKFILE "/tmp/ftntoss.lock" /* modified on 2026-06-11, PL */
#define FTN_WRAP_COL 78 /* modified on 2026-06-13, PL */

#define FTN_OUR_ZONE  21   /* modified on 2026-06-12, PL */
#define FTN_OUR_NET   3    /* modified on 2026-06-12, PL */
#define FTN_OUR_NODE  242  /* modified on 2026-06-12, PL */
#define FTN_OUR_POINT 0    /* modified on 2026-06-12, PL */

#define FTN_HUB_ZONE  21   /* modified on 2026-06-12, PL */
#define FTN_HUB_NET   3    /* modified on 2026-06-12, PL */
#define FTN_HUB_NODE  100  /* modified on 2026-06-12, PL */
#define FTN_HUB_POINT 0    /* modified on 2026-06-12, PL */

#define FTN_LOCAL_ATTR 0x0100 /* modified on 2026-06-12, PL */
#define FTN_ORIGIN "Twilight Node" /* modified on 2026-06-12, PL */

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
    long parent_textnum;
    int orphan;
    struct planref *next;
};

struct msgitem {
    char filename[PATH_MAX];
    char path[PATH_MAX];
    char msgid[256];
    char reply[256];
    char subject[256]; /* modified on 2026-06-10, PL */
    struct msgitem *next;
};

static int export_test_ftn(const char *area);
static int next_msg_path(const char *area, char *out, size_t outsz, long *out_num);
static int write_fido_msg_out(const char *path, const char *area,
    const char *from, const char *to, const char *subject,
    const char *body, const char *reply, const char *msgid);
static void write_fixed_field(unsigned char *dst, size_t len, const char *src);
static void write_u16_le(unsigned char *dst, unsigned int val);
static void make_fido_date(char *out, size_t outsz);
static void make_ftn_msgid(char *out, size_t outsz, unsigned long serial);
static unsigned long make_export_test_serial(const char *area, long msgnum);

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
    const char *msgid, long planned_textnum, long parent_textnum, int orphan);
static const struct planref *find_planref_by_filename(struct planref *list,
    const char *filename);
static long find_planref_by_msgid(struct planref *list, const char *msgid);
static void free_planrefs(struct planref *list);

static int add_msgitem(struct msgitem **list, struct msgitem **tail,
    const char *filename, const char *path, const char *msgid,
    const char *reply, const char *subject);
static void free_msgitems(struct msgitem *list);

static int scan_existing_skl_msgids(const struct ftn_conf_info *ce,
    struct skref **out_refs, long *out_indexed);
static int extract_ftn_msgid_from_line(const char *line, char *out, size_t outsz);
static int build_spool_index(const char *spooldir, struct msgref **out_refs,
    struct msgitem **out_items, long *out_seen, long *out_indexed, long *out_failed);
static int build_import_plan(struct msgitem *items, struct skref *skrefs,
    long first_textnum, int include_unsafe, struct planref **out_plans,
    long *out_planned, long *out_next_textnum, long *out_top_level,
    long *out_reply_existing, long *out_reply_planned, long *out_orphan);
static void print_visible_ctrl(const char *s);
static long count_body_lines(const char *s);
static int dump_import_text(const char *area, const struct ftn_conf_info *ce,
    const char *filename, struct msgitem *items, struct planref *plans);
static int dump_one_import(const char *area, const struct ftn_conf_info *ce,
    const char *filename);

static char *build_ftn_mbuf(const char *area, const struct fido_msg *msg,
    const char *unsafe_reason);
static long send_ftn(int confid, const char *area, const struct fido_msg *msg,
    long com, const char *unsafe_reason);
static int rewrite_conf_last_text(int confid, long *new_textnum);
static int append_comment_link(int confid, long parent_text, long child_text);
static int import_one_ftn(const char *area, const char *filename);
static int import_all_ftn(const char *area, int include_unsafe);
static int diagnose_ftn(const char *area, int include_unsafe);
static void print_unsafe_reason(const char *filename, const struct fido_msg *msg,
    const char *reason);
static int subject_looks_like_reply(const char *subject);

static int import_all_areas_ftn(int include_unsafe);

static int acquire_ftntoss_lock(void);
static void release_ftntoss_lock(void);
static int run_with_lock(int (*fn)(void *), void *arg);

static int run_import_one_locked(void *arg);
static int run_import_area_locked(void *arg);
static int run_import_all_areas_locked(void *arg);

static char *wrap_ftn_body_for_skom(const char *body);
static void append_wrapped_segment(char *out, size_t outsz,
    const char *seg, size_t len);
static void append_to_dynbuf(char **buf, size_t *cap, size_t *len,
    const char *text);

struct import_one_args {
    const char *area;
    const char *filename;
};

struct import_area_args {
    const char *area;
    int include_unsafe;
};

struct import_all_areas_args {
    int include_unsafe;
};

static void
write_fixed_field(unsigned char *dst, size_t len, const char *src)
{
    size_t n;

    if (dst == NULL || len == 0)
        return;

    memset(dst, 0, len);

    if (src == NULL)
        return;

    n = strlen(src);
    if (n >= len)
        n = len - 1;

    memcpy(dst, src, n);
}

static void
write_u16_le(unsigned char *dst, unsigned int val)
{
    if (dst == NULL)
        return;

    dst[0] = (unsigned char)(val & 0xff);
    dst[1] = (unsigned char)((val >> 8) & 0xff);
}

static void
make_fido_date(char *out, size_t outsz)
{
    time_t now;
    struct tm *tm;
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    if (out == NULL || outsz == 0)
        return;

    out[0] = '\0';

    now = time(NULL);
    tm = localtime(&now);
    if (tm == NULL)
        return;

    snprintf(out, outsz, "%02d %s %02d  %02d:%02d:%02d",
        tm->tm_mday,
        months[tm->tm_mon],
        tm->tm_year % 100,
        tm->tm_hour,
        tm->tm_min,
        tm->tm_sec);
}

static void
make_ftn_addr(char *out, size_t outsz, int zone, int net, int node, int point)
{
    if (out == NULL || outsz == 0)
        return;

    /*
     * Keep point-zero addresses in classic 2D form for echomail kludges.
     * modified on 2026-06-13, PL
     */
    if (point == 0)
        snprintf(out, outsz, "%d:%d/%d", zone, net, node);
    else
        snprintf(out, outsz, "%d:%d/%d.%d", zone, net, node, point);
}

static void
make_ftn_msgid(char *out, size_t outsz, unsigned long serial)
{
    char addr[64];

    if (out == NULL || outsz == 0)
        return;

    make_ftn_addr(addr, sizeof(addr), FTN_OUR_ZONE, FTN_OUR_NET,
        FTN_OUR_NODE, FTN_OUR_POINT);

    /*
     * FTS-0009 MSGID format is:
     *
     *   ^AMSGID: <origin-address> <8-hex-digit-serial>
     *
     * Keep the serial field at exactly eight hexadecimal digits.
     *
     * modified on 2026-06-14, PL
     */
    snprintf(out, outsz, "%s %08lx", addr, serial & 0xffffffffUL);
}

static unsigned long
make_export_test_serial(const char *area, long msgnum)
{
    unsigned long hash = 5381UL;
    const unsigned char *p;

    if (area != NULL) {
        for (p = (const unsigned char *)area; *p != '\0'; p++)
            hash = ((hash << 5) + hash) ^ (unsigned long)(*p);
    }

    /*
     * Deterministic test serial:
     * high 16 bits = area hash
     * low 16 bits  = local outgoing .MSG number
     *
     * modified on 2026-06-14, PL
     */
    return ((hash & 0xffffUL) << 16) |
        ((unsigned long)msgnum & 0xffffUL);
}

static void
append_to_dynbuf(char **buf, size_t *cap, size_t *len, const char *text)
{
    size_t need;
    size_t add;
    char *nbuf;

    if (buf == NULL || cap == NULL || len == NULL || text == NULL)
        return;

    add = strlen(text);
    need = *len + add + 1;

    if (*buf == NULL || need > *cap) {
        size_t newcap = (*cap > 0) ? *cap : 1024;

        while (newcap < need)
            newcap *= 2;

        nbuf = realloc(*buf, newcap);
        if (nbuf == NULL)
            return;

        *buf = nbuf;
        *cap = newcap;
    }

    memcpy(*buf + *len, text, add);
    *len += add;
    (*buf)[*len] = '\0';
}

static void
append_wrapped_segment(char *out, size_t outsz, const char *seg, size_t len)
{
    size_t pos = 0;

    if (out == NULL || outsz == 0 || seg == NULL)
        return;

    out[0] = '\0';

    while (pos < len) {
        size_t left = len - pos;
        size_t take = left;
        size_t i;
        size_t start;

        while (left > 0 && (seg[pos] == ' ' || seg[pos] == '\t')) {
            pos++;
            left--;
        }

        if (left == 0)
            break;

        take = left;
        if (take > FTN_WRAP_COL) {
            take = FTN_WRAP_COL;

            /*
             * Prefer breaking at whitespace before the wrap column.
             * If no whitespace exists, hard-wrap the long word.
             *
             * modified on 2026-06-13, PL
             */
            for (i = take; i > 0; i--) {
                if (seg[pos + i] == ' ' || seg[pos + i] == '\t') {
                    take = i;
                    break;
                }
            }

            if (i == 0)
                take = FTN_WRAP_COL;
        }

        start = strlen(out);
        if (start + take + 2 >= outsz)
            break;

        memcpy(out + start, seg + pos, take);
        out[start + take] = '\n';
        out[start + take + 1] = '\0';

        pos += take;

        while (pos < len && (seg[pos] == ' ' || seg[pos] == '\t'))
            pos++;
    }
}

static char *
wrap_ftn_body_for_skom(const char *body)
{
    char *out = NULL;
    size_t cap = 0;
    size_t outlen = 0;
    const char *p;
    const char *line_start;

    if (body == NULL)
        return NULL;

    p = body;
    line_start = body;

    while (1) {
        if (*p == '\n' || *p == '\0') {
            size_t linelen = (size_t)(p - line_start);

            /*
             * Preserve blank lines. Preserve quoted/origin/control-ish lines
             * as-is, but wrap normal prose before storing it in SklaffKOM.
             *
             * modified on 2026-06-13, PL
             */
            if (linelen == 0) {
                append_to_dynbuf(&out, &cap, &outlen, "\n");
            } else if (line_start[0] == '>' ||
                       line_start[0] == '|' ||
                       line_start[0] == '*' ||
                       line_start[0] == '-' ||
                       line_start[0] == '\001') {
                char *tmp;

                tmp = malloc(linelen + 2);
                if (tmp != NULL) {
                    memcpy(tmp, line_start, linelen);
                    tmp[linelen] = '\n';
                    tmp[linelen + 1] = '\0';
                    append_to_dynbuf(&out, &cap, &outlen, tmp);
                    free(tmp);
                }
            } else if (linelen <= FTN_WRAP_COL) {
                char *tmp;

                tmp = malloc(linelen + 2);
                if (tmp != NULL) {
                    memcpy(tmp, line_start, linelen);
                    tmp[linelen] = '\n';
                    tmp[linelen + 1] = '\0';
                    append_to_dynbuf(&out, &cap, &outlen, tmp);
                    free(tmp);
                }
            } else {
                char wrapped[4096];

                append_wrapped_segment(wrapped, sizeof(wrapped),
                    line_start, linelen);
                append_to_dynbuf(&out, &cap, &outlen, wrapped);
            }

            if (*p == '\0')
                break;

            p++;
            line_start = p;
            continue;
        }

        p++;
    }

    if (out == NULL) {
        out = malloc(1);
        if (out != NULL)
            out[0] = '\0';
    }

    return out;
}

static int
next_msg_path(const char *area, char *out, size_t outsz, long *out_num)
{
    char dirpath[PATH_MAX];
    DIR *dir;
    struct dirent *de;
    long maxnum = 0;

    if (area == NULL || out == NULL || outsz == 0)
        return -1;

    if (snprintf(dirpath, sizeof(dirpath), "%s/%s", FTN_SPOOL, area) >=
            (int)sizeof(dirpath)) {
        fprintf(stderr, "[ERROR] FTN area path too long: %s/%s\n", FTN_SPOOL, area);
        return -1;
    }

    dir = opendir(dirpath);
    if (dir == NULL) {
        perror(dirpath);
        return -1;
    }

    while ((de = readdir(dir)) != NULL) {
        char *endp;
        long n;

        if (de->d_name[0] == '.')
            continue;

        errno = 0;
        n = strtol(de->d_name, &endp, 10);
        if (errno != 0 || endp == de->d_name)
            continue;

        if (strcmp(endp, ".msg") != 0 && strcmp(endp, ".MSG") != 0)
            continue;

        if (n > maxnum)
            maxnum = n;
    }

    closedir(dir);

    maxnum++;

    if (snprintf(out, outsz, "%s/%ld.msg", dirpath, maxnum) >= (int)outsz) {
        fprintf(stderr, "[ERROR] Output .MSG path too long\n");
        return -1;
    }

    if (out_num != NULL)
        *out_num = maxnum;

    return 0;
}

static int
write_fido_msg_out(const char *path, const char *area,
    const char *from, const char *to, const char *subject,
    const char *body, const char *reply, const char *msgid)
{
    FILE *fp;
    unsigned char hdr[190];
    char datebuf[32];

    if (path == NULL || area == NULL || from == NULL || to == NULL ||
            subject == NULL || body == NULL || msgid == NULL ||
            *msgid == '\0')
        return -1;

    memset(hdr, 0, sizeof(hdr));

    make_fido_date(datebuf, sizeof(datebuf));

    /*
     * Fido .MSG header:
     * from[36], to[36], subject[72], date[20],
     * then 13 little-endian 16-bit fields.
     *
     * modified on 2026-06-12, PL
     */
    write_fixed_field(hdr + 0,   36, from);
    write_fixed_field(hdr + 36,  36, to);
    write_fixed_field(hdr + 72,  72, subject);
    write_fixed_field(hdr + 144, 20, datebuf);

    write_u16_le(hdr + 164, 0);                  /* times read */
    write_u16_le(hdr + 166, FTN_HUB_NODE);       /* dest node */
    write_u16_le(hdr + 168, FTN_OUR_NODE);       /* orig node */
    write_u16_le(hdr + 170, 0);                  /* cost */
    write_u16_le(hdr + 172, FTN_OUR_NET);        /* orig net */
    write_u16_le(hdr + 174, FTN_HUB_NET);        /* dest net */
    write_u16_le(hdr + 176, FTN_HUB_ZONE);       /* dest zone */
    write_u16_le(hdr + 178, FTN_OUR_ZONE);       /* orig zone */
    write_u16_le(hdr + 180, FTN_HUB_POINT);      /* dest point */
    write_u16_le(hdr + 182, FTN_OUR_POINT);      /* orig point */
    write_u16_le(hdr + 184, 0);                  /* reply to */
    write_u16_le(hdr + 186, FTN_LOCAL_ATTR);     /* attr: local */
    write_u16_le(hdr + 188, 0);                  /* next reply */

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    if (fwrite(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
        perror("write .MSG header");
        fclose(fp);
        return -1;
    }

    /*
     * Do not write AREA here: CrashMail adds AREA:<tag> when packing.
     * Writing it in the local .MSG too creates a duplicate AREA line.
     * modified on 2026-06-13, PL
     */
    fprintf(fp, "\001MSGID: %s\r", msgid);

    if (reply != NULL && *reply != '\0')
        fprintf(fp, "\001REPLY: %s\r", reply);

    fprintf(fp, "\001PID: SklaffKOM ftntoss\r");
    fprintf(fp, "\001CHRS: UTF-8 4\r");
    fprintf(fp, "\r");

    while (*body != '\0') {
        if (*body == '\n')
            fputc('\r', fp);
        else
            fputc((unsigned char)*body, fp);
        body++;
    }

    fprintf(fp, "\r");
    {
        char origin_addr[64];

        make_ftn_addr(origin_addr, sizeof(origin_addr), FTN_OUR_ZONE,
            FTN_OUR_NET, FTN_OUR_NODE, FTN_OUR_POINT);
        fprintf(fp, "--- SklaffKOM\r");
        fprintf(fp, " * Origin: %s (%s)\r", FTN_ORIGIN, origin_addr);
    }

    fputc('\0', fp);

    if (fclose(fp) != 0) {
        perror(path);
        return -1;
    }

    printf("Wrote FTN .MSG: %s\n", path);
    printf("MSGID: %s\n", msgid);

    return 0;
}

static void
print_unsafe_reason(const char *filename, const struct fido_msg *msg,
    const char *reason)
{
    printf("%-8s %-18s %-28s %s\n",
        filename ? filename : "(unknown)",
        reason ? reason : "(unknown)",
        msg && msg->from[0] ? msg->from : "(unknown)",
        msg && msg->subject[0] ? msg->subject : "(no subject)");
}

static int
acquire_ftntoss_lock(void)
{
    int fd;
    char buf[64];

    fd = open(FTNTOSS_LOCKFILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd == -1) {
        if (errno == EEXIST) {
            fprintf(stderr, "[ERROR] ftntoss is already running, lock exists: %s\n",
                FTNTOSS_LOCKFILE);
            fprintf(stderr, "[ERROR] Remove the lock only if you are sure no ftntoss process is active.\n");
        } else {
            perror(FTNTOSS_LOCKFILE);
        }
        return -1;
    }

    snprintf(buf, sizeof(buf), "%ld\n", (long)getpid());
    if (write(fd, buf, strlen(buf)) == -1) {
        perror("write lockfile");
        close(fd);
        unlink(FTNTOSS_LOCKFILE);
        return -1;
    }

    close(fd);
    return 0;
}

static void
release_ftntoss_lock(void)
{
    if (unlink(FTNTOSS_LOCKFILE) == -1 && errno != ENOENT)
        perror("unlink lockfile");
}

static int
run_with_lock(int (*fn)(void *), void *arg)
{
    int rc;

    if (fn == NULL)
        return -1;

    if (acquire_ftntoss_lock() != 0)
        return -1;

    rc = fn(arg);

    release_ftntoss_lock();

    return rc;
}

static int
subject_looks_like_reply(const char *subject)
{
    const char *p;

    if (subject == NULL)
        return 0;

    p = subject;

    while (*p == ' ' || *p == '\t')
        p++;

    /*
     * Conservative FTN batch-import safety:
     * if a message says "Re:" but has no REPLY kludge, do not import it
     * as a top-level message.  It is probably a reply whose parent cannot
     * be resolved safely yet.
     *
     * modified on 2026-06-10, PL
     */
    if ((p[0] == 'R' || p[0] == 'r') &&
        (p[1] == 'E' || p[1] == 'e') &&
        p[2] == ':')
        return 1;

    return 0;
}

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
    const char *msgid, long planned_textnum, long parent_textnum, int orphan)
{
    struct planref *n;

    if (list == NULL || filename == NULL || *filename == '\0' || planned_textnum <= 0)
        return;

    n = (struct planref *)calloc(1, sizeof(*n));
    if (n == NULL)
        return;

    strlcpy(n->filename, filename, sizeof(n->filename)); /* modified on 2026-06-09, PL */
    if (msgid != NULL)
        strlcpy(n->msgid, msgid, sizeof(n->msgid)); /* modified on 2026-06-09, PL */
    n->planned_textnum = planned_textnum;
    n->parent_textnum = parent_textnum;
    n->orphan = orphan;

    n->next = *list;
    *list = n;
}

static const struct planref *
find_planref_by_filename(struct planref *list, const char *filename)
{
    struct planref *p;

    if (filename == NULL || *filename == '\0')
        return NULL;

    for (p = list; p != NULL; p = p->next) {
        if (strcmp(p->filename, filename) == 0)
            return p;
    }

    return NULL;
}

static long
find_planref_by_msgid(struct planref *list, const char *msgid)
{
    struct planref *p;

    if (msgid == NULL || *msgid == '\0')
        return 0;

    for (p = list; p != NULL; p = p->next) {
        if (p->msgid[0] != '\0' && strcmp(p->msgid, msgid) == 0)
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
    const char *filename, const char *path, const char *msgid,
    const char *reply, const char *subject)
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
    if (subject != NULL)
        strlcpy(n->subject, subject, sizeof(n->subject)); /* modified on 2026-06-10, PL */
    if (*list == NULL) {
        *list = n;
        *tail = n;
    } else {
        (*tail)->next = n;
        *tail = n;
    }

    return 0;
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
        if (fp == NULL)
            continue;

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
build_spool_index(const char *spooldir, struct msgref **out_refs,
    struct msgitem **out_items, long *out_seen, long *out_indexed, long *out_failed)
{
    DIR *dir;
    struct dirent *de;
    struct msgitem *items = NULL;
    struct msgitem *tail = NULL;
    char path[PATH_MAX];
    long seen = 0;
    long indexed = 0;
    long failed = 0;

    if (spooldir == NULL || out_refs == NULL || out_items == NULL ||
        out_seen == NULL || out_indexed == NULL || out_failed == NULL)
        return -1;

    *out_refs = NULL;
    *out_items = NULL;
    *out_seen = 0;
    *out_indexed = 0;
    *out_failed = 0;

    dir = opendir(spooldir);
    if (dir == NULL) {
        fprintf(stderr, "[ERROR] Could not open FTN spool directory '%s'\n", spooldir);
        perror("opendir");
        return -1;
    }

    printf("Indexing .MSG files by MSGID...\n");

    while ((de = readdir(dir)) != NULL) {
        struct fido_msg msg;

        if (!is_msg_file(de->d_name))
            continue;

        seen++;

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
            add_msgref(out_refs, msg.msgid, de->d_name);
            indexed++;
        }

        if (add_msgitem(&items, &tail, de->d_name, path, msg.msgid,
            msg.reply, msg.subject) != 0) {
            fprintf(stderr, "[ERROR] Out of memory while indexing: %s\n", path);
            free_fido_msg(&msg);
            closedir(dir);
            free_msgitems(items);
            free_msgrefs(*out_refs);
            *out_refs = NULL;
            return -1;
        }

        free_fido_msg(&msg);
    }

    closedir(dir);

    *out_items = items;
    *out_seen = seen;
    *out_indexed = indexed;
    *out_failed = failed;

    printf("Indexed:     %ld MSGID value(s)\n\n", indexed);

    return failed ? -1 : 0;
}
static int
build_import_plan(struct msgitem *items, struct skref *skrefs,
    long first_textnum, int include_unsafe, struct planref **out_plans,
    long *out_planned, long *out_next_textnum, long *out_top_level,
    long *out_reply_existing, long *out_reply_planned, long *out_orphan)
{
    struct planref *plans = NULL;
    struct msgitem *m;
    long next_textnum = first_textnum;
    long planned = 0;
    long top_level = 0;
    long reply_existing = 0;
    long reply_planned = 0;
    long orphan = 0;
    int changed;

    if (out_plans == NULL || out_planned == NULL || out_next_textnum == NULL ||
        out_top_level == NULL || out_reply_existing == NULL ||
        out_reply_planned == NULL || out_orphan == NULL)
        return -1;

    *out_plans = NULL;
    *out_planned = 0;
    *out_next_textnum = first_textnum;
    *out_top_level = 0;
    *out_reply_existing = 0;
    *out_reply_planned = 0;
    *out_orphan = 0;

    printf("Planning dry-run import order...\n");

    /*
     * Pass 1:
     * Plan true top-level messages and replies to messages that already
     * exist in SklaffKOM.
     */
    for (m = items; m != NULL; m = m->next) {
        long parent_text;

        if (find_planref_by_filename(plans, m->filename) != NULL)
            continue;

        /*
         * Do not plan messages already imported into SklaffKOM.
         *
         * modified on 2026-06-11, PL
         */
        if (find_skref(skrefs, m->msgid) > 0)
            continue;

        if (m->reply[0] == '\0') {
            if (subject_looks_like_reply(m->subject)) {
                if (!include_unsafe)
                    continue;

                /*
                 * Diagnose/include-unsafe:
                 * Re:-without-REPLY would be imported as top-level fallback.
                 *
                 * modified on 2026-06-11, PL
                 */
                add_planref(&plans, m->filename, m->msgid,
                    next_textnum++, 0, 1);
                planned++;
                top_level++;
                continue;
            }

            add_planref(&plans, m->filename, m->msgid,
                next_textnum++, 0, 0);
            planned++;
            top_level++;
            continue;
        }

        parent_text = find_skref(skrefs, m->reply);
        if (parent_text > 0) {
            add_planref(&plans, m->filename, m->msgid,
                next_textnum++, parent_text, 0);
            planned++;
            reply_existing++;
        }
    }

    /*
     * Pass 2..N:
     * Plan replies whose parent was planned in an earlier pass.
     */
    do {
        changed = 0;

        for (m = items; m != NULL; m = m->next) {
            long parent_text;

            if (find_planref_by_filename(plans, m->filename) != NULL)
                continue;

            /*
             * Do not plan messages already imported into SklaffKOM.
             *
             * modified on 2026-06-11, PL
             */
            if (find_skref(skrefs, m->msgid) > 0)
                continue;

            if (m->reply[0] == '\0')
                continue;

            parent_text = find_planref_by_msgid(plans, m->reply);
            if (parent_text > 0) {
                add_planref(&plans, m->filename, m->msgid,
                    next_textnum++, parent_text, 0);
                planned++;
                reply_planned++;
                changed = 1;
            }
        }
    } while (changed);

    /*
     * Include-unsafe mode:
     * Any still-unplanned message is importable as top-level fallback.
     * This matches --import-all --include-unsafe.
     *
     * modified on 2026-06-11, PL
     */
    if (include_unsafe) {
        for (m = items; m != NULL; m = m->next) {
            if (find_planref_by_filename(plans, m->filename) != NULL)
                continue;

            if (find_skref(skrefs, m->msgid) > 0)
                continue;

            add_planref(&plans, m->filename, m->msgid,
                next_textnum++, 0, 1);
            planned++;
            orphan++;
        }
    }

    *out_plans = plans;
    *out_planned = planned;
    *out_next_textnum = next_textnum;
    *out_top_level = top_level;
    *out_reply_existing = reply_existing;
    *out_reply_planned = reply_planned;
    *out_orphan = orphan;

    printf("Planned:     %ld simulated import(s)\n", planned);
    printf("Next text:   %ld\n\n", next_textnum);

    return 0;
}


static void
print_visible_ctrl(const char *s)
{
    if (s == NULL)
        return;

    while (*s) {
        if ((unsigned char)*s == '\001')
            fputs("^A", stdout);
        else
            putchar((unsigned char)*s);
        s++;
    }
}

static long
count_body_lines(const char *s)
{
    long lines = 0;

    if (s == NULL)
        return 0;

    while (*s != '\0') {
        if (*s == '\n')
            lines++;
        s++;
    }

    return lines;
}

static int
dump_import_text(const char *area, const struct ftn_conf_info *ce,
    const char *filename, struct msgitem *items, struct planref *plans)
{
    struct msgitem *m;
    struct fido_msg msg;
    const struct planref *plan;
    long com = 0;
    long size;

    if (area == NULL || ce == NULL || filename == NULL || items == NULL || plans == NULL)
        return -1;

    for (m = items; m != NULL; m = m->next) {
        if (strcmp(m->filename, filename) == 0)
            break;
    }

    if (m == NULL) {
        fprintf(stderr, "[ERROR] No such .MSG file in spool plan: %s\n", filename);
        return -1;
    }

    plan = find_planref_by_filename(plans, filename);
    if (plan == NULL) {
        fprintf(stderr, "[ERROR] No import plan found for %s\n", filename);
        return -1;
    }

    if (read_fido_msg(m->path, &msg) != 0) {
        fprintf(stderr, "[ERROR] Could not parse .MSG file: %s\n", m->path);
        return -1;
    }

    com = plan->parent_textnum;

    /* Approximation for dry-run: subject + FTN metadata + blank line + raw body. */
    size = 7 + count_body_lines(msg.raw_body);
    if (msg.chrs[0] != '\0')
        size++;
    if (msg.msgid[0] != '\0')
        size++;
    if (msg.reply[0] != '\0')
        size++;

    printf("\n");
    printf("FTN import dump\n");
    printf("---------------\n");
    printf("Area:          %s\n", area);
    printf("Conference:    %s (%d)\n", ce->name, ce->num);
    printf("Source file:   %s\n", filename);
    printf("Source path:   %s\n", m->path);
    printf("Planned text:  %ld\n", plan->planned_textnum);
    printf("Comment to:    %ld", com);
    if (plan->orphan)
        printf(" (orphan fallback/top-level)");
    printf("\n");
    printf("\n");

    printf("Would write approximate SklaffKOM text file:\n");
    printf("------------------------------------------------------------\n");

    /*
     * Dry-run approximation of the SklaffKOM text header:
     *   textno:anonymous?:time:comment-to:...:...:line-count
     * The real importer should use the same header logic as newstoss/send_news.
     */
    printf("%ld:%d:%ld:%ld:%d:%d:%ld\n",
        plan->planned_textnum, 0, 0L, com, 0, 0, size);

    printf("%s\n", msg.subject[0] ? msg.subject : "(no subject)");
    printf("From: %s\n", msg.from[0] ? msg.from : "(unknown)");
    printf("To: %s\n", msg.to[0] ? msg.to : "(unknown)");
    printf("Subject: %s\n", msg.subject[0] ? msg.subject : "(no subject)");
    printf("Date: %s\n", msg.date[0] ? msg.date : "(unknown)");
    printf("FTN-Area: %s\n", area);

    if (msg.chrs[0] != '\0')
        printf("FTN-CHRS: %s\n", msg.chrs);
    if (msg.msgid[0] != '\0')
        printf("FTN-MSGID: %s\n", msg.msgid);
    if (msg.reply[0] != '\0')
        printf("FTN-REPLY: %s\n", msg.reply);

    printf("\n");

    /* Show real FTN kludges visibly as ^A in the dump. */
    print_visible_ctrl(msg.raw_body);

    printf("\n------------------------------------------------------------\n");
    printf("End dry-run import dump\n");

    free_fido_msg(&msg);
    return 0;
}

static int
export_test_ftn(const char *area)
{
    struct ftn_conf_info ce;
    char path[PATH_MAX];
    long msgnum = 0;
    char body[1024];
    char msgid[128];
    unsigned long serial;

    if (area == NULL || *area == '\0')
        return -1;

    printf("ftntoss export-test starting\n");
    printf("============================\n\n");

    printf("Checking conference: %s... ", area);
    fflush(stdout);

    if (find_ftn_conf(area, &ce) != 0) {
        printf("FAILED\n");
        fprintf(stderr, "[ERROR] Could not find FTN conference '%s'\n", area);
        return -1;
    }

    printf("OK!\n");

    if (ce.type != FTN_CONF) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            area, ce.type);
        return -1;
    }

    if (next_msg_path(area, path, sizeof(path), &msgnum) != 0)
        return -1;
        
		serial = make_export_test_serial(area, msgnum);
		make_ftn_msgid(msgid, sizeof(msgid), serial);

    snprintf(body, sizeof(body),
        "This is a SklaffKOM/ftntoss echomail export test.\n"
        "\n"
        "(Testing again with fully compliant MSGID (hopefully)."
		"\n"
		"BBS name:      %s\n"
        "Hostname:      %s\n"
        "Location:      %s\n"
        "Sysop:         %s\n"
        "\n"
        "FTN area:      %s\n"
        "Local conf:    %d\n"
        "Local msg no:  %ld\n"
        "FTN MSGID:     %s\n"

        "\n"
        "If you can read this message, outgoing FTN echomail from this\n"
        "SklaffKOM system appears to be working.\n"
        "\n"
        "Please reply if you can. Thanks!\n\n",
        SKLAFF_ID,
        MACHINE_NAME,
        SKLAFF_LOC,
        SKLAFF_SYSOP,
        area,
        ce.num,
        msgnum,
        msgid);

    printf("FTN export-test setup\n");
    printf("---------------------\n");
    printf("BBS name:   %s\n", SKLAFF_ID);
    printf("Hostname:   %s\n", MACHINE_NAME);
    printf("Location:   %s\n", SKLAFF_LOC);
    printf("Sysop:      %s\n", SKLAFF_SYSOP);
    printf("Area:       %s\n", area);
    printf("Conf num:   %d\n", ce.num);
    printf("Conf type:  %d (FTN_CONF)\n", ce.type);
    printf("Msg no:     %ld\n", msgnum);
    printf("MSGID:      %s\n", msgid);
    printf("Output:     %s\n\n", path);

	return write_fido_msg_out(path, area,
        SKLAFF_SYSOP,
        "All",
        SKLAFF_ID " FTN export test",
        body,
        NULL,
        msgid);
}

static int
import_one_ftn(const char *area, const char *filename)
{
    struct ftn_conf_info ce;
    struct msgref *refs = NULL;
    struct skref *skrefs = NULL;
    struct msgitem *items = NULL;
    struct msgitem *m = NULL;
    struct planref *plans = NULL;
    const struct planref *plan = NULL;
    char spooldir[PATH_MAX];
    struct fido_msg msg;
    long indexed = 0;
    long existing_indexed = 0;
    long failed = 0;
    long seen = 0;
    long planned = 0;
    long next_textnum = 0;
    long top_level = 0;
    long reply_existing = 0;
    long reply_planned = 0;
    long reply_orphan = 0;
    long com = 0;
    long imported_text = 0;
    int rc = -1;

    if (area == NULL || filename == NULL)
        return -1;

    printf("ftntoss import-one starting\n");
    printf("===========================\n\n");

    if (find_ftn_conf(area, &ce) != 0)
        return -1;

    if (ce.type != FTN_CONF) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce.name, ce.type);
        return -1;
    }

    if (snprintf(spooldir, sizeof(spooldir), "%s/%s", FTN_SPOOL, area) >= (int)sizeof(spooldir)) {
        fprintf(stderr, "[ERROR] Spool path too long: %s/%s\n", FTN_SPOOL, area);
        return -1;
    }

    printf("FTN import-one setup\n");
    printf("--------------------\n");
    printf("Area:        %s\n", area);
    printf("Spool:       %s\n", spooldir);
    printf("Conf name:   %s\n", ce.name);
    printf("Conf num:    %d\n", ce.num);
    printf("Conf type:   %d (FTN_CONF)\n", ce.type);
    printf("Last text:   %ld\n", ce.last_text);
    printf("Target file: %s\n\n", filename);

    if (scan_existing_skl_msgids(&ce, &skrefs, &existing_indexed) != 0)
        goto cleanup;

    if (build_spool_index(spooldir, &refs, &items, &seen, &indexed, &failed) != 0)
        goto cleanup;

    if (build_import_plan(items, skrefs, ce.last_text + 1, 0,
            &plans, &planned, &next_textnum,
            &top_level, &reply_existing, &reply_planned, &reply_orphan) != 0)
        goto cleanup;

    for (m = items; m != NULL; m = m->next) {
        if (strcmp(m->filename, filename) == 0)
            break;
    }

    if (m == NULL) {
        fprintf(stderr, "[ERROR] No such .MSG file in spool: %s\n", filename);
        goto cleanup;
    }

    plan = find_planref_by_filename(plans, filename);
    if (plan == NULL) {
        fprintf(stderr, "[ERROR] No import plan found for %s\n", filename);
        goto cleanup;
    }

    if (read_fido_msg(m->path, &msg) != 0) {
        fprintf(stderr, "[ERROR] Could not parse .MSG file: %s\n", m->path);
        goto cleanup;
    }
    
    if (msg.msgid[0] != '\0') {
        long already_imported = 0;

        already_imported = find_skref(skrefs, msg.msgid);
        if (already_imported > 0) {
            fprintf(stderr,
                "[REFUSE] %s is already imported as SklaffKOM text %ld\n",
                filename, already_imported);
            fprintf(stderr,
                "[REFUSE] MSGID: %s\n",
                msg.msgid);

            free_fido_msg(&msg);
            goto cleanup;
        }
    } else {
        fprintf(stderr,
            "[REFUSE] %s has no MSGID; refusing import to avoid duplicates\n",
            filename);

        free_fido_msg(&msg);
        goto cleanup;
    }
    /*
     * First safe import-one rules:
     *
     * - top-level messages are OK
     * - replies to already existing SklaffKOM texts are OK
     * - replies to messages only planned in this same batch are refused
     * - orphan replies are refused
     */
    if (msg.reply[0] != '\0') {
        if (plan->orphan) {
            fprintf(stderr,
                "[REFUSE] %s is an orphan reply; parent not found in SklaffKOM or current spool.\n",
                filename);
            fprintf(stderr,
                "[REFUSE] First version of --import-one only imports top-level messages or replies to existing SklaffKOM texts.\n");
            free_fido_msg(&msg);
            goto cleanup;
        }

        if (plan->parent_textnum <= 0) {
            fprintf(stderr,
                "[REFUSE] %s has REPLY but planner did not assign a parent text.\n",
                filename);
            free_fido_msg(&msg);
            goto cleanup;
        }

        if (plan->parent_textnum > ce.last_text) {
            fprintf(stderr,
                "[REFUSE] %s is a reply to planned text %ld, but that parent is not imported yet.\n",
                filename, plan->parent_textnum);
            fprintf(stderr,
                "[REFUSE] Import the parent first, then run --import-one again.\n");
            free_fido_msg(&msg);
            goto cleanup;
        }

        com = plan->parent_textnum;
    } else {
        com = 0;
    }

    printf("Import decision\n");
    printf("---------------\n");
    printf("File:          %s\n", filename);
    printf("Path:          %s\n", m->path);
    printf("Subject:       %s\n", msg.subject);
    printf("MSGID:         %s\n", msg.msgid[0] ? msg.msgid : "(missing)");
    printf("REPLY:         %s\n", msg.reply[0] ? msg.reply : "(missing)");
    printf("Planned text:  %ld\n", plan->planned_textnum);
    printf("Actual com:    %ld\n", com);
    printf("\n");

	imported_text = send_ftn(ce.num, area, &msg, com, NULL);
    if (imported_text <= 0) {
        fprintf(stderr, "[ERROR] send_ftn() failed\n");
        free_fido_msg(&msg);
        goto cleanup;
    }

    printf("\nftntoss import-one done\n");
    printf("Imported: SklaffKOM text %ld\n", imported_text);

    free_fido_msg(&msg);
    rc = 0;

cleanup:
    free_msgrefs(refs);
    free_skrefs(skrefs);
    free_msgitems(items);
    free_planrefs(plans);

    return rc;
}

static int
import_all_ftn(const char *area, int include_unsafe)
{
    struct ftn_conf_info ce;
    struct msgref *refs = NULL;
    struct skref *skrefs = NULL;
    struct msgitem *items = NULL;
    struct msgitem *m;
    char spooldir[PATH_MAX];
    long indexed = 0;
    long existing_indexed = 0;
    long failed = 0;
    long seen = 0;
    long imported = 0;
    long skipped_duplicate = 0;
    long skipped_nomsgid = 0;
    long skipped_re_without_reply = 0; /* modified on 2026-06-10, PL */
    long deferred = 0;
    long orphan = 0;
    long pass = 0;
    int changed;
    int rc = -1;

    if (area == NULL)
        return -1;

    printf("ftntoss import-all starting\n");
    printf("===========================\n\n");

    if (find_ftn_conf(area, &ce) != 0)
        return -1;

    if (ce.type != FTN_CONF) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce.name, ce.type);
        return -1;
    }

    if (snprintf(spooldir, sizeof(spooldir), "%s/%s", FTN_SPOOL, area) >= (int)sizeof(spooldir)) {
        fprintf(stderr, "[ERROR] Spool path too long: %s/%s\n", FTN_SPOOL, area);
        return -1;
    }

    printf("FTN import-all setup\n");
    printf("--------------------\n");
    printf("Area:        %s\n", area);
    printf("Spool:       %s\n", spooldir);
    printf("Conf name:   %s\n", ce.name);
    printf("Conf num:    %d\n", ce.num);
    printf("Conf type:   %d (FTN_CONF)\n", ce.type);
    printf("Last text:   %ld\n", ce.last_text);
    printf("Include unsafe: %s\n\n", include_unsafe ? "yes" : "no");
    
    if (scan_existing_skl_msgids(&ce, &skrefs, &existing_indexed) != 0)
        goto cleanup;

    if (build_spool_index(spooldir, &refs, &items, &seen, &indexed, &failed) != 0)
        goto cleanup;

    printf("Importing messages%s...\n\n",
        include_unsafe ? " including unsafe fallbacks" : " safely");

    /*
     * Multi-pass import:
     *
     * Pass 1 imports top-level messages and replies to already imported
     * SklaffKOM texts.
     *
     * Later passes can import replies whose parent was imported in an
     * earlier pass. This avoids needing the filesystem/readdir order to
     * be thread-safe.
     */
    do {
        changed = 0;
        pass++;

        printf("Import pass %ld\n", pass);
        printf("---------------\n");

        for (m = items; m != NULL; m = m->next) {
            struct fido_msg msg;
            long already_imported = 0;
            long com = 0;
            long imported_text = 0;
			const char *unsafe_reason = NULL; /* modified on 2026-06-11, PL */
			
            if (read_fido_msg(m->path, &msg) != 0) {
                fprintf(stderr, "[ERROR] Could not parse .MSG file: %s\n", m->path);
                failed++;
                continue;
            }

            if (msg.msgid[0] == '\0') {
                skipped_nomsgid++;
                free_fido_msg(&msg);
                continue;
            }

            already_imported = find_skref(skrefs, msg.msgid);
            if (already_imported > 0) {
                free_fido_msg(&msg);
                continue;
            }
			if (msg.reply[0] == '\0' && subject_looks_like_reply(msg.subject)) {
    			if (!include_unsafe) {
        		free_fido_msg(&msg);
        		continue;
    		}

    	/*
	     * Unsafe mode:
	     * Import Re:-without-REPLY as top-level. This preserves readability,
	     * but does not pretend we know the thread parent.
	     *
	     * modified on 2026-06-11, PL
	     */
	    com = 0;
	    unsafe_reason = "re-no-reply";
	}
            if (msg.reply[0] != '\0') {
    			com = find_skref(skrefs, msg.reply);
    				if (com <= 0) {
        				if (!include_unsafe) {
            			free_fido_msg(&msg);
            			continue;
        	}
        	
			/*
        	 * Unsafe mode:
        	 * Parent cannot be resolved. Import as top-level rather than
      		 * inventing a false parent.
     	     *
     	     * modified on 2026-06-11, PL
    	     */
    	    com = 0;

  	      if (find_msgref(refs, msg.reply) != NULL)
  	          unsafe_reason = "deferred";
  	      else
  	          unsafe_reason = "orphan";
 	   		}
		}

            printf("Importing %-8s -> ", m->filename);

			imported_text = send_ftn(ce.num, area, &msg, com, unsafe_reason);
            if (imported_text <= 0) {
                printf("FAILED\n");
                fprintf(stderr, "[ERROR] send_ftn() failed for %s\n", m->filename);
                free_fido_msg(&msg);
                goto cleanup;
            }

            /*
             * Add the newly imported MSGID to the in-memory SklaffKOM index
             * so replies later in this run can attach to it.
             */
            add_skref(&skrefs, msg.msgid, imported_text);

            imported++;
            changed = 1;

            free_fido_msg(&msg);
        }

        printf("\n");
    } while (changed);

    /*
     * Final diagnostics: anything not imported now is either a duplicate,
     * missing MSGID, or unresolved reply/orphan.
     */
    for (m = items; m != NULL; m = m->next) {
        struct fido_msg msg;
        long existing = 0;

        if (read_fido_msg(m->path, &msg) != 0)
        continue;

        if (msg.msgid[0] == '\0') {
        free_fido_msg(&msg);
        continue;
        }

        existing = find_skref(skrefs, msg.msgid);
        if (existing > 0) {
           /*
            * Already imported before this run or during this run.
            */
           if (existing <= ce.last_text)
                skipped_duplicate++;

            free_fido_msg(&msg);
            continue;
            }

        if (msg.reply[0] == '\0' && subject_looks_like_reply(msg.subject)) {
            skipped_re_without_reply++;
            free_fido_msg(&msg);
            continue;
        }

        if (msg.reply[0] != '\0') {
            if (find_msgref(refs, msg.reply) != NULL)
                deferred++;
            else
                orphan++;
        }

        free_fido_msg(&msg);
    }

    printf("FTN import-all summary\n");
    printf("----------------------\n");
    printf("Area:             %s\n", area);
    printf("Seen:             %ld .MSG file(s)\n", seen);
    printf("Indexed:          %ld MSGID value(s)\n", indexed);
    printf("Existing IDs:     %ld SklaffKOM MSGID value(s) at start\n", existing_indexed);
    printf("Imported:         %ld\n", imported);
    printf("Duplicates:       %ld already imported\n", skipped_duplicate);
    printf("Missing MSGID:    %ld skipped\n", skipped_nomsgid);
    printf("Deferred replies: %ld unresolved parent in spool\n", deferred);
    printf("Orphan replies:   %ld parent not found\n", orphan);
    printf("Re without REPLY: %ld skipped\n", skipped_re_without_reply);
    printf("Failed:           %ld\n", failed);
    rc = failed ? -1 : 0;

cleanup:
    free_msgrefs(refs);
    free_skrefs(skrefs);
    free_msgitems(items);

    return rc;
}

static int
import_all_areas_ftn(int include_unsafe)
{
    FILE *fp;
    char line[1024];
    long areas = 0;
    long ok = 0;
    long failed = 0;

    printf("ftntoss import-all-areas starting\n");
    printf("=================================\n\n");
    printf("CONF_FILE:      %s\n", CONF_FILE);
    printf("Include unsafe: %s\n\n", include_unsafe ? "yes" : "no");

    fp = fopen(CONF_FILE, "r");
    if (fp == NULL) {
        perror(CONF_FILE);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        struct ftn_conf_info ce;

        if (parse_conf_line(line, &ce) != 0)
            continue;

        if (ce.type != FTN_CONF)
            continue;

        areas++;

        printf("\n");
        printf("============================================================\n");
        printf("Importing FTN area: %s (conf %d)\n", ce.name, ce.num);
        printf("============================================================\n\n");

        if (import_all_ftn(ce.name, include_unsafe) == 0) {
            ok++;
        } else {
            failed++;
            fprintf(stderr, "[ERROR] FTN import failed for area '%s'\n", ce.name);
        }
    }

    fclose(fp);

    printf("\n");
    printf("FTN import-all-areas summary\n");
    printf("----------------------------\n");
    printf("Areas found:      %ld\n", areas);
    printf("Areas OK:         %ld\n", ok);
    printf("Areas failed:     %ld\n", failed);
    printf("Include unsafe:   %s\n", include_unsafe ? "yes" : "no");

    return failed ? -1 : 0;
}

static int
diagnose_ftn(const char *area, int include_unsafe)
{
    struct ftn_conf_info ce;
    struct msgref *refs = NULL;
    struct skref *skrefs = NULL;
    struct msgitem *items = NULL;
    struct msgitem *m;
    struct planref *plans = NULL;
    char spooldir[PATH_MAX];
    long indexed = 0;
    long existing_indexed = 0;
    long failed = 0;
    long seen = 0;
    long duplicates = 0;
    long missing_msgid = 0;
    long re_without_reply = 0;
    long deferred = 0;
    long orphan = 0;
    long planned = 0;
    long next_textnum = 0;
    long top_level = 0;
    long reply_existing = 0;
    long reply_planned = 0;
    long reply_orphan = 0;
    int rc = -1;
    
    if (area == NULL)
        return -1;

    printf("ftntoss diagnose starting\n");
    printf("=========================\n\n");

    if (find_ftn_conf(area, &ce) != 0)
        return -1;

    if (ce.type != FTN_CONF) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce.name, ce.type);
        return -1;
    }

    if (snprintf(spooldir, sizeof(spooldir), "%s/%s", FTN_SPOOL, area) >= (int)sizeof(spooldir)) {
        fprintf(stderr, "[ERROR] Spool path too long: %s/%s\n", FTN_SPOOL, area);
        return -1;
    }

    printf("FTN diagnose setup\n");
    printf("------------------\n");
    printf("Area:        %s\n", area);
    printf("Spool:       %s\n", spooldir);
    printf("Conf name:   %s\n", ce.name);
    printf("Conf num:    %d\n", ce.num);
    printf("Conf type:   %d (FTN_CONF)\n", ce.type);
    printf("Last text:   %ld\n\n", ce.last_text);
	printf("Include unsafe: %s\n\n", include_unsafe ? "yes" : "no");
	
    if (scan_existing_skl_msgids(&ce, &skrefs, &existing_indexed) != 0)
        goto cleanup;

    if (build_spool_index(spooldir, &refs, &items, &seen, &indexed, &failed) != 0)
        goto cleanup;
    
    if (build_import_plan(items, skrefs, ce.last_text + 1, include_unsafe,
        	&plans, &planned, &next_textnum,
        	&top_level, &reply_existing, &reply_planned, &reply_orphan) != 0)
    	goto cleanup;

    printf("Unsafe / skipped diagnostics\n");
    printf("----------------------------\n");
    printf("%-8s %-18s %-28s %s\n", "File", "Reason", "From", "Subject");
    printf("%-8s %-18s %-28s %s\n", "----", "------", "----", "-------");

    for (m = items; m != NULL; m = m->next) {
        struct fido_msg msg;
        long existing = 0;

        if (read_fido_msg(m->path, &msg) != 0) {
            failed++;
            continue;
        }

        if (msg.msgid[0] == '\0') {
            missing_msgid++;
            print_unsafe_reason(m->filename, &msg, "missing-msgid");
            free_fido_msg(&msg);
            continue;
        }

        existing = find_skref(skrefs, msg.msgid);
        if (existing > 0) {
            duplicates++;
            free_fido_msg(&msg);
            continue;
        }
        if (find_planref_by_msgid(plans, msg.msgid) > 0) {
            free_fido_msg(&msg);
            continue;
        }
        if (msg.reply[0] == '\0' && subject_looks_like_reply(msg.subject)) {
            re_without_reply++;
            print_unsafe_reason(m->filename, &msg, "re-no-reply");
            free_fido_msg(&msg);
            continue;
        }

        if (msg.reply[0] != '\0') {
            if (find_skref(skrefs, msg.reply) > 0) {
                free_fido_msg(&msg);
                continue;
            }

            if (find_msgref(refs, msg.reply) != NULL) {
                deferred++;
                print_unsafe_reason(m->filename, &msg, "deferred");
            } else {
                orphan++;
                print_unsafe_reason(m->filename, &msg, "orphan");
            }
        }

        free_fido_msg(&msg);
    }

    printf("\n");
    printf("FTN diagnose summary\n");
    printf("--------------------\n");
    printf("Area:             %s\n", area);
    printf("Seen:             %ld .MSG file(s)\n", seen);
    printf("Indexed:          %ld MSGID value(s)\n", indexed);
    printf("Existing IDs:     %ld SklaffKOM MSGID value(s)\n", existing_indexed);
    printf("Duplicates:       %ld already imported\n", duplicates);
	printf("Would import:     %ld %s\n", planned,
    include_unsafe ? "including unsafe fallbacks" : "safely");
    printf("Missing MSGID:    %ld\n", missing_msgid);
    printf("Re without REPLY: %ld\n", re_without_reply);
    printf("Deferred replies: %ld\n", deferred);
    printf("Orphan replies:   %ld\n", orphan);
    printf("Failed:           %ld\n", failed);

    rc = failed ? -1 : 0;

cleanup:
    free_msgrefs(refs);
    free_skrefs(skrefs);
    free_msgitems(items);
    free_planrefs(plans);
    return rc;
}
static int
dump_one_import(const char *area, const struct ftn_conf_info *ce,
    const char *filename)
{
    struct skref *skrefs = NULL;
    struct msgref *refs = NULL;
    struct msgitem *items = NULL;
    struct planref *plans = NULL;
    char spooldir[PATH_MAX];
    long existing_indexed = 0;
    long seen = 0;
    long failed = 0;
    long indexed = 0;
    long top_level = 0;
    long reply_existing = 0;
    long reply_planned = 0;
    long reply_orphan = 0;
    long next_textnum;
    long planned = 0;
    int rc = -1;

    if (snprintf(spooldir, sizeof(spooldir), "%s/%s", FTN_SPOOL, area) >= (int)sizeof(spooldir)) {
        fprintf(stderr, "[ERROR] Spool path too long: %s/%s\n", FTN_SPOOL, area);
        return -1;
    }

    printf("\n");
    printf("FTN dump-import setup\n");
    printf("---------------------\n");
    printf("Area:        %s\n", area);
    printf("Spool:       %s\n", spooldir);
    printf("Conf name:   %s\n", ce->name);
    printf("Conf num:    %d\n", ce->num);
    printf("Conf type:   %d", ce->type);
    if (ce->type == FTN_CONF)
        printf(" (FTN_CONF)");
    printf("\n");
    printf("Last text:   %ld\n", ce->last_text);
    printf("Target file: %s\n", filename);
    printf("\n");

    if (ce->type != FTN_CONF) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce->name, ce->type);
        return -1;
    }

    if (scan_existing_skl_msgids(ce, &skrefs, &existing_indexed) != 0)
        goto cleanup;

    if (build_spool_index(spooldir, &refs, &items, &seen, &indexed, &failed) != 0)
        goto cleanup;

    next_textnum = ce->last_text + 1;

	if (build_import_plan(items, skrefs, next_textnum, 0, &plans, &planned,
        &next_textnum, &top_level, &reply_existing, &reply_planned,
        &reply_orphan) != 0)
        goto cleanup;

    printf("Dump plan summary\n");
    printf("-----------------\n");
    printf("Seen:          %ld .MSG file(s)\n", seen);
    printf("Indexed:       %ld MSGID value(s)\n", indexed);
    printf("Existing IDs:  %ld SklaffKOM MSGID value(s)\n", existing_indexed);
    printf("Failed:        %ld\n", failed);
    printf("Top-level:     %ld\n", top_level);
    printf("Reply existing:%ld\n", reply_existing);
    printf("Reply planned: %ld\n", reply_planned);
    printf("Orphan reply:  %ld\n", reply_orphan);
    printf("Planned:       %ld simulated import(s)\n", planned);
    printf("Next text no:  %ld\n", next_textnum);

    rc = dump_import_text(area, ce, filename, items, plans);

cleanup:
    free_msgrefs(refs);
    free_skrefs(skrefs);
    free_msgitems(items);
    free_planrefs(plans);

    return rc;
}

static char *
build_ftn_mbuf(const char *area, const struct fido_msg *msg,
    const char *unsafe_reason)
{
    char *mbuf;
    char *wrapped_body;
    const char *body;
    size_t need;

    if (area == NULL || msg == NULL)
        return NULL;

    /*
     * Store cleaned and wrapped FTN text in SklaffKOM.  The raw body may
     * contain FTN kludges, SEEN-BY/PATH lines and very long modern lines
     * that SklaffKOM's old display code does not handle nicely.
     *
     * modified on 2026-06-13, PL
     */
    body = msg->clean_body;
    if (body == NULL)
        body = msg->raw_body;
    if (body == NULL)
        body = "";

    wrapped_body = wrap_ftn_body_for_skom(body);
    if (wrapped_body == NULL)
        return NULL;

    need = 1024;
    need += strlen(area);
    need += strlen(msg->from);
    need += strlen(msg->to);
    need += strlen(msg->subject);
    need += strlen(msg->date);
    need += strlen(msg->msgid);
    need += strlen(msg->reply);
    need += strlen(msg->chrs);
    need += strlen(wrapped_body);

    if (unsafe_reason != NULL && *unsafe_reason != '\0')
        need += strlen("FTN-Unsafe: ") + strlen(unsafe_reason) + 1; /* modified on 2026-06-13, PL */

    mbuf = (char *)calloc(1, need);
    if (mbuf == NULL) {
        free(wrapped_body);
        return NULL;
    }

    snprintf(mbuf, need,
        "From: %s\n"
        "To: %s\n"
        "Subject: %s\n"
        "Date: %s\n"
        "FTN-Area: %s\n",
        msg->from,
        msg->to,
        msg->subject,
        msg->date,
        area);

    if (msg->msgid[0] != '\0') {
        strlcat(mbuf, "FTN-MSGID: ", need); /* modified on 2026-06-09, PL */
        strlcat(mbuf, msg->msgid, need);    /* modified on 2026-06-09, PL */
        strlcat(mbuf, "\n", need);          /* modified on 2026-06-09, PL */
    }

    if (msg->reply[0] != '\0') {
        strlcat(mbuf, "FTN-REPLY: ", need); /* modified on 2026-06-09, PL */
        strlcat(mbuf, msg->reply, need);    /* modified on 2026-06-09, PL */
        strlcat(mbuf, "\n", need);          /* modified on 2026-06-09, PL */
    }

    if (msg->chrs[0] != '\0') {
        strlcat(mbuf, "FTN-CHRS: ", need);  /* modified on 2026-06-09, PL */
        strlcat(mbuf, msg->chrs, need);     /* modified on 2026-06-09, PL */
        strlcat(mbuf, "\n", need);          /* modified on 2026-06-09, PL */
    }

    if (unsafe_reason != NULL && *unsafe_reason != '\0') {
        strlcat(mbuf, "FTN-Unsafe: ", need); /* modified on 2026-06-13, PL */
        strlcat(mbuf, unsafe_reason, need);  /* modified on 2026-06-13, PL */
        strlcat(mbuf, "\n", need);           /* modified on 2026-06-13, PL */
    }

    strlcat(mbuf, "\n", need);              /* modified on 2026-06-09, PL */
    strlcat(mbuf, wrapped_body, need);      /* modified on 2026-06-13, PL */

    free(wrapped_body);

    return mbuf;
}

static int
rewrite_conf_last_text(int confid, long *new_textnum)
{
    FILE *in;
    FILE *out;
    char tmpfile[PATH_MAX];
    LONG_LINE line;
    int found = 0;

    if (new_textnum == NULL)
        return -1;

    if (snprintf(tmpfile, sizeof(tmpfile), "%s.ftntoss.tmp", CONF_FILE) >= (int)sizeof(tmpfile)) {
        fprintf(stderr, "[ERROR] CONF_FILE temp path too long\n");
        return -1;
    }

    in = fopen(CONF_FILE, "r");
    if (in == NULL) {
        perror(CONF_FILE);
        return -1;
    }

    out = fopen(tmpfile, "w");
    if (out == NULL) {
        perror(tmpfile);
        fclose(in);
        return -1;
    }

    while (fgets(line, sizeof(line), in) != NULL) {
        struct ftn_conf_info ce;

        if (parse_conf_line(line, &ce) == 0 && ce.num == confid) {
            ce.last_text++;
            *new_textnum = ce.last_text;
            found = 1;

            fprintf(out, "%d:%ld:%d:%ld:%d:%d:%d:%s\n",
                ce.num,
                ce.last_text,
                ce.creator,
                ce.time,
                ce.type,
                ce.life,
                ce.comconf,
                ce.name);
        } else {
            fputs(line, out);
        }
    }

    if (fclose(in) != 0) {
        perror("fclose");
        fclose(out);
        unlink(tmpfile);
        return -1;
    }

    if (fclose(out) != 0) {
        perror("fclose");
        unlink(tmpfile);
        return -1;
    }

    if (!found) {
        fprintf(stderr, "[ERROR] Conference number %d not found in %s\n",
            confid, CONF_FILE);
        unlink(tmpfile);
        return -1;
    }

    if (rename(tmpfile, CONF_FILE) != 0) {
        perror("rename");
        unlink(tmpfile);
        return -1;
    }

    return 0;
}

static int
append_comment_link(int confid, long parent_text, long child_text)
{
    FILE *fp;
    char path[PATH_MAX];

    if (parent_text <= 0 || child_text <= 0)
        return 0;

    if (snprintf(path, sizeof(path), "%s/%d/%ld", SKLAFF_DB, confid, parent_text) >= (int)sizeof(path)) {
        fprintf(stderr, "[ERROR] Parent text path too long: %s/%d/%ld\n",
            SKLAFF_DB, confid, parent_text);
        return -1;
    }

    fp = fopen(path, "a");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    /*
     * Same comment-link style as send_news():
     *   child_text:0
     */
    if (fprintf(fp, "%ld:%d\n", child_text, 0) < 0) {
        perror("fprintf");
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0) {
        perror("fclose");
        return -1;
    }

    return 0;
}

static long
send_ftn(int confid, const char *area, const struct fido_msg *msg, long com,
    const char *unsafe_reason)
{
    char path[PATH_MAX];
    char *mbuf = NULL;
    FILE *fp = NULL;
    long new_textnum = 0;
    long size = 0;
    time_t now;

    if (area == NULL || msg == NULL)
        return -1;

    /*
     * This mirrors send_news():
     *   - size is counted from the imported message buffer
     *   - timestamp is import time for now
     */
	mbuf = build_ftn_mbuf(area, msg, unsafe_reason);
    if (mbuf == NULL) {
        fprintf(stderr, "[ERROR] Could not build FTN message buffer\n");
        return -1;
    }

    size = count_body_lines(mbuf);
    now = time(NULL);

    if (rewrite_conf_last_text(confid, &new_textnum) != 0) {
        free(mbuf);
        return -1;
    }

    if (snprintf(path, sizeof(path), "%s/%d/%ld", SKLAFF_DB, confid, new_textnum) >= (int)sizeof(path)) {
        fprintf(stderr, "[ERROR] Text path too long: %s/%d/%ld\n",
            SKLAFF_DB, confid, new_textnum);
        free(mbuf);
        return -1;
    }

    fp = fopen(path, "w");
    if (fp == NULL) {
        perror(path);
        free(mbuf);
        return -1;
    }

    /*
     * Same basic header shape as send_news():
     *   textno:0:timestamp:com:0:0:size
     */
    if (fprintf(fp, "%ld:%d:%ld:%ld:%d:%d:%ld\n",
            new_textnum, 0, (long)now, com, 0, 0, size) < 0) {
        perror("fprintf");
        fclose(fp);
        free(mbuf);
        return -1;
    }

    /*
     * SklaffKOM stores the subject on the line after the metadata header.
     */
    if (fprintf(fp, "%s\n", msg->subject) < 0) {
        perror("fprintf");
        fclose(fp);
        free(mbuf);
        return -1;
    }

    if (fputs(mbuf, fp) == EOF) {
        perror("fputs");
        fclose(fp);
        free(mbuf);
        return -1;
    }

    if (fclose(fp) != 0) {
        perror("fclose");
        free(mbuf);
        return -1;
    }

    free(mbuf);

    if (com > 0) {
        if (append_comment_link(confid, com, new_textnum) != 0)
            return -1;
    }

    printf("Imported FTN message as SklaffKOM text %ld", new_textnum);
    if (com > 0)
        printf(" (comment to %ld)", com);
    printf("\n");

    return new_textnum;
}

static int
scan_ftn_area(const char *area, const struct ftn_conf_info *ce)
{
    struct skref *skrefs = NULL;
    struct msgref *refs = NULL;
    struct msgitem *items = NULL;
    struct msgitem *m;
    struct planref *plans = NULL;
    char spooldir[PATH_MAX];
    long existing_indexed = 0;
    long seen = 0;
    long parsed = 0;
    long failed = 0;
    long indexed = 0;
    long top_level = 0;
    long reply_existing = 0;
    long reply_planned = 0;
    long reply_orphan = 0;
    long next_textnum;
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

    if (build_spool_index(spooldir, &refs, &items, &seen, &indexed, &failed) != 0) {
        free_skrefs(skrefs);
        free_msgrefs(refs);
        free_msgitems(items);
        return -1;
    }

    next_textnum = ce->last_text + 1;

	if (build_import_plan(items, skrefs, next_textnum, 0, &plans, &planned,
        &next_textnum, &top_level, &reply_existing, &reply_planned,
        &reply_orphan) != 0) {
        free_skrefs(skrefs);
        free_msgrefs(refs);
        free_msgitems(items);
        free_planrefs(plans);
        return -1;
    }

    printf("Scanning for .MSG files...\n\n");

    for (m = items; m != NULL; m = m->next) {
        struct fido_msg msg;
        const struct planref *plan;
        const char *parent_file;

        printf("============================================================\n");
        printf("File:    %s\n", m->path);

        if (read_fido_msg(m->path, &msg) != 0) {
            printf("[ERROR] Could not parse .MSG file\n");
            failed++;
            continue;
        }

        parsed++;
        plan = find_planref_by_filename(plans, m->filename);

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

            if (plan != NULL && plan->parent_textnum > 0) {
                if (find_skref(skrefs, msg.reply) > 0)
                    printf("Thread:  reply to existing SklaffKOM text %ld\n", plan->parent_textnum);
                else
                    printf("Thread:  reply to newly planned SklaffKOM text %ld\n", plan->parent_textnum);

                printf("Plan:    would import %s as SklaffKOM text %ld, comment to text %ld\n",
                    m->filename, plan->planned_textnum, plan->parent_textnum);
            } else if (plan != NULL && plan->orphan) {
                parent_file = find_msgref(refs, msg.reply);
                if (parent_file != NULL)
                    printf("Thread:  unresolved reply; parent exists in current spool as %s but could not be planned\n",
                        parent_file);
                else
                    printf("Thread:  orphan reply, parent not found in SklaffKOM or current spool\n");

                printf("Plan:    would import %s as SklaffKOM text %ld as top-level for now\n",
                    m->filename, plan->planned_textnum);
            } else if (plan != NULL) {
                printf("Thread:  new top-level text\n");
                printf("Plan:    would import %s as SklaffKOM text %ld as top-level\n",
                    m->filename, plan->planned_textnum);
            } else {
                printf("Thread:  no plan found\n");
                printf("Plan:    would skip %s for now\n", m->filename);
            }
        } else {
            printf("REPLY:   (missing)\n");
            printf("Thread:  new top-level text\n");
            if (plan != NULL)
                printf("Plan:    would import %s as SklaffKOM text %ld as top-level\n",
                    m->filename, plan->planned_textnum);
            else
                printf("Plan:    would skip %s for now\n", m->filename);
        }

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
    free_msgitems(items);
    free_planrefs(plans);

    return failed ? -1 : 0;
}

static int
run_import_one_locked(void *arg)
{
    struct import_one_args *a = arg;

    if (a == NULL)
        return -1;

    return import_one_ftn(a->area, a->filename);
}

static int
run_import_area_locked(void *arg)
{
    struct import_area_args *a = arg;

    if (a == NULL)
        return -1;

    return import_all_ftn(a->area, a->include_unsafe);
}

static int
run_import_all_areas_locked(void *arg)
{
    struct import_all_areas_args *a = arg;

    if (a == NULL)
        return -1;

    return import_all_areas_ftn(a->include_unsafe);
}

int
main(int argc, char **argv)
{
    struct ftn_conf_info ce;
    
    if (argc == 3 && strcmp(argv[1], "--export-test") == 0) {
    return export_test_ftn(argv[2]) == 0 ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "--dump-import") == 0) {
        printf("ftntoss dump-import dry-run starting\n");
        printf("====================================\n\n");

        if (find_ftn_conf(argv[2], &ce) != 0)
            return 1;

        if (dump_one_import(argv[2], &ce, argv[3]) != 0)
            return 1;

        printf("\nftntoss dump-import dry-run done\n");
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "--import-one") == 0) {
    	struct import_one_args a;

    	a.area = argv[2];
    	a.filename = argv[3];

    	return run_with_lock(run_import_one_locked, &a) == 0 ? 0 : 1;
	}
	
    if (argc == 3 && strcmp(argv[1], "--import-all") == 0) {
    	struct import_area_args a;

   	 	a.area = argv[2];
    	a.include_unsafe = 0;

    	return run_with_lock(run_import_area_locked, &a) == 0 ? 0 : 1;
	}
    
    if (argc == 4 && strcmp(argv[1], "--import-all") == 0 &&
        	strcmp(argv[3], "--include-unsafe") == 0) {
    	struct import_area_args a;

    	a.area = argv[2];
    	a.include_unsafe = 1;

    return run_with_lock(run_import_area_locked, &a) == 0 ? 0 : 1;
	}

	if (argc == 3 && strcmp(argv[1], "--diagnose") == 0) {
    	return diagnose_ftn(argv[2], 0) == 0 ? 0 : 1;
	}

	if (argc == 4 && strcmp(argv[1], "--diagnose") == 0 &&
        strcmp(argv[3], "--include-unsafe") == 0) {
    return diagnose_ftn(argv[2], 1) == 0 ? 0 : 1;
	}

	if (argc == 2 && strcmp(argv[1], "--import-all-areas") == 0) {
    	struct import_all_areas_args a;

    	a.include_unsafe = 0;

    return run_with_lock(run_import_all_areas_locked, &a) == 0 ? 0 : 1;
	}

	if (argc == 3 && strcmp(argv[1], "--import-all-areas") == 0 &&
        	strcmp(argv[2], "--include-unsafe") == 0) {
    	struct import_all_areas_args a;

    	a.include_unsafe = 1;

    	return run_with_lock(run_import_all_areas_locked, &a) == 0 ? 0 : 1;
	}

    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s <FTN-area / SklaffKOM conference>\n", argv[0]);
        fprintf(stderr, "       %s --dump-import <FTN-area / SklaffKOM conference> <file.msg>\n", argv[0]);
        fprintf(stderr, "       %s --import-one <FTN-area> <file.msg>\n", argv[0]);
        fprintf(stderr, "       %s --import-all <FTN-area>\n", argv[0]);
        fprintf(stderr, "       %s --import-all <FTN-area> --include-unsafe\n", argv[0]);
        fprintf(stderr, "       %s --diagnose <FTN-area>\n", argv[0]);
        fprintf(stderr, "       %s --diagnose <FTN-area> --include-unsafe\n", argv[0]);
        fprintf(stderr, "       %s --import-all-areas\n", argv[0]);
        fprintf(stderr, "       %s --import-all-areas --include-unsafe\n", argv[0]);
        fprintf(stderr, "       %s --export-test <FTN-area>\n\n", argv[0]);
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s FSX_GEN\n", argv[0]);
        fprintf(stderr, "  %s --dump-import FSX_BBS 32.msg\n\n", argv[0]);
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

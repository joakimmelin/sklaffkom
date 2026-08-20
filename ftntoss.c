/* ftntoss.c */

#include "sklaff.h"
#include "ftnmsg.h"
#include "ftnnetmail.h" /* modified on 2026-07-09, PL */
#include "ext_globals.h"
#include "ftnconfig.h" /* modified on 2026-07-14, PL */

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
#include <sys/types.h>
#include <sys/stat.h> /* modified on 2026-07-09, PL */
#include <pwd.h> /* modified on 2026-06-14, PL */
#include <grp.h> /* modified on 2026-07-09, PL */

#define FTNTOSS_LOCKFILE "/tmp/ftntoss.lock" /* modified on 2026-06-11, PL */
#define FTN_WRAP_COL 78 /* modified on 2026-06-13, PL */

/*
 * FTN addresses and messagebase paths are read from crashmail.prefs.
 * No network-specific AKA, hub or spool path belongs in ftntoss.c.
 *
 * modified on 2026-07-16, PL
 */
#define FTN_LOCAL_ATTR 0x0100 /* modified on 2026-06-12, PL */

#define FTN_PRIVATE_ATTR 0x0001 /* modified on 2026-07-11, PL */
#define FTN_NETMAIL_ATTR (FTN_PRIVATE_ATTR | FTN_LOCAL_ATTR) /* modified on 2026-07-11, PL */

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

struct meeting_ftn_config {
    int version;
    char type[32];
    char domain[FTN_DOMAIN_LEN];
    char tag[FTN_TAG_LEN];
}; /* modified on 2026-07-15, PL */

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

struct export_one_args {
    const char *area;
    long textnum;
}; /* modified on 2026-06-14, PL */

struct export_one_conf_args {
    int confnum;
    long textnum;
}; /* modified on 2026-08-11, PL */

struct import_netmail_args {
    const char *prefsfile;
}; /* modified on 2026-07-16, PL */

struct export_netmail_job_args {
    const char *jobfile;
}; /* modified on 2026-07-11, PL */

struct export_ibol_job_args {
    const char *jobfile;
}; /* modified on 2026-08-13, PL */

struct netmail_job {
    int fromuid;
    char toname[128];
    char toaddr[64];
    char subject[128];
    char reply[128];
    long created;
    char *body;
}; /* modified on 2026-07-11, PL */

struct ibol_job {
    int fromuid;
    char author[128];
    long created;
    char text[IBOL_ONELINER_MAX + 1];
}; /* modified on 2026-08-13, PL */

struct netmail_destination {
    struct ftn_address address;
    char domain[FTN_DOMAIN_LEN];
    int has_domain;
}; /* modified on 2026-07-16, PL */

static int run_export_netmail_job_locked(void *arg); /* modified on 2026-07-11, PL */
static int export_netmail_job(const char *jobfile); /* modified on 2026-07-11, PL */
static int read_netmail_job(const char *path, struct netmail_job *job); /* modified on 2026-07-11, PL */
static int run_export_ibol_job_locked(void *arg); /* modified on 2026-08-13, PL */
static int export_ibol_job(const char *jobfile); /* modified on 2026-08-13, PL */
static int read_ibol_job(const char *path, struct ibol_job *job); /* modified on 2026-08-13, PL */
static void free_netmail_job(struct netmail_job *job); /* modified on 2026-07-11, PL */
static int parse_ftn_addr_5d(const char *addr,
    struct netmail_destination *destination); /* modified on 2026-07-16, PL */
static int load_netmail_area_for_destination(
    const struct netmail_destination *destination,
    struct ftn_config *config, const struct ftn_area **out_area); /* modified on 2026-07-16, PL */
static int import_all_netmail_spools(const char *prefsfile); /* modified on 2026-07-16, PL */
static int write_fido_netmail_out(const char *path,
    const struct netmail_job *job, const char *from,
    const struct ftn_address *orig, const struct ftn_address *dest,
    const char *msgid); /* modified on 2026-07-16, PL */

static int export_test_ftn(const char *area);

static int export_one_ftn(const char *area, long textnum); /* modified on 2026-06-14, PL */
static int export_one_ftn_conf(int confnum, long textnum); /* modified on 2026-08-11, PL */
static unsigned long make_export_one_serial(int confnum, long textnum); /* modified on 2026-06-14, PL */
static int read_skom_export_text(int confnum, long textnum,
    long *out_uid, long *out_time, long *out_com,
    char *subject, size_t subjectsz, char **out_body); /* modified on 2026-06-14, PL */
static int read_skom_ftn_msgid(int confnum, long textnum,
    char *out, size_t outsz); /* modified on 2026-06-14, PL */
static void strip_eol(char *s); /* modified on 2026-06-14, PL */

static int next_msg_path(const char *spooldir, char *out, size_t outsz,
    long *out_num);
static int write_fido_msg_out(const char *path,
    const struct ftn_area *ftn_area, const struct ftn_link *feed,
    const char *from, const char *to, const char *subject,
    const char *body, const char *reply, const char *msgid);
static void write_fixed_field(unsigned char *dst, size_t len, const char *src);
static void write_u16_le(unsigned char *dst, unsigned int val);
static void make_fido_date(char *out, size_t outsz);
static int parse_fido_msg_date(const char *s, time_t *out); /* modified on 2026-06-15, PL */
static void make_ftn_msgid_for_aka(char *out, size_t outsz,
    const struct ftn_address *aka, unsigned long serial);
static unsigned long make_export_test_serial(const char *area, long msgnum);
static int load_meeting_ftnconf(const struct ftn_conf_info *ce,
    struct meeting_ftn_config *meeting, int *out_found);
static int load_echomail_area_config(const char *fallback_tag,
    const struct ftn_conf_info *ce, struct ftn_config *config,
    const struct ftn_area **out_area); /* modified on 2026-07-15, PL */
static int load_echomail_area(const char *fallback_tag,
    const struct ftn_conf_info *ce, struct ftn_config *config,
    const struct ftn_area **out_area,
    const struct ftn_link **out_feed); /* modified on 2026-07-15, PL */
static int load_echomail_area_by_identity(const char *domain,
    const char *tag, struct ftn_config *config,
    const struct ftn_area **out_area,
    const struct ftn_link **out_feed); /* modified on 2026-08-13, PL */

static int find_ftn_conf(const char *name, struct ftn_conf_info *out_ce);
static int find_ftn_conf_num(int confnum, struct ftn_conf_info *out_ce); /* modified on 2026-08-11, PL */
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

static long msgitem_filename_number(const char *filename); /* modified on 2026-06-15, PL */
static struct msgitem *sort_msgitems_by_number(struct msgitem *list); /* modified on 2026-06-15, PL */

static int scan_existing_skl_msgids(const struct ftn_conf_info *ce,
    const struct ftn_address *aka, struct skref **out_refs,
    long *out_indexed);
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
static int run_import_netmail_locked(void *arg); /* modified on 2026-07-09, PL */

static int run_export_one_locked(void *arg); /* modified on 2026-06-14, PL */
static int run_export_one_conf_locked(void *arg); /* modified on 2026-08-11, PL */
static void make_skom_from_name(long uid, char *out, size_t outsz); /* modified on 2026-06-14, PL */

static char *wrap_ftn_body_for_skom(const char *body);
static void append_wrapped_segment(char *out, size_t outsz,
    const char *seg, size_t len, const char *cont_prefix); /* modified on 2026-06-17, PL */
static void ftn_quote_prefix(const char *line, size_t len,
    char *out, size_t outsz); /* modified on 2026-06-17, PL */
static void append_to_dynbuf(char **buf, size_t *cap, size_t *len,
    const char *text);

static void sklaff_version_no_build(char *out, size_t outsz);

static void ftntoss_notify_all_processes(int sig); /* modified on 2026-07-05, PL */

static int ftntoss_get_sklaff_ids(uid_t *uid, gid_t *gid); /* modified on 2026-07-09, PL */
static int ftntoss_fix_fd_to_sklaff(FILE *fp, const char *path, mode_t mode); /* modified on 2026-07-09, PL */
static int ftntoss_fix_fd_like_stat(FILE *fp, const char *path, const struct stat *st); /* modified on 2026-07-09, PL */
static int ftntoss_fix_control_file(const char *path, mode_t mode); /* modified on 2026-07-09, PL */

static int dump_ftn_config_file(const char *path); /* modified on 2026-07-14, PL */

static void
sklaff_version_no_build(char *out, size_t outsz)
{
    char *p;

    if (out == NULL || outsz == 0)
        return;

    strlcpy(out, sklaff_version, outsz);

    /*
     * FTN tearline should show the release version, not the local build number.
     *
     * modified on 2026-06-23, PL
     */
    p = strstr(out, "(#");
    if (p != NULL)
        *p = '\0';
}

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
    if (n >= len) {
        n = len - 1;

        /*
         * Header fields are byte-sized in the classic .MSG format.  When
         * truncating UTF-8, never leave half of a multibyte character at
         * the end of the field.
         *
         * modified on 2026-08-10, PL
         */
        while (n > 0 &&
            (((unsigned char)src[n] & 0xc0) == 0x80))
            n--;
    }

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
make_tzutc(char *out, size_t outsz)
{
    time_t now;
    struct tm *tm;

    if (out == NULL || outsz == 0)
        return;

    strlcpy(out, "+0000", outsz);

    now = time(NULL);
    tm = localtime(&now);
    if (tm == NULL)
        return;

    /*
     * FTN TZUTC kludge: local UTC offset as +/-HHMM.
     *
     * modified on 2026-07-12, PL
     */
    if (strftime(out, outsz, "%z", tm) == 0)
        strlcpy(out, "+0000", outsz);
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

static int
parse_fido_msg_date(const char *s, time_t *out)
{
    struct tm tm;
    char mon[4];
    int day;
    int year;
    int hour;
    int min;
    int sec;
    int month = -1;
    int i;
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    if (s == NULL || out == NULL)
        return -1;

    memset(&tm, 0, sizeof(tm));
    memset(mon, 0, sizeof(mon));

    /*
     * Fido .MSG dates normally look like:
     *
     *   14 Jun 26  21:55:49
     *
     * Use the message timestamp for imported SklaffKOM texts, falling back
     * to import time if parsing fails.
     *
     * modified on 2026-06-15, PL
     */
    if (sscanf(s, "%d %3s %d %d:%d:%d",
            &day, mon, &year, &hour, &min, &sec) != 6)
        return -1;

    for (i = 0; i < 12; i++) {
        if (strcasecmp(mon, months[i]) == 0) {
            month = i;
            break;
        }
    }

    if (month < 0)
        return -1;

    if (year < 70)
        year += 2000;
    else if (year < 100)
        year += 1900;

    if (day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        min < 0 || min > 59 ||
        sec < 0 || sec > 60)
        return -1;

    tm.tm_mday = day;
    tm.tm_mon = month;
    tm.tm_year = year - 1900;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;

    *out = mktime(&tm);
    if (*out == (time_t)-1)
        return -1;

    return 0;
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
make_ftn_msgid_for_aka(char *out, size_t outsz,
    const struct ftn_address *aka, unsigned long serial)
{
    char addr[64];

    if (out == NULL || outsz == 0)
        return;

    out[0] = '\0';

    if (aka == NULL)
        return;

    make_ftn_addr(addr, sizeof(addr), aka->zone, aka->net,
        aka->node, aka->point);

    /*
     * FTS-0009 MSGID format is:
     *
     *   ^AMSGID: <origin-address> <8-hex-digit-serial>
     *
     * Keep the serial field at exactly eight hexadecimal digits.
     *
     * modified on 2026-07-15, PL
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

static unsigned long
make_export_one_serial(int confnum, long textnum)
{
    /*
     * Deterministic real-export serial:
     * high 16 bits = local SklaffKOM conference number
     * low 16 bits  = local SklaffKOM text number
     *
     * Example: conf 48, text 123 -> 0030007b
     *
     * modified on 2026-06-14, PL
     */
    return (((unsigned long)confnum & 0xffffUL) << 16) |
        ((unsigned long)textnum & 0xffffUL);
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

/*
 * ftn_quote_prefix - extract quote prefix for FTN-style quoted lines
 * args: source line (line), line length (len), output buffer (out), output size
 * ret: none
 *
 * Recognizes:
 *   > quoted
 *   >> quoted
 *   > > quoted
 *   V> quoted
 *   Ni>> quoted
 *
 * The returned prefix includes trailing whitespace if present, so wrapped
 * continuation lines keep the same readable quote marker.
 *
 * modified on 2026-06-17, PL
 */
static void
ftn_quote_prefix(const char *line, size_t len, char *out, size_t outsz)
{
    size_t i = 0;
    size_t start;
    int initials = 0;
    int depth = 0;

    if (out == NULL || outsz == 0)
        return;

    out[0] = '\0';

    if (line == NULL || len == 0)
        return;

    /*
     * Allow leading whitespace as part of the prefix.
     * modified on 2026-06-17, PL
     */
    while (i < len && (line[i] == ' ' || line[i] == '\t'))
        i++;

    start = 0;

    /*
     * Classic quote: >, >>, > >, etc.
     * modified on 2026-06-17, PL
     */
    if (i < len && line[i] == '>') {
        while (i < len && line[i] == '>') {
            i++;
            depth++;

            if (i < len && line[i] == ' ')
                i++;
        }

        if (depth > 0) {
            if (i < len && (line[i] == ' ' || line[i] == '\t'))
                i++;

            if (i > start) {
                size_t n = i - start;

                if (n >= outsz)
                    n = outsz - 1;

                memcpy(out, line + start, n);
                out[n] = '\0';
            }
        }

        return;
    }

    /*
     * FTN initials quote: A>, AB>, Ni>>, etc.
     * Be conservative: 1-3 alphabetic initials, then one or more '>'.
     *
     * modified on 2026-06-17, PL
     */
    while (i < len && isalpha((unsigned char)line[i]) && initials < 3) {
        i++;
        initials++;
    }

    if (initials == 0)
        return;

    while (i < len && line[i] == '>') {
        i++;
        depth++;
    }

    if (depth == 0)
        return;

    /*
     * Require whitespace/end after the quote marker to avoid matching
     * normal strings like "foo>bar".
     *
     * modified on 2026-06-17, PL
     */
    if (i < len &&
        line[i] != ' ' && line[i] != '\t' &&
        line[i] != '\r' && line[i] != '\n')
        return;

    if (i < len && (line[i] == ' ' || line[i] == '\t'))
        i++;

    if (i > start) {
        size_t n = i - start;

        if (n >= outsz)
            n = outsz - 1;

        memcpy(out, line + start, n);
        out[n] = '\0';
    }
}

static void
append_wrapped_segment(char *out, size_t outsz,
    const char *seg, size_t len, const char *cont_prefix)
{
    size_t pos = 0;
    int first = 1;
    size_t prefix_len = 0;

    if (out == NULL || outsz == 0 || seg == NULL)
        return;

    out[0] = '\0';

    if (cont_prefix != NULL)
        prefix_len = strlen(cont_prefix);

    while (pos < len) {
        size_t left = len - pos;
        size_t take = left;
        size_t i;
        size_t start;
        size_t wrap_col = FTN_WRAP_COL;

        while (left > 0 && (seg[pos] == ' ' || seg[pos] == '\t')) {
            pos++;
            left--;
        }

        if (left == 0)
            break;

        /*
         * Continuation lines include the quote prefix, so leave room for it
         * when choosing the wrap point.
         *
         * modified on 2026-06-17, PL
         */
        if (!first && prefix_len > 0 && prefix_len < wrap_col)
            wrap_col -= prefix_len;

        take = left;
        if (take > wrap_col) {
            take = wrap_col;

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
                take = wrap_col;
        }

        start = strlen(out);

        /*
         * Prefix continuation lines of quoted FTN text:
         *
         *   V> long text...
         *   V> continuation...
         *
         * modified on 2026-06-17, PL
         */
        if (!first && prefix_len > 0) {
            if (start + prefix_len + 1 >= outsz)
                break;

            memcpy(out + start, cont_prefix, prefix_len);
            start += prefix_len;
            out[start] = '\0';
        }

        if (start + take + 2 >= outsz)
            break;

        memcpy(out + start, seg + pos, take);
        out[start + take] = '\n';
        out[start + take + 1] = '\0';

        pos += take;
        first = 0;

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
			
			char quote_prefix[32];

			ftn_quote_prefix(line_start, linelen, quote_prefix, sizeof(quote_prefix)); /* modified on 2026-06-17, PL */
            
			/*
             * Preserve blank lines. Preserve quoted/origin/control-ish lines
             * as-is, but wrap normal prose before storing it in SklaffKOM.
             *
             * modified on 2026-06-13, PL
             */
            if (linelen == 0) {
                append_to_dynbuf(&out, &cap, &outlen, "\n");
           	} else if (quote_prefix[0] != '\0' && linelen > FTN_WRAP_COL) {
    			char wrapped[4096];

	    /*
	     * Long FTN quote lines should wrap with the same quote prefix on
	     * continuation lines, otherwise SklaffKOM cannot color them as quotes.
	     *
	     * modified on 2026-06-17, PL
	     */

				append_wrapped_segment(wrapped, sizeof(wrapped),
			    line_start, linelen, quote_prefix); /* modified on 2026-06-17, PL */
    			append_to_dynbuf(&out, &cap, &outlen, wrapped);
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
				    line_start, linelen, NULL); /* modified on 2026-06-17, PL */
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
next_msg_path(const char *spooldir, char *out, size_t outsz, long *out_num)
{
    DIR *dir;
    struct dirent *de;
    long maxnum = 1;

    if (spooldir == NULL || *spooldir == '\0' ||
        out == NULL || outsz == 0)
        return -1;

    dir = opendir(spooldir);
    if (dir == NULL) {
        perror(spooldir);
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

    if (snprintf(out, outsz, "%s/%ld.msg", spooldir, maxnum) >=
            (int)outsz) {
        fprintf(stderr, "[ERROR] Output .MSG path too long\n");
        return -1;
    }

    if (out_num != NULL)
        *out_num = maxnum;

    return 0;
}

static int
write_fido_msg_out(const char *path, const struct ftn_area *ftn_area,
    const struct ftn_link *feed, const char *from, const char *to,
    const char *subject, const char *body, const char *reply,
    const char *msgid)
{
    FILE *fp;
    unsigned char hdr[190];
    char datebuf[32];
    char version[64];
    char tzbuf[16];
    const struct ftn_address *orig;
    const struct ftn_address *dest;

    if (path == NULL || ftn_area == NULL || feed == NULL ||
            from == NULL || to == NULL || subject == NULL || body == NULL ||
            msgid == NULL || *msgid == '\0')
        return -1;

    orig = &ftn_area->aka;
    dest = &feed->address;

    memset(hdr, 0, sizeof(hdr));

    make_fido_date(datebuf, sizeof(datebuf));
    make_tzutc(tzbuf, sizeof(tzbuf));

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

    write_u16_le(hdr + 164, 0);              /* times read */
    write_u16_le(hdr + 166, dest->node);     /* dest node */
    write_u16_le(hdr + 168, orig->node);     /* orig node */
    write_u16_le(hdr + 170, 0);              /* cost */
    write_u16_le(hdr + 172, orig->net);      /* orig net */
    write_u16_le(hdr + 174, dest->net);      /* dest net */
    write_u16_le(hdr + 176, dest->zone);     /* dest zone */
    write_u16_le(hdr + 178, orig->zone);     /* orig zone */
    write_u16_le(hdr + 180, dest->point);    /* dest point */
    write_u16_le(hdr + 182, orig->point);    /* orig point */
    write_u16_le(hdr + 184, 0);              /* reply to */
    write_u16_le(hdr + 186, FTN_LOCAL_ATTR); /* attr: local */
    write_u16_le(hdr + 188, 0);              /* next reply */

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
    fprintf(fp, "\001TZUTC: %s\r", tzbuf);
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

		sklaff_version_no_build(version, sizeof(version));

        make_ftn_addr(origin_addr, sizeof(origin_addr),
            orig->zone, orig->net, orig->node, orig->point);
        fprintf(fp, "\r--- SklaffKOM v%s\r", version);
		fprintf(fp, " * Origin: %s, %s (%s)\r",
    		SKLAFF_ID, SKLAFF_LOC, origin_addr);
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

static int
parse_ftn_addr_5d(const char *addr, struct netmail_destination *destination)
{
    const char *p;
    const char *domain;
    char *end;
    long z;
    long n;
    long nd;
    long pt;
    size_t domain_len;

    if (addr == NULL || destination == NULL)
        return -1;

    memset(destination, 0, sizeof(*destination));
    p = addr;

    z = strtol(p, &end, 10);
    if (p == end || *end != ':' || z < 0 || z > 65535)
        return -1;

    p = end + 1;
    n = strtol(p, &end, 10);
    if (p == end || *end != '/' || n < 0 || n > 65535)
        return -1;

    p = end + 1;
    nd = strtol(p, &end, 10);
    if (p == end || nd < 0 || nd > 65535)
        return -1;

    pt = 0;
    if (*end == '.') {
        p = end + 1;
        pt = strtol(p, &end, 10);
        if (p == end || pt < 0 || pt > 65535)
            return -1;
    }

    if (*end == '@') {
        domain = end + 1;
        domain_len = strlen(domain);

        if (domain_len == 0 || domain_len >= sizeof(destination->domain))
            return -1;

        for (p = domain; *p != '\0'; p++) {
            unsigned char ch;

            ch = (unsigned char)*p;
            if (!isalnum(ch) && ch != '-' && ch != '_' && ch != '.')
                return -1;
        }

        strlcpy(destination->domain, domain,
            sizeof(destination->domain));
        destination->has_domain = 1;
    } else if (*end != '\0') {
        return -1;
    }

    destination->address.zone = (int)z;
    destination->address.net = (int)n;
    destination->address.node = (int)nd;
    destination->address.point = (int)pt;

    return 0;
}

/*
 * load_netmail_area_for_destination - select outgoing netmail context
 * args: parsed destination, loaded-config output, selected-area output
 * ret: success (0) or error (-1)
 *
 * A 5D address selects its CrashMail NETMAIL area by domain.  A legacy 4D
 * address is accepted when the destination zone maps to exactly one NETMAIL
 * domain, or when only one NETMAIL area exists at all.
 *
 * modified on 2026-07-16, PL
 */
static int
load_netmail_area_for_destination(
    const struct netmail_destination *destination,
    struct ftn_config *config, const struct ftn_area **out_area)
{
    const struct ftn_area *found;
    const struct ftn_area *only_area;
    char error[512];
    size_t i;
    size_t total;
    size_t matches;

    if (destination == NULL || config == NULL || out_area == NULL)
        return -1;

    *out_area = NULL;
    found = NULL;
    only_area = NULL;
    total = 0;
    matches = 0;

    ftn_config_init(config);

    if (ftn_config_load_crashmail(CRASHMAIL_PREFS_FILE, config,
            error, sizeof(error)) != 0) {
        fprintf(stderr, "[ERROR] ftntoss: %s\n", error);
        return -1;
    }

    for (i = 0; i < config->area_count; i++) {
        const struct ftn_area *candidate;

        candidate = &config->areas[i];

        if (candidate->type != FTN_AREA_NETMAIL)
            continue;

        total++;
        only_area = candidate;

        if (destination->has_domain) {
            if (strcasecmp(candidate->domain, destination->domain) != 0)
                continue;
        } else {
            if (candidate->aka.zone != destination->address.zone)
                continue;
        }

        found = candidate;
        matches++;
    }

    if (!destination->has_domain && matches == 0 && total == 1) {
        found = only_area;
        matches = 1;
    }

    if (total == 0) {
        fprintf(stderr,
            "[ERROR] ftntoss: no NETMAIL areas were found in %s\n",
            CRASHMAIL_PREFS_FILE);
        ftn_config_free(config);
        return -1;
    }

    if (matches == 0) {
        if (destination->has_domain) {
            fprintf(stderr,
                "[ERROR] ftntoss: no NETMAIL area for domain '%s' in %s\n",
                destination->domain, CRASHMAIL_PREFS_FILE);
        } else {
            fprintf(stderr,
                "[ERROR] ftntoss: 4D destination zone %d does not identify "
                "a unique NETMAIL domain\n",
                destination->address.zone);
            fprintf(stderr,
                "[ERROR] ftntoss: use a 5D address such as "
                "%d:%d/%d@domain\n",
                destination->address.zone,
                destination->address.net,
                destination->address.node);
        }

        ftn_config_free(config);
        return -1;
    }

    if (matches > 1) {
        if (destination->has_domain) {
            fprintf(stderr,
                "[ERROR] ftntoss: more than one NETMAIL area uses domain "
                "'%s' in %s\n",
                destination->domain, CRASHMAIL_PREFS_FILE);
        } else {
            fprintf(stderr,
                "[ERROR] ftntoss: destination zone %d matches more than one "
                "NETMAIL area\n",
                destination->address.zone);
            fprintf(stderr,
                "[ERROR] ftntoss: use a full 5D destination address with "
                "@domain\n");
        }

        ftn_config_free(config);
        return -1;
    }

    if (found->messagebase[0] == '\0' ||
        strcasecmp(found->messagebase, "MSG") != 0) {
        fprintf(stderr,
            "[ERROR] ftntoss: NETMAIL area '%s' uses unsupported "
            "messagebase '%s'; only MSG is currently supported\n",
            found->tag,
            found->messagebase[0] ? found->messagebase : "(missing)");
        ftn_config_free(config);
        return -1;
    }

    if (found->path[0] == '\0') {
        fprintf(stderr,
            "[ERROR] ftntoss: NETMAIL area '%s' has no messagebase path "
            "in %s\n",
            found->tag, CRASHMAIL_PREFS_FILE);
        ftn_config_free(config);
        return -1;
    }

    if (found->domain[0] == '\0') {
        fprintf(stderr,
            "[ERROR] ftntoss: NETMAIL area '%s' has no resolved domain "
            "in %s\n",
            found->tag, CRASHMAIL_PREFS_FILE);
        ftn_config_free(config);
        return -1;
    }

    *out_area = found;
    return 0;
}

static void
free_netmail_job(struct netmail_job *job)
{
    if (job == NULL)
        return;

    free(job->body);
    job->body = NULL;
}

static int
read_netmail_job(const char *path, struct netmail_job *job)
{
    FILE *fp;
    char line[1024];
    int in_body;
    size_t cap, len;
    char *body;

    if (path == NULL || job == NULL)
        return -1;

    memset(job, 0, sizeof(*job));

    fp = fopen(path, "r");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    in_body = 0;
    cap = 0;
    len = 0;
    body = NULL;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (!in_body) {
            strip_eol(line);

            if (line[0] == '\0') {
                in_body = 1;
                continue;
            }

            if (strncmp(line, "TYPE:", 5) == 0) {
                char *v = line + 5;
                while (*v == ' ' || *v == '\t')
                    v++;
                if (strcmp(v, "NETMAIL") != 0) {
                    fclose(fp);
                    free(body);
                    return -1;
                }
            } else if (strncmp(line, "FROMUID:", 8) == 0) {
                char *v = line + 8;
                while (*v == ' ' || *v == '\t')
                    v++;
                job->fromuid = atoi(v);
            } else if (strncmp(line, "TONAME:", 7) == 0) {
                char *v = line + 7;
                while (*v == ' ' || *v == '\t')
                    v++;
                strlcpy(job->toname, v, sizeof(job->toname));
            } else if (strncmp(line, "TOADDR:", 7) == 0) {
                char *v = line + 7;
                while (*v == ' ' || *v == '\t')
                    v++;
                strlcpy(job->toaddr, v, sizeof(job->toaddr));
            } else if (strncmp(line, "SUBJECT:", 8) == 0) {
                char *v = line + 8;
                while (*v == ' ' || *v == '\t')
                    v++;
                strlcpy(job->subject, v, sizeof(job->subject));
            } else if (strncmp(line, "REPLY:", 6) == 0) {
                char *v = line + 6;
                while (*v == ' ' || *v == '\t')
                    v++;
                strlcpy(job->reply, v, sizeof(job->reply));
            } else if (strncmp(line, "CREATED:", 8) == 0) {
                char *v = line + 8;
                while (*v == ' ' || *v == '\t')
                    v++;
                job->created = atol(v);
            }
            continue;
        }

        append_to_dynbuf(&body, &cap, &len, line);
    }

    fclose(fp);

    if (job->fromuid <= 0 || job->toname[0] == '\0' ||
        job->toaddr[0] == '\0') {
        free(body);
        return -1;
    }

    if (body == NULL)
        body = strdup("");

    if (body == NULL)
        return -1;

    job->body = body;
    return 0;
}


static int
write_fido_netmail_out(const char *path, const struct netmail_job *job,
    const char *from, const struct ftn_address *orig,
    const struct ftn_address *dest, const char *msgid)
{
    FILE *fp;
    unsigned char hdr[190];
    char datebuf[32];
    char dest_addr[64];
    char orig_addr[64];
    char intl_dest[64];
    char intl_orig[64];
    const char *body;
    char *utf8_toname;
    char *utf8_subject;
    char *utf8_body;
    char tzbuf[16];

    if (path == NULL || job == NULL || from == NULL ||
        orig == NULL || dest == NULL ||
        msgid == NULL || *msgid == '\0')
        return -1;

    /*
     * Netmail queue jobs contain SklaffKOM's internal SF7 text.  Convert
     * recipient name, subject and body before emitting a message that
     * advertises CHRS: UTF-8 4.  The sender name comes from Unix/GECOS and
     * is therefore deliberately left alone.
     *
     * modified on 2026-08-10, PL
     */
    utf8_toname = sf7_to_utf8_dup(job->toname);
    utf8_subject = sf7_to_utf8_dup(job->subject);
    utf8_body = sf7_to_utf8_dup(job->body ? job->body : "");

    if (utf8_toname == NULL || utf8_subject == NULL || utf8_body == NULL) {
        fprintf(stderr,
            "[ERROR] Could not convert outgoing FTN netmail from SF7 to UTF-8\n");
        free(utf8_toname);
        free(utf8_subject);
        free(utf8_body);
        return -1;
    }

    body = utf8_body;

    memset(hdr, 0, sizeof(hdr));
    make_fido_date(datebuf, sizeof(datebuf));
    make_tzutc(tzbuf, sizeof(tzbuf));

    make_ftn_addr(dest_addr, sizeof(dest_addr),
        dest->zone, dest->net, dest->node, dest->point);
    make_ftn_addr(orig_addr, sizeof(orig_addr),
        orig->zone, orig->net, orig->node, orig->point);

    /*
     * INTL carries zone:net/node boss addresses.  Point information is
     * represented separately by TOPT/FMPT.
     *
     * modified on 2026-07-16, PL
     */
    snprintf(intl_dest, sizeof(intl_dest), "%d:%d/%d",
        dest->zone, dest->net, dest->node);
    snprintf(intl_orig, sizeof(intl_orig), "%d:%d/%d",
        orig->zone, orig->net, orig->node);

    write_fixed_field(hdr + 0,   36, from);
    write_fixed_field(hdr + 36,  36, utf8_toname);
    write_fixed_field(hdr + 72,  72, utf8_subject);
    write_fixed_field(hdr + 144, 20, datebuf);

    write_u16_le(hdr + 164, 0);                 /* times read */
    write_u16_le(hdr + 166, dest->node);        /* dest node */
    write_u16_le(hdr + 168, orig->node);        /* orig node */
    write_u16_le(hdr + 170, 0);                 /* cost */
    write_u16_le(hdr + 172, orig->net);         /* orig net */
    write_u16_le(hdr + 174, dest->net);         /* dest net */
    write_u16_le(hdr + 176, dest->zone);        /* dest zone */
    write_u16_le(hdr + 178, orig->zone);        /* orig zone */
    write_u16_le(hdr + 180, dest->point);       /* dest point */
    write_u16_le(hdr + 182, orig->point);       /* orig point */
    write_u16_le(hdr + 184, 0);                 /* reply to */
    write_u16_le(hdr + 186, FTN_NETMAIL_ATTR);  /* private + local */
    write_u16_le(hdr + 188, 0);                 /* next reply */

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror(path);
        free(utf8_toname);
        free(utf8_subject);
        free(utf8_body);
        return -1;
    }

    if (fwrite(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
        perror("write netmail .MSG header");
        fclose(fp);
        free(utf8_toname);
        free(utf8_subject);
        free(utf8_body);
        return -1;
    }

    fprintf(fp, "\001INTL %s %s\r", intl_dest, intl_orig);

    if (dest->point != 0)
        fprintf(fp, "\001TOPT %d\r", dest->point);
    if (orig->point != 0)
        fprintf(fp, "\001FMPT %d\r", orig->point);

    fprintf(fp, "\001MSGID: %s\r", msgid);

    if (job->reply[0] != '\0')
        fprintf(fp, "\001REPLY: %s\r", job->reply);
    fprintf(fp, "\001PID: SklaffKOM ftntoss\r");
    fprintf(fp, "\001CHRS: UTF-8 4\r");
    fprintf(fp, "\001TZUTC: %s\r", tzbuf);
    fprintf(fp, "\r");

    while (*body != '\0') {
        if (*body == '\n')
            fputc('\r', fp);
        else
            fputc((unsigned char)*body, fp);
        body++;
    }

    /*
     * Add a tearline and origin line to netmail too.  Netmail routing does
     * not depend on this, but some FTN tools expect it and report *NO ORIGIN*
     * otherwise.
     *
     * modified on 2026-07-13, PL
     */
    {
        char version[64];

        sklaff_version_no_build(version, sizeof(version));

        fprintf(fp, "\r--- SklaffKOM v%s\r", version);
        fprintf(fp, " * Origin: %s, %s (%s)\r",
            SKLAFF_ID, SKLAFF_LOC, orig_addr);
    }
    fputc('\0', fp);

    if (fclose(fp) != 0) {
        perror(path);
        free(utf8_toname);
        free(utf8_subject);
        free(utf8_body);
        return -1;
    }

    chmod(path, 0600);

    printf("Wrote FTN netmail .MSG: %s\n", path);
    printf("To: %s (%s)\n", utf8_toname, dest_addr);
    printf("MSGID: %s\n", msgid);

    free(utf8_toname);
    free(utf8_subject);
    free(utf8_body);
    return 0;
}

static int
export_netmail_job(const char *jobfile)
{
    struct netmail_job job;
    struct netmail_destination destination;
    struct ftn_config config;
    const struct ftn_area *netmail_area;
    char path[PATH_MAX];
    char msgid[128];
    char from[128];
    char aka[64];
    char dest_addr[64];
    long msgnum;
    unsigned long serial;
    int rc;

    if (jobfile == NULL)
        return -1;

    memset(&job, 0, sizeof(job));
    memset(&destination, 0, sizeof(destination));
    ftn_config_init(&config);
    netmail_area = NULL;
    rc = -1;

    if (read_netmail_job(jobfile, &job) != 0) {
        fprintf(stderr, "[ERROR] Could not read netmail job: %s\n", jobfile);
        goto cleanup;
    }

    if (parse_ftn_addr_5d(job.toaddr, &destination) != 0) {
        fprintf(stderr, "[ERROR] Invalid FTN netmail address: %s\n",
            job.toaddr);
        goto cleanup;
    }

    if (load_netmail_area_for_destination(&destination, &config,
            &netmail_area) != 0)
        goto cleanup;

    make_skom_from_name(job.fromuid, from, sizeof(from));

    if (next_msg_path(netmail_area->path, path, sizeof(path), &msgnum) != 0)
        goto cleanup;

    /*
     * Netmail MSGID serial: keep it stable for this queued job using the
     * queue timestamp plus the local .MSG number.  The origin address now
     * comes from the selected CrashMail NETMAIL area's AKA.
     *
     * modified on 2026-07-16, PL
     */
    serial = (((unsigned long)job.created & 0xffffUL) << 16) |
        ((unsigned long)msgnum & 0xffffUL);
    make_ftn_msgid_for_aka(msgid, sizeof(msgid),
        &netmail_area->aka, serial);

    ftn_address_format(&netmail_area->aka, aka, sizeof(aka));
    ftn_address_format(&destination.address, dest_addr, sizeof(dest_addr));

    printf("FTN netmail export setup\n");
    printf("------------------------\n");
    printf("Domain:      %s\n", netmail_area->domain);
    printf("Netmail tag: %s\n", netmail_area->tag);
    printf("Local AKA:   %s\n", aka);
    printf("Destination: %s@%s\n", dest_addr, netmail_area->domain);
    printf("Spool:       %s\n", netmail_area->path);
    printf("Msg no:      %ld\n", msgnum);
    printf("Output:      %s\n\n", path);

    rc = write_fido_netmail_out(path, &job, from,
        &netmail_area->aka, &destination.address, msgid);

cleanup:
    free_netmail_job(&job);
    ftn_config_free(&config);
    return rc;
}

/*
 * read_ibol_job - read one queued InterBBS Oneliner job
 * args: queue path, output job
 * ret: success (0) or error (-1)
 *
 * Queue text is SklaffKOM SF7.  Conversion to UTF-8 happens only at the
 * FTN export boundary.
 *
 * modified on 2026-08-13, PL
 */
static int
read_ibol_job(const char *path, struct ibol_job *job)
{
    FILE *fp;
    char line[1024];
    int in_body;
    int seen_type;
    int seen_text;

    if (path == NULL || job == NULL)
        return -1;

    memset(job, 0, sizeof(*job));

    fp = fopen(path, "r");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    in_body = 0;
    seen_type = 0;
    seen_text = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        strip_eol(line);

        if (!in_body) {
            char *v;

            if (line[0] == '\0') {
                in_body = 1;
                continue;
            }

            if (strncmp(line, "TYPE:", 5) == 0) {
                v = line + 5;
                while (*v == ' ' || *v == '\t')
                    v++;

                if (strcmp(v, "IBOL") != 0) {
                    fclose(fp);
                    return -1;
                }
                seen_type = 1;
            } else if (strncmp(line, "FROMUID:", 8) == 0) {
                v = line + 8;
                while (*v == ' ' || *v == '\t')
                    v++;
                job->fromuid = atoi(v);
            } else if (strncmp(line, "AUTHOR:", 7) == 0) {
                v = line + 7;
                while (*v == ' ' || *v == '\t')
                    v++;
                strlcpy(job->author, v, sizeof(job->author));
            } else if (strncmp(line, "CREATED:", 8) == 0) {
                v = line + 8;
                while (*v == ' ' || *v == '\t')
                    v++;
                job->created = atol(v);
            }

            continue;
        }

        /*
         * A SklaffKOM IBOL post is deliberately one physical line.
         * Ignore trailing blank lines from the queue file, but reject a
         * second non-empty line rather than silently creating a multi-liner.
         */
        if (line[0] == '\0')
            continue;

        if (seen_text) {
            fclose(fp);
            return -1;
        }

        if (strlen(line) > IBOL_ONELINER_MAX) {
            fclose(fp);
            return -1;
        }

        strlcpy(job->text, line, sizeof(job->text));
        seen_text = 1;
    }

    fclose(fp);

    if (!seen_type || job->fromuid <= 0 || job->author[0] == '\0' ||
        job->created <= 0 || !seen_text || job->text[0] == '\0')
        return -1;

    return 0;
}

/*
 * export_ibol_job - turn one queued SklaffKOM oneliner into FSX_DAT .MSG
 * args: queue job path
 * ret: success (0) or error (-1)
 *
 * The target spool path, local AKA and feed are resolved from
 * crashmail.prefs using IBOL_FTN_AREA@IBOL_FTN_DOMAIN.  No spool path or
 * node address is hard-coded here.
 *
 * modified on 2026-08-13, PL
 */
static int
export_ibol_job(const char *jobfile)
{
    struct ibol_job job;
    struct ftn_config config;
    const struct ftn_area *ftn_area;
    const struct ftn_link *feed;
    char path[PATH_MAX];
    char msgid[128];
    char aka[64];
    char feedaddr[64];
    char body[1024];
    char *utf8_author;
    char *utf8_source;
    char *utf8_text;
    long msgnum;
    unsigned long serial;
    int rc;

    if (jobfile == NULL)
        return -1;

    memset(&job, 0, sizeof(job));
    ftn_config_init(&config);
    ftn_area = NULL;
    feed = NULL;
    utf8_author = NULL;
    utf8_source = NULL;
    utf8_text = NULL;
    rc = -1;

    if (read_ibol_job(jobfile, &job) != 0) {
        fprintf(stderr, "[ERROR] Could not read IBOL job: %s\n", jobfile);
        goto cleanup;
    }

    if (load_echomail_area_by_identity(IBOL_FTN_DOMAIN, IBOL_FTN_AREA,
            &config, &ftn_area, &feed) != 0)
        goto cleanup;

    /*
     * Interactive SklaffKOM input and user names are stored as SF7.
     * IBOL .MSG files advertise UTF-8, so convert both at the boundary.
     */
    utf8_author = sf7_to_utf8_dup(job.author);
    utf8_source = sf7_to_utf8_dup(SKLAFF_ID);
    utf8_text = sf7_to_utf8_dup(job.text);

    if (utf8_author == NULL || utf8_source == NULL || utf8_text == NULL) {
        fprintf(stderr,
            "[ERROR] Could not convert outgoing IBOL text from SF7 to UTF-8\n");
        goto cleanup;
    }

    if (next_msg_path(ftn_area->path, path, sizeof(path), &msgnum) != 0)
        goto cleanup;

    serial = (((unsigned long)job.created & 0xffffUL) << 16) |
        ((unsigned long)msgnum & 0xffffUL);
    make_ftn_msgid_for_aka(msgid, sizeof(msgid), &ftn_area->aka, serial);

    if (snprintf(body, sizeof(body),
            "Author: %s\n"
            "Source: %s\n"
            "\n"
            "Oneliner: %s",
            utf8_author, utf8_source, utf8_text) >= (int)sizeof(body)) {
        fprintf(stderr, "[ERROR] IBOL message body too long\n");
        goto cleanup;
    }

    ftn_address_format(&ftn_area->aka, aka, sizeof(aka));
    ftn_address_format(&feed->address, feedaddr, sizeof(feedaddr));

    printf("IBOL export setup\n");
    printf("-----------------\n");
    printf("Domain:     %s\n", ftn_area->domain);
    printf("Area:       %s\n", ftn_area->tag);
    printf("AKA:        %s\n", aka);
    printf("Feed:       %s\n", feedaddr);
    printf("Spool:      %s\n", ftn_area->path);
    printf("Msg no:     %ld\n", msgnum);
    printf("Author:     %s\n", utf8_author);
    printf("Source:     %s\n", utf8_source);
    printf("MSGID:      %s\n", msgid);
    printf("Output:     %s\n\n", path);

    rc = write_fido_msg_out(path, ftn_area, feed,
        utf8_author,
        "IBBS1LINE",
        "InterBBS Oneliner",
        body,
        NULL,
        msgid);

cleanup:
    free(utf8_author);
    free(utf8_source);
    free(utf8_text);
    ftn_config_free(&config);
    return rc;
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
    struct ftn_conf_info tag_match;
    size_t tag_matches;

    if (name == NULL || *name == '\0' || out_ce == NULL)
        return -1;

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

    /*
     * Preserve the old behaviour first: an exact SklaffKOM conference
     * name always wins over an FTN echotag.
     *
     * modified on 2026-08-15, PL
     */
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

    /*
     * New conference layout: the FTN tag is stored in
     *   SKLAFF_DB/<confnum>/ftnconf
     * rather than being required to equal the SklaffKOM conference name.
     * If no conference name matched, resolve the supplied value as an
     * echotag.  Tags are case-insensitive in FTN configuration.
     *
     * modified on 2026-08-15, PL
     */
    rewind(fp);
    tag_matches = 0;
    memset(&tag_match, 0, sizeof(tag_match));

    while (fgets(line, sizeof(line), fp) != NULL) {
        struct meeting_ftn_config meeting;
        int has_ftnconf;

        if (parse_conf_line(line, &ce) != 0)
            continue;

        if (!conf_is_ftn(ce.type))
            continue;

        has_ftnconf = 0;
        if (load_meeting_ftnconf(&ce, &meeting, &has_ftnconf) != 0) {
            fclose(fp);
            return -1;
        }

        if (!has_ftnconf)
            continue;

        if (strcasecmp(name, meeting.tag) != 0)
            continue;

        tag_match = ce;
        tag_matches++;
    }

    fclose(fp);

    if (tag_matches == 1) {
        *out_ce = tag_match;
        printf("OK! (%s, echotag)\n", tag_match.name);
        return 0;
    }

    if (tag_matches > 1) {
        fprintf(stderr,
            "\n[ERROR] Echotag '%s' matches more than one FTN conference\n",
            name);
        return -1;
    }

    fprintf(stderr,
        "\n[ERROR] Conference or FTN echotag '%s' not found in %s\n",
        name, CONF_FILE);
    return -1;
}

static int
find_ftn_conf_num(int confnum, struct ftn_conf_info *out_ce)
{
    FILE *fp;
    LONG_LINE line;
    struct ftn_conf_info ce;

    if (confnum <= 0 || out_ce == NULL)
        return -1;

    printf("Checking the SklaffKOM CONF_FILE: %s... ", CONF_FILE);
    fflush(stdout);

    fp = fopen(CONF_FILE, "r");
    if (fp == NULL) {
        fprintf(stderr, "\n[ERROR] Could not open file '%s'\n", CONF_FILE);
        perror("fopen");
        return -1;
    }

    printf("OK!\n");
    printf("Checking conference number: %d... ", confnum);
    fflush(stdout);

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (parse_conf_line(line, &ce) != 0)
            continue;

        if (ce.num == confnum) {
            *out_ce = ce;
            fclose(fp);
            printf("OK! (%s)\n", ce.name);
            return 0;
        }
    }

    fclose(fp);
    fprintf(stderr, "\n[ERROR] Conference number %d not found in %s\n",
        confnum, CONF_FILE);
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

static long
msgitem_filename_number(const char *filename)
{
    char *endp;
    long n;

    if (filename == NULL || *filename == '\0')
        return 0;

    errno = 0;
    n = strtol(filename, &endp, 10);

    if (errno != 0 || endp == filename)
        return 0;

    if (strcasecmp(endp, ".msg") != 0)
        return 0;

    return n;
}

static struct msgitem *
sort_msgitems_by_number(struct msgitem *list)
{
    struct msgitem *sorted = NULL;

    while (list != NULL) {
        struct msgitem *item = list;
        struct msgitem **pp;
        long itemnum;

        list = list->next;
        item->next = NULL;

        itemnum = msgitem_filename_number(item->filename);

        /*
         * Keep FTN imports stable and predictable.  readdir() order is not
         * guaranteed, so sort .MSG files by their numeric spool filename
         * before planning/importing.  This makes SklaffKOM text numbers
         * follow the local FTN spool order instead of filesystem order.
         *
         * modified on 2026-06-15, PL
         */
        pp = &sorted;
        while (*pp != NULL) {
            long curnum;

            curnum = msgitem_filename_number((*pp)->filename);

            if (itemnum > 0 && curnum > 0 && itemnum < curnum)
                break;

            if (itemnum == 0 && curnum > 0)
                break;

            pp = &(*pp)->next;
        }

        item->next = *pp;
        *pp = item;
    }

    return sorted;
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
    else if (strncmp(line, "FTN-MSGID:", 10) == 0)
        p = line + 10; /* modified on 2026-06-15, PL */
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
    const struct ftn_address *aka, struct skref **out_refs,
    long *out_indexed)
{
    long i;
    long indexed = 0;
    char path[PATH_MAX];

    if (ce == NULL || aka == NULL || out_refs == NULL ||
        out_indexed == NULL)
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
    int found_msgid = 0;

    if (snprintf(path, sizeof(path), "%s/%d/%ld",
            SKLAFF_DB, ce->num, i) >= (int)sizeof(path)) {
        fprintf(stderr,
            "[ERROR] SklaffKOM text path too long: %s/%d/%ld\n",
            SKLAFF_DB, ce->num, i);
        return -1;
    }

    fp = fopen(path, "r");
    if (fp == NULL)
        continue;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (extract_ftn_msgid_from_line(line, msgid,
                sizeof(msgid))) {
            add_skref(out_refs, msgid, i);
            indexed++;
            found_msgid = 1;
            break;
        }
    }

    fclose(fp);

    /*
     * Locally written FTN texts have a real SklaffKOM author uid but no
     * stored FTN-MSGID.  Recreate the deterministic MSGID used by export-one
     * so echoed messages are recognized instead of imported as duplicates.
     *
     * modified on 2026-06-21, PL
     */
    if (!found_msgid) {
        long uid = 0;
        long when = 0;
        long com = 0;
        unsigned long serial;
        char subject[256];
        char *body = NULL;

        if (read_skom_export_text(ce->num, i, &uid, &when, &com,
                subject, sizeof(subject), &body) == 0) {
            if (uid > 0) {
                serial = make_export_one_serial(ce->num, i);
                make_ftn_msgid_for_aka(msgid, sizeof(msgid), aka, serial);
                add_skref(out_refs, msgid, i);
                indexed++;
            }

            free(body);
        }
    }
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

    items = sort_msgitems_by_number(items); /* modified on 2026-06-15, PL */

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

static void
strip_eol(char *s)
{
    size_t len;

    if (s == NULL)
        return;

    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

/*
 * load_user_signature - read ![sig] directly from user's sklaffrc
 * args: user id
 * ret: malloced signature or NULL if none
 *
 * modified on 2026-08-12, PL
 */
static char *
load_user_signature(int uid)
{
    FILE *fp;
    char path[PATH_MAX];
    char line[4096];
    char *sig;
    size_t len;
    size_t cap;
    int in_sig;

    if (uid <= 0)
        return NULL;

    if (snprintf(path, sizeof(path), "%s/%d%s",
            USER_DB, uid, SKLAFFRC_FILE) >= (int)sizeof(path))
        return NULL;

    fp = fopen(path, "r");
    if (fp == NULL)
        return NULL;

    sig = NULL;
    len = 0;
    cap = 0;
    in_sig = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t line_len;

        if (strncmp(line, "![", 2) == 0) {
            char heading[4096];

            strlcpy(heading, line, sizeof(heading));
            strip_eol(heading);

            if (in_sig)
                break;

            if (strcmp(heading, "![sig]") == 0)
                in_sig = 1;

            continue;
        }

        if (!in_sig)
            continue;

        line_len = strlen(line);

        if (len + line_len + 1 > cap) {
            char *new_sig;
            size_t new_cap;

            new_cap = cap ? cap * 2 : 4096;
            while (new_cap < len + line_len + 1)
                new_cap *= 2;

            new_sig = realloc(sig, new_cap);
            if (new_sig == NULL) {
                free(sig);
                fclose(fp);
                return NULL;
            }

            sig = new_sig;
            cap = new_cap;
        }

        memcpy(sig + len, line, line_len);
        len += line_len;
        sig[len] = '\0';
    }

    fclose(fp);

    if (sig == NULL)
        return NULL;

    strip_eol(sig);

    if (sig[0] == '\0') {
        free(sig);
        return NULL;
    }

    return sig;
}

static int
ftntoss_get_sklaff_ids(uid_t *uid, gid_t *gid)
{
    static int cached = 0;
    static uid_t sklaff_uid;
    static gid_t sklaff_gid;
    struct passwd *pw;
    struct group *gr;

    if (uid == NULL || gid == NULL)
        return -1;

    if (!cached) {
        pw = getpwnam("sklaff");
        gr = getgrnam("sklaff");

        if (pw == NULL || gr == NULL) {
            fprintf(stderr,
                "[ERROR] ftntoss: could not resolve user/group sklaff:sklaff\n");
            fprintf(stderr,
                "[ERROR] ftntoss: imported SklaffKOM files need owner sklaff\n");
            return -1;
        }

        sklaff_uid = pw->pw_uid;
        sklaff_gid = gr->gr_gid;
        cached = 1;
    }

    *uid = sklaff_uid;
    *gid = sklaff_gid;
    return 0;
}

static int
ftntoss_fix_fd_to_sklaff(FILE *fp, const char *path, mode_t mode)
{
    uid_t uid;
    gid_t gid;
    int fd;

    if (fp == NULL || path == NULL)
        return -1;

    if (ftntoss_get_sklaff_ids(&uid, &gid) != 0)
        return -1;

    fd = fileno(fp);
    if (fd == -1) {
        perror("fileno");
        return -1;
    }

    if (geteuid() == 0) {
        if (fchown(fd, uid, gid) == -1) {
            fprintf(stderr,
                "[ERROR] ftntoss: fchown(%s, sklaff:sklaff) failed: %s\n",
                path, strerror(errno));
            return -1;
        }
    } else if (geteuid() != uid) {
        fprintf(stderr,
            "[ERROR] ftntoss: %s was created while running as uid %ld\n",
            path, (long)geteuid());
        fprintf(stderr,
            "[ERROR] ftntoss: run this command as root/sudo or as user sklaff, "
            "otherwise SklaffKOM may not be able to read imported texts.\n");
        return -1;
    }

    if (fchmod(fd, mode) == -1) {
        fprintf(stderr,
            "[ERROR] ftntoss: fchmod(%s, %04o) failed: %s\n",
            path, (unsigned)mode, strerror(errno));
        return -1;
    }

    return 0;
}

static int
ftntoss_fix_fd_like_stat(FILE *fp, const char *path, const struct stat *st)
{
    uid_t uid;
    gid_t gid;
    int fd;
    mode_t mode;

    if (fp == NULL || path == NULL || st == NULL)
        return -1;

    if (ftntoss_get_sklaff_ids(&uid, &gid) != 0)
        return -1;

    fd = fileno(fp);
    if (fd == -1) {
        perror("fileno");
        return -1;
    }

    /*
     * rewrite_conf_last_text() writes a temporary CONF_FILE and renames it
     * over the real one.  If ftntoss is run with sudo, the replacement must
     * not accidentally become root:root 0600, because SklaffKOM later reads
     * CONF_FILE while running as user sklaff.
     *
     * Always force owner to sklaff.  Preserve the existing group when we have
     * privileges, because many stock files are sklaff:root.  When running
     * directly as sklaff, preserving the root group may not be possible, but
     * owner sklaff plus mode 0600 is enough for the live BBS to read the file.
     *
     * modified on 2026-07-09, PL
     */
    if (geteuid() == 0) {
        if (fchown(fd, uid, st->st_gid) == -1) {
            fprintf(stderr,
                "[ERROR] ftntoss: fchown(%s, sklaff:%ld) failed: %s\n",
                path, (long)st->st_gid, strerror(errno));
            return -1;
        }
    } else if (geteuid() != uid) {
        fprintf(stderr,
            "[ERROR] ftntoss: cannot safely rewrite %s as uid %ld; "
            "expected user sklaff\n",
            path, (long)geteuid());
        fprintf(stderr,
            "[ERROR] ftntoss: run this command as root/sudo or as the "
            "SklaffKOM owner user.\n");
        return -1;
    }

    mode = st->st_mode & 07777;
    if (fchmod(fd, mode) == -1) {
        fprintf(stderr,
            "[ERROR] ftntoss: fchmod(%s, %04o) failed: %s\n",
            path, (unsigned)mode, strerror(errno));
        return -1;
    }

    return 0;
}

static int
ftntoss_fix_control_file(const char *path, mode_t mode)
{
    struct stat st;
    uid_t uid;
    gid_t gid;

    if (path == NULL)
        return -1;

    if (stat(path, &st) == -1) {
        if (errno == ENOENT)
            return 0;
        fprintf(stderr,
            "[ERROR] ftntoss: stat(%s) failed: %s\n",
            path, strerror(errno));
        return -1;
    }

    if (!S_ISREG(st.st_mode))
        return 0;

    if (ftntoss_get_sklaff_ids(&uid, &gid) != 0)
        return -1;

    /*
     * ACTIVE_FILE is normally a SklaffKOM control file owned by sklaff,
     * often with group root and mode 0600.  ftntoss only reads it, but after
     * an import it wakes already active SklaffKOM sessions.  Those sessions
     * may then open ACTIVE_FILE while rebuilding their prompt, so repair the
     * same ownership class here before signalling them.
     *
     * Preserve the existing group when running as root, just like the
     * CONF_FILE rewrite helper does.
     *
     * modified on 2026-07-09, PL
     */
    if (geteuid() == 0) {
        if (chown(path, uid, st.st_gid) == -1) {
            fprintf(stderr,
                "[ERROR] ftntoss: chown(%s, sklaff:%ld) failed: %s\n",
                path, (long)st.st_gid, strerror(errno));
            return -1;
        }
    } else if (geteuid() != uid) {
        fprintf(stderr,
            "[ERROR] ftntoss: cannot safely fix %s as uid %ld; "
            "expected root or sklaff\n",
            path, (long)geteuid());
        return -1;
    }

    if (chmod(path, mode) == -1) {
        fprintf(stderr,
            "[ERROR] ftntoss: chmod(%s, %04o) failed: %s\n",
            path, (unsigned)mode, strerror(errno));
        return -1;
    }

    return 0;
}

static void
ftntoss_notify_all_processes(int sig)
{
    FILE *fp;
    char line[512];
    char *p;
    char *end;
    long pid;

    /*
     * ftntoss is linked as a small standalone tool.  Do not use
     * notify_all_processes() from msg.c or get_active_entry() from buf.c
     * here, because those objects pull in large parts of the interactive
     * SklaffKOM binary.
     *
     * ACTIVE_FILE format:
     * uid:pid:login_time:avail:from:tty:...
     *
     * modified on 2026-07-05, PL
     */
    if (ftntoss_fix_control_file(ACTIVE_FILE, 0600) != 0)
        return;

    fp = fopen(ACTIVE_FILE, "r");
    if (fp == NULL)
        return;

    while (fgets(line, sizeof(line), fp) != NULL) {
        p = strchr(line, ':');
        if (p == NULL)
            continue;

        p++;
        pid = strtol(p, &end, 10);
        if (p == end || *end != ':')
            continue;

        if (pid > 1)
            kill((pid_t)pid, sig);
    }

    fclose(fp);
}

static int
read_skom_export_text(int confnum, long textnum, long *out_uid,
    long *out_time, long *out_com, char *subject, size_t subjectsz,
    char **out_body)
{
    FILE *fp;
    char path[PATH_MAX];
    char *buf;
    char *p;
    char *subj;
    char *body;
    long size;
    long hdr_textnum;
    long uid;
    long when;
    long com;
    long dummy1;
    long dummy2;
    long lines;

    if (subject == NULL || subjectsz == 0 || out_body == NULL)
        return -1;

    *out_body = NULL;
    subject[0] = '\0';

    if (snprintf(path, sizeof(path), "%s/db/%d/%ld",
            SKLAFFDIR, confnum, textnum) >= (int)sizeof(path)) {
        fprintf(stderr, "[ERROR] SklaffKOM text path too long\n");
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        perror(path);
        fclose(fp);
        return -1;
    }

    size = ftell(fp);
    if (size < 0) {
        perror(path);
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        perror(path);
        fclose(fp);
        return -1;
    }

    buf = (char *)calloc(1, (size_t)size + 1);
    if (buf == NULL) {
        fclose(fp);
        return -1;
    }

    if (size > 0 && fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        perror(path);
        free(buf);
        fclose(fp);
        return -1;
    }

    fclose(fp);

    p = strchr(buf, '\n');
    if (p == NULL) {
        fprintf(stderr, "[ERROR] SklaffKOM text %ld has no subject line\n",
            textnum);
        free(buf);
        return -1;
    }

    *p++ = '\0';
    strip_eol(buf);

    if (sscanf(buf, "%ld:%ld:%ld:%ld:%ld:%ld:%ld",
            &hdr_textnum, &uid, &when, &com, &dummy1, &dummy2,
            &lines) < 4) {
        fprintf(stderr, "[ERROR] Could not parse SklaffKOM text header: %s\n",
            buf);
        free(buf);
        return -1;
    }

    subj = p;
    p = strchr(subj, '\n');
    if (p != NULL) {
        *p++ = '\0';
        body = p;
    } else {
        body = "";
    }

    strip_eol(subj);

    if (*subj != '\0')
        strlcpy(subject, subj, subjectsz);
    else
        strlcpy(subject, "(no subject)", subjectsz);

    *out_body = (char *)calloc(1, strlen(body) + 1);
    if (*out_body == NULL) {
        free(buf);
        return -1;
    }

    strlcpy(*out_body, body, strlen(body) + 1);

    if (out_uid != NULL)
        *out_uid = uid;
    if (out_time != NULL)
        *out_time = when;
    if (out_com != NULL)
        *out_com = com;

    free(buf);
    return 0;
}

static int
read_skom_ftn_msgid(int confnum, long textnum, char *out, size_t outsz)
{
    FILE *fp;
    char path[PATH_MAX];
    char line[1024];
    char *p;

    if (out == NULL || outsz == 0)
        return -1;

    out[0] = '\0';

    if (snprintf(path, sizeof(path), "%s/db/%d/%ld",
            SKLAFFDIR, confnum, textnum) >= (int)sizeof(path))
        return -1;

    fp = fopen(path, "r");
    if (fp == NULL)
        return -1;

    while (fgets(line, sizeof(line), fp) != NULL) {
        strip_eol(line);

        if (strncmp(line, "FTN-MSGID:", 10) == 0) {
            p = line + 10;
            while (*p != '\0' && isspace((unsigned char)*p))
                p++;

            if (*p != '\0') {
                strlcpy(out, p, outsz);
                fclose(fp);
                return 0;
            }
        }
    }

    fclose(fp);
    return -1;
}

static int
export_test_ftn(const char *area)
{
    struct ftn_conf_info ce;
    struct ftn_config config;
    const struct ftn_area *ftn_area;
    const struct ftn_link *feed;
    char path[PATH_MAX];
    char body[1024];
    char msgid[128];
    char aka[64];
    char feedaddr[64];
    long msgnum;
    unsigned long serial;
    int rc;

    if (area == NULL || *area == '\0')
        return -1;

    ftn_config_init(&config);
    ftn_area = NULL;
    feed = NULL;
    msgnum = 0;
    rc = -1;

    printf("ftntoss export-test starting\n");
    printf("============================\n\n");

    printf("Checking conference: %s... ", area);
    fflush(stdout);

    if (find_ftn_conf(area, &ce) != 0) {
        printf("FAILED\n");
        fprintf(stderr,
            "[ERROR] Could not find FTN conference '%s'\n", area);
        goto cleanup;
    }

    printf("OK!\n");

    if (!conf_is_ftn(ce.type)) {
        fprintf(stderr,
            "[ERROR] Conference '%s' exists, but is not FTN_CONF "
            "(type=%d)\n", area, ce.type);
        goto cleanup;
    }

    if (load_echomail_area(area, &ce, &config, &ftn_area, &feed) != 0)
        goto cleanup;

    if (next_msg_path(ftn_area->path, path, sizeof(path), &msgnum) != 0)
        goto cleanup;

    serial = make_export_test_serial(ftn_area->tag, msgnum);
    make_ftn_msgid_for_aka(msgid, sizeof(msgid), &ftn_area->aka, serial);

    snprintf(body, sizeof(body),
        "This is a SklaffKOM/ftntoss echomail export test.\n"
        "\n"
        "\n"
        "BBS name:      %s\n"
        "Hostname:      %s\n"
        "Location:      %s\n"
        "Sysop:         %s\n"
        "\n"
        "FTN domain:    %s\n"
        "FTN area:      %s\n"
        "Local conf:    %d\n"
        "Local msg no:  %ld\n"
        "FTN MSGID:     %s\n"
        "\n"
        "If you can read this message, outgoing FTN echomail from this\n"
        "SklaffKOM system appears to be working.\n"
        "\n",
        SKLAFF_ID,
        MACHINE_NAME,
        SKLAFF_LOC,
        SKLAFF_SYSOP,
        ftn_area->domain,
        ftn_area->tag,
        ce.num,
        msgnum,
        msgid);

    ftn_address_format(&ftn_area->aka, aka, sizeof(aka));
    ftn_address_format(&feed->address, feedaddr, sizeof(feedaddr));

    printf("FTN export-test setup\n");
    printf("---------------------\n");
    printf("BBS name:   %s\n", SKLAFF_ID);
    printf("Hostname:   %s\n", MACHINE_NAME);
    printf("Location:   %s\n", SKLAFF_LOC);
    printf("Sysop:      %s\n", SKLAFF_SYSOP);
    printf("Domain:     %s\n", ftn_area->domain);
    printf("Area:       %s\n", ftn_area->tag);
    printf("AKA:        %s\n", aka);
    printf("Feed:       %s\n", feedaddr);
    printf("Spool:      %s\n", ftn_area->path);
    printf("Conf num:   %d\n", ce.num);
    printf("Conf type:  %d (FTN_CONF)\n", ce.type);
    printf("Msg no:     %ld\n", msgnum);
    printf("MSGID:      %s\n", msgid);
    printf("Output:     %s\n\n", path);

    rc = write_fido_msg_out(path, ftn_area, feed,
        SKLAFF_SYSOP,
        "All",
        SKLAFF_ID " FTN export test",
        body,
        NULL,
        msgid);

cleanup:
    ftn_config_free(&config);
    return rc;
}

static void
make_skom_from_name(long uid, char *out, size_t outsz)
{
    struct passwd *pw;
    char gecos[128];
    char *comma;

    if (out == NULL || outsz == 0)
        return;

    out[0] = '\0';

    if (uid <= 0) {
        strlcpy(out, SKLAFF_SYSOP, outsz);
        return;
    }

    pw = getpwuid((uid_t)uid);
    if (pw == NULL) {
        strlcpy(out, SKLAFF_SYSOP, outsz);
        return;
    }

    /*
     * Prefer the Unix GECOS/full name field when available, but strip the
     * comma-separated extra fields.  Fall back to login name if GECOS is
     * empty.  This keeps ftntoss standalone while still giving outgoing
     * FTN messages a useful local sender name.
     *
     * modified on 2026-06-14, PL
     */
    if (pw->pw_gecos != NULL && pw->pw_gecos[0] != '\0') {
        strlcpy(gecos, pw->pw_gecos, sizeof(gecos));
        comma = strchr(gecos, ',');
        if (comma != NULL)
            *comma = '\0';

        if (gecos[0] != '\0') {
            strlcpy(out, gecos, outsz);
            return;
        }
    }

    if (pw->pw_name != NULL && pw->pw_name[0] != '\0')
        strlcpy(out, pw->pw_name, outsz);
    else
        strlcpy(out, SKLAFF_SYSOP, outsz);
}

static int
export_one_ftn_loaded(const struct ftn_conf_info *source_ce, long textnum)
{
    struct ftn_conf_info ce;
    struct ftn_config config;
    const struct ftn_area *ftn_area;
    const struct ftn_link *feed;
    char path[PATH_MAX];
    char subject[256];
    char msgid[128];
    char reply[256];
    char from[128];
    char aka[64];
    char feedaddr[64];
    char *body;
    char *utf8_subject;
    char *utf8_body;
    long msgnum;
    long uid;
    long when;
    long com;
    unsigned long serial;
    unsigned long reply_serial;
    int rc;

    if (source_ce == NULL || source_ce->num <= 0 || textnum <= 0)
        return -1;

    ce = *source_ce;

    ftn_config_init(&config);
    ftn_area = NULL;
    feed = NULL;
    body = NULL;
    utf8_subject = NULL;
    utf8_body = NULL;
    msgnum = 0;
    uid = 0;
    when = 0;
    com = 0;
    rc = -1;

    printf("ftntoss export-one starting\n");
    printf("===========================\n\n");

    if (!conf_is_ftn(ce.type)) {
        fprintf(stderr,
            "[ERROR] Conference '%s' exists, but is not FTN_CONF "
            "(type=%d)\n", ce.name, ce.type);
        goto cleanup;
    }

    if (load_echomail_area(ce.name, &ce, &config, &ftn_area, &feed) != 0)
        goto cleanup;

    if (read_skom_export_text(ce.num, textnum, &uid, &when, &com,
            subject, sizeof(subject), &body) != 0)
        goto cleanup;

    /*
     * SklaffKOM stores local text in SF7.  The outgoing FTN message is
     * declared as UTF-8, so convert the stored subject and body at the
     * export boundary before writing the .MSG file.
     *
     * modified on 2026-08-10, PL
     */
    utf8_subject = sf7_to_utf8_dup(subject);
    utf8_body = sf7_to_utf8_dup(body);
    if (utf8_subject == NULL || utf8_body == NULL) {
        fprintf(stderr,
            "[ERROR] Could not convert outgoing FTN text from SF7 to UTF-8\n");
        goto cleanup;
    }

    /*
     * Add the user's SklaffKOM signature to outgoing FTN echomail.
     * The signature is stored as SF7 and must therefore be converted
     * to UTF-8 before being appended to the outgoing body.
     *
     * modified on 2026-08-12, PL
     */
    /*
     * Add the user's SklaffKOM signature to outgoing FTN echomail.
     *
     * modified on 2026-08-12, PL
     */
    if (uid > 0) {
        char *sig;
        char *utf8_sig;

        sig = load_user_signature((int)uid);
        if (sig != NULL) {
            char *new_body;
            size_t body_len;
            size_t sig_len;

            utf8_sig = sf7_to_utf8_dup(sig);
            free(sig);

            if (utf8_sig == NULL) {
                fprintf(stderr,
                    "[ERROR] Could not convert outgoing FTN "
                    "signature from SF7 to UTF-8\n");
                goto cleanup;
            }

            strip_eol(utf8_body);
            strip_eol(utf8_sig);

            body_len = strlen(utf8_body);
            sig_len = strlen(utf8_sig);

            new_body = realloc(utf8_body,
                body_len + sig_len + 3);
            if (new_body == NULL) {
                free(utf8_sig);
                fprintf(stderr,
                    "[ERROR] Could not allocate outgoing "
                    "FTN message with signature\n");
                goto cleanup;
            }

            utf8_body = new_body;
            memcpy(utf8_body + body_len, "\n\n", 2);
            memcpy(utf8_body + body_len + 2,
                utf8_sig, sig_len + 1);

            free(utf8_sig);
        }
    }

    make_skom_from_name(uid, from, sizeof(from));

    if (next_msg_path(ftn_area->path, path, sizeof(path), &msgnum) != 0)
        goto cleanup;

    serial = make_export_one_serial(ce.num, textnum);
    make_ftn_msgid_for_aka(msgid, sizeof(msgid), &ftn_area->aka, serial);

    reply[0] = '\0';

    if (com > 0) {
        if (read_skom_ftn_msgid(ce.num, com, reply, sizeof(reply)) != 0) {
            /*
             * A locally written parent has no stored FTN-MSGID. Recreate
             * its deterministic MSGID using this area's local AKA.
             *
             * modified on 2026-07-15, PL
             */
            reply_serial = make_export_one_serial(ce.num, com);
            make_ftn_msgid_for_aka(reply, sizeof(reply), &ftn_area->aka,
                reply_serial);
        }
    }

    ftn_address_format(&ftn_area->aka, aka, sizeof(aka));
    ftn_address_format(&feed->address, feedaddr, sizeof(feedaddr));

    printf("FTN export-one setup\n");
    printf("--------------------\n");
    printf("BBS name:   %s\n", SKLAFF_ID);
    printf("Hostname:   %s\n", MACHINE_NAME);
    printf("Location:   %s\n", SKLAFF_LOC);
    printf("Sysop:      %s\n", SKLAFF_SYSOP);
    printf("Domain:     %s\n", ftn_area->domain);
    printf("Area:       %s\n", ftn_area->tag);
    printf("AKA:        %s\n", aka);
    printf("Feed:       %s\n", feedaddr);
    printf("Spool:      %s\n", ftn_area->path);
    printf("Conf num:   %d\n", ce.num);
    printf("Conf type:  %d (FTN_CONF)\n", ce.type);
    printf("Text num:   %ld\n", textnum);
    printf("Text uid:   %ld\n", uid);
    printf("From:       %s\n", from);
    printf("Comment to: %ld\n", com);
    printf("Subject:    %s\n", utf8_subject);
    printf("MSGID:      %s\n", msgid);
    if (reply[0] != '\0')
        printf("REPLY:      %s\n", reply);
    printf("Output:     %s\n\n", path);

    rc = write_fido_msg_out(path, ftn_area, feed,
        from,
        "All",
        utf8_subject,
        utf8_body,
        reply[0] != '\0' ? reply : NULL,
        msgid);

cleanup:
    free(utf8_subject);
    free(utf8_body);
    free(body);
    ftn_config_free(&config);
    return rc;
}

static int
export_one_ftn(const char *area, long textnum)
{
    struct ftn_conf_info ce;

    if (area == NULL || *area == '\0' || textnum <= 0)
        return -1;

    if (find_ftn_conf(area, &ce) != 0)
        return -1;

    return export_one_ftn_loaded(&ce, textnum);
}

static int
export_one_ftn_conf(int confnum, long textnum)
{
    struct ftn_conf_info ce;

    if (confnum <= 0 || textnum <= 0)
        return -1;

    if (find_ftn_conf_num(confnum, &ce) != 0)
        return -1;

    return export_one_ftn_loaded(&ce, textnum);
}

static int
import_one_ftn(const char *area, const char *filename)
{
    struct ftn_conf_info ce;
    struct ftn_config config;
    const struct ftn_area *ftn_area;
    struct msgref *refs = NULL;
    struct skref *skrefs = NULL;
    struct msgitem *items = NULL;
    struct msgitem *m = NULL;
    struct planref *plans = NULL;
    const struct planref *plan = NULL;
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

    ftn_config_init(&config);
    ftn_area = NULL;

    printf("ftntoss import-one starting\n");
    printf("===========================\n\n");

    if (find_ftn_conf(area, &ce) != 0)
        goto cleanup;

    if (!conf_is_ftn(ce.type)) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce.name, ce.type);
        goto cleanup;
    }

    if (load_echomail_area_config(area, &ce, &config, &ftn_area) != 0)
        goto cleanup;

    printf("FTN import-one setup\n");
    printf("--------------------\n");
    printf("Domain:      %s\n", ftn_area->domain);
    printf("Area:        %s\n", ftn_area->tag);
    printf("Spool:       %s\n", ftn_area->path);
    printf("Conf name:   %s\n", ce.name);
    printf("Conf num:    %d\n", ce.num);
    printf("Conf type:   %d (FTN_CONF)\n", ce.type);
    printf("Last text:   %ld\n", ce.last_text);
    printf("Target file: %s\n\n", filename);

    if (scan_existing_skl_msgids(&ce, &ftn_area->aka, &skrefs,
            &existing_indexed) != 0)
        goto cleanup;

    if (build_spool_index(ftn_area->path, &refs, &items, &seen,
            &indexed, &failed) != 0)
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

	imported_text = send_ftn(ce.num, ftn_area->tag, &msg, com, NULL);
    if (imported_text <= 0) {
        fprintf(stderr, "[ERROR] send_ftn() failed\n");
        free_fido_msg(&msg);
        goto cleanup;
    }

    /*
     * Wake active SklaffKOM sessions so their prompts notice newly imported
     * FTN text, matching newstoss/mailtoss behaviour.
     *
     * modified on 2026-06-24, PL
     */
    ftntoss_notify_all_processes(SIGNAL_NEW_TEXT); /* modified on 2026-07-05, PL */

    printf("\nftntoss import-one done\n");
    printf("Imported: SklaffKOM text %ld\n", imported_text);

    free_fido_msg(&msg);
    rc = 0;

cleanup:
    free_msgrefs(refs);
    free_skrefs(skrefs);
    free_msgitems(items);
    free_planrefs(plans);
    ftn_config_free(&config);

    return rc;
}

static int
import_all_ftn(const char *area, int include_unsafe)
{
    struct ftn_conf_info ce;
    struct ftn_config config;
    const struct ftn_area *ftn_area;
    struct msgref *refs = NULL;
    struct skref *skrefs = NULL;
    struct msgitem *items = NULL;
    struct msgitem *m;
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

    ftn_config_init(&config);
    ftn_area = NULL;

    printf("ftntoss import-all starting\n");
    printf("===========================\n\n");

    if (find_ftn_conf(area, &ce) != 0)
        goto cleanup;

    if (!conf_is_ftn(ce.type)) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce.name, ce.type);
        goto cleanup;
    }

    if (load_echomail_area_config(area, &ce, &config, &ftn_area) != 0)
        goto cleanup;

    printf("FTN import-all setup\n");
    printf("--------------------\n");
    printf("Domain:      %s\n", ftn_area->domain);
    printf("Area:        %s\n", ftn_area->tag);
    printf("Spool:       %s\n", ftn_area->path);
    printf("Conf name:   %s\n", ce.name);
    printf("Conf num:    %d\n", ce.num);
    printf("Conf type:   %d (FTN_CONF)\n", ce.type);
    printf("Last text:   %ld\n", ce.last_text);
    printf("Include unsafe: %s\n\n", include_unsafe ? "yes" : "no");
    
    if (scan_existing_skl_msgids(&ce, &ftn_area->aka, &skrefs,
            &existing_indexed) != 0)
        goto cleanup;

    if (build_spool_index(ftn_area->path, &refs, &items, &seen,
            &indexed, &failed) != 0)
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

			imported_text = send_ftn(ce.num, ftn_area->tag, &msg, com,
                unsafe_reason);
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
    printf("Domain:           %s\n", ftn_area->domain);
    printf("Area:             %s\n", ftn_area->tag);
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

    /*
     * Wake active SklaffKOM sessions once after a successful import batch.
     *
     * modified on 2026-06-24, PL
     */
    if (imported > 0)
    ftntoss_notify_all_processes(SIGNAL_NEW_TEXT); /* modified on 2026-07-05, PL */

    rc = failed ? -1 : 0;

cleanup:
    free_msgrefs(refs);
    free_skrefs(skrefs);
    free_msgitems(items);
    ftn_config_free(&config);

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

        if (!conf_is_ftn(ce.type))
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
    struct ftn_config config;
    const struct ftn_area *ftn_area;
    struct msgref *refs = NULL;
    struct skref *skrefs = NULL;
    struct msgitem *items = NULL;
    struct msgitem *m;
    struct planref *plans = NULL;
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

    ftn_config_init(&config);
    ftn_area = NULL;

    printf("ftntoss diagnose starting\n");
    printf("=========================\n\n");

    if (find_ftn_conf(area, &ce) != 0)
        goto cleanup;

    if (!conf_is_ftn(ce.type)) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce.name, ce.type);
        goto cleanup;
    }

    if (load_echomail_area_config(area, &ce, &config, &ftn_area) != 0)
        goto cleanup;

    printf("FTN diagnose setup\n");
    printf("------------------\n");
    printf("Domain:      %s\n", ftn_area->domain);
    printf("Area:        %s\n", ftn_area->tag);
    printf("Spool:       %s\n", ftn_area->path);
    printf("Conf name:   %s\n", ce.name);
    printf("Conf num:    %d\n", ce.num);
    printf("Conf type:   %d (FTN_CONF)\n", ce.type);
    printf("Last text:   %ld\n\n", ce.last_text);
	printf("Include unsafe: %s\n\n", include_unsafe ? "yes" : "no");
	
    if (scan_existing_skl_msgids(&ce, &ftn_area->aka, &skrefs,
            &existing_indexed) != 0)
        goto cleanup;

    if (build_spool_index(ftn_area->path, &refs, &items, &seen,
            &indexed, &failed) != 0)
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
    printf("Domain:           %s\n", ftn_area->domain);
    printf("Area:             %s\n", ftn_area->tag);
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
    ftn_config_free(&config);
    return rc;
}
static int
dump_one_import(const char *area, const struct ftn_conf_info *ce,
    const char *filename)
{
    struct ftn_config config;
    const struct ftn_area *ftn_area;
    struct skref *skrefs = NULL;
    struct msgref *refs = NULL;
    struct msgitem *items = NULL;
    struct planref *plans = NULL;
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

    ftn_config_init(&config);
    ftn_area = NULL;

    if (area == NULL || ce == NULL || filename == NULL)
        goto cleanup;

    if (!conf_is_ftn(ce->type)) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce->name, ce->type);
        goto cleanup;
    }

    if (load_echomail_area_config(area, ce, &config, &ftn_area) != 0)
        goto cleanup;

    printf("\n");
    printf("FTN dump-import setup\n");
    printf("---------------------\n");
    printf("Domain:      %s\n", ftn_area->domain);
    printf("Area:        %s\n", ftn_area->tag);
    printf("Spool:       %s\n", ftn_area->path);
    printf("Conf name:   %s\n", ce->name);
    printf("Conf num:    %d\n", ce->num);
    printf("Conf type:   %d", ce->type);
    if (conf_is_ftn(ce->type))
        printf(" (FTN_CONF)");
    printf("\n");
    printf("Last text:   %ld\n", ce->last_text);
    printf("Target file: %s\n", filename);
    printf("\n");

    if (scan_existing_skl_msgids(ce, &ftn_area->aka, &skrefs,
            &existing_indexed) != 0)
        goto cleanup;

    if (build_spool_index(ftn_area->path, &refs, &items, &seen,
            &indexed, &failed) != 0)
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

    rc = dump_import_text(ftn_area->tag, ce, filename, items, plans);

cleanup:
    free_msgrefs(refs);
    free_skrefs(skrefs);
    free_msgitems(items);
    free_planrefs(plans);
    ftn_config_free(&config);

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
    struct stat conf_st; /* modified on 2026-07-09, PL */
    int found = 0;

    if (new_textnum == NULL)
        return -1;

    if (snprintf(tmpfile, sizeof(tmpfile), "%s.ftntoss.tmp", CONF_FILE) >= (int)sizeof(tmpfile)) {
        fprintf(stderr, "[ERROR] CONF_FILE temp path too long\n");
        return -1;
    }

    if (stat(CONF_FILE, &conf_st) == -1) {
        perror(CONF_FILE);
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

    if (ftntoss_fix_fd_like_stat(out, tmpfile, &conf_st) != 0) {
        fclose(in);
        fclose(out);
        unlink(tmpfile);
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
     *   - timestamp is taken from the FTN .MSG date when possible
     *
     * modified on 2026-06-15, PL
     */
	mbuf = build_ftn_mbuf(area, msg, unsafe_reason);
    if (mbuf == NULL) {
        fprintf(stderr, "[ERROR] Could not build FTN message buffer\n");
        return -1;
    }

    size = count_body_lines(mbuf);
	 if (parse_fido_msg_date(msg->date, &now) != 0)
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

    if (ftntoss_fix_fd_to_sklaff(fp, path, 0600) != 0) {
        fclose(fp);
        unlink(path);
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
    struct ftn_config config;
    const struct ftn_area *ftn_area;
    struct skref *skrefs = NULL;
    struct msgref *refs = NULL;
    struct msgitem *items = NULL;
    struct msgitem *m;
    struct planref *plans = NULL;
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

    ftn_config_init(&config);
    ftn_area = NULL;

    if (area == NULL || ce == NULL)
        return -1;

    if (!conf_is_ftn(ce->type)) {
        fprintf(stderr, "[ERROR] Conference '%s' exists, but is not FTN_CONF (type=%d)\n",
            ce->name, ce->type);
        ftn_config_free(&config);
        return -1;
    }

    if (load_echomail_area_config(area, ce, &config, &ftn_area) != 0)
        return -1;

    printf("\n");
    printf("FTN dry-run setup\n");
    printf("-----------------\n");
    printf("Domain:      %s\n", ftn_area->domain);
    printf("Area:        %s\n", ftn_area->tag);
    printf("Spool:       %s\n", ftn_area->path);
    printf("Conf name:   %s\n", ce->name);
    printf("Conf num:    %d\n", ce->num);
    printf("Conf type:   %d", ce->type);
    if (conf_is_ftn(ce->type))
        printf(" (FTN_CONF)");
    printf("\n");
    printf("Last text:   %ld\n", ce->last_text);
    printf("\n");

    if (scan_existing_skl_msgids(ce, &ftn_area->aka, &skrefs,
            &existing_indexed) != 0) {
        ftn_config_free(&config);
        return -1;
    }

    if (build_spool_index(ftn_area->path, &refs, &items, &seen,
            &indexed, &failed) != 0) {
        free_skrefs(skrefs);
        free_msgrefs(refs);
        free_msgitems(items);
        ftn_config_free(&config);
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
        ftn_config_free(&config);
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
    printf("Domain:        %s\n", ftn_area->domain);
    printf("Area:          %s\n", ftn_area->tag);
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
    ftn_config_free(&config);

    return failed ? -1 : 0;
}

/*
 * import_all_netmail_spools - import every CrashMail NETMAIL messagebase
 * args: CrashMail preferences file
 * ret: success (0) or error (-1)
 *
 * Distinct NETMAIL areas may belong to different FTN domains and use
 * different MSG directories.  A shared path is only safe for aliases with
 * the same domain and local AKA; otherwise the imported message would have
 * ambiguous 5D routing metadata.
 *
 * modified on 2026-07-16, PL
 */
static int
import_all_netmail_spools(const char *prefsfile)
{
    struct ftn_config config;
    char error[512];
    size_t i;
    long found;
    long imported_ok;
    long failed;
    long duplicate_paths;

    if (prefsfile == NULL || *prefsfile == '\0')
        return -1;

    ftn_config_init(&config);
    found = 0;
    imported_ok = 0;
    failed = 0;
    duplicate_paths = 0;

    if (ftn_config_load_crashmail(prefsfile, &config,
            error, sizeof(error)) != 0) {
        fprintf(stderr, "[ERROR] ftntoss: %s\n", error);
        ftn_config_free(&config);
        return -1;
    }

    printf("ftntoss import-netmail starting\n");
    printf("===============================\n\n");
    printf("CrashMail prefs: %s\n\n", prefsfile);

    for (i = 0; i < config.area_count; i++) {
        const struct ftn_area *area;
        char aka[64];
        const struct ftn_area *same_path_area;
        size_t j;

        area = &config.areas[i];

        if (area->type != FTN_AREA_NETMAIL)
            continue;

        found++;
        same_path_area = NULL;

        for (j = 0; j < i; j++) {
            const struct ftn_area *previous;

            previous = &config.areas[j];

            if (previous->type != FTN_AREA_NETMAIL)
                continue;

            if (area->path[0] != '\0' &&
                strcmp(previous->path, area->path) == 0) {
                same_path_area = previous;
                break;
            }
        }

        ftn_address_format(&area->aka, aka, sizeof(aka));

        printf("NETMAIL area\n");
        printf("------------\n");
        printf("Tag:         %s\n", area->tag);
        printf("Domain:      %s\n",
            area->domain[0] ? area->domain : "(missing)");
        printf("Local AKA:   %s\n", aka);
        printf("Messagebase: %s\n",
            area->messagebase[0] ? area->messagebase : "(missing)");
        printf("Spool:       %s\n",
            area->path[0] ? area->path : "(missing)");

        if (same_path_area != NULL) {
            if (strcasecmp(same_path_area->domain, area->domain) == 0 &&
                ftn_address_equal(&same_path_area->aka, &area->aka)) {
                printf("Result:      skipped alias for same spool context\n\n");
                duplicate_paths++;
                continue;
            }

            fprintf(stderr,
                "[ERROR] NETMAIL areas '%s@%s' and '%s@%s' share spool "
                "path %s but use different FTN contexts\n",
                same_path_area->tag, same_path_area->domain,
                area->tag, area->domain, area->path);
            fprintf(stderr,
                "[ERROR] Use one NETMAIL MSG directory per FTN domain/AKA "
                "so incoming 5D reply metadata is unambiguous\n");
            printf("Result:      FAILED ambiguous shared spool\n\n");
            failed++;
            continue;
        }

        if (area->messagebase[0] == '\0' ||
            strcasecmp(area->messagebase, "MSG") != 0) {
            fprintf(stderr,
                "[ERROR] NETMAIL area '%s' uses unsupported messagebase "
                "'%s'; only MSG is currently supported\n",
                area->tag,
                area->messagebase[0] ? area->messagebase : "(missing)");
            printf("Result:      FAILED\n\n");
            failed++;
            continue;
        }

        if (area->path[0] == '\0') {
            fprintf(stderr,
                "[ERROR] NETMAIL area '%s' has no messagebase path\n",
                area->tag);
            printf("Result:      FAILED\n\n");
            failed++;
            continue;
        }

        if (area->domain[0] == '\0') {
            fprintf(stderr,
                "[ERROR] NETMAIL area '%s' has no resolved FTN domain\n",
                area->tag);
            printf("Result:      FAILED\n\n");
            failed++;
            continue;
        }

        if (import_ftn_netmail_spool_5d(area->path, area->domain,
                aka) != 0) {
            fprintf(stderr,
                "[ERROR] Netmail import failed for %s@%s (%s)\n",
                area->tag, area->domain, area->path);
            printf("Result:      FAILED\n\n");
            failed++;
            continue;
        }

        printf("Result:      OK\n\n");
        imported_ok++;
    }

    printf("FTN import-netmail summary\n");
    printf("--------------------------\n");
    printf("NETMAIL areas:   %ld\n", found);
    printf("Spools OK:       %ld\n", imported_ok);
    printf("Alias paths:     %ld\n", duplicate_paths);
    printf("Failed:          %ld\n", failed);

    if (found == 0) {
        fprintf(stderr,
            "[ERROR] No NETMAIL areas were found in %s\n",
            prefsfile);
        failed++;
    }

    ftn_config_free(&config);
    return failed ? -1 : 0;
}

static int
run_export_one_locked(void *arg)
{
    struct export_one_args *a = arg;

    if (a == NULL)
        return -1;

    return export_one_ftn(a->area, a->textnum);
}

static int
run_export_one_conf_locked(void *arg)
{
    struct export_one_conf_args *a = arg;

    if (a == NULL)
        return -1;

    return export_one_ftn_conf(a->confnum, a->textnum);
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

static int
run_import_netmail_locked(void *arg)
{
    struct import_netmail_args *a = arg;

    if (a == NULL || a->prefsfile == NULL)
        return -1;

    return import_all_netmail_spools(a->prefsfile);
}

static int
run_export_netmail_job_locked(void *arg)
{
    struct export_netmail_job_args *a = arg;

    if (a == NULL || a->jobfile == NULL)
        return -1;

    return export_netmail_job(a->jobfile);
}

static int
run_export_ibol_job_locked(void *arg)
{
    struct export_ibol_job_args *a = arg;

    if (a == NULL || a->jobfile == NULL)
        return -1;

    return export_ibol_job(a->jobfile);
}

static int
load_meeting_ftnconf(const struct ftn_conf_info *ce,
    struct meeting_ftn_config *meeting, int *out_found)
{
    FILE *fp;
    char path[PATH_MAX];
    char line[1024];
    char section[32];
    int seen_version;
    int seen_type;
    int seen_domain;
    int seen_tag;

    if (ce == NULL || meeting == NULL || out_found == NULL)
        return -1;

    memset(meeting, 0, sizeof(*meeting));
    *out_found = 0;
    section[0] = '\0';
    seen_version = 0;
    seen_type = 0;
    seen_domain = 0;
    seen_tag = 0;

    if (snprintf(path, sizeof(path), "%s/%d/ftnconf",
            SKLAFF_DB, ce->num) >= (int)sizeof(path)) {
        fprintf(stderr,
            "[ERROR] ftntoss: ftnconf path is too long for conference %d\n",
            ce->num);
        return -1;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT)
            return 0;

        fprintf(stderr, "[ERROR] ftntoss: could not open %s: %s\n",
            path, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *end;

        strip_eol(line);
        trim_left(line);

        end = line + strlen(line);
        while (end > line && isspace((unsigned char)end[-1]))
            *--end = '\0';

        if (line[0] == '\0' || line[0] == '#')
            continue;

        if (line[0] == '!' && line[1] == '[') {
            size_t len;

            end = strchr(line + 2, ']');
            if (end == NULL || end[1] != '\0') {
                fprintf(stderr,
                    "[ERROR] ftntoss: malformed section header in %s: %s\n",
                    path, line);
                fclose(fp);
                return -1;
            }

            len = (size_t)(end - (line + 2));
            if (len == 0 || len >= sizeof(section)) {
                fprintf(stderr,
                    "[ERROR] ftntoss: invalid section name in %s: %s\n",
                    path, line);
                fclose(fp);
                return -1;
            }

            memcpy(section, line + 2, len);
            section[len] = '\0';
            continue;
        }

        if (section[0] == '\0') {
            fprintf(stderr,
                "[ERROR] ftntoss: value outside a ![section] in %s: %s\n",
                path, line);
            fclose(fp);
            return -1;
        }

        if (strcasecmp(section, "version") == 0) {
            char *endp;
            long version;

            if (seen_version) {
                fprintf(stderr,
                    "[ERROR] ftntoss: duplicate ![version] in %s\n", path);
                fclose(fp);
                return -1;
            }

            errno = 0;
            version = strtol(line, &endp, 10);
            if (errno != 0 || endp == line || *endp != '\0' ||
                version < 0 || version > INT_MAX) {
                fprintf(stderr,
                    "[ERROR] ftntoss: invalid version in %s: %s\n",
                    path, line);
                fclose(fp);
                return -1;
            }

            meeting->version = (int)version;
            seen_version = 1;
        } else if (strcasecmp(section, "type") == 0) {
            if (seen_type) {
                fprintf(stderr,
                    "[ERROR] ftntoss: duplicate ![type] in %s\n", path);
                fclose(fp);
                return -1;
            }
            strlcpy(meeting->type, line, sizeof(meeting->type));
            seen_type = 1;
        } else if (strcasecmp(section, "domain") == 0) {
            if (seen_domain) {
                fprintf(stderr,
                    "[ERROR] ftntoss: duplicate ![domain] in %s\n", path);
                fclose(fp);
                return -1;
            }
            strlcpy(meeting->domain, line, sizeof(meeting->domain));
            seen_domain = 1;
        } else if (strcasecmp(section, "tag") == 0) {
            if (seen_tag) {
                fprintf(stderr,
                    "[ERROR] ftntoss: duplicate ![tag] in %s\n", path);
                fclose(fp);
                return -1;
            }
            strlcpy(meeting->tag, line, sizeof(meeting->tag));
            seen_tag = 1;
        } else {
            fprintf(stderr,
                "[ERROR] ftntoss: unknown section ![%s] in %s\n",
                section, path);
            fclose(fp);
            return -1;
        }

        section[0] = '\0';
    }

    if (ferror(fp)) {
        fprintf(stderr, "[ERROR] ftntoss: error while reading %s\n", path);
        fclose(fp);
        return -1;
    }

    fclose(fp);

    if (!seen_version || !seen_type || !seen_domain || !seen_tag) {
        fprintf(stderr,
            "[ERROR] ftntoss: incomplete %s; required sections are "
            "version, type, domain and tag\n",
            path);
        return -1;
    }

    if (meeting->version != 1) {
        fprintf(stderr,
            "[ERROR] ftntoss: unsupported ftnconf version %d in %s "
            "(supported: 1)\n",
            meeting->version, path);
        return -1;
    }

    if (strcasecmp(meeting->type, "echomail") != 0) {
        fprintf(stderr,
            "[ERROR] ftntoss: unsupported ftnconf type '%s' in %s "
            "(expected: echomail)\n",
            meeting->type, path);
        return -1;
    }

    if (meeting->domain[0] == '\0' || meeting->tag[0] == '\0') {
        fprintf(stderr,
            "[ERROR] ftntoss: domain and tag must not be empty in %s\n",
            path);
        return -1;
    }

    *out_found = 1;
    return 0;
}

static int
load_echomail_area_config(const char *fallback_tag,
    const struct ftn_conf_info *ce, struct ftn_config *config,
    const struct ftn_area **out_area)
{
    struct meeting_ftn_config meeting;
    const struct ftn_area *found;
    const char *wanted_domain;
    const char *wanted_tag;
    char error[512];
    size_t i;
    size_t matches;
    int has_ftnconf;

    if (fallback_tag == NULL || *fallback_tag == '\0' || ce == NULL ||
        config == NULL || out_area == NULL)
        return -1;

    *out_area = NULL;
    found = NULL;
    matches = 0;
    has_ftnconf = 0;

    if (load_meeting_ftnconf(ce, &meeting, &has_ftnconf) != 0)
        return -1;

    wanted_domain = has_ftnconf ? meeting.domain : NULL;
    wanted_tag = has_ftnconf ? meeting.tag : fallback_tag;

    ftn_config_init(config);

    if (ftn_config_load_crashmail(CRASHMAIL_PREFS_FILE, config,
            error, sizeof(error)) != 0) {
        fprintf(stderr, "[ERROR] ftntoss: %s\n", error);
        return -1;
    }

    for (i = 0; i < config->area_count; i++) {
        const struct ftn_area *candidate;

        candidate = &config->areas[i];

        if (candidate->type != FTN_AREA_ECHOMAIL)
            continue;

        if (strcasecmp(candidate->tag, wanted_tag) != 0)
            continue;

        if (wanted_domain != NULL &&
            strcasecmp(candidate->domain, wanted_domain) != 0)
            continue;

        found = candidate;
        matches++;
    }

    if (matches == 0) {
        if (has_ftnconf) {
            fprintf(stderr,
                "[ERROR] ftntoss: ftnconf for conference '%s' points to "
                "echomail area %s@%s, but it was not found in %s\n",
                ce->name, meeting.tag, meeting.domain,
                CRASHMAIL_PREFS_FILE);
        } else {
            fprintf(stderr,
                "[ERROR] ftntoss: echomail area '%s' was not found in %s\n",
                fallback_tag, CRASHMAIL_PREFS_FILE);
            fprintf(stderr,
                "[ERROR] ftntoss: create %s/%d/ftnconf when the "
                "SklaffKOM conference name differs from the FTN tag\n",
                SKLAFF_DB, ce->num);
        }
        ftn_config_free(config);
        return -1;
    }

    if (matches > 1) {
        if (has_ftnconf) {
            fprintf(stderr,
                "[ERROR] ftntoss: %s contains more than one echomail area "
                "matching %s@%s\n",
                CRASHMAIL_PREFS_FILE, meeting.tag, meeting.domain);
        } else {
            fprintf(stderr,
                "[ERROR] ftntoss: echomail tag '%s' occurs in more than one "
                "FTN domain in %s\n",
                fallback_tag, CRASHMAIL_PREFS_FILE);
            fprintf(stderr,
                "[ERROR] ftntoss: create %s/%d/ftnconf and specify domain\n",
                SKLAFF_DB, ce->num);
        }
        ftn_config_free(config);
        return -1;
    }

    if (found->messagebase[0] == '\0' ||
        strcasecmp(found->messagebase, "MSG") != 0) {
        fprintf(stderr,
            "[ERROR] ftntoss: area '%s' uses unsupported messagebase '%s'; "
            "only MSG is currently supported\n",
            found->tag,
            found->messagebase[0] ? found->messagebase : "(missing)");
        ftn_config_free(config);
        return -1;
    }

    if (found->path[0] == '\0') {
        fprintf(stderr,
            "[ERROR] ftntoss: area '%s' has no messagebase path in %s\n",
            found->tag, CRASHMAIL_PREFS_FILE);
        ftn_config_free(config);
        return -1;
    }

    *out_area = found;
    return 0;
}

static int
load_echomail_area(const char *fallback_tag,
    const struct ftn_conf_info *ce, struct ftn_config *config,
    const struct ftn_area **out_area, const struct ftn_link **out_feed)
{
    const struct ftn_area *found;
    const struct ftn_link *feed;
    size_t i;
    size_t primary_feeds;

    if (fallback_tag == NULL || *fallback_tag == '\0' || ce == NULL ||
        config == NULL || out_area == NULL || out_feed == NULL)
        return -1;

    *out_area = NULL;
    *out_feed = NULL;

    if (load_echomail_area_config(fallback_tag, ce, config, out_area) != 0)
        return -1;

    found = *out_area;
    feed = NULL;
    primary_feeds = 0;

    for (i = 0; i < found->link_count; i++) {
        if (found->links[i].modifier != '%')
            continue;

        feed = &found->links[i];
        primary_feeds++;
    }

    if (primary_feeds == 0 && found->link_count == 1)
        feed = &found->links[0];

    if (primary_feeds > 1) {
        fprintf(stderr,
            "[ERROR] ftntoss: area '%s' has more than one %% feed in %s\n",
            found->tag, CRASHMAIL_PREFS_FILE);
        fprintf(stderr,
            "[ERROR] ftntoss: mark exactly one EXPORT address with %%\n");
        ftn_config_free(config);
        *out_area = NULL;
        return -1;
    }

    if (feed == NULL) {
        fprintf(stderr,
            "[ERROR] ftntoss: area '%s' has no unique CrashMail feed\n",
            found->tag);
        fprintf(stderr,
            "[ERROR] ftntoss: mark exactly one EXPORT address with %%\n");
        ftn_config_free(config);
        *out_area = NULL;
        return -1;
    }

    *out_feed = feed;
    return 0;
}

/*
 * load_echomail_area_by_identity - resolve a non-conference echomail target
 * args: FTN domain, echo tag, loaded-config output, area output, feed output
 * ret: success (0) or error (-1)
 *
 * IBOL lives in a data echo rather than a normal SklaffKOM conference.
 * Resolve it directly from CrashMail configuration while keeping the same
 * unique-feed rules as normal FTN conference export.
 *
 * modified on 2026-08-13, PL
 */
static int
load_echomail_area_by_identity(const char *domain, const char *tag,
    struct ftn_config *config, const struct ftn_area **out_area,
    const struct ftn_link **out_feed)
{
    const struct ftn_area *found;
    const struct ftn_link *feed;
    char error[512];
    size_t i;
    size_t matches;
    size_t primary_feeds;

    if (domain == NULL || *domain == '\0' ||
        tag == NULL || *tag == '\0' ||
        config == NULL || out_area == NULL || out_feed == NULL)
        return -1;

    *out_area = NULL;
    *out_feed = NULL;
    found = NULL;
    feed = NULL;
    matches = 0;
    primary_feeds = 0;

    ftn_config_init(config);

    if (ftn_config_load_crashmail(CRASHMAIL_PREFS_FILE, config,
            error, sizeof(error)) != 0) {
        fprintf(stderr, "[ERROR] ftntoss: %s\n", error);
        return -1;
    }

    for (i = 0; i < config->area_count; i++) {
        const struct ftn_area *candidate;

        candidate = &config->areas[i];

        if (candidate->type != FTN_AREA_ECHOMAIL)
            continue;
        if (strcasecmp(candidate->tag, tag) != 0)
            continue;
        if (strcasecmp(candidate->domain, domain) != 0)
            continue;

        found = candidate;
        matches++;
    }

    if (matches == 0) {
        fprintf(stderr,
            "[ERROR] ftntoss: echomail area %s@%s was not found in %s\n",
            tag, domain, CRASHMAIL_PREFS_FILE);
        ftn_config_free(config);
        return -1;
    }

    if (matches > 1) {
        fprintf(stderr,
            "[ERROR] ftntoss: more than one echomail area matches %s@%s "
            "in %s\n",
            tag, domain, CRASHMAIL_PREFS_FILE);
        ftn_config_free(config);
        return -1;
    }

    if (found->messagebase[0] == '\0' ||
        strcasecmp(found->messagebase, "MSG") != 0) {
        fprintf(stderr,
            "[ERROR] ftntoss: area '%s' uses unsupported messagebase '%s'; "
            "only MSG is currently supported\n",
            found->tag,
            found->messagebase[0] ? found->messagebase : "(missing)");
        ftn_config_free(config);
        return -1;
    }

    if (found->path[0] == '\0') {
        fprintf(stderr,
            "[ERROR] ftntoss: area '%s' has no messagebase path in %s\n",
            found->tag, CRASHMAIL_PREFS_FILE);
        ftn_config_free(config);
        return -1;
    }

    for (i = 0; i < found->link_count; i++) {
        if (found->links[i].modifier != '%')
            continue;

        feed = &found->links[i];
        primary_feeds++;
    }

    if (primary_feeds == 0 && found->link_count == 1)
        feed = &found->links[0];

    if (primary_feeds > 1) {
        fprintf(stderr,
            "[ERROR] ftntoss: area '%s' has more than one %% feed in %s\n",
            found->tag, CRASHMAIL_PREFS_FILE);
        ftn_config_free(config);
        return -1;
    }

    if (feed == NULL) {
        fprintf(stderr,
            "[ERROR] ftntoss: area '%s' has no unique CrashMail feed\n",
            found->tag);
        ftn_config_free(config);
        return -1;
    }

    *out_area = found;
    *out_feed = feed;
    return 0;
}

static int
dump_ftn_config_file(const char *path)
{
    struct ftn_config config;
    char error[512];

    ftn_config_init(&config);

    if (ftn_config_load_crashmail(path, &config,
            error, sizeof(error)) != 0) {
        fprintf(stderr, "[ERROR] ftntoss: %s\n", error);
        ftn_config_free(&config);
        return -1;
    }

    printf("CrashMail prefs: %s\n\n", path);
    ftn_config_dump(stdout, &config);
    ftn_config_free(&config);
    return 0;
}

int
main(int argc, char **argv)
{
    struct ftn_conf_info ce;
    
    if (argc == 2 && strcmp(argv[1], "--dump-ftn-config") == 0) {
        return dump_ftn_config_file(CRASHMAIL_PREFS_FILE) == 0 ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--dump-ftn-config") == 0) {
        return dump_ftn_config_file(argv[2]) == 0 ? 0 : 1;
    }
    
        if (argc == 4 && strcmp(argv[1], "--export-one") == 0) {
        struct export_one_args a;
        char *endp;
        long textnum;

        errno = 0;
        textnum = strtol(argv[3], &endp, 10);
        if (errno != 0 || endp == argv[3] || *endp != '\0' || textnum <= 0) {
            fprintf(stderr, "[ERROR] Invalid text number: %s\n", argv[3]);
            return 1;
        }

        a.area = argv[2];
        a.textnum = textnum;

        return run_with_lock(run_export_one_locked, &a) == 0 ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "--export-one-conf") == 0) {
        struct export_one_conf_args a;
        char *endp;
        long confnum;
        long textnum;

        errno = 0;
        confnum = strtol(argv[2], &endp, 10);
        if (errno != 0 || endp == argv[2] || *endp != '\0' ||
            confnum <= 0 || confnum > INT_MAX) {
            fprintf(stderr, "[ERROR] Invalid conference number: %s\n", argv[2]);
            return 1;
        }

        errno = 0;
        textnum = strtol(argv[3], &endp, 10);
        if (errno != 0 || endp == argv[3] || *endp != '\0' || textnum <= 0) {
            fprintf(stderr, "[ERROR] Invalid text number: %s\n", argv[3]);
            return 1;
        }

        a.confnum = (int)confnum;
        a.textnum = textnum;

        return run_with_lock(run_export_one_conf_locked, &a) == 0 ? 0 : 1;
    }
    
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

	if (argc == 2 && strcmp(argv[1], "--import-netmail") == 0) {
        struct import_netmail_args a;

        a.prefsfile = CRASHMAIL_PREFS_FILE;

        return run_with_lock(run_import_netmail_locked, &a) == 0 ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--import-netmail") == 0) {
        struct import_netmail_args a;

        a.prefsfile = argv[2];

        return run_with_lock(run_import_netmail_locked, &a) == 0 ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--export-ibol-job") == 0) {
        struct export_ibol_job_args a;

        a.jobfile = argv[2];

        return run_with_lock(run_export_ibol_job_locked, &a) == 0 ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "--export-netmail-job") == 0) {
        struct export_netmail_job_args a;

        a.jobfile = argv[2];

        return run_with_lock(run_export_netmail_job_locked, &a) == 0 ? 0 : 1;
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
        fprintf(stderr, "       %s --export-test <FTN-area>\n", argv[0]);
        fprintf(stderr, "       %s --export-one <FTN-area> <textnum>\n", argv[0]);
        fprintf(stderr, "       %s --export-one-conf <confnum> <textnum>\n", argv[0]);
        fprintf(stderr, "       %s --export-netmail-job <jobfile>\n", argv[0]);
        fprintf(stderr, "       %s --export-ibol-job <jobfile>\n", argv[0]);
        fprintf(stderr, "       %s --import-netmail <crashmail.prefs>\n", argv[0]);
        fprintf(stderr, "       %s --dump-ftn-config\n", argv[0]);
        fprintf(stderr, "       %s --dump-ftn-config <crashmail.prefs>\n\n", argv[0]);
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

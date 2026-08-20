/* ftn.c */

/*
 *   SklaffKOM, a simple conference system for UNIX.
 *
 *   Copyright (C) 1993-1994  Torbj|rn B}}th, Peter Forsberg, Peter Lindberg,
 *                            Odd Petersson, Carl Sundbom
 *
 *   Program dedicated to the memory of Staffan Bergstr|m.
 *
 *   For questions about this program, mail sklaff@sklaffkom.se
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* Improved 5D netmail address support PL 2026-08-07 */
#include <time.h>
#include <unistd.h>
#include <sys/stat.h> /* chmod queue jobs (2026-07-10, PL) */

#include "../sklaff.h"

#define FTNQUEUE_DIR     SKLAFFDIR "/ftnqueue"         /* modified on 2026-06-14, PL */
#define FTNQUEUE_TMP     SKLAFFDIR "/ftnqueue/tmp"     /* modified on 2026-06-14, PL */
#define FTNQUEUE_PENDING SKLAFFDIR "/ftnqueue/pending" /* modified on 2026-06-14, PL */

#define FTNQUEUE_NETMAIL_TMP \
    SKLAFFDIR "/ftnqueue/netmail/tmp" /* modified on 2026-07-10, PL */

#define FTNQUEUE_NETMAIL_PENDING \
    SKLAFFDIR "/ftnqueue/netmail/pending" /* modified on 2026-07-10, PL */

int
ftn_netmail_address_needs_domain(const char *addr)
{
    FILE *fp;
    char line[1024];
    char *p, *q, *end;
    long wanted_zone, zone;
    int matches;

    if (addr == NULL || *addr == '\0')
        return 0;

    /*
     * A 5D address is already explicit and cannot be ambiguous here.
     *
     * modified on 2026-08-07, PL
     */
    if (strchr(addr, '@') != NULL)
        return 0;

    wanted_zone = strtol(addr, &end, 10);
    if (end == addr || *end != ':' ||
        wanted_zone < 0 || wanted_zone > 65535)
        return 0;

    fp = fopen(CRASHMAIL_PREFS_FILE, "r");
    if (fp == NULL) {
        /*
         * Do not break otherwise valid 4D netmail merely because the
         * configuration cannot be inspected here.  ftntoss will perform
         * its normal validation later.
         */
        dlog(2, "ftn_netmail_address_needs_domain: cannot open %s",
            CRASHMAIL_PREFS_FILE);
        return 0;
    }

    matches = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        p = line;

        while (*p != '\0' && isspace((unsigned char)*p))
            p++;

        if (*p == '\0' || *p == ';' || *p == '#')
            continue;

        if (strncasecmp(p, "NETMAIL", 7) != 0 ||
            !isspace((unsigned char)p[7]))
            continue;

        p += 7;

        while (*p != '\0' && isspace((unsigned char)*p))
            p++;

        /*
         * Skip the NETMAIL tag, normally:
         *
         *   NETMAIL "FIDO_NETMAIL" 2:221/250.0 ...
         */
        if (*p == '"') {
            p++;
            q = strchr(p, '"');
            if (q == NULL)
                continue;
            p = q + 1;
        } else {
            while (*p != '\0' && !isspace((unsigned char)*p))
                p++;
        }

        while (*p != '\0' && isspace((unsigned char)*p))
            p++;

        zone = strtol(p, &end, 10);
        if (end == p || *end != ':')
            continue;

        if (zone == wanted_zone) {
            matches++;

            if (matches > 1) {
                fclose(fp);
                return 1;
            }
        }
    }

    fclose(fp);
    return 0;
}

static int
is_safe_ftn_queue_header_value(const char *s)
{
    const unsigned char *p;

    if (s == NULL)
        return 0;

    /*
     * Netmail queue jobs use simple "KEY: value" headers.  Do not allow
     * embedded newlines in header values, or one field could inject another.
     *
     * modified on 2026-07-10, PL
     */
    for (p = (const unsigned char *)s; *p != '\0'; p++) {
        if (*p == '\r' || *p == '\n')
            return 0;
    }

    return 1;
}

static int
parse_ftn_address(const char *s, char *out, size_t outsz)
{
    char buf[128];
    char *domain;
    const char *p;
    char *end;
    long zone, net, node, point;
    const unsigned char *q;

    if (s == NULL || *s == '\0' || out == NULL || outsz == 0)
        return -1;

    out[0] = '\0';

    if (strlen(s) >= sizeof(buf))
        return -1;

    strcpy(buf, s);

    /*
     * Optional FTN domain:
     *
     *   zone:net/node[.point]@domain
     *
     * modified on 2026-08-07, PL
     */
    domain = strchr(buf, '@');
    if (domain != NULL) {
        *domain++ = '\0';

        if (*domain == '\0' || strchr(domain, '@') != NULL)
            return -1;

        /*
         * Keep domains deliberately boring.  This covers fidonet,
         * fsxnet, nixnet, tqwnet etc.
         */
        for (q = (const unsigned char *)domain; *q != '\0'; q++) {
            if (!isalnum(*q) &&
                *q != '.' && *q != '-' && *q != '_')
                return -1;
        }
    }

    p = buf;

    zone = strtol(p, &end, 10);
    if (p == end || *end != ':' || zone < 0 || zone > 65535)
        return -1;

    p = end + 1;
    net = strtol(p, &end, 10);
    if (p == end || *end != '/' || net < 0 || net > 65535)
        return -1;

    p = end + 1;
    node = strtol(p, &end, 10);
    if (p == end || node < 0 || node > 65535)
        return -1;

    point = 0;

    if (*end == '.') {
        p = end + 1;
        point = strtol(p, &end, 10);

        if (p == end || point < 0 || point > 65535)
            return -1;
    }

    if (*end != '\0')
        return -1;

    if (domain != NULL) {
        if (point == 0) {
            snprintf(out, outsz, "%ld:%ld/%ld@%s",
                zone, net, node, domain);
        } else {
            snprintf(out, outsz, "%ld:%ld/%ld.%ld@%s",
                zone, net, node, point, domain);
        }
    } else {
        if (point == 0) {
            snprintf(out, outsz, "%ld:%ld/%ld",
                zone, net, node);
        } else {
            snprintf(out, outsz, "%ld:%ld/%ld.%ld",
                zone, net, node, point);
        }
    }

    if (out[0] == '\0')
        return -1;

    return 0;
}


int
parse_ftn_netmail_recipient(const char *s, char *name, size_t namesz,
    char *addr, size_t addrsz)
{
    const char *at;
    size_t namelen;

    if (s == NULL || name == NULL || namesz == 0 ||
        addr == NULL || addrsz == 0)
        return -1;

    name[0] = '\0';
    addr[0] = '\0';

    /*
     * Supported forms:
     *
     *   Name@zone:net/node[.point]
     *   Name@zone:net/node[.point]@domain
     *
     * Do not blindly use the first or last '@'.  Try each '@' as the
     * separator between display name and FTN address, and accept the
     * first one followed by a valid FTN address.
     *
     * This also preserves odd display names containing '@'.
     *
     * modified on 2026-08-07, PL
     */
    at = strchr(s, '@');

    while (at != NULL) {
        if (at != s && at[1] != '\0' &&
            parse_ftn_address(at + 1, addr, addrsz) == 0) {

            namelen = (size_t)(at - s);

            if (namelen >= namesz) {
                addr[0] = '\0';
                return -1;
            }

            memcpy(name, s, namelen);
            name[namelen] = '\0';

            while (name[0] == ' ' || name[0] == '\t')
                memmove(name, name + 1, strlen(name));

            while (strlen(name) > 0 &&
                (name[strlen(name) - 1] == ' ' ||
                 name[strlen(name) - 1] == '\t'))
                name[strlen(name) - 1] = '\0';

            if (name[0] == '\0') {
                addr[0] = '\0';
                return -1;
            }

            return 0;
        }

        at = strchr(at + 1, '@');
    }

    addr[0] = '\0';
    return -1;
}

static int
queue_ftn_export(int confnum, long textnum)
{
    char tmpfile[4096];
    char pendingfile[4096];
    long now;
    FILE *fp;
    int n;

    if (confnum <= 0 || textnum <= 0)
        return -1;

    now = (long)time(NULL);

    /*
     * Echomail queue jobs identify the local SklaffKOM conference by its
     * numeric id, not by its display name or FTN echo tag:
     *
     *   CONFNUM:TEXTNUM:TIMESTAMP
     *
     * Conference names may contain spaces and ':' and are allowed to change.
     * The FTN domain/tag is resolved later by ftntoss from the conference's
     * ftnconf file.
     *
     * Write to tmp first and then rename into pending.  This makes queue
     * creation atomic, so the cron runner never sees a half-written job.
     *
     * modified on 2026-08-11, PL
     */
    n = snprintf(tmpfile, sizeof(tmpfile), "%s/%d.%ld.%ld.tmp",
        FTNQUEUE_TMP, confnum, textnum, (long)getpid());

    if (n < 0 || (size_t)n >= sizeof(tmpfile)) {
        dlog(2, "queue_ftn_export: tmp filename too long");
        return -1;
    }

    n = snprintf(pendingfile, sizeof(pendingfile), "%s/%d.%ld.%ld",
        FTNQUEUE_PENDING, confnum, textnum, now);

    if (n < 0 || (size_t)n >= sizeof(pendingfile)) {
        dlog(2, "queue_ftn_export: pending filename too long");
        return -1;
    }

    fp = fopen(tmpfile, "w");
    if (fp == NULL) {
        dlog(2, "queue_ftn_export: fopen failed for conf %d text %ld, errno=%d",
            confnum, textnum, errno);
        return -1;
    }

    if (fprintf(fp, "%d:%ld:%ld\n", confnum, textnum, now) < 0) {
        dlog(2, "queue_ftn_export: fprintf failed for conf %d text %ld, errno=%d",
            confnum, textnum, errno);
        fclose(fp);
        unlink(tmpfile);
        return -1;
    }

    if (fclose(fp) != 0) {
        dlog(2, "queue_ftn_export: fclose failed for conf %d text %ld, errno=%d",
            confnum, textnum, errno);
        unlink(tmpfile);
        return -1;
    }

    if (rename(tmpfile, pendingfile) != 0) {
        dlog(2, "queue_ftn_export: rename failed for conf %d text %ld, errno=%d",
            confnum, textnum, errno);
        unlink(tmpfile);
        return -1;
    }

    dlog(6, "queue_ftn_export: queued FTN export [conf=%d text=%ld created=%ld]",
        confnum, textnum, now);

    return 0;
}

/*
 * queue_ibol_oneliner - queue one InterBBS Oneliner for FTN export
 * args: local uid, SklaffKOM display name (SF7), one-line text (SF7)
 * ret: success (0) or error (-1)
 *
 * IBOL is transported through the ordinary FTN export runner, but uses a
 * typed queue job because it is not a normal SklaffKOM conference text.
 * The job is written atomically into the existing echomail queue so no new
 * queue-directory permissions are required.
 *
 * modified on 2026-08-13, PL
 */
int
queue_ibol_oneliner(int fromuid, const char *author, const char *text)
{
    static unsigned long sequence;
    char tmpfile[4096];
    char pendingfile[4096];
    long now;
    unsigned long seq;
    FILE *fp;
    int n;

    if (fromuid <= 0 || author == NULL || *author == '\0' ||
        text == NULL || *text == '\0')
        return -1;

    if (strlen(text) > IBOL_ONELINER_MAX) {
        dlog(2, "queue_ibol_oneliner: text longer than %d characters",
            IBOL_ONELINER_MAX);
        return -1;
    }

    if (!is_safe_ftn_queue_header_value(author) ||
        !is_safe_ftn_queue_header_value(text)) {
        dlog(2, "queue_ibol_oneliner: embedded newline in job value");
        return -1;
    }

    now = (long)time(NULL);
    seq = ++sequence;

    n = snprintf(tmpfile, sizeof(tmpfile),
        "%s/@ibol.%d.%ld.%ld.%lu.tmp",
        FTNQUEUE_TMP, fromuid, now, (long)getpid(), seq);
    if (n < 0 || (size_t)n >= sizeof(tmpfile)) {
        dlog(2, "queue_ibol_oneliner: tmp filename too long");
        return -1;
    }

    n = snprintf(pendingfile, sizeof(pendingfile),
        "%s/@ibol.%d.%ld.%ld.%lu",
        FTNQUEUE_PENDING, fromuid, now, (long)getpid(), seq);
    if (n < 0 || (size_t)n >= sizeof(pendingfile)) {
        dlog(2, "queue_ibol_oneliner: pending filename too long");
        return -1;
    }

    fp = fopen(tmpfile, "w");
    if (fp == NULL) {
        dlog(2, "queue_ibol_oneliner: fopen failed for uid %d, errno=%d",
            fromuid, errno);
        return -1;
    }

    if (fprintf(fp,
            "TYPE: IBOL\n"
            "FROMUID: %d\n"
            "AUTHOR: %s\n"
            "CREATED: %ld\n"
            "\n"
            "%s\n",
            fromuid, author, now, text) < 0) {
        dlog(2, "queue_ibol_oneliner: write failed for uid %d, errno=%d",
            fromuid, errno);
        fclose(fp);
        unlink(tmpfile);
        return -1;
    }

    if (fclose(fp) != 0) {
        dlog(2, "queue_ibol_oneliner: fclose failed for uid %d, errno=%d",
            fromuid, errno);
        unlink(tmpfile);
        return -1;
    }

    if (rename(tmpfile, pendingfile) != 0) {
        dlog(2, "queue_ibol_oneliner: rename failed for uid %d, errno=%d",
            fromuid, errno);
        unlink(tmpfile);
        return -1;
    }

    dlog(6, "queue_ibol_oneliner: queued IBOL uid %d [%s]",
        fromuid, author);

    return 0;
}

int
queue_ftn_netmail(int fromuid, const char *toname, const char *toaddr,
    const char *subject, const char *body)
{
    return queue_ftn_netmail_reply(fromuid, toname, toaddr, subject, body,
        NULL);
}

int
queue_ftn_netmail_reply(int fromuid, const char *toname, const char *toaddr,
    const char *subject, const char *body, const char *reply)
{
    char tmpfile[4096];
    char pendingfile[4096];
    long now;
    FILE *fp;
    int n;

    if (fromuid <= 0 || toname == NULL || *toname == '\0' ||
        toaddr == NULL || *toaddr == '\0' || body == NULL)
        return -1;

    if (subject == NULL)
        subject = "";

    if (reply == NULL)
        reply = "";

    if (!is_safe_ftn_queue_header_value(toname) ||
        !is_safe_ftn_queue_header_value(toaddr) ||
        !is_safe_ftn_queue_header_value(subject) ||
        !is_safe_ftn_queue_header_value(reply)) {
        dlog(2, "queue_ftn_netmail: unsafe header value");
        return -1;
    }

    now = (long)time(NULL);

    /*
     * Netmail queue format:
     *
     *   TYPE: NETMAIL
     *   FROMUID: <local uid>
     *   TONAME: <FTN recipient name>
     *   TOADDR: <zone:net/node[.point]>
     *   SUBJECT: <subject>
     *   REPLY: <original FTN MSGID, if any>
     *   CREATED: <timestamp>
     *
     *   <message body>
     *
     * Write to tmp first and then rename into pending, matching the existing
     * echomail export queue behaviour.
     *
     * modified on 2026-07-10, PL
     */
    n = snprintf(tmpfile, sizeof(tmpfile), "%s/netmail.%d.%ld.%ld.tmp",
        FTNQUEUE_NETMAIL_TMP, fromuid, now, (long)getpid());

    if (n < 0 || (size_t)n >= sizeof(tmpfile)) {
        dlog(2, "queue_ftn_netmail: tmp filename too long");
        return -1;
    }

    n = snprintf(pendingfile, sizeof(pendingfile), "%s/netmail.%d.%ld.%ld",
        FTNQUEUE_NETMAIL_PENDING, fromuid, now, (long)getpid());

    if (n < 0 || (size_t)n >= sizeof(pendingfile)) {
        dlog(2, "queue_ftn_netmail: pending filename too long");
        return -1;
    }

    fp = fopen(tmpfile, "w");
    if (fp == NULL) {
        dlog(2, "queue_ftn_netmail: fopen failed for uid %d to [%s], errno=%d",
            fromuid, toaddr, errno);
        return -1;
    }

    if (fprintf(fp,
            "TYPE: NETMAIL\n"
            "FROMUID: %d\n"
            "TONAME: %s\n"
            "TOADDR: %s\n"
            "SUBJECT: %s\n"
            "REPLY: %s\n"
            "CREATED: %ld\n"
            "\n",
            fromuid, toname, toaddr, subject, reply, now) < 0) {
        dlog(2, "queue_ftn_netmail: fprintf header failed for uid %d, errno=%d",
            fromuid, errno);
        fclose(fp);
        unlink(tmpfile);
        return -1;
    }

    if (fputs(body, fp) == EOF) {
        dlog(2, "queue_ftn_netmail: fputs body failed for uid %d, errno=%d",
            fromuid, errno);
        fclose(fp);
        unlink(tmpfile);
        return -1;
    }

    if (*body != '\0' && body[strlen(body) - 1] != '\n')
        fputc('\n', fp);

    if (fclose(fp) != 0) {
        dlog(2, "queue_ftn_netmail: fclose failed for uid %d, errno=%d",
            fromuid, errno);
        unlink(tmpfile);
        return -1;
    }

    /*
     * Netmail queue jobs contain private mail.  If this turns out to block
     * the runner on a non-setuid install, change the queue directory to use
     * a shared setgid group and use 0640 instead.
     *
     * modified on 2026-07-10, PL
     */
    chmod(tmpfile, 0600);

    if (rename(tmpfile, pendingfile) != 0) {
        dlog(2, "queue_ftn_netmail: rename failed for uid %d to [%s], errno=%d",
            fromuid, toaddr, errno);
        unlink(tmpfile);
        return -1;
    }

    dlog(6, "queue_ftn_netmail: queued FTN netmail uid %d to [%s]",
        fromuid, toaddr);

    return 0;
}

void
export_ftn_post_if_needed(struct CONF_ENTRY *ce, long textnum)
{
    if (ce == NULL || textnum <= 0)
        return;

    if (!conf_is_ftn(ce->type))
        return;

    if (ce->num <= 0) {
        dlog(2, "export_ftn_post_if_needed: invalid conference number %d",
            ce->num);
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
    if (queue_ftn_export(ce->num, textnum) != 0) {
        dlog(2, "export_ftn_post_if_needed: failed to queue text %ld for conf %d [%s]",
            textnum, ce->num, ce->name ? ce->name : "(null)");
    }
}

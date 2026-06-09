/*
 * ftnmsg.c - simple Fido/FTN .MSG parser for SklaffKOM
 *
 * This module only parses .MSG files. It does not import anything into
 * SklaffKOM. ftntoss.c should use this parser later.
 *
 * modified on 2026-06-09, PL
 */

#include "ftnmsg.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
copy_field(char *dst, size_t dstsz, const unsigned char *src, size_t n)
{
    size_t i;

    if (dstsz == 0)
        return;

    for (i = 0; i < n && i + 1 < dstsz; i++) {
        if (src[i] == '\0')
            break;
        dst[i] = (char)src[i];
    }

    dst[i] = '\0';

    while (i > 0 && isspace((unsigned char)dst[i - 1])) {
        dst[i - 1] = '\0';
        i--;
    }
}

static void
trim_leading_spaces(char *s)
{
    while (*s == ' ' || *s == '\t')
        memmove(s, s + 1, strlen(s));
}

static void
strip_pipe_colors(char *s)
{
    char *r = s;
    char *w = s;

    while (*r) {
        if (r[0] == '|' &&
            isdigit((unsigned char)r[1]) &&
            isdigit((unsigned char)r[2])) {
            r += 3;
            continue;
        }

        *w++ = *r++;
    }

    *w = '\0';
}

static void
parse_kludge(struct fido_msg *msg, const char *line)
{
    const char *p;

    if (line == NULL || line[0] != '\001')
        return;

    p = line + 1;

    if (strncmp(p, "CHRS:", 5) == 0) {
        snprintf(msg->chrs, sizeof(msg->chrs), "%s", p + 5);
        trim_leading_spaces(msg->chrs);
    } else if (strncmp(p, "MSGID:", 6) == 0) {
        snprintf(msg->msgid, sizeof(msg->msgid), "%s", p + 6);
        trim_leading_spaces(msg->msgid);
    } else if (strncmp(p, "REPLY:", 6) == 0) {
        snprintf(msg->reply, sizeof(msg->reply), "%s", p + 6);
        trim_leading_spaces(msg->reply);
    } else if (strncmp(p, "TID:", 4) == 0) {
        snprintf(msg->tid, sizeof(msg->tid), "%s", p + 4);
        trim_leading_spaces(msg->tid);
    } else if (strncmp(p, "TZUTC:", 6) == 0) {
        snprintf(msg->tzutc, sizeof(msg->tzutc), "%s", p + 6);
        trim_leading_spaces(msg->tzutc);
    }
}

static int
should_hide_clean_line(const char *line)
{
    if (line == NULL)
        return 1;

    if (line[0] == '\001')
        return 1;

    if (strncmp(line, "SEEN-BY:", 8) == 0)
        return 1;

    if (strncmp(line, "AREA:", 5) == 0)
        return 1;

    /*
     * PATH usually appears as ^APATH, but keep this here in case something
     * has already stripped the ^A or generated a plain PATH line.
     */
    if (strncmp(line, "PATH:", 5) == 0)
        return 1;

    return 0;
}

static int
append_line(char **out, size_t *outcap, size_t *outlen, const char *line)
{
    size_t need;
    char *tmp;

    if (out == NULL || outcap == NULL || outlen == NULL || line == NULL)
        return -1;

    need = strlen(line) + 2;

    if (*outlen + need >= *outcap) {
        size_t newcap = (*outcap * 2) + need + 1024;

        tmp = (char *)realloc(*out, newcap);
        if (tmp == NULL)
            return -1;

        *out = tmp;
        *outcap = newcap;
    }

    memcpy(*out + *outlen, line, strlen(line));
    *outlen += strlen(line);
    (*out)[(*outlen)++] = '\n';
    (*out)[*outlen] = '\0';

    return 0;
}

static int
process_body(const unsigned char *buf, size_t len, struct fido_msg *msg)
{
    char line[4096];
    char *raw = NULL;
    char *clean = NULL;
    size_t rawcap, cleancap;
    size_t rawlen, cleanlen;
    size_t i, linelen;

    rawcap = len * 2 + 1024;
    cleancap = len * 2 + 1024;

    raw = (char *)calloc(1, rawcap);
    clean = (char *)calloc(1, cleancap);
    if (raw == NULL || clean == NULL) {
        free(raw);
        free(clean);
        return -1;
    }

    rawlen = 0;
    cleanlen = 0;
    linelen = 0;
    memset(line, 0, sizeof(line));

    for (i = 0; i < len; i++) {
        unsigned char c = buf[i];

        if (c == '\0')
            break;

        if (c == '\r' || c == '\n') {
            line[linelen] = '\0';

            parse_kludge(msg, line);

            if (append_line(&raw, &rawcap, &rawlen, line) != 0) {
                free(raw);
                free(clean);
                return -1;
            }

            if (!should_hide_clean_line(line)) {
                strip_pipe_colors(line);

                if (append_line(&clean, &cleancap, &cleanlen, line) != 0) {
                    free(raw);
                    free(clean);
                    return -1;
                }
            }

            linelen = 0;
            line[0] = '\0';
            continue;
        }

        if (linelen + 1 < sizeof(line)) {
            line[linelen++] = (char)c;
        }
    }

    /*
     * Handle final unterminated line, just in case.
     */
    if (linelen > 0) {
        line[linelen] = '\0';

        parse_kludge(msg, line);

        if (append_line(&raw, &rawcap, &rawlen, line) != 0) {
            free(raw);
            free(clean);
            return -1;
        }

        if (!should_hide_clean_line(line)) {
            strip_pipe_colors(line);

            if (append_line(&clean, &cleancap, &cleanlen, line) != 0) {
                free(raw);
                free(clean);
                return -1;
            }
        }
    }

    msg->raw_body = raw;
    msg->clean_body = clean;

    return 0;
}

static int
read_file(const char *path, unsigned char **outbuf, size_t *outlen)
{
    FILE *fp;
    long sz;
    unsigned char *buf;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(fp);
        return -1;
    }

    sz = ftell(fp);
    if (sz < 0) {
        perror("ftell");
        fclose(fp);
        return -1;
    }

    rewind(fp);

    buf = (unsigned char *)malloc((size_t)sz);
    if (buf == NULL) {
        fclose(fp);
        return -1;
    }

    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        perror("fread");
        free(buf);
        fclose(fp);
        return -1;
    }

    fclose(fp);

    *outbuf = buf;
    *outlen = (size_t)sz;

    return 0;
}

int
read_fido_msg(const char *path, struct fido_msg *msg)
{
    unsigned char *buf = NULL;
    size_t len = 0;

    if (path == NULL || msg == NULL)
        return -1;

    memset(msg, 0, sizeof(*msg));

    if (read_file(path, &buf, &len) != 0)
        return -1;

    if (len < FIDO_HDR_SIZE) {
        fprintf(stderr, "%s: too small to be a .MSG file\n", path);
        free(buf);
        return -1;
    }

    copy_field(msg->from, sizeof(msg->from), buf + 0, FIDO_FROM_LEN);
    copy_field(msg->to, sizeof(msg->to), buf + 36, FIDO_TO_LEN);
    copy_field(msg->subject, sizeof(msg->subject), buf + 72, FIDO_SUBJ_LEN);
    copy_field(msg->date, sizeof(msg->date), buf + 144, FIDO_DATE_LEN);

    if (process_body(buf + FIDO_HDR_SIZE, len - FIDO_HDR_SIZE, msg) != 0) {
        free(buf);
        free_fido_msg(msg);
        return -1;
    }

    free(buf);
    return 0;
}

void
free_fido_msg(struct fido_msg *msg)
{
    if (msg == NULL)
        return;

    free(msg->raw_body);
    free(msg->clean_body);

    memset(msg, 0, sizeof(*msg));
}

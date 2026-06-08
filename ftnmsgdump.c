/*
 * ftnmsgdump.c - simple Fido .MSG dumper for SklaffKOM experiments
 *
 * Build:
 *   cc -Wall -Wextra -O2 -o ftnmsgdump ftnmsgdump.c
 *
 * Usage:
 *   ./ftnmsgdump /usr/local/ftn/echomail/FSX_GEN/4.msg
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define FIDO_FROM_LEN    36
#define FIDO_TO_LEN      36
#define FIDO_SUBJ_LEN    72
#define FIDO_DATE_LEN    20
#define FIDO_HDR_SIZE    190

struct fido_msg {
    char from[FIDO_FROM_LEN + 1];
    char to[FIDO_TO_LEN + 1];
    char subject[FIDO_SUBJ_LEN + 1];
    char date[FIDO_DATE_LEN + 1];

    char chrs[128];
    char msgid[256];
    char reply[256];

    char *body;
};

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

    /* Trim trailing spaces */
    while (i > 0 && isspace((unsigned char)dst[i - 1])) {
        dst[i - 1] = '\0';
        i--;
    }
}

static void
strip_pipe_colors(char *s)
{
    char *r = s;
    char *w = s;

    while (*r) {
        if (r[0] == '|' && isdigit((unsigned char)r[1]) && isdigit((unsigned char)r[2])) {
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

    if (line[0] != '\001')
        return;

    p = line + 1;

    if (strncmp(p, "CHRS:", 5) == 0) {
        snprintf(msg->chrs, sizeof(msg->chrs), "%s", p + 5);
        while (msg->chrs[0] == ' ')
            memmove(msg->chrs, msg->chrs + 1, strlen(msg->chrs));
    } else if (strncmp(p, "MSGID:", 6) == 0) {
        snprintf(msg->msgid, sizeof(msg->msgid), "%s", p + 6);
        while (msg->msgid[0] == ' ')
            memmove(msg->msgid, msg->msgid + 1, strlen(msg->msgid));
    } else if (strncmp(p, "REPLY:", 6) == 0) {
        snprintf(msg->reply, sizeof(msg->reply), "%s", p + 6);
        while (msg->reply[0] == ' ')
            memmove(msg->reply, msg->reply + 1, strlen(msg->reply));
    }
}

static int
should_hide_line(const char *line)
{
    if (line[0] == '\001')
        return 1;

    if (strncmp(line, "SEEN-BY:", 8) == 0)
        return 1;

    if (strncmp(line, "AREA:", 5) == 0)
        return 1;

    return 0;
}

static char *
decode_body(const unsigned char *buf, size_t len, struct fido_msg *msg)
{
    char *out;
    char line[4096];
    size_t outcap, outlen;
    size_t i, linelen;

    outcap = len * 2 + 1;
    out = calloc(1, outcap);
    if (out == NULL)
        return NULL;

    outlen = 0;
    linelen = 0;
    memset(line, 0, sizeof(line));

    for (i = 0; i < len; i++) {
        unsigned char c = buf[i];

        if (c == '\0')
            break;

        if (c == '\r' || c == '\n') {
            line[linelen] = '\0';

            parse_kludge(msg, line);

            if (!should_hide_line(line)) {
                strip_pipe_colors(line);

                if (outlen + strlen(line) + 2 >= outcap) {
                    size_t newcap = outcap * 2 + strlen(line) + 2;
                    char *tmp = realloc(out, newcap);
                    if (tmp == NULL) {
                        free(out);
                        return NULL;
                    }
                    out = tmp;
                    outcap = newcap;
                }

                memcpy(out + outlen, line, strlen(line));
                outlen += strlen(line);
                out[outlen++] = '\n';
                out[outlen] = '\0';
            }

            linelen = 0;
            line[0] = '\0';
            continue;
        }

        if (linelen + 1 < sizeof(line))
            line[linelen++] = (char)c;
    }

    return out;
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

    buf = malloc((size_t)sz);
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
main(int argc, char **argv)
{
    unsigned char *buf;
    size_t len;
    struct fido_msg msg;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file.msg\n", argv[0]);
        return 1;
    }

    memset(&msg, 0, sizeof(msg));

    if (read_file(argv[1], &buf, &len) != 0)
        return 1;

    if (len < FIDO_HDR_SIZE) {
        fprintf(stderr, "%s: too small to be a .MSG file\n", argv[1]);
        free(buf);
        return 1;
    }

    copy_field(msg.from, sizeof(msg.from), buf + 0, FIDO_FROM_LEN);
    copy_field(msg.to, sizeof(msg.to), buf + 36, FIDO_TO_LEN);
    copy_field(msg.subject, sizeof(msg.subject), buf + 72, FIDO_SUBJ_LEN);
    copy_field(msg.date, sizeof(msg.date), buf + 144, FIDO_DATE_LEN);

    msg.body = decode_body(buf + FIDO_HDR_SIZE, len - FIDO_HDR_SIZE, &msg);
    if (msg.body == NULL) {
        fprintf(stderr, "Could not decode body\n");
        free(buf);
        return 1;
    }

    printf("File:    %s\n", argv[1]);
    printf("From:    %s\n", msg.from);
    printf("To:      %s\n", msg.to);
    printf("Subject: %s\n", msg.subject);
    printf("Date:    %s\n", msg.date);

    if (msg.chrs[0] != '\0')
        printf("CHRS:    %s\n", msg.chrs);
    if (msg.msgid[0] != '\0')
        printf("MSGID:   %s\n", msg.msgid);
    if (msg.reply[0] != '\0')
        printf("REPLY:   %s\n", msg.reply);

    printf("\n--- BODY ---\n");
    printf("%s", msg.body);

    free(msg.body);
    free(buf);

    return 0;
}

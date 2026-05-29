#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "sklaff.h"

static char *
mail_body_fallback_dup(const char *msg)
{
    const char *body;

    if (msg == NULL)
        return NULL;

    body = strstr(msg, "\r\n\r\n");
    if (body)
        return strdup(body + 4);

    body = strstr(msg, "\n\n");
    if (body)
        return strdup(body + 2);

    return strdup(msg);
}

char *
mail_extract_text_plain_dup(const char *msg)
{
    /*
     * Första enkla versionen:
     * - hitta boundary=
     * - leta text/plain-del
     * - returnera bodyn före nästa boundary
     * - fallback: body after headers
     */
    const char *p, *body, *next_boundary;
    char boundary[256];
    const char *bstart, *bend;
    char marker[300];
    size_t len;

    /*
     * Find boundary="..."
     */
    bstart = strcasestr(msg, "boundary=");
    if (!bstart)
		return mail_body_fallback_dup(msg);

    bstart += 9;

    if (*bstart == '"') {
        bstart++;
        bend = strchr(bstart, '"');
    } else {
        bend = bstart;
        while (*bend && *bend != '\r' && *bend != '\n' && *bend != ';')
            bend++;
    }

    if (!bend || bend <= bstart)
		return mail_body_fallback_dup(msg);

    len = (size_t)(bend - bstart);
    if (len >= sizeof(boundary))
        len = sizeof(boundary) - 1;

    memcpy(boundary, bstart, len);
    boundary[len] = '\0';

    snprintf(marker, sizeof(marker), "--%s", boundary);

    p = msg;
    while ((p = strstr(p, marker)) != NULL) {
        const char *part_headers;
        const char *part_body;

        p += strlen(marker);

        /*
         * End marker --boundary--
         */
        if (p[0] == '-' && p[1] == '-')
            break;

        part_headers = p;

        part_body = strstr(part_headers, "\n\n");
        if (!part_body)
            part_body = strstr(part_headers, "\r\n\r\n");

        if (!part_body)
            break;

        if (strstr(part_headers, "text/plain") ||
            strstr(part_headers, "Text/Plain") ||
            strstr(part_headers, "TEXT/PLAIN")) {
            if (part_body[0] == '\r' && part_body[1] == '\n' &&
                part_body[2] == '\r' && part_body[3] == '\n')
                body = part_body + 4;
            else
                body = part_body + 2;

            next_boundary = strstr(body, marker);
			if (!next_boundary)
    			return strdup(body);

			len = (size_t)(next_boundary - body);
			{
    			char *out = malloc(len + 1);
    			if (!out)
        			return NULL;
    			memcpy(out, body, len);
    			out[len] = '\0';
    			return out;
			}
        }
    }

    /*
     * Fallback: old behavior-ish.
     */
    return mail_body_fallback_dup(msg);
}

static int
hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

char *
mail_qp_decode_dup(const char *src)
{
    const unsigned char *s;
    char *out, *d;
    int h1, h2;

    if (!src)
        return NULL;

    out = malloc(strlen(src) + 1);
    if (!out)
        return NULL;

    s = (const unsigned char *)src;
    d = out;

    while (*s) {
        if (*s == '=') {
            /*
             * Soft line break:
             * =\n
             * =\r\n
             */
            if (s[1] == '\r' && s[2] == '\n') {
                s += 3;
                continue;
            }
            if (s[1] == '\n') {
                s += 2;
                continue;
            }

            h1 = hexval(s[1]);
            h2 = hexval(s[2]);
            if (h1 >= 0 && h2 >= 0) {
                *d++ = (char)((h1 << 4) | h2);
                s += 3;
                continue;
            }
        }

        *d++ = (char)*s++;
    }

    *d = '\0';
    return out;
}

int
mail_has_quoted_printable(const char *msg)
{
    if (msg == NULL)
        return 0;

    return strcasestr(msg, "Content-Transfer-Encoding: quoted-printable") != NULL;
}
static int
b64val(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;

    return -1;
}

char *
mail_base64_decode_dup(const char *src)
{
    const unsigned char *s;
    unsigned char *out, *d;
    int vals[4];
    int n, v;
    size_t len;

    if (src == NULL)
        return NULL;

    len = strlen(src);
    out = malloc(len + 1);
    if (out == NULL)
        return NULL;

    s = (const unsigned char *)src;
    d = out;
    n = 0;

    while (*s) {
        if (*s == '=') {
            vals[n++] = -2;      /* padding */
        } else {
            v = b64val(*s);
            if (v < 0) {
                s++;
                continue;        /* skip CR/LF/spaces/etc */
            }
            vals[n++] = v;
        }

        if (n == 4) {
            if (vals[0] >= 0 && vals[1] >= 0) {
                *d++ = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
            }

            if (vals[2] >= 0) {
                *d++ = (unsigned char)(((vals[1] & 0x0f) << 4) | (vals[2] >> 2));
            }

            if (vals[3] >= 0) {
                *d++ = (unsigned char)(((vals[2] & 0x03) << 6) | vals[3]);
            }

            n = 0;
        }

        s++;
    }

    *d = '\0';
    return (char *)out;
}

int
mail_has_base64(const char *msg)
{
    if (msg == NULL)
        return 0;

    return strcasestr(msg, "Content-Transfer-Encoding: base64") != NULL;
}

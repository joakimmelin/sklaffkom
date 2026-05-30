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

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <sys/wait.h>
#ifdef LINUX
#include <bsd/string.h>  /* for strlcat on Linux */
#endif
#include "sklaff.h"
#include "ext_globals.h"

/*
 * se_time_string - format UTC time as Swedish local time
 * args: UTC epoch time (utc), output buffer (out), time_string flags (flags)
 * ret: nothing
 */

void se_time_string(time_t utc, LINE out, int flags) {
    char *old = getenv("TZ"); char *save = old ? strdup(old) : NULL;
    setenv("TZ", "Europe/Stockholm", 1); tzset();
    time_string(utc, out, flags);         /* provided by your codebase */
    if (save) { setenv("TZ", save, 1); free(save); } else unsetenv("TZ");
    tzset();
}

/*
 * parse_tz_token - parse timezone token from Usenet Date header
 * args: timezone token (p), output offset in seconds (out_sec)
 * ret: success (1) or failure (0)
 */

int parse_tz_token(const char *p, int *out_sec) {
    if (!p || !*p) { *out_sec = 0; return 1; }
    if (strncasecmp(p, "GMT", 3) == 0 || strncasecmp(p, "UTC", 3) == 0) {
        *out_sec = 0; return 1;
    }
    if ((p[0] == '+' || p[0] == '-') &&
        isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2]) &&
        isdigit((unsigned char)p[3]) && isdigit((unsigned char)p[4])) {
        int sign = (p[0] == '-') ? -1 : 1;
        int hh = (p[1]-'0')*10 + (p[2]-'0');
        int mm = (p[3]-'0')*10 + (p[4]-'0');
        *out_sec = sign * (hh*3600 + mm*60);
        return 1;
    }
    /* tolerate things like "-0000 (UTC)" */
    if (p[0]=='(') { *out_sec = 0; return 1; }
    return 0;
}

/*
 * parse_usenet_date_utc - parse Usenet Date header into UTC time
 * args: Date header or value (date_line), output UTC epoch time (out_utc)
 * ret: success (1) or failure (0)
 */

int parse_usenet_date_utc(const char *date_line, time_t *out_utc) {
    if (!date_line) return 0;

    /* Accept both "Date: ..." and just the value */
    const char *p = date_line;
    if (strncasecmp(p, "Date:", 5) == 0) p += 5;
    while (*p == ' ' || *p == '\t') p++;

    /* optional weekday "Fri, " */
    const char *comma = strchr(p, ',');
    if (comma && (comma - p) <= 10) { p = comma + 1; while (*p==' '||*p=='\t') p++; }

    /* day */
    char *q;
    long day = strtol(p, &q, 10);
    if (q == p || day < 1 || day > 31) return 0;
    p = q; while (*p==' '||*p=='\t') p++;

    /* month */
    if (!isalpha((unsigned char)p[0]) || !isalpha((unsigned char)p[1]) || !isalpha((unsigned char)p[2])) return 0;
    int mon = month3_to_num(p); if (!mon) return 0;
    p += 3; while (*p==' '||*p=='\t') p++;

    /* year */
    long year = strtol(p, &q, 10);
    if (q == p) return 0;
    if (year < 100) year += (year < 70 ? 2000 : 1900);
    p = q; while (*p==' '||*p=='\t') p++;

    /* time HH:MM[:SS] */
    long hh = strtol(p, &q, 10);
    if (q == p) return 0;
    p = q;

    if (*p != ':')
    return 0;
    p++;

    long mm = strtol(p, &q, 10);
    if (q == p) return 0;
    p = q;

    long ss = 0;
    if (*p == ':') {
    p++;
    ss = strtol(p, &q, 10);
    if (q == p) return 0;
    p = q;
    }

while (*p == ' ' || *p == '\t') p++;


    /* TZ */
    int tzsec = 0;
    if (!parse_tz_token(p, &tzsec)) return 0;

    struct tm t; memset(&t, 0, sizeof t);
    t.tm_year = (int)year - 1900;
    t.tm_mon  = mon - 1;
    t.tm_mday = (int)day;
    t.tm_hour = (int)hh;
    t.tm_min  = (int)mm;
    t.tm_sec  = (int)ss;

    time_t as_if_utc = timegm_compat(&t); /* interpret the calendar time as if UTC */
    if (as_if_utc == (time_t)-1) return 0;

    *out_utc = as_if_utc - tzsec; /* convert from given zone to real UTC */
    return 1;
}

/*
 * month3_to_num - convert three-letter English month name to number
 * args: month string (m)
 * ret: month number 1..12 or failure (0)
 */

int month3_to_num(const char *m) {
    char a = tolower((unsigned char)m[0]);
    char b = tolower((unsigned char)m[1]);
    char c = tolower((unsigned char)m[2]);
    if (a=='j' && b=='a' && c=='n') return 1;
    if (a=='f' && b=='e' && c=='b') return 2;
    if (a=='m' && b=='a' && c=='r') return 3;
    if (a=='a' && b=='p' && c=='r') return 4;
    if (a=='m' && b=='a' && c=='y') return 5;
    if (a=='j' && b=='u' && c=='n') return 6;
    if (a=='j' && b=='u' && c=='l') return 7;
    if (a=='a' && b=='u' && c=='g') return 8;
    if (a=='s' && b=='e' && c=='p') return 9;
    if (a=='o' && b=='c' && c=='t') return 10;
    if (a=='n' && b=='o' && c=='v') return 11;
    if (a=='d' && b=='e' && c=='c') return 12;
    return 0;
}

/*
 * timegm_compat - portable timegm replacement
 * args: broken-down UTC time (t)
 * ret: UTC epoch time or failure ((time_t)-1)
 */

time_t timegm_compat(struct tm *t) {
#if defined(__FreeBSD__) || defined(__GLIBC__)
    return timegm(t);
#else
    char *old = getenv("TZ");
    if (old) old = strdup(old);
    setenv("TZ", "UTC", 1); tzset();
    time_t res = mktime(t);
    if (old) { setenv("TZ", old, 1); free(old); }
    else unsetenv("TZ");
    tzset();
    return res;
#endif
}

/*
 * rfc2047_decode - decode RFC 2047 encoded words in a header field
 * args: input header string (in), output buffer (out), output buffer length (outlen)
 * ret: nothing
 */

void rfc2047_decode(const char *in, char *out, size_t outlen)
{
    const char *p = in;
    size_t o = 0;
    if (!in || !*in) { if (outlen) out[0] = '\0'; return; }

    while (*p && o + 1 < outlen) {
        const char *start = strstr(p, "=?");
        if (!start) {
            /* copy the rest */
            size_t rem = strlen(p);
            if (rem >= outlen - 1 - o) rem = outlen - 1 - o;
            memcpy(out + o, p, rem);
            o += rem;
            break;
        }
        /* copy literal up to start */
        size_t lit = (size_t)(start - p);
        if (lit) {
            size_t rem = (lit >= outlen - 1 - o) ? (outlen - 1 - o) : lit;
            memcpy(out + o, p, rem);
            o += rem;
        }

        /* parse =?charset?enc?text?= */
        const char *q1 = strchr(start + 2, '?'); if (!q1) { p = start + 2; continue; }
        const char *q2 = strchr(q1 + 1, '?');    if (!q2) { p = q1 + 1; continue; }
        const char *q3 = strstr(q2 + 1, "?=");   if (!q3) { p = q2 + 1; continue; }

        char charset[32];
        size_t cslen = (size_t)(q1 - (start + 2));
        if (cslen >= sizeof(charset)) cslen = sizeof(charset) - 1;
        memcpy(charset, start + 2, cslen);
        charset[cslen] = '\0';

        char enc = (char)toupper((unsigned char)q1[1]);

        /* raw decoded bytes */
        unsigned char bytes[512];
        size_t blen = 0;

        const char *payload = q2 + 1;
        size_t plen = (size_t)(q3 - payload);

        if (enc == 'B') {
            blen = b64_decode_bytes(payload, plen, bytes, sizeof(bytes));
        } else if (enc == 'Q') {
            blen = qp_decode_bytes(payload, plen, bytes, sizeof(bytes));
        } else {
            /* unknown encoding, copy raw */
            blen = (plen > sizeof(bytes)) ? sizeof(bytes) : plen;
            memcpy(bytes, payload, blen);
        }

        /* convert to UTF-8 (minimal supported charsets) */
        o += bytes_to_utf8(charset, bytes, blen, out + o, (outlen - o));

        /* advance past ?= and any single space between adjacent encoded-words */
        p = q3 + 2;
        while (*p == ' ' || *p == '\t') {
            const char *peek = p;
            while (*peek == ' ' || *peek == '\t') peek++;
            if (peek[0] == '=' && peek[1] == '?') p = peek; /* glue encoded-words */
            break;
        }
    }
    if (o < outlen) out[o] = '\0';
}

/*
 * qp_decode_bytes - decode RFC 2047 Q encoded bytes
 * args: input bytes (in), input length (inlen), output buffer (out), output length (outlen)
 * ret: number of bytes written
 */

size_t qp_decode_bytes(const char *in, size_t inlen, unsigned char *out, size_t outlen)
{
    size_t i = 0, o = 0;
    while (i < inlen && o < outlen) {
        char c = in[i++];
        if (c == '_') { out[o++] = ' '; continue; }
        if (c == '=' && i + 1 < inlen && isxdigit((unsigned char)in[i]) && isxdigit((unsigned char)in[i+1])) {
            int hi = isdigit((unsigned char)in[i]) ? in[i]-'0' : (tolower((unsigned char)in[i])-'a'+10);
            int lo = isdigit((unsigned char)in[i+1]) ? in[i+1]-'0' : (tolower((unsigned char)in[i+1])-'a'+10);
            unsigned char b = (unsigned char)((hi<<4) | lo);
            if (o < outlen) out[o++] = b;
            i += 2;
        } else {
            out[o++] = (unsigned char)c;
        }
    }
    return o;
}

/*
 * b64_decode_bytes - decode RFC 2047 B/base64 encoded bytes
 * args: input bytes (in), input length (inlen), output buffer (out), output length (outlen)
 * ret: number of bytes written
 */

size_t b64_decode_bytes(const char *in, size_t inlen, unsigned char *out, size_t outlen)
{
    size_t i = 0, o = 0;
    while (i + 3 < inlen) {
        int a = b64v(in[i++]);
        int b = b64v(in[i++]);
        int c = (in[i] == '=') ? -1 : b64v(in[i]);
        i++;
        int d = (in[i] == '=') ? -1 : b64v(in[i]);
        i++;
        if (a < 0 || b < 0 || (c < -1) || (d < -1)) break;
        if (o < outlen) out[o++] = (unsigned char)((a<<2) | (b>>4));
        if (c >= 0 && o < outlen) out[o++] = (unsigned char)(((b&0x0F)<<4) | (c>>2));
        if (d >= 0 && o < outlen) out[o++] = (unsigned char)(((c&0x03)<<6) | d);
    }
    return o;
}

/*
 * bytes_to_utf8 - convert decoded header bytes to UTF-8
 * args: source charset (charset), input bytes (in), input length (inlen), output buffer (out), output length (outlen)
 * ret: number of bytes written
 */

size_t bytes_to_utf8(const char *charset, const unsigned char *in, size_t inlen, char *out, size_t outlen)
{
    if (!charset) charset = "us-ascii";
    /* lowercase compare */
    char cs[32]; size_t n = 0;
    while (charset[n] && n+1 < sizeof(cs)) { cs[n] = (char)tolower((unsigned char)charset[n]); n++; }
    cs[n] = '\0';

    if (!strcmp(cs, "utf-8") || !strcmp(cs, "us-ascii")) {
        size_t copy = (inlen >= outlen-1) ? (outlen-1) : inlen;
        memcpy(out, in, copy);
        out[copy] = '\0';
        return copy;
    }
    if (!strcmp(cs, "iso-8859-1") || !strcmp(cs, "latin1") || !strcmp(cs, "iso8859-1")) {
        return latin1_to_utf8(in, inlen, out, outlen);
    }

    /* Fallback: best-effort raw copy (won't crash; shows something) */
    size_t copy = (inlen >= outlen-1) ? (outlen-1) : inlen;
    memcpy(out, in, copy);
    out[copy] = '\0';
    return copy;
}

/*
 * extract_display_name - extract printable display name from From header
 * args: From header value (from), output buffer (out), output buffer length (outlen)
 * ret: nothing
 */

void extract_display_name(const char *from, char *out, size_t outlen)
{
        
    if (!from || !*from) { out[0] = '\0'; return; }


    char buf[256];
    snprintf(buf, sizeof(buf), "%s", from);

    /* Trim leading/trailing spaces */
    char *s = buf;
    while (*s && (*s==' ' || *s=='\t')) s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1]==' ' || e[-1]=='\t' || e[-1]=='\r' || e[-1]=='\n')) --e;
    *e = '\0';

    /* Case 1: "Name" <email>  => prefer Name (strip quotes) */
    char *lt = strchr(s, '<');
    if (lt) {
        /* take the part before '<' */
        while (lt > s && (lt[-1]==' ' || lt[-1]=='\t')) --lt;
        *lt = '\0';
        /* strip optional surrounding quotes */
        if (*s=='"' && e> s+1 && e[-1]=='"') { s++; e--; *e='\0'; }
        /* if non-empty, use it */
        if (*s) { snprintf(out, outlen, "%s", s); return; }
        /* else fall back to content inside <...> */
        char *gt = strchr(lt+1, '>');
        if (gt && gt > lt+1) {
            *gt = '\0';
            snprintf(out, outlen, "%s", lt+1);
            return;
        }
    }

    /* Case 2: email (Name) => prefer (Name) */
    char *lp = strchr(s, '(');
    if (lp) {
        char *rp = strchr(lp+1, ')');
        if (rp && rp > lp+1) {
            *rp = '\0';
            /* trim inner spaces/quotes */
            char *ns = lp+1;
            while (*ns==' '||*ns=='\t') ns++;
            char *ne = ns + strlen(ns);
            while (ne>ns && (ne[-1]==' '||ne[-1]=='\t'||ne[-1]=='"')) --ne;
            if (*ns=='"') ns++;
            *ne = '\0';
            if (*ns) { snprintf(out, outlen, "%s", ns); return; }
        }
    }

    /* Default: just use input as-is */
    snprintf(out, outlen, "%s", *s ? s : "");  
}

/*
 * is_blank_line - check if a line is empty or contains only whitespace
 * args: input line (line)
 * ret: blank (1) or not blank (0)
 */

int is_blank_line(const char *line) {
    if (!line) return 1;
    while (*line) {
        if (!isspace((unsigned char)*line))
            return 0;
        line++;
    }
    return 1;
}

/*
 * utf8_disp_len - count printable UTF-8 codepoints in a string
 * args: input string (s)
 * ret: number of display characters
 */

size_t utf8_disp_len(const char *s)
{
    size_t n = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if ((c & 0xC0) != 0x80) /* count non-continuation bytes */
            n++;
    }
    return n;
}

/*
 * utf8_trunc_cols - truncate UTF-8 string to a maximum display width
 * args: input string (in), max columns (max_cols), output buffer (out), output buffer length (outlen)
 * ret: nothing
 */

void utf8_trunc_cols(const char *in, size_t max_cols, char *out, size_t outlen) 
{
    size_t cols = 0, o = 0;
    if (!in || !out || outlen == 0) return;
    while (*in && o + 4 < outlen) {
        unsigned char c = (unsigned char)in[0];
        size_t clen;
        if ((c & 0x80) == 0x00) clen = 1;
        else if ((c & 0xE0) == 0xC0) clen = 2;
        else if ((c & 0xF0) == 0xE0) clen = 3;
        else if ((c & 0xF8) == 0xF0) clen = 4;
        else clen = 1; 

        if (cols + 1 > max_cols) break;
        if (o + clen >= outlen) break;

        
        for (size_t i = 0; i < clen && in[i]; i++) out[o++] = in[i];
        in += clen;
        cols++;
    }
    out[o] = '\0';
}

/*
 * print_underlined_line - print a line followed by a matching underline
 * args: input line (line)
 * ret: nothing
 */

void print_underlined_line(const char *line)
{
    LINE under;
    size_t i, w = utf8_disp_len(line);
    if (w >= sizeof(under)) w = sizeof(under) - 1;
    for (i = 0; i < w; i++) under[i] = '-';
    under[i] = '\0';
    output("%s\n", line);
    output("%s\n", under);
}

/*
 * latin1_to_utf8 - convert ISO-8859-1 bytes to UTF-8
 * args: input bytes (in), input length (inlen), output buffer (out), output length (outlen)
 * ret: number of bytes written
 */

size_t latin1_to_utf8(const unsigned char *in, size_t inlen, char *out, size_t outlen)

{
    size_t o = 0;
    for (size_t i = 0; i < inlen; i++) {
        unsigned char c = in[i];
        if (c < 0x80) {
            if (o + 1 >= outlen) break;
            out[o++] = (char)c;
        } else {
            if (o + 2 >= outlen) break;
            out[o++] = (char)(0xC0 | (c >> 6));
            out[o++] = (char)(0x80 | (c & 0x3F));
        }
    }
    if (o < outlen) out[o] = '\0';
    return o;
}


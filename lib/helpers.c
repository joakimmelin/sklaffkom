/* helpers.c */

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
#include <sys/wait.h>
#ifdef LINUX
#include <bsd/string.h>  /* for strlcat on Linux */
#endif
#include "sklaff.h"
#include "ext_globals.h"




/* format a UTC epoch into Swedish time using your existing time_string() */
void se_time_string(time_t utc, LINE out, int flags) {
    char *old = getenv("TZ"); char *save = old ? strdup(old) : NULL;
    setenv("TZ", "Europe/Stockholm", 1); tzset();
    time_string(utc, out, flags);         /* provided by your codebase */
    if (save) { setenv("TZ", save, 1); free(save); } else unsetenv("TZ");
    tzset();
}

/* parse "+HHMM" or "-HHMM" -> seconds offset; "GMT"/"UTC" -> 0; 1 on ok */
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

/* Parse an RFC-822/2822 Date header into UTC epoch. Returns 1 on success. */
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

/* map 3-letter month -> 1..12, 0 on error */
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

/* portable timegm() */
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

/* Two Little helpers to avoid extra '(') 2025-08-26 PL */
void clear_prompt(int num) 
{
    int x;
    output("\r");
    for (x = 0; x < num; x++)
        output(" ");
    output("\r");
}

void clear_prompt_cols(int cols)
{
    output_ansi_fmt("\r\033[0K", "\r");
        if (!Ansi_output) {
        int i;
        for (i = 0; i < cols; i++)
            output(" ");
        output("\r");
    }
}

/* Count quote depth: skip leading spaces, then count '>' allowing optional single
 * space after each '>' so it matches ">>", "> >", "> > >", etc.
 */
int quote_depth(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    while (*p == ' ' || *p == '\t') p++;
    int d = 0;
    while (*p == '>') {
        d++;
        p++;
        if (*p == ' ') p++;   /* tolerate one space after each '>' */
    }
    return d;
}
/*
 * get_wallclock_localtime - little helper to show correct time at all times (2025-08-16 PL)
 */

void get_wallclock_localtime(const time_t *t, struct tm *out)
{
    char *saved = NULL;
    const char *tz = getenv("TZ");
    if (tz && tz[0]) {
        saved = strdup(tz);            
    }

    unsetenv("TZ");                    
    tzset();

    {
        struct tm *tmp = localtime(t); 
        if (tmp) *out = *tmp;
    }

    if (saved) {
        setenv("TZ", saved, 1);       
        free(saved);
    } else {
        unsetenv("TZ");                
    }
    tzset();
}

/*
 * extract_display_name - helper for humanizing header (real name in usenet posts)
 * PL 2025-08-09
 *       
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
 * better blank line detection for usenet headers PL 2025.
 * sometimes sklaffkom stripped not only the usenet-header but the
 * body also, this helper prevents that
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
 * RFC 2047 + underline helpers. This code is partly AI-generated and I must
 * admit I don't understand it fully myself, but it has been tested thoroughly
 * and works. Purpose : display text humans can read in usenet subjects and
 * name of the author. PL 2025-08-09
*/

size_t utf8_disp_len(const char *s) /* Count display chars (UTF-8 codepoints ≈ 1 col each; good enough for headers) */
{
    size_t n = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if ((c & 0xC0) != 0x80) /* count non-continuation bytes */
            n++;
    }
    return n;
}

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
void print_underlined_line(const char *line) /* Build underline matching a printed line */
{
    LINE under;
    size_t i, w = utf8_disp_len(line);
    if (w >= sizeof(under)) w = sizeof(under) - 1;
    for (i = 0; i < w; i++) under[i] = '-';
    under[i] = '\0';
    output("%s\n", line);
    output("%s\n", under);
}

int b64v(int c)  /* Base64 table */
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
size_t qp_decode_bytes(const char *in, size_t inlen, unsigned char *out, size_t outlen)    /* Decode a single encoded-word's bytes with quoted-printable (Q) */
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
size_t b64_decode_bytes(const char *in, size_t inlen, unsigned char *out, size_t outlen)   /* Decode a single encoded-word's bytes with base64 (B) */
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

size_t latin1_to_utf8(const unsigned char *in, size_t inlen, char *out, size_t outlen) /* Minimal charset -> UTF-8: utf-8 (pass), us-ascii (pass), iso-8859-1 (map) */

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
void rfc2047_decode(const char *in, char *out, size_t outlen)    /* RFC 2047 decoder: decodes any number of encoded-words in a header field */
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

void normalize_label(const char *raw, char *norm, size_t nlen)   /* Normalize a label to ensure exactly one trailing ": " */
{
    size_t L = raw ? strlen(raw) : 0;
    int ends_with_colon = (L > 0 && raw[L-1] == ':');
    snprintf(norm, nlen, "%s%s", raw ? raw : "", ends_with_colon ? " " : ": ");
}


/* output_body_line - outputs a body line with ANSI color if enabled
 * 2025-09-24, PL
 */

int output_body_line(const char *line, const char *col)
{
    if (Ansi_output) {
        return output_ansi_fmt("%s%s\x1b[0m\n", "%s\n", col, line);
    } else {
        if (output((char *)line) == -1 || output("\n") == -1)
            return -1;
    }
    return 0;
}

/*
 * run_external_cmd_args - runs an external program with argument list
 * argv[0] = command to run, argv[1..n] = arguments, NULL-terminated
 * use_fallback: whether to run 'which' on argv[0] if not found
 * 2025-09-17, PL
 */
int run_external_cmd_args(const char *argv[], int use_fallback)
{
    sigset_t sigmask, oldsigmask;
    char exe_path[256];
    FILE *which_fp;
    char which_buf[256];

    if (!argv || !argv[0])
        return 1;

    strlcpy(exe_path, argv[0], sizeof(exe_path));

    if (access(exe_path, X_OK) != 0 && use_fallback) {
        const char *base = strrchr(argv[0], '/');
        if (base)
            snprintf(which_buf, sizeof(which_buf), "which %s", base + 1);
        else
            snprintf(which_buf, sizeof(which_buf), "which %s", argv[0]);

        which_fp = popen(which_buf, "r");
        if (which_fp && fgets(which_buf, sizeof(which_buf), which_fp)) {
            which_buf[strcspn(which_buf, "\n")] = '\0';
            if (access(which_buf, X_OK) == 0)
                strlcpy(exe_path, which_buf, sizeof(exe_path));
        }
        if (which_fp)
            pclose(which_fp);
    }

  if (access(exe_path, X_OK) != 0) {
    output("\nFel: Kan inte starta spelet eller scriptet - meddela Sysop!\n");
    output("  Försökte köra: %s\n", exe_path);

    if (access(exe_path, F_OK) != 0) {
        output("  Filen finns inte (ENOENT).\n");
    } else {
        output("  Filen finns, men är inte körbar (EACCES eller liknande).\n");
    }

    return 1;
    }

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    signal(SIGNAL_NEW_TEXT, SIG_IGN);
    signal(SIGNAL_NEW_MSG, SIG_IGN);
    set_avail(Uid, 1);

    if (!fork()) {
        sig_reset();
        tty_reset();
        execvp(exe_path, (char *const *)argv);
        perror("execvp");
        _exit(1);
    } else {
        wait(NULL);
    }

    signal(SIGNAL_NEW_TEXT, baffo);
    signal(SIGNAL_NEW_MSG, newmsg);
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
    tty_raw();
    output("\n");
    set_avail(Uid, 0);

    return 0;
}
/*
 * display_langfile - display language-aware file
 * Updated 2025-09-15 by PL
 */
void
display_langfile(const char *base, const char *base_eng, const char *base_swe)
{
    int fd;
    char *buf = NULL;
    const char *filename = NULL;

/* Debug */
/*
output("DEBUG: language = ");

#ifdef ENGLISH
    output("ENGLISH\n");
#elif defined(SWEDISH)
    output("SWEDISH\n");
#else
    output("UNKNOWN\n");
#endif
*/
/* END OF DEBUG */

#ifdef ENGLISH
    if (file_exists(base_eng) != -1)
        filename = base_eng;
#elif defined(SWEDISH)
    if (file_exists(base_swe) != -1)
        filename = base_swe;
#endif

    if (!filename && file_exists(base) != -1)
        filename = base;

    if (filename) {
        if ((fd = open_file(filename, OPEN_QUIET)) == -1)
            return;
        if ((buf = read_file(fd)) == NULL)
            return;
        if (close_file(fd) == -1)
            return;
        output("%s", buf);
        free(buf);
    }
}


void clear_screen(void)
{
    output(ANSI_CLS);  /* or printf(ANSI_CLS); */
    fflush(stdout);
    Lines = 1;
}

void display_news(void)
{
    display_langfile(NEWS_FILE, NEWS_FILE_ENG, NEWS_FILE_SWE);
}

void display_logout(void)
{
    display_langfile(LOGOUT_FILE, LOGOUT_FILE_ENG, LOGOUT_FILE_SWE);
}

const char *
month_name(int mon)
{
    static const char *months[] = {
        "januari", "februari", "mars", "april", "maj", "juni",
        "juli", "augusti", "september", "oktober", "november", "december"
    };
    if (mon >= 0 && mon <= 11)
        return months[mon];
    return "okänd";
}


void
chomp(char *s)
{
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}
/*
 * display_header - displays textheader
 * args: pointer to TEXT_HEADER (th), allow editing of subject (edit_subject),
 *       conf/uid (type), absolute date? (dtype)
 */

/*
 * display_header - displays textheader
 * args: pointer to TEXT_HEADER (th), allow editing of subject (edit_subject),
 *       conf/uid (type), absolute date? (dtype)
 */

void
display_header(struct TEXT_HEADER * th, int edit_subject, int type, int dtype, char *mailrec)
{
    LINE time_val, confname; /* + confname for humanized header */
    char username[256];  /* expanded buffer to prevent overflow - 2025-09-14, PL */
	char fname[128];  /* increased from LINE to avoid overflow, modified on 2025-07-12, PL */
    int uid, right, nc, fd;
    char *tmp, *buf, *oldbuf;
    char *ptr = NULL;   /* modified on 2025-07-12, PL */
    struct CONF_ENTRY *ce = NULL;

    if (mailrec && type && (th->author == 0)) {
        strcpy(username, mailrec);
    } else {
        user_name(th->author, username);
        Current_author = th->author;
    }
  /* Humanized headers PL 2025-08-09 */
{
    LINE from_dec, disp;
    rfc2047_decode(username, from_dec, sizeof(from_dec));
    extract_display_name(from_dec, disp, sizeof(disp));
    snprintf(username, sizeof(username), "%.*s", (int)sizeof(username)-1, disp);
}
/* 2025-08-10, PL: strip surrounding quotes from display name */
{
    size_t L__ = strlen(username);
    if (L__ >= 2 && username[0] == '"' && username[L__-1] == '"') {
        username[L__-1] = '\0';
        memmove(username, username + 1, L__ - 1);
    }
}
    if (th->num == 0) {
        output("%s %s\n", MSG_WRITTENBY, username);
    } else {

int used_news_date = 0;
struct CONF_ENTRY *ce = get_conf_struct(Current_conf);
if (ce && ce->type == NEWS_CONF) {
    /* open the stored article and scan headers for "Date:" */
    LINE fname;
    int fd;
    char *buf = NULL, *oldbuf = NULL;
    snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, Current_conf, th->num);
    if ((fd = open_file(fname, OPEN_QUIET)) != -1) {
        if ((buf = read_file(fd)) != NULL) {
            oldbuf = buf;
            /* look only in the header block (up to first blank line) */
            char *hdr_end = strstr(buf, "\n\n");
            size_t hdr_len = hdr_end ? (size_t)(hdr_end - buf) : strlen(buf);
            /* crude but fast: search for "\nDate:" or "Date:" at start */
            char *d = strstr(buf, "\nDate:");
            if (!d && strncasecmp(buf, "Date:", 5) == 0) d = buf - 1; /* so d+1 points to 'D' */
            if (d && (size_t)( (d - buf) + 1 ) < hdr_len) {
                char *line_start = d + 1;
                char *line_end = memchr(line_start, '\n', hdr_len - (line_start - buf));
                if (line_end) *line_end = '\0';
                time_t utc;
                if (parse_usenet_date_utc(line_start, &utc)) {
                    se_time_string(utc, time_val, (dtype | Date));
                    used_news_date = 1;
                }
                if (line_end) *line_end = '\n';
            }
            free(oldbuf);
        }
        close_file(fd);
    }
}

if (!used_news_date) {
    /* fallback: original import timestamp */
    time_string(th->time, time_val, (dtype | Date));
}


/* 2025-08-09, PL: Human-first layout */

if (Current_conf != 0) {
    conf_name(Current_conf, confname);
    output_ansi_fmt("%s " CYAN "%d" DOT " %s " BR_RED "%s " DOT CYAN"%s\n"DOT, "%s %d %s %s %s\n",
        (th->type == TYPE_TEXT) ? MSG_TEXTNAME : MSG_SURVEYNAME,
        th->num, MSG_IN, confname, time_val);

    output_ansi_fmt("%s " BR_YELLOW "%s\n" DOT, "%s %s\n", MSG_WRITTENBY, username);
} else {
        output("%s %d %s %s %s\n",
        (th->type == TYPE_TEXT) ? MSG_TEXTNAME : MSG_SURVEYNAME,
        th->num, MSG_WRITTENBY, username, time_val);
}
    }
    switch (th->size) {
    case 0:
        if (th->num)
            output(" %s\n",
                (th->type == TYPE_TEXT) ? MSG_EMPTYTEXT : MSG_EMPTYSURVEY);
        break;
    case 1:
        //output(" %s\n", MSG_ONELINE);
        break;
    default:
	//output(" %d %s\n", th->size, MSG_LINES);
        break;
    }
    if (th->type == TYPE_SURVEY && (th->num != 0)) {
	time_string(th->sh.time, time_val, (dtype | Date));
        output("%s: %d; %s: %s\n", MSG_NQUESTIONS, th->sh.n_questions,
            MSG_REPORTRESULT, time_val);
    }
    if (th->comment_num) {
        if (th->comment_conf) {
            ce = get_conf_struct(th->comment_conf);
            right = can_see_conf(Uid, th->comment_conf, ce->type, ce->creator);
        } else {
            right = 1;
        }
        if (right) {
            output_ansi_fmt("%s " CYAN "%d " DOT, "%s %d ", MSG_REPLYTO, th->comment_num);
	    nc = th->comment_conf;
            if (!nc)
                nc = Current_conf;
            /* I put this chunk last instead to allow for display of author
             * also for text commented from other conferences. /OR 98-07-29 if
             * (th->comment_conf) { if (!nc) nc = Current_conf; conf_name(nc,
             * username); output ("%s %s\n", MSG_IN, username); } else */
            {
                if (!th->comment_author) {
                    strcpy(username, MSG_UNKNOWNU);
                    if (nc) {
                        sprintf(fname, "%s/%d/%ld", SKLAFF_DB,
                            nc, th->comment_num);
                    } else {
                        snprintf(fname, sizeof(fname), "%s/%ld", Mbox, th->comment_num);  /* modified on 2025-07-12, PL */
                    }
                    if ((fd = open_file(fname, OPEN_QUIET)) != -1) {
                        if ((buf = read_file(fd)) == NULL) {
                            output("\n%s\n\n", MSG_NOREAD);
                            return;
                        }
                        oldbuf = buf;
                        if (close_file(fd) == -1) {
                            return;
                        }
                        ptr = strstr(buf, MSG_EMFROM);
                        if (ptr) {
                            tmp = strchr(ptr, '\n');
                            *tmp = '\0';
                            //strcpy(username, (ptr + strlen(MSG_EMFROM)));
                            strlcpy(username, ptr + 6, sizeof(username));  /* fixed to prevent buffer overflow, 2025-09-14, PL */
							*tmp = '\n';
                        }
                        free(oldbuf);
                    }
                } else {
                    user_name(th->comment_author, username);
                }
		/* 2025-08-09, PL: prefer human name (no email) on follow-up line */
		{
		    char disp[256];
		    extract_display_name(username, disp, sizeof(disp));
		    snprintf(username, sizeof(username), "%.*s", (int)sizeof(username)-1, disp);
		}
		
		/* 2025-08-10, PL: strip surrounding quotes from display name */
		{
		    size_t L__ = strlen(username);
		    if (L__ >= 2 && username[0] == '"' && username[L__-1] == '"') {
		        username[L__-1] = '\0';
		        memmove(username, username + 1, L__ - 1);
		    }
		}
output_ansi_fmt("%s " BR_YELLOW "%s" DOT, "%s %s", MSG_BY, username); /* 2025-08-09, PL: print "av <name>" only once */
		if (th->comment_conf) {
                    conf_name(nc, username);
        	    sprintf(fname, "  showing MSG_IN");
                    debuglog(fname, 20);
                    output(" %s %s\n", MSG_IN, username);
                } else
                    output("\n");

            }
        }
    }
    if (!Current_conf && (th->author == Uid) &&
        (th->time > 0) && th->comment_author &&
        (th->comment_author != Uid) && (!Last_conf)) {
        user_name(th->comment_author, username);
        output("%s %s\n", MSG_COPYTO, username);
    }
    /* 2025-08-09, PL: Conference name is baked into line 1 now */

    if (Current_conf == 0) {
    /* Mailbox: keep "Mottagare:" logic unchanged */
    if (mailrec && !type) {
        output("%s %s\n", MSG_RECIPIENT, mailrec);
    } else {
        if (type < 0) {
            uid = -type;
            user_name(uid, username);
        } else {
            conf_name(type, username);
        }
        output("%s %s\n", MSG_RECIPIENT, username);
        }
    }
    //* decoded subject + trying to match underline everywhere WORK IN PROGRESS PL 2025-08-10*/
    if (edit_subject) {
    output(MSG_SUBJECT);
    input(th->subject, th->subject, SUBJECT_LEN, 0, 0, 0);
    } else {
    const char *raw_label = MSG_SUBJECT; /* 2025-08-10, PL: safe default to avoid NULL */
    LINE subj_dec, label_norm, subj_line;

    rfc2047_decode(th->subject, subj_dec, sizeof(subj_dec));
    normalize_label(raw_label, label_norm, sizeof(label_norm));

    //snprintf(subj_line, sizeof(subj_line), "%s%s", label_norm, subj_dec); //TO BE REMOVED
	subj_line[0] = '\0';
	strlcpy(subj_line, label_norm, sizeof(subj_line));        /* fixed on 2025-09-15, PL */
	strlcat(subj_line, subj_dec, sizeof(subj_line));          /* fixed on 2025-09-15, PL */



print_underlined_line(subj_line);  /* prints line + perfectly matching dashes (soon ;)) */
    }
}


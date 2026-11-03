/*
 * mailbody.c - MIME body extraction and HTML-to-text rendering for SklaffKOM
 *
 * This code intentionally does not try to be a complete mail client.  It is a
 * pragmatic importer helper: choose the best readable text part, decode common
 * transfer encodings, render HTML through lynx, and optionally clean tracking
 * URL noise.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mailbody.h"
#include "sklaff_programs.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_WIDTH "78"

struct part {
    char *ctype;
    char *charset;
    char *cte;
    char *body;
    size_t body_len;
};

static int mb_verbose;

static const char *
yesno(int v)
{
    return v ? "yes" : "no";
}

static void
die(const char *msg)
{
    perror(msg);
    exit(1);
}

static char *
xstrdup(const char *s)
{
    char *p;

    p = strdup(s ? s : "");
    if (p == NULL)
        die("strdup");

    return p;
}

static void *
xmalloc(size_t n)
{
    void *p;

    p = malloc(n ? n : 1);
    if (p == NULL)
        die("malloc");

    return p;
}

static int
ci_equal_n(const char *a, const char *b, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)a[i]) !=
            tolower((unsigned char)b[i]))
            return 0;
    }

    return 1;
}

static char *
ci_strstr(const char *haystack, const char *needle)
{
    size_t nlen;

    if (haystack == NULL || needle == NULL)
        return NULL;

    nlen = strlen(needle);
    if (nlen == 0)
        return (char *)haystack;

    for (; *haystack; haystack++) {
        if (ci_equal_n(haystack, needle, nlen))
            return (char *)haystack;
    }

    return NULL;
}

static char *
trim(char *s)
{
    char *e;

    while (*s && isspace((unsigned char)*s))
        s++;

    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = '\0';

    return s;
}

/*
 * Small header getter with enough unfolding for MIME Content-Type headers.
 * Continuation lines beginning with SP/HTAB are appended with one space.
 */
static char *
header_value(const char *headers, const char *name)
{
    const char *p;
    size_t namelen;

    namelen = strlen(name);
    p = headers;

    while (*p) {
        const char *line_end;
        size_t line_len;

        line_end = strchr(p, '\n');
        if (line_end == NULL)
            line_end = p + strlen(p);

        line_len = (size_t)(line_end - p);
        if (line_len > 0 && p[line_len - 1] == '\r')
            line_len--;

        if (line_len == 0)
            break;

        if (line_len > namelen &&
            ci_equal_n(p, name, namelen) &&
            p[namelen] == ':') {
            char *v;
            size_t cap;
            size_t len;
            const char *q;
            char *t;

            cap = line_len + 1;
            v = xmalloc(cap);
            len = line_len - namelen - 1;
            memcpy(v, p + namelen + 1, len);
            v[len] = '\0';

            q = (*line_end == '\n') ? line_end + 1 : line_end;
            while (*q == ' ' || *q == '\t') {
                const char *cont_end;
                size_t cont_len;
                char *nv;

                while (*q == ' ' || *q == '\t')
                    q++;

                cont_end = strchr(q, '\n');
                if (cont_end == NULL)
                    cont_end = q + strlen(q);

                cont_len = (size_t)(cont_end - q);
                if (cont_len > 0 && q[cont_len - 1] == '\r')
                    cont_len--;

                if (len + cont_len + 2 > cap) {
                    cap = len + cont_len + 2;
                    nv = realloc(v, cap);
                    if (nv == NULL)
                        die("realloc");
                    v = nv;
                }

                v[len++] = ' ';
                memcpy(v + len, q, cont_len);
                len += cont_len;
                v[len] = '\0';

                q = (*cont_end == '\n') ? cont_end + 1 : cont_end;
            }

            t = trim(v);
            if (t != v)
                memmove(v, t, strlen(t) + 1);

            return v;
        }

        if (*line_end == '\0')
            break;

        p = line_end + 1;
    }

    return NULL;
}

static char *
param_value(const char *s, const char *name)
{
    char *p;
    size_t namelen;

    if (s == NULL)
        return NULL;

    namelen = strlen(name);
    p = ci_strstr(s, name);
    if (p == NULL)
        return NULL;

    p += namelen;

    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p != '=')
        return NULL;

    p++;

    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p == '"') {
        char *end;
        char *out;

        p++;
        end = strchr(p, '"');
        if (end == NULL)
            return NULL;

        out = xmalloc((size_t)(end - p) + 1);
        memcpy(out, p, (size_t)(end - p));
        out[end - p] = '\0';
        return out;
    } else {
        char *end;
        char *out;

        end = p;
        while (*end && *end != ';' && !isspace((unsigned char)*end))
            end++;

        out = xmalloc((size_t)(end - p) + 1);
        memcpy(out, p, (size_t)(end - p));
        out[end - p] = '\0';
        return out;
    }
}

static int
hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

static char *
decode_qp(const char *in, size_t in_len, size_t *out_len)
{
    char *out;
    size_t i, o;

    out = xmalloc(in_len + 1);
    o = 0;

    for (i = 0; i < in_len; i++) {
        if (in[i] == '=' && i + 2 < in_len) {
            if (in[i + 1] == '\r' && in[i + 2] == '\n') {
                i += 2;
                continue;
            }

            if (in[i + 1] == '\n') {
                i += 1;
                continue;
            }

            if (i + 3 < in_len &&
                in[i + 1] == '\r' &&
                in[i + 2] == '\n') {
                i += 2;
                continue;
            }

            if (isxdigit((unsigned char)in[i + 1]) &&
                isxdigit((unsigned char)in[i + 2])) {
                int hi, lo;

                hi = hexval((unsigned char)in[i + 1]);
                lo = hexval((unsigned char)in[i + 2]);

                out[o++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }

        out[o++] = in[i];
    }

    out[o] = '\0';

    if (out_len != NULL)
        *out_len = o;

    return out;
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

static char *
decode_base64(const char *in, size_t in_len, size_t *out_len)
{
    char *out;
    int val;
    int valb;
    size_t i, o;

    out = xmalloc((in_len * 3) / 4 + 4);
    val = 0;
    valb = -8;
    o = 0;

    for (i = 0; i < in_len; i++) {
        int c;
        int d;

        c = (unsigned char)in[i];

        if (c == '=')
            break;

        d = b64val(c);
        if (d == -1)
            continue;

        val = (val << 6) | d;
        valb += 6;

        if (valb >= 0) {
            out[o++] = (char)((val >> valb) & 0xff);
            valb -= 8;
        }
    }

    out[o] = '\0';

    if (out_len != NULL)
        *out_len = o;

    return out;
}

static char *
latin1_to_utf8(const char *in, size_t in_len, size_t *out_len)
{
    char *out;
    size_t i, o;

    out = xmalloc(in_len * 2 + 1);
    o = 0;

    for (i = 0; i < in_len; i++) {
        unsigned char c;

        c = (unsigned char)in[i];

        if (c < 0x80) {
            out[o++] = (char)c;
        } else {
            out[o++] = (char)(0xc0 | (c >> 6));
            out[o++] = (char)(0x80 | (c & 0x3f));
        }
    }

    out[o] = '\0';

    if (out_len != NULL)
        *out_len = o;

    return out;
}

static char *
decode_body(const char *body, size_t body_len,
            const char *cte, const char *charset,
            size_t *out_len)
{
    char *tmp;
    size_t tmp_len;
    char *out;

    if (cte != NULL && ci_strstr(cte, "quoted-printable") != NULL) {
        tmp = decode_qp(body, body_len, &tmp_len);
    } else if (cte != NULL && ci_strstr(cte, "base64") != NULL) {
        tmp = decode_base64(body, body_len, &tmp_len);
    } else {
        tmp = xmalloc(body_len + 1);
        memcpy(tmp, body, body_len);
        tmp[body_len] = '\0';
        tmp_len = body_len;
    }

    if (charset != NULL &&
        (ci_strstr(charset, "iso-8859-1") != NULL ||
         ci_strstr(charset, "latin1") != NULL ||
         ci_strstr(charset, "latin-1") != NULL)) {
        out = latin1_to_utf8(tmp, tmp_len, out_len);
        free(tmp);
        return out;
    }

    if (out_len != NULL)
        *out_len = tmp_len;

    return tmp;
}

static int
looks_like_html(const char *s)
{
    if (s == NULL)
        return 0;

    while (*s && isspace((unsigned char)*s))
        s++;

    if (ci_strstr(s, "<!doctype html") != NULL)
        return 1;
    if (ci_strstr(s, "<html") != NULL)
        return 1;
    if (ci_strstr(s, "<body") != NULL)
        return 1;
    if (ci_strstr(s, "<table") != NULL)
        return 1;
    if (ci_strstr(s, "<div") != NULL)
        return 1;
    if (ci_strstr(s, "<p") != NULL && ci_strstr(s, "</p>") != NULL)
        return 1;

    return 0;
}

static void
free_part(struct part *p)
{
    if (p == NULL)
        return;

    free(p->ctype);
    free(p->charset);
    free(p->cte);
    free(p->body);

    p->ctype = NULL;
    p->charset = NULL;
    p->cte = NULL;
    p->body = NULL;
    p->body_len = 0;
}

static void
parse_part_headers(const char *headers, struct part *p)
{
    char *ct;

    memset(p, 0, sizeof(*p));

    ct = header_value(headers, "Content-Type");
    p->cte = header_value(headers, "Content-Transfer-Encoding");

    if (ct != NULL) {
        char *semi;

        p->charset = param_value(ct, "charset");

        semi = strchr(ct, ';');
        if (semi != NULL)
            *semi = '\0';

        p->ctype = xstrdup(trim(ct));
        free(ct);
    } else {
        p->ctype = xstrdup("text/plain");
    }

    if (p->charset == NULL)
        p->charset = xstrdup("utf-8");

    if (p->cte == NULL)
        p->cte = xstrdup("7bit");
}

static char *
find_body_start(char *msg)
{
    char *p;

    p = strstr(msg, "\r\n\r\n");
    if (p != NULL)
        return p + 4;

    p = strstr(msg, "\n\n");
    if (p != NULL)
        return p + 2;

    return NULL;
}

static char *
copy_range(const char *a, const char *b)
{
    char *out;
    size_t len;

    if (b < a)
        b = a;

    len = (size_t)(b - a);
    out = xmalloc(len + 1);
    memcpy(out, a, len);
    out[len] = '\0';

    return out;
}

static int
is_multipart_type(const char *ctype)
{
    return ctype != NULL && ci_strstr(ctype, "multipart/") == ctype;
}

static void
indent_debug(int depth)
{
    int i;

    for (i = 0; i < depth; i++)
        fprintf(stderr, "  ");
}

static int
extract_best_part_rec(char *msg, struct part *best,
                      char *selected_reason, size_t selected_reason_len,
                      int depth, const char *label)
{
    char *body_start;
    char *headers;
    char *top_ct;
    char *boundary;
    char marker[512];
    char *pos;
    struct part best_html;
    struct part best_plain;
    int have_html;
    int have_plain;
    int part_no;
    struct part container;

    memset(best, 0, sizeof(*best));
    memset(&best_html, 0, sizeof(best_html));
    memset(&best_plain, 0, sizeof(best_plain));

    body_start = find_body_start(msg);
    if (body_start == NULL)
        return -1;

    headers = copy_range(msg, body_start);
    top_ct = header_value(headers, "Content-Type");
    parse_part_headers(headers, &container);

    if (mb_verbose && depth == 0) {
        fprintf(stderr, "top Content-Type: %s\n",
                top_ct != NULL ? top_ct : "text/plain");
    }

    boundary = param_value(top_ct, "boundary");

    if (mb_verbose && depth == 0)
        fprintf(stderr, "boundary: %s\n", boundary != NULL ? boundary : "(none)");

    if (!is_multipart_type(container.ctype) || boundary == NULL) {
        struct part p;
        size_t dec_len;
        char *decoded;

        parse_part_headers(headers, &p);
        decoded = decode_body(body_start, strlen(body_start),
                              p.cte, p.charset, &dec_len);

        p.body = decoded;
        p.body_len = dec_len;

        if (mb_verbose) {
            indent_debug(depth);
            if (label != NULL)
                fprintf(stderr,
                        "part %s: %s, %s, %s, raw=%zu, decoded=%zu, looks_like_html=%s\n",
                        label, p.ctype, p.cte, p.charset, strlen(body_start), dec_len,
                        yesno(looks_like_html(p.body)));
            else
                fprintf(stderr,
                        "part 1: %s, %s, %s, raw=%zu, decoded=%zu, looks_like_html=%s\n",
                        p.ctype, p.cte, p.charset, strlen(body_start), dec_len,
                        yesno(looks_like_html(p.body)));
        }

        snprintf(selected_reason, selected_reason_len, "single part");
        *best = p;

        free_part(&container);
        free(headers);
        free(top_ct);
        free(boundary);
        return 0;
    }

    snprintf(marker, sizeof(marker), "--%s", boundary);

    pos = strstr(body_start, marker);
    have_html = 0;
    have_plain = 0;
    part_no = 0;

    while (pos != NULL) {
        char *part_start;
        char *part_body;
        char *next;
        char *part_headers;
        struct part p;
        size_t raw_len;
        size_t dec_len;
        char *decoded;
        int is_htmlish;
        int is_multi;
        char child_label[64];

        part_no++;
        pos += strlen(marker);

        if (pos[0] == '-' && pos[1] == '-')
            break;

        if (pos[0] == '\r' && pos[1] == '\n')
            pos += 2;
        else if (pos[0] == '\n')
            pos += 1;

        part_start = pos;
        part_body = find_body_start(part_start);
        if (part_body == NULL) {
            if (mb_verbose) {
                indent_debug(depth);
                fprintf(stderr, "part %d: no header/body separator found\n", part_no);
            }
            break;
        }

        next = strstr(part_body, marker);
        if (next == NULL)
            next = part_body + strlen(part_body);

        part_headers = copy_range(part_start, part_body);
        parse_part_headers(part_headers, &p);

        raw_len = (size_t)(next - part_body);
        is_multi = is_multipart_type(p.ctype);

        if (label != NULL)
            snprintf(child_label, sizeof(child_label), "%s.%d", label, part_no);
        else
            snprintf(child_label, sizeof(child_label), "%d", part_no);

        if (is_multi) {
            struct part child_best;
            char child_reason[128];
            char *part_msg;
            int rc;

            if (mb_verbose) {
                char *child_boundary;

                child_boundary = param_value(part_headers, "boundary");
                indent_debug(depth);
                fprintf(stderr,
                        "part %s: %s, %s, %s, raw=%zu, stored=recurse, boundary=%s\n",
                        child_label, p.ctype, p.cte, p.charset, raw_len,
                        child_boundary != NULL ? child_boundary : "(none)");
                free(child_boundary);
            }

            part_msg = copy_range(part_start, next);
            child_reason[0] = '\0';
            rc = extract_best_part_rec(part_msg, &child_best,
                                       child_reason, sizeof(child_reason),
                                       depth + 1, child_label);
            free(part_msg);
            free_part(&p);

            if (rc == 0) {
                int child_htmlish;
                const char *stored;

                child_htmlish = (ci_strstr(child_best.ctype, "text/html") != NULL ||
                                 looks_like_html(child_best.body));

                if (child_htmlish) {
                    if (!have_html) {
                        best_html = child_best;
                        have_html = 1;
                        stored = "nested-html";
                    } else {
                        free_part(&child_best);
                        stored = "duplicate-nested-html-ignore";
                    }
                } else if (ci_strstr(child_best.ctype, "text/plain") != NULL) {
                    if (!have_plain) {
                        best_plain = child_best;
                        have_plain = 1;
                        stored = "nested-plain";
                    } else {
                        free_part(&child_best);
                        stored = "duplicate-nested-plain-ignore";
                    }
                } else {
                    free_part(&child_best);
                    stored = "nested-ignored";
                }

                if (mb_verbose) {
                    indent_debug(depth);
                    fprintf(stderr, "part %s result: %s, stored=%s\n",
                            child_label, child_reason, stored);
                }
            }
        } else {
            decoded = decode_body(part_body, raw_len,
                                  p.cte, p.charset, &dec_len);

            p.body = decoded;
            p.body_len = dec_len;
            is_htmlish = (ci_strstr(p.ctype, "text/html") != NULL ||
                          looks_like_html(p.body));

            if (mb_verbose) {
                indent_debug(depth);
                fprintf(stderr,
                        "part %s: %s, %s, %s, raw=%zu, decoded=%zu, looks_like_html=%s",
                        child_label, p.ctype, p.cte, p.charset, raw_len, dec_len,
                        yesno(looks_like_html(p.body)));
            }

            if (is_htmlish) {
                if (!have_html) {
                    if (mb_verbose)
                        fprintf(stderr, ", stored=html\n");
                    best_html = p;
                    have_html = 1;
                } else {
                    if (mb_verbose)
                        fprintf(stderr, ", stored=duplicate-html-ignore\n");
                    free_part(&p);
                }
            } else if (ci_strstr(p.ctype, "text/plain") != NULL) {
                if (!have_plain) {
                    if (mb_verbose)
                        fprintf(stderr, ", stored=plain\n");
                    best_plain = p;
                    have_plain = 1;
                } else {
                    if (mb_verbose)
                        fprintf(stderr, ", stored=duplicate-plain-ignore\n");
                    free_part(&p);
                }
            } else {
                if (mb_verbose)
                    fprintf(stderr, ", stored=ignored\n");
                free_part(&p);
            }
        }

        free(part_headers);
        pos = next;
    }

    free_part(&container);
    free(headers);
    free(top_ct);
    free(boundary);

    if (have_plain && !looks_like_html(best_plain.body)) {
        snprintf(selected_reason, selected_reason_len, "plain text");
        *best = best_plain;
        if (have_html)
            free_part(&best_html);
        return 0;
    }

    if (have_html) {
        if (have_plain)
            snprintf(selected_reason, selected_reason_len,
                     "html via text/plain fallback");
        else
            snprintf(selected_reason, selected_reason_len, "html");
        *best = best_html;
        if (have_plain)
            free_part(&best_plain);
        return 0;
    }

    if (have_plain) {
        snprintf(selected_reason, selected_reason_len, "plain text fallback");
        *best = best_plain;
        return 0;
    }

    return -1;
}

static int
extract_best_part(char *msg, struct part *best,
                  char *selected_reason, size_t selected_reason_len)
{
    return extract_best_part_rec(msg, best,
                                 selected_reason, selected_reason_len,
                                 0, NULL);
}


struct sbuf {
    char *data;
    size_t len;
    size_t cap;
};

static void
sbuf_init(struct sbuf *b)
{
    b->cap = 1024;
    b->len = 0;
    b->data = xmalloc(b->cap);
    b->data[0] = '\0';
}

static void
sbuf_reserve(struct sbuf *b, size_t extra)
{
    if (b->len + extra + 1 > b->cap) {
        char *nbuf;

        while (b->len + extra + 1 > b->cap)
            b->cap *= 2;

        nbuf = realloc(b->data, b->cap);
        if (nbuf == NULL)
            die("realloc");
        b->data = nbuf;
    }
}

static void
sbuf_putc(struct sbuf *b, int c)
{
    sbuf_reserve(b, 1);
    b->data[b->len++] = (char)c;
    b->data[b->len] = '\0';
}

static void
sbuf_putn(struct sbuf *b, const char *s, size_t n)
{
    sbuf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static int
url_char(int c)
{
    return c > 32 && c != '<' && c != '>' && c != '"';
}

static int
is_url_start(const char *s)
{
    return strncmp(s, "http://", 7) == 0 ||
           strncmp(s, "https://", 8) == 0;
}

static int
is_tracking_param(const char *key, size_t key_len)
{
    if (key_len >= 4 && strncmp(key, "utm_", 4) == 0)
        return 1;

    if (key_len == 4 && strncmp(key, "ecid", 4) == 0)
        return 1;
    if (key_len == 6 && strncmp(key, "_hsenc", 6) == 0)
        return 1;
    if (key_len == 5 && strncmp(key, "_hsmi", 5) == 0)
        return 1;
    if (key_len == 13 && strncmp(key, "hsCtaTracking", 13) == 0)
        return 1;

    return 0;
}

static char *
clean_url(const char *url, size_t url_len, int *changed)
{
    struct sbuf out;
    const char *q;
    const char *frag;
    const char *end;
    const char *p;
    int first;

    *changed = 0;

    q = memchr(url, '?', url_len);
    if (q == NULL)
        return copy_range(url, url + url_len);

    frag = memchr(url, '#', url_len);
    if (frag != NULL && frag < q)
        return copy_range(url, url + url_len);

    end = url + url_len;
    sbuf_init(&out);
    sbuf_putn(&out, url, (size_t)(q - url));

    first = 1;
    p = q + 1;

    while (p < end && *p != '#') {
        const char *param_start;
        const char *param_end;
        const char *eq;
        const char *key_end;
        size_t key_len;

        param_start = p;
        param_end = p;
        while (param_end < end && *param_end != '&' && *param_end != '#')
            param_end++;

        eq = memchr(param_start, '=', (size_t)(param_end - param_start));
        key_end = eq != NULL ? eq : param_end;
        key_len = (size_t)(key_end - param_start);

        if (!is_tracking_param(param_start, key_len)) {
            sbuf_putc(&out, first ? '?' : '&');
            sbuf_putn(&out, param_start, (size_t)(param_end - param_start));
            first = 0;
        } else {
            *changed = 1;
        }

        p = param_end;
        if (p < end && *p == '&')
            p++;
    }

    if (p < end && *p == '#')
        sbuf_putn(&out, p, (size_t)(end - p));

    return out.data;
}

static char *
cleanup_text(const char *in, size_t in_len, size_t *out_len,
             int *urls_cleaned, int *blank_lines_removed)
{
    struct sbuf out;
    size_t i;
    int col;
    int blank_run;
    int line_has_text;

    *urls_cleaned = 0;
    *blank_lines_removed = 0;

    sbuf_init(&out);
    i = 0;
    col = 0;
    blank_run = 0;
    line_has_text = 0;

    while (i < in_len) {
        if (is_url_start(in + i)) {
            size_t j;
            char *u;
            int changed;
            size_t ulen;

            j = i;
            while (j < in_len && url_char((unsigned char)in[j]))
                j++;

            while (j > i && (in[j - 1] == ')' || in[j - 1] == '.' ||
                             in[j - 1] == ',' || in[j - 1] == ';'))
                j--;

            u = clean_url(in + i, j - i, &changed);
            ulen = strlen(u);

            if (changed)
                (*urls_cleaned)++;

            sbuf_putn(&out, u, ulen);
            col += (int)ulen;
            line_has_text = 1;
            free(u);
            i = j;
            continue;
        }

        if (in[i] == '\r') {
            i++;
            continue;
        }

        if (in[i] == '\n') {
            while (out.len > 0 && (out.data[out.len - 1] == ' ' ||
                                   out.data[out.len - 1] == '\t')) {
                out.data[--out.len] = '\0';
            }

            if (!line_has_text) {
                blank_run++;
                if (blank_run > 2) {
                    (*blank_lines_removed)++;
                    i++;
                    col = 0;
                    continue;
                }
            } else {
                blank_run = 0;
            }

            sbuf_putc(&out, '\n');
            col = 0;
            line_has_text = 0;
            i++;
            continue;
        }

        if ((in[i] == ' ' || in[i] == '\t') && col == 0) {
            i++;
            continue;
        }

        sbuf_putc(&out, in[i]);
        if (!isspace((unsigned char)in[i]))
            line_has_text = 1;
        col++;
        i++;
    }

    while (out.len > 0 && isspace((unsigned char)out.data[out.len - 1]))
        out.data[--out.len] = '\0';

    sbuf_putc(&out, '\n');

    if (out_len != NULL)
        *out_len = out.len;

    return out.data;
}



static char *
render_html_with_lynx_dup(const char *html, size_t html_len, size_t *out_len)
{
    char template[] = "/tmp/mailbody.XXXXXX";
    int fd;
    int outpipe[2];
    pid_t pid;
    int status;
    struct sbuf out;
    char buf[4096];
    ssize_t n;

    fd = mkstemp(template);
    if (fd == -1)
        return NULL;

    if (write(fd, html, html_len) == -1) {
        close(fd);
        unlink(template);
        return NULL;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        close(fd);
        unlink(template);
        return NULL;
    }

    if (pipe(outpipe) == -1) {
        close(fd);
        unlink(template);
        return NULL;
    }

    pid = fork();
    if (pid == -1) {
        close(fd);
        close(outpipe[0]);
        close(outpipe[1]);
        unlink(template);
        return NULL;
    }

    if (pid == 0) {
        dup2(fd, STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        close(fd);
        close(outpipe[0]);
        close(outpipe[1]);

        execlp(LYNX_BIN,
               LYNX_BIN,
               "-dump",
               "-stdin",
               "-width=" DEFAULT_WIDTH,
               "-nolist",
               (char *)NULL);

        perror("execlp " LYNX_BIN);
        _exit(127);
    }

    close(fd);
    close(outpipe[1]);
    unlink(template);

    sbuf_init(&out);
    while ((n = read(outpipe[0], buf, sizeof(buf))) > 0)
        sbuf_putn(&out, buf, (size_t)n);

    close(outpipe[0]);

    if (waitpid(pid, &status, 0) == -1) {
        free(out.data);
        return NULL;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(out.data);
        return NULL;
    }

    if (out_len != NULL)
        *out_len = out.len;

    return out.data;
}

char *
mailbody_render_utf8_opts_dup(const char *msg, int flags)
{
    char *work;
    struct part best;
    char selected_reason[128];
    int htmlish;
    char *rendered;
    size_t rendered_len;
    int clean;

    if (msg == NULL)
        return NULL;

    clean = (flags & MAILBODY_CLEANUP) != 0;
    mb_verbose = (flags & MAILBODY_VERBOSE) != 0;
    selected_reason[0] = '\0';

    work = xstrdup(msg);

    if (extract_best_part(work, &best,
                          selected_reason, sizeof(selected_reason)) == -1) {
        free(work);
        return NULL;
    }

    htmlish = looks_like_html(best.body) ||
              ci_strstr(best.ctype, "text/html") != NULL;

    if (mb_verbose) {
        fprintf(stderr, "selected: %s\n", selected_reason);
        fprintf(stderr, "renderer: %s%s\n",
                htmlish ? LYNX_BIN : "plain stdout",
                clean ? " + cleanup" : "");
    }

    if (htmlish) {
        rendered = render_html_with_lynx_dup(best.body, best.body_len,
                                            &rendered_len);
        if (rendered == NULL) {
            free_part(&best);
            free(work);
            return NULL;
        }
    } else {
        rendered = xmalloc(best.body_len + 2);
        memcpy(rendered, best.body, best.body_len);
        rendered_len = best.body_len;
        if (rendered_len == 0 || rendered[rendered_len - 1] != '\n')
            rendered[rendered_len++] = '\n';
        rendered[rendered_len] = '\0';
    }

    free_part(&best);
    free(work);

    if (clean) {
        char *cleaned;
        size_t cleaned_len;
        int urls_cleaned;
        int blank_lines_removed;

        cleaned = cleanup_text(rendered, rendered_len, &cleaned_len,
                               &urls_cleaned, &blank_lines_removed);
        free(rendered);

        if (mb_verbose) {
            fprintf(stderr, "cleanup: urls_cleaned=%d, "
                    "extra_blank_lines_removed=%d\n",
                    urls_cleaned, blank_lines_removed);
        }

        return cleaned;
    }

    return rendered;
}

char *
mailbody_render_utf8_dup(const char *msg, int clean, int verbose)
{
    int flags;

    flags = 0;
    if (clean)
        flags |= MAILBODY_CLEANUP;
    if (verbose)
        flags |= MAILBODY_VERBOSE;

    return mailbody_render_utf8_opts_dup(msg, flags);
}

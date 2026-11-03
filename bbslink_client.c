/*
 * SklaffKOM BBSLink client - experimental standalone implementation.
 *
 * This is an original implementation of the BBSLink connection protocol,
 * not a translation or redistribution of an upstream connection script.
 *
 * Requires libcurl and OpenSSL's libcrypto. It deliberately leaves Telnet
 * negotiation to the platform telnet client.
 *
 * NOTE: BBSLink's documented legacy protocol uses HTTP and Telnet without
 * transport encryption. Obtain the service operator's agreement before
 * publishing or shipping a third-party client.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#ifndef SKLAFFDIR
#define SKLAFFDIR "/usr/local/sklaff"
#endif

#ifndef BBSLINK_HOST
#define BBSLINK_HOST "games.bbslink.net"
#endif
#ifndef BBSLINK_CREDENTIALS
#define BBSLINK_CREDENTIALS SKLAFFDIR "/etc/bbslink_credentials"
#endif
#ifndef BBSLINK_TELNET_BIN
#define BBSLINK_TELNET_BIN "telnet"
#endif

#define MAX_CODE 128
#define MAX_REPLY 4096

struct credentials {
    char syscode[MAX_CODE];
    char authcode[MAX_CODE];
    char schemecode[MAX_CODE];
};

struct reply {
    char data[MAX_REPLY];
    size_t len;
};

static char *
trim(char *s)
{
    char *end;
    while (isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

static int
copy_code(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    size_t i;
    if (n == 0 || n >= cap) return -1;
    for (i = 0; i < n; ++i)
        if ((unsigned char)src[i] < 33 || (unsigned char)src[i] > 126)
            return -1;
    memcpy(dst, src, n + 1);
    return 0;
}

static int
load_credentials(const char *path, struct credentials *c)
{
    FILE *f;
    struct stat st;
    char line[512];
    int fd, ok = 0;
    unsigned int seen = 0;

    memset(c, 0, sizeof(*c));
#ifdef O_NOFOLLOW
    fd = open(path, O_RDONLY | O_NOFOLLOW);
#else
    fd = open(path, O_RDONLY);
#endif
    if (fd < 0) {
        fprintf(stderr, "Cannot open BBSLink credentials: %s\n", strerror(errno));
        return -1;
    }
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || (st.st_mode & 077) != 0) {
        fprintf(stderr, "BBSLink credentials must be a regular private file (chmod 600).\n");
        close(fd);
        return -1;
    }
    f = fdopen(fd, "r");
    if (f == NULL) { close(fd); return -1; }
    while (fgets(line, sizeof(line), f)) {
        char *k, *v, *equals;
        if (!strchr(line, '\n') && !feof(f)) goto done;
        k = trim(line);
        if (!*k || *k == '#') continue;
        equals = strchr(k, '=');
        if (!equals) goto done;
        *equals = '\0';
        k = trim(k);
        v = trim(equals + 1);
        if (strcmp(k, "syscode") == 0) {
            if (seen & 1 || copy_code(c->syscode, sizeof(c->syscode), v)) goto done;
            seen |= 1;
        } else if (strcmp(k, "authcode") == 0) {
            if (seen & 2 || copy_code(c->authcode, sizeof(c->authcode), v)) goto done;
            seen |= 2;
        } else if (strcmp(k, "schemecode") == 0) {
            if (seen & 4 || copy_code(c->schemecode, sizeof(c->schemecode), v)) goto done;
            seen |= 4;
        } else {
            goto done;
        }
    }
    if (!ferror(f) && seen == 7) ok = 1;
done:
    fclose(f);
    if (!ok) fprintf(stderr, "Invalid BBSLink credentials file.\n");
    return ok ? 0 : -1;
}

static int
make_nonce(char out[7])
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    unsigned char v;
    size_t i = 0;
    while (i < 6) {
        if (RAND_bytes(&v, 1) != 1) return -1;
        if (v < 248) out[i++] = alphabet[v % 62];
    }
    out[6] = '\0';
    return 0;
}

static int
make_signature(const char *secret, const char *token, char hex[33])
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len;
    char *buf;
    size_t a = strlen(secret), b = strlen(token);
    unsigned int i;
    static const char hexchars[] = "0123456789abcdef";
    if (a > 1024 || b > 1024) return -1;
    buf = malloc(a + b + 1);
    if (!buf) return -1;
    memcpy(buf, secret, a);
    memcpy(buf + a, token, b);
    buf[a + b] = '\0';
    if (EVP_Digest(buf, a + b, digest, &len, EVP_md5(), NULL) != 1 || len != 16) {
        memset(buf, 0, a + b + 1);
        free(buf);
        return -1;
    }
    memset(buf, 0, a + b + 1);
    free(buf);
    for (i = 0; i < len; ++i) {
        hex[2 * i] = hexchars[digest[i] >> 4];
        hex[2 * i + 1] = hexchars[digest[i] & 15];
    }
    hex[32] = '\0';
    return 0;
}

static size_t
receive_reply(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct reply *r = (struct reply *)userdata;
    size_t n;
    if (size != 0 && nmemb > (size_t)-1 / size) return 0;
    n = size * nmemb;
    if (n > MAX_REPLY - r->len - 1) return 0;
    memcpy(r->data + r->len, ptr, n);
    r->len += n;
    r->data[r->len] = '\0';
    return n;
}

static int
http_get(CURL *curl, const char *endpoint, struct curl_slist *headers,
         struct reply *r)
{
    char url[256];
    CURLcode rc;
    long status = 0;
    if (snprintf(url, sizeof(url), "http://%s/%s", BBSLINK_HOST, endpoint)
        >= (int)sizeof(url)) return -1;
    memset(r, 0, sizeof(*r));
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_reply);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, r);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SklaffKOM-BBSLink/0.1");
    rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        fprintf(stderr, "BBSLink HTTP error: %s\n", curl_easy_strerror(rc));
        return -1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) {
        fprintf(stderr, "BBSLink returned HTTP %ld\n", status);
        return -1;
    }
    return 0;
}

static int
add_header(struct curl_slist **list, const char *key, const char *value)
{
    char line[256];
    struct curl_slist *next;
    if (snprintf(line, sizeof(line), "%s: %s", key, value)
        >= (int)sizeof(line)) return -1;
    next = curl_slist_append(*list, line);
    if (next == NULL) return -1;
    *list = next;
    return 0;
}

static int
is_identifier(const char *s, int digits_only)
{
    size_t n = strlen(s), i;
    if (n == 0 || n > 40) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (digits_only) {
            if (!isdigit(c)) return 0;
        } else if (!isalnum(c) && c != '_' && c != '-') {
            return 0;
        }
    }
    return 1;
}

int
main(int argc, char **argv)
{
    struct credentials creds;
    struct curl_slist *headers = NULL;
    struct reply result;
    CURL *curl = NULL;
    char key[7], urlpath[64];
    char signature_auth[33], signature_scheme[33];
    char *body;
    const char *path = BBSLINK_CREDENTIALS;
    const char *door;
    const char *user;
    int arg = 1, success = 0;

    if (argc > 2 && strcmp(argv[1], "-c") == 0) {
        path = argv[2];
        arg = 3;
    }
    if (argc - arg != 2 || !is_identifier(argv[arg], 0) ||
        !is_identifier(argv[arg + 1], 1)) {
        fprintf(stderr, "Usage: bbslink_client [-c credentials] <door> <user-number>\n");
        return 2;
    }
    door = argv[arg];
    user = argv[arg + 1];
    if (getuid() != geteuid() || getgid() != getegid()) {
        fprintf(stderr, "Refusing to run with setuid/setgid privileges.\n");
        return 1;
    }
    if (load_credentials(path, &creds) != 0 || make_nonce(key) != 0)
        return 1;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) return 1;
    curl = curl_easy_init();
    if (curl == NULL) goto done;
    snprintf(urlpath, sizeof(urlpath), "token.php?key=%s", key);
    if (http_get(curl, urlpath, NULL, &result) != 0) goto done;
    body = trim(result.data);
    if (strlen(body) < 1 || strlen(body) > 256 ||
        strchr(body, '\r') != NULL || strchr(body, '\n') != NULL) {
        fprintf(stderr, "Invalid BBSLink authentication token.\n");
        goto done;
    }
    if (make_signature(creds.authcode, body, signature_auth) != 0 ||
        make_signature(creds.schemecode, body, signature_scheme) != 0)
        goto done;

#define ADD(k, v) do { if (add_header(&headers, (k), (v)) != 0) goto done; } while (0)
    ADD("X-User", user);
    ADD("X-System", creds.syscode);
    ADD("X-Auth", signature_auth);
    ADD("X-Code", signature_scheme);
    ADD("X-Rows", "24");
    ADD("X-Key", key);
    ADD("X-Door", door);
    ADD("X-Token", body);
    ADD("X-Type", "C");
    ADD("X-Version", "0.1");
#undef ADD

    snprintf(urlpath, sizeof(urlpath), "auth.php?key=%s", key);
    if (http_get(curl, urlpath, headers, &result) != 0) goto done;
    if (strcmp(trim(result.data), "complete") != 0) {
        fprintf(stderr, "BBSLink rejected authentication.\n");
        goto done;
    }
    success = 1;

done:
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    memset(&creds, 0, sizeof(creds));
    if (!success) return 1;
    puts("\033[2J\033[HConnecting to BBSLink...");
    fflush(stdout);
    setenv("TERM", "linux", 1);
    {
        char *const telnet_args[] = {
            (char *)BBSLINK_TELNET_BIN, "-E", "-K", "-8",
            (char *)BBSLINK_HOST, "23", NULL
        };
        execvp(BBSLINK_TELNET_BIN, telnet_args);
    }
    perror("Cannot start telnet");
    return 1;
}

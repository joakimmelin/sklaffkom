/* mailtoss.c */

/*
 *   SklaffKOM, a simple conference system for UNIX.
 *
 *   Copyright (C) 1994  Carl Sundbom
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

#include "sklaff.h"
#include "ext_globals.h"
#include <pwd.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include "globals.h"
#include <stdlib.h>
#include <string.h>  /* strlen, strstr, strerror (2026-05-13, PL) */
#include <errno.h>   /* ENOENT for graceful "no mail" exit (2025-08-13, PL) */
#include <stdarg.h>  /* verbose status output (2026-05-13, PL) */

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
qp_decode_dup(const char *src)
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

int send_mail(int uid, char *mbuf, int ouid, int ogrp);
static int count_mbox_messages(const char *buf);
static int truncate_spool(const char *mbox);
static void mt_status(const char *fmt, ...);

static void
mt_status(const char *fmt, ...)
{
    va_list ap;

    printf("mailtoss: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

static int
count_mbox_messages(const char *buf)
{
    const char *ptr;
    int count = 1;

    if (buf == NULL || strlen(buf) < 50)
        return 0;

    ptr = buf;
    while ((ptr = strstr(ptr, "\nFrom ")) != NULL) {
        count++;
        ptr++;
    }

    return count;
}

static int
truncate_spool(const char *mbox)
{
    /* Keep the spool file in place; only remove its contents after import (2026-05-13, PL). */
    if (truncate(mbox, 0) == -1)
        return -1;

    return 0;
}

int
main(int argc, char *argv[])
{
    LINE username, mbox;
    struct passwd *pw;
    char *ptr, *ptr2, *buf, *oldbuf;
    int uid, fd = -1; /* init avoids -Wmaybe-uninitialized (2025-08-13, PL) */
    int identified = 0, imported = 0;

    if (argc != 2) {
        printf("\n%s\n\n", MSG_MTINFO);
        exit(1);
    }
    pw = getpwnam(argv[1]);
    if (pw == NULL) {
        mt_status("användaren '%s' finns inte.", argv[1]);
        exit(1);
    }
    uid = pw->pw_uid;

    pw = getpwnam(SKLAFF_ACCT);
    if (pw == NULL) {
        mt_status("systemanvändaren '%s' finns inte.", SKLAFF_ACCT);
        exit(1);
    }

    if ((uid == 0) || (user_name(uid, username) == NULL)) {
        mt_status("'%s' är inte en giltig SklaffKOM-användare. Inget importerat.", argv[1]);
        exit(0);
    }

    snprintf(mbox, sizeof(mbox), "%s/%s", MAIL_LIB, argv[1]);
    mt_status("kontrollerar mailspool: %s", mbox);

    /* Graceful exit if the spool is empty (common under cron - not a real error). */
    if (access(mbox, R_OK) == -1) {
        if (errno == ENOENT) {
            mt_status("ingen spoolfil hittades. Inget att importera.");
            return 0; /* no mail -- not an error (2025-08-13, PL) */
        }
        mt_status("kan inte läsa spoolfilen: %s", strerror(errno));
    }

    if ((fd = open_file(mbox, 0)) == -1) {
        mt_status("kunde inte öppna spoolfilen.");
        exit(1);
    }
    if (fd < 0) /* belt & suspenders in case of weird flow */
        return 1;

    mt_status("spool hittad: OK!");

    if ((buf = read_file(fd)) == NULL) {
        mt_status("kunde inte läsa spoolfilen.");
        exit(1);
    }
    oldbuf = buf;
    if (close_file(fd) == -1) {
        mt_status("kunde inte stänga spoolfilen.");
        exit(1);
    }

    identified = count_mbox_messages(buf);
    if (identified == 0) {
        mt_status("spoolfilen är tom eller innehåller ingen importerbar maildata.");
        mt_status("spoolen är redan tom. Ingen trunkering utförd.");
        free(oldbuf);
        exit(0);
    }

    mt_status("%d nya mail identifierade.", identified);

    if (truncate_spool(mbox) == -1) {
        mt_status("kunde inte trunkera spoolen: %s", strerror(errno));
        free(oldbuf);
        exit(1);
    }

    mt_status("importerar %d mail...", identified);

    while (1) {
        ptr = strstr(buf, "\nFrom ");
        if (ptr) {
            *ptr = '\0';
            if (send_mail(uid, buf, pw->pw_uid, pw->pw_gid) == -1) {
                mt_status("import misslyckades efter %d importerade mail.", imported);
                free(oldbuf);
                exit(1);
            }
            imported++;
            *ptr = '\n';
            buf = ptr + 1;
        } else
            break;
    }

    ptr = buf;
    while (1) {
        ptr2 = ptr;
        ptr++;
        ptr = strchr(ptr, '\n');
        if (!ptr)
            break;
    }
    *ptr2 = '\0';

    if (send_mail(uid, buf, pw->pw_uid, pw->pw_gid) == -1) {
        mt_status("import misslyckades efter %d importerade mail.", imported);
        free(oldbuf);
        exit(1);
    }
    imported++;
    free(oldbuf);

    mt_status("importerar %d mail... OK!", imported);
    mt_status("%d nya mail importerade.", imported);
    mt_status("spoolen trunkerad: OK!");

    if (user_is_active(uid))
        notify_user(uid, SIGNAL_NEW_TEXT);

    exit(0);
}

int
send_mail(int uid, char *mbuf, int ouid, int ogrp)
{
    LINE home;
    char conffile[256];   /* increased from 80 to prevent truncation warnings (2025-08-06, PL) */
    char confdir[256];    /* increased from 80 to prevent truncation warnings (2025-08-06, PL) */
    char textfile[512];   /* increased from 80 to prevent truncation warnings (2025-08-06, PL) */
    struct CONF_ENTRY ce;
    struct TEXT_HEADER th;
    int fd, fdo;
	char *buf, *oldbuf, *nbuf = NULL, *ptr, *tmp, *fbuf;
	char *plainbuf = NULL, *decodedbuf = NULL, *sf7body = NULL, *storebuf = NULL; 	/* added 2026-05-28 for better looking e-mail imports */
	const char *hdr_end;															/* added 2026-05-28 for better looking e-mail imports */
	size_t header_len, store_len;													/* added 2026-05-28 for better looking e-mail imports */

    mbox_dir(uid, home);
    snprintf(conffile, sizeof(conffile), "%s%s", home, MAILBOX_FILE);  		/* fixed on 2025-08-06, PL */
    snprintf(confdir, sizeof(confdir), "%s/", home);                   		/* fixed on 2025-08-06, PL */


    if ((fd = open_file(conffile, 0)) == -1)
        return -1;
    if ((buf = read_file(fd)) == NULL)
        return -1;
    oldbuf = buf;
    while ((buf = get_conf_entry(buf, &ce)))
        if (ce.num == 0)
            break;
    if (ce.num == 0) {
        ce.last_text++;
        nbuf = replace_conf(&ce, oldbuf);
        if (!nbuf) {
            printf("\n%s\n\n", MSG_CONFMISSING);
            return -1;
        }
    }
    snprintf(textfile, sizeof(textfile), "%s%ld", confdir, ce.last_text); 	/* fixed on 2025-08-06, PL */
    if ((fdo = open_file(textfile, OPEN_QUIET | OPEN_CREATE)) == -1) {
        printf("\n%s\n\n", MSG_ERRCREATET);
        return -1;
    }
 	ptr = strstr(mbuf, MSG_EMSUB);
    if (ptr) {
        ptr = ptr + strlen(MSG_EMSUB);
        tmp = strchr(ptr, '\n');
        if (tmp) {
            *tmp = '\0';
            strncpy(th.subject, ptr, (SUBJECT_LEN - 2));
            th.subject[SUBJECT_LEN - 1] = 0;
            *tmp = '\n';
        } else {
            strncpy(th.subject, ptr, (SUBJECT_LEN - 2));
            th.subject[SUBJECT_LEN - 1] = 0;
        }
    } else {
        strcpy(th.subject, "");
    }

    /*
     * Modern incoming mail handling:
     * - extract text/plain from MIME multipart if present
     * - decode quoted-printable if needed
     * - convert UTF-8 body to SklaffKOM internal SF7
     * - preserve original mail headers
     */
    plainbuf = mail_extract_text_plain_dup(mbuf);
    if (plainbuf == NULL) {
        sys_error("send_mail", 1, "mail_extract_text_plain_dup");
        return -1;
    }

 	if (mail_has_base64(mbuf)) {
    	decodedbuf = mail_base64_decode_dup(plainbuf);
	} else if (mail_has_quoted_printable(mbuf)) {
    	decodedbuf = mail_qp_decode_dup(plainbuf);
	} else {
    	decodedbuf = strdup(plainbuf);
	}
    free(plainbuf);

    if (decodedbuf == NULL) {
        sys_error("send_mail", 1, "mail_qp_decode_dup/strdup");
        return -1;
    }

    sf7body = utf8_to_sf7_dup(decodedbuf);
    free(decodedbuf);

    if (sf7body == NULL) {
        sys_error("send_mail", 1, "utf8_to_sf7_dup");
        return -1;
    }

    /*
     * Preserve original headers, but replace raw MIME body with clean SF7 text.
     */
    hdr_end = strstr(mbuf, "\r\n\r\n");
    if (hdr_end) {
        header_len = (size_t)(hdr_end - mbuf);
    } else {
        hdr_end = strstr(mbuf, "\n\n");
        if (hdr_end)
            header_len = (size_t)(hdr_end - mbuf);
        else
            header_len = 0;
    }

    if (header_len > 0) {
		store_len = header_len + 2 + strlen(sf7body) + 1;
        storebuf = malloc(store_len);
        if (storebuf == NULL) {
            free(sf7body);
            sys_error("send_mail", 1, "malloc storebuf");
            return -1;
        }

        memcpy(storebuf, mbuf, header_len);
        storebuf[header_len] = '\0';
        strcat(storebuf, "\n\n");
        strcat(storebuf, sf7body);
    } else {
        storebuf = strdup(sf7body);
        if (storebuf == NULL) {
            free(sf7body);
            sys_error("send_mail", 1, "strdup storebuf");
            return -1;
        }
    }

    free(sf7body);

    th.size = 0;
    ptr = storebuf;
    while (1) {
        ptr = strchr(ptr, '\n');
        if (ptr) {
            th.size++;
            ptr++;
        } else {
            break;
        }
    }

 	store_len = strlen(storebuf) + strlen(th.subject) + sizeof(LONG_LINE);

    fbuf = malloc(store_len);
    if (fbuf == NULL) {
        free(storebuf);
        sys_error("send_mail", 1, "malloc");
        return -1;
    }

    memset(fbuf, 0, store_len);
    sprintf(fbuf, "%ld:%d:%lld:%ld:%d:%d:%d\n",
        ce.last_text,         /* Text number */
        0,                    /* Author UID */
        (long long) time(0),  /* Unix time */
        0L,                   /* Unknown */
        0,                    /* Unknown */
        0,                    /* Receiver UID? */
        th.size);             /* Number of lines */


    strcat(fbuf, th.subject);
    strcat(fbuf, "\n");
    strcat(fbuf, storebuf);

    if (write_file(fdo, fbuf) == -1) {
    	free(storebuf);
    	return -1;
	}
	if (close_file(fdo) == -1) {
    	free(storebuf);
    	return -1;
	}

	free(storebuf);

    if (chown(textfile, ouid, ogrp) == -1) { /* Error handling PL 2025-08-10  */
    /* TODO perror("chown"); debuglog(...); */
    }

    if (write_file(fd, nbuf) == -1)
        return -1;
    if (close_file(fd) == -1)
        return -1;

    return 0;
}

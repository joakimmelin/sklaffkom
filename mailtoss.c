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
#include <errno.h>   /* ENOENT for graceful "no mail" exit (2025-08-13, PL) */

int send_mail(int uid, char *mbuf, int ouid, int ogrp);

int
main(int argc, char *argv[])
{
    LINE username, mbox;
    struct passwd *pw;
    char *ptr, *ptr2, *buf, *oldbuf;
    int uid, fd = -1; /* init avoids -Wmaybe-uninitialized (2025-08-13, PL) */

    if (argc != 2) {
        printf("\n%s\n\n", MSG_MTINFO);
        exit(1);
    }
    pw = getpwnam(argv[1]);
    if (pw == NULL)
        exit(1);
    uid = pw->pw_uid;

    pw = getpwnam(SKLAFF_ACCT);
    if (pw == NULL)
        exit(1);

    if ((uid == 0) || (user_name(uid, username) == NULL))
        exit(0);

    sprintf(mbox, "%s/%s", MAIL_LIB, argv[1]);
    /* Graceful exit if the spool is empty (common under cron - not a real error). */
    if (access(mbox, R_OK) == -1) {
        if (errno == ENOENT) {
            return 0; /* no mail -- not an error (2025-08-13, PL) */
        }
    }
    if ((fd = open_file(mbox, 0)) == -1)
        exit(1);
    if (fd < 0) /* belt & suspenders in case of weird flow */
        return 1;
    if ((buf = read_file(fd)) == NULL)
        exit(1);
    oldbuf = buf;
    if (close_file(fd) == -1)
	{
        exit(1);
	}
    //unlink(mbox);
	/* modified on 2025-09-14, PL: truncate the spool file instead of deleting it */
	int trunc_fd = open(mbox, O_WRONLY | O_TRUNC);
	if (trunc_fd >= 0) {
    close(trunc_fd);
	} else {
    perror("open for truncating spool file");
	}
	/* If you want the old behaviour, delete the above code block and uncomment unlink(mbox); */

    if (strlen(buf) < 50)
        exit(0);
    while (1) {
        ptr = strstr(buf, "\nFrom ");
        if (ptr) {
            *ptr = '\0';
            send_mail(uid, buf, pw->pw_uid, pw->pw_gid);
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

    if (send_mail(uid, buf, pw->pw_uid, pw->pw_gid) == -1)
        exit(1);
    free(oldbuf);

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
    char *buf, *oldbuf, *nbuf = NULL, *ptr, *tmp, *fbuf; 			/* fixed on 2025-08-06, PL */

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
    ptr = mbuf;
    while (*ptr) {
        if ((unsigned char) *ptr == 134)
            *ptr = '}';
        else if ((unsigned char) *ptr == 132)
            *ptr = '{';
        else if ((unsigned char) *ptr == 148)
            *ptr = '|';
        else if ((unsigned char) *ptr == 143)
            *ptr = ']';
        else if ((unsigned char) *ptr == 142)
            *ptr = '[';
        else if ((unsigned char) *ptr == 153)
            *ptr = 0x05c;
        if ((unsigned char) *ptr == 229)
            *ptr = '}';
        else if ((unsigned char) *ptr == 228)
            *ptr = '{';
        else if ((unsigned char) *ptr == 246)
            *ptr = '|';
        else if ((unsigned char) *ptr == 197)
            *ptr = ']';
        else if ((unsigned char) *ptr == 196)
            *ptr = '[';
        else if ((unsigned char) *ptr == 214)
            *ptr = 0x05c;
        if ((unsigned char) *ptr == 140)
            *ptr = '}';
        else if ((unsigned char) *ptr == 138)
            *ptr = '{';
        else if ((unsigned char) *ptr == 154)
            *ptr = '|';
        else if ((unsigned char) *ptr == 129)
            *ptr = ']';
        else if ((unsigned char) *ptr == 128)
            *ptr = '[';
        else if ((unsigned char) *ptr == 133)
            *ptr = 0x05c;
        ptr++;
    }

    fbuf = (char *) malloc(strlen(mbuf) + sizeof(LONG_LINE));
    if (fbuf == NULL) {
        sys_error("send_mail", 1, "malloc");
        return -1;
    }
    ptr = strstr(mbuf, MSG_EMSUB);
    if (ptr) {
        ptr = ptr + strlen(MSG_EMSUB);
        tmp = strchr(ptr, '\n');
        *tmp = '\0';
        strncpy(th.subject, ptr, (SUBJECT_LEN - 2));
        th.subject[SUBJECT_LEN - 1] = 0;
        *tmp = '\n';
    } else
        strcpy(th.subject, "");

    th.size = 0;
    ptr = mbuf;
    while (1) {
        ptr = strchr(ptr, '\n');
        if (ptr) {
            th.size++;
            ptr++;
        } else
            break;
    }

    th.time = time(0);

    memset(fbuf, 0, strlen(mbuf) + sizeof(LONG_LINE));
    sprintf(fbuf, "%ld:%d:%lld:%ld:%d:%d:%d\n",
        ce.last_text,         /* Text number */
        0,                    /* Author UID */
        (long long) th.time,  /* Unix time */
        0L,                   /* Unknown */
        0,                    /* Unknown */
        0,                    /* Receiver UID? */
        th.size);             /* Number of lines */


    strcat(fbuf, th.subject);
    strcat(fbuf, "\n");
    strcat(fbuf, mbuf);

    if (write_file(fdo, fbuf) == -1)
        return -1;
    if (close_file(fdo) == -1)
        return -1;

    if (chown(textfile, ouid, ogrp) == -1) { /* Error handling PL 2025-08-10  */
    /* TODO perror("chown"); debuglog(...); */
    }

    if (write_file(fd, nbuf) == -1)
        return -1;
    if (close_file(fd) == -1)
        return -1;

    return 0;
}

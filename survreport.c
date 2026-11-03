/* survreport.c */

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
#include <sys/stat.h>
#include <signal.h>

#include "globals.h"

long post_survey_result(char *, struct TEXT_HEADER *, int, int, int);

int
main(int argc, char *argv[])
{
    int confid, fd;
    long num;
    struct passwd *pw;
    struct TEXT_ENTRY te;
    struct TEXT_HEADER *th;
    char *buf, *oldbuf, *bing;
    LINE survtxt, tmp;

    if (argc != 3) {
        printf("\n%s\n\n", MSG_SRINFO);
        exit(1);
    }
    confid = atoi(argv[1]);
    num = atol(argv[2]);


    pw = getpwnam(SKLAFF_ACCT);
    if (pw == NULL)
        exit(1);

    sprintf(survtxt, "%s/%d/%ld", SKLAFF_DB, confid, num);

    if ((fd = open_file(survtxt, OPEN_QUIET)) == -1) {
        output("\n%s\n\n", MSG_NOSURVEY);
        return 1;
    }
    if ((buf = read_file(fd)) == NULL) {
        output("\n%s\n\n", MSG_NOREAD);
        return 1;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        return 1;
    }
    buf = get_text_entry(buf, &te);


    free(oldbuf);

    th = &te.th;

    if (th->type == TYPE_SURVEY && th->sh.time <= time(0)) {
        th->comment_author = th->author;
        th->author = SKLAFF_SURVEY_REPORTER;
        th->time = th->sh.time;
        th->comment_num = num;
        th->comment_conf = 0;
        strcpy(tmp, MSG_REPORT);
        strlcat(tmp, th->subject, sizeof(tmp));  /* modified on 2025-07-13, PL */
        strcpy(th->subject, tmp);
        th->type = TYPE_TEXT;

        bing = show_survey_result(num, confid, te.body, th->sh.n_questions);

        if (bing)
            post_survey_result(bing, th, confid, pw->pw_uid, pw->pw_gid);
        free(bing);
    } else {
        if (th->type != TYPE_SURVEY)
            output("%s\n", MSG_NOTASURVEY);
        else
            output("%s\n", MSG_NOREPORT);
    }

    free_text_entry(&te);

    exit(0);
}

/*
     post_survey_result is essentially a copy of save_text()...
*/

long
post_survey_result(char *resultbuf, struct TEXT_HEADER * th, int conf, int ouid, int ogrp)
{
    int fd, fdoutfile, usernum, i;
    char *buf, *oldbuf, *nbuf, *fbuf, *sb;
    LINE cname, newline;
    char conffile[256], confdir[256], textfile[256], home[256];  /* increased size to prevent truncation, modified on 2025-07-12, PL */
    struct CONF_ENTRY ce;

    if (conf < 0) {
        usernum = conf - (conf * 2);
        mbox_dir(usernum, home);
        snprintf(conffile, sizeof(conffile), "%.200s%s", home, MAILBOX_FILE); /* 2025-07-13 PL */
        snprintf(confdir, sizeof(confdir), "%.200s/", home); /* 2025-07-13 PL */
        conf = 0;
    } else {
        strcpy(conffile, CONF_FILE);
        sprintf(confdir, "%s/%d/", SKLAFF_DB, conf);
    }

    if ((fd = open_file(conffile, 0)) == -1) {
        return -1L;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1L;
    }
    oldbuf = buf;

    while ((buf = get_conf_entry(buf, &ce))) {
        if (ce.num == conf)
            break;
    }

    if (ce.num == conf) {
        ce.last_text++;
        nbuf = replace_conf(&ce, oldbuf);
        if (!nbuf) {
            output("\n%s\n\n", MSG_CONFMISSING);
            return -1L;
        }
    } else {
        output("\n%s\n\n", MSG_CONFMISSING);
        return -1L;
    }

    snprintf(textfile, sizeof(textfile), "%.200s%ld", confdir, ce.last_text); /* 2025-07-13 PL */

    if ((fdoutfile = open_file(textfile, OPEN_QUIET | OPEN_CREATE)) == -1) {
        output("\n%s\n\n", MSG_ERRCREATET);
        return -1L;
    }
    fbuf = (char *) malloc(strlen(resultbuf) + sizeof(LONG_LINE));
    if (fbuf == NULL) {
        sys_error("post_survey_result", 1, "malloc");
        return -1L;
    }
    memset(fbuf, 0, strlen(resultbuf) + sizeof(LONG_LINE));

    /* Compute size of buffer, in lines */

    for (th->size = 0, sb = resultbuf; (sb = strchr(sb, '\n')) != NULL; sb++, th->size++);

    if (th->type == TYPE_TEXT)
        sprintf(fbuf, "%ld:%d:%lld:%ld:%d:%d:%d:%d\n", ce.last_text, th->author,
            (long long) th->time, th->comment_num, th->comment_conf,
            th->comment_author, th->size, th->type);
    else
        sprintf(fbuf, "%ld:%d:%lld:%ld:%d:%d:%d:%d:%d:%lld\n", ce.last_text, th->author,
            (long long) th->time, th->comment_num, th->comment_conf,
            th->comment_author, th->size, th->type,
            th->sh.n_questions, (long long) th->sh.time);
    strcat(fbuf, th->subject);
    strcat(fbuf, "\n");
    strcat(fbuf, resultbuf);

    critical();
    if (write_file(fdoutfile, fbuf) == -1) {
        return -1L;
    }
    if (close_file(fdoutfile) == -1) {
        return -1L;
    }
    if (chown(textfile, ouid, ogrp) == -1) {			/* 2025-08-10 PL compiler silencer */
    /* maybe perror("chown"); or debuglog(...) */
    }

    if (write_file(fd, nbuf) == -1) {
        return -1L;
    }
    if (close_file(fd) == -1) {
        return -1L;
    }
    non_critical();

    /* Add comment line in original survey */


    sprintf(cname, "%s/%d/%ld", SKLAFF_DB,
        conf, th->comment_num);

    if ((fd = open_file(cname, OPEN_QUIET)) == -1) {
        output("\n%s\n\n", MSG_NOTEXT);
        return 0;
    }
    if ((buf = read_file(fd)) == NULL) {
        output("\n%s\n\n", MSG_NOREAD);
        return 0;
    }
    i = strlen(buf) + LINE_LEN;
    nbuf = (char *) malloc(i);
    if (!nbuf) {
        sys_error("cmd_comment", 1, "malloc");
        return -1;
    }
    memset(nbuf, 0, i);

    sprintf(newline, "%ld:%d\n", ce.last_text, th->author);
    strcpy(nbuf, buf);
    strcat(nbuf, newline);
    free(buf);

    critical();
    if (write_file(fd, nbuf) == -1) {
        output("\n%s\n\n", MSG_NOREPPTR);
        return 0;
    }
    if (close_file(fd) == -1) {
        return 0;
    }
    non_critical();

    notify_all_processes(SIGNAL_NEW_TEXT);
    return ce.last_text;
}

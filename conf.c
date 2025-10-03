/* conf.c */

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

#include <sys/stat.h>

#include "sklaff.h"
#include "ext_globals.h"

void free_userlist(struct USER_LIST *);

/*
 * conf_name - finds out name of conference
 * args: conference number (num), pointer to string (name)
 * ret: pointer to string or NULL
 */

char *
conf_name(int num, char *name)
{
    int fd;
    char *buf, *oldbuf;
    struct CONF_ENTRY ce;

    if (num == 0) {
        strcpy(name, MSG_MAILBOX);
        return name;
    }
    if ((fd = open_file(CONF_FILE, 0)) == -1) {
        sys_error("conf_name", 1, "open_file");
        return NULL;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("conf_name", 2, "read_file");
        return NULL;
    }
    oldbuf = buf;
    if (close_file(fd) == -1) {
        sys_error("conf_name", 3, "close_file");
        return NULL;
    }
    buf = get_conf_entry(buf, &ce);
    while ((ce.num != num) && buf)
        buf = get_conf_entry(buf, &ce);
    free(oldbuf);
    if (ce.num == num) {
        strcpy(name, ce.name);
        return name;
    } else
        return NULL;
}

/*
 * conf_num - get number of conference
 * args: name of conference (name)
 * ret: number of conference or failure (0) or error (-1)
 */

int
conf_num(char *name)
{
    int fd;
    char *buf, *oldbuf;
    struct CONF_ENTRY ce;
    LINE mboxname;

    if (name == NULL)
        return -1;
    strcpy(mboxname, MSG_MAILBOX);
    if (cmp_strings(mboxname, name))
        return 0;
    if ((fd = open_file(CONF_FILE, 0)) == -1) {
        sys_error("conf_name", 1, "open_file");
        return 0;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("conf_name", 2, "read_file");
        return 0;
    }
    oldbuf = buf;
    if (close_file(fd) == -1) {
        sys_error("conf_name", 3, "close_file");
        return 0;
    }
    buf = get_conf_entry(buf, &ce);
    while (buf && (strcmp(name, ce.name) != 0))
        buf = get_conf_entry(buf, &ce);
    free(oldbuf);
    if (strcmp(name, ce.name) == 0)
        return ce.num;
    else
        return -1;
}

/*
 * get_conf_struct - get conference entry struct
 * args: conference (num)
 * ret: pointer to static CONF_ENTRY or NULL
 */

struct CONF_ENTRY *
get_conf_struct(int num)
{
    int fd;
    char *buf, *oldbuf;
    static struct CONF_ENTRY ce;

    if (!num) {
    }
    if ((fd = open_file(CONF_FILE, 0)) == -1) {
        sys_error("get_conf_struct", 1, "open_file");
        return NULL;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("get_conf_struct", 2, "read_file");
        return NULL;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        sys_error("get_conf_struct", 3, "close_file");
        return NULL;
    }
    buf = get_conf_entry(buf, &ce);
    while ((ce.num != num) && buf)
        buf = get_conf_entry(buf, &ce);
    free(oldbuf);
    if (ce.num == num)
        return &ce;
    else
        return NULL;
}

/*
 * get_confrc_struct - get confrc struct for conference
 * args: conference (num)
 * ret: pointer to static USER_LIST or NULL
 */

struct USER_LIST *
get_confrc_struct(int num)
{
    int fd;
    char *buf, *oldbuf;
    LONG_LINE file_name;
    struct USER_LIST *ul;

    if (num <= 0) {
        return NULL;
    }
    snprintf(file_name, sizeof(file_name), "%s/%d%s", SKLAFF_DB, num, CONFRC_FILE);
    if ((fd = open_file(file_name, 0)) == -1) {
        sys_error("get_confrc_struct", 1, "open_file");
        return NULL;
    }
    fflush(stdout);
    if ((buf = read_file(fd)) == NULL) {
        sys_error("get_confrc_struct", 2, "read_file");
        return NULL;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        sys_error("get_confrc_struct", 3, "close_file");
        return NULL;
    }
    ul = get_confrc_users(buf);
    free(oldbuf);
    return ul;
}

/*
 * is_conf_creator - check if user if creator of conference
 * args: user (u_num), conference (c_num)
 * ret: yes (1) or no (0)
 */

int
is_conf_creator(int u_num, int c_num)
{
    struct CONF_ENTRY *ce;

    if (!c_num)
        return 1;
    if ((ce = get_conf_struct(c_num)) == NULL) {
        return 0;
    }
    return (u_num == ce->creator);
}

/*
 * can_see_conf - is user allowed to see conference?
 * args: user (u_num), conference (c_num), type (t_num) creator (cr_num)
 * ret: yes (1) or no (0)
 */

int
can_see_conf(int u_num, int c_num, int t_num, int cr_num)
{
    int right;
    struct USER_LIST *ul;

    if (t_num != SECRET_CONF)
        return 1;
    if (cr_num == u_num)
        return 1;
    if (member_of(u_num, c_num)) {
        return 1;
    }
    ul = get_confrc_struct(c_num);
    right = 1;
    right = conf_right(ul, u_num, t_num, cr_num);
    free_userlist(ul);
    return ((right < 2) && (right >= 0));
}


/*
 * stringify_confs_struct - make string from confs struct
 * args: pointer to CONFS_ENTRY
 * ret: pointer to static string
 */

char *
stringify_confs_struct(struct CONFS_ENTRY * cse, char *buf)
{
    LINE intbuf;
    struct INT_LIST *int_list_sav;

    sprintf(buf, "%d:", cse->num);
    int_list_sav = cse->il;
    while (cse->il) {
        sprintf(intbuf, "%ld-%ld", cse->il->from, cse->il->to);
        strcat(buf, intbuf);
        cse->il = cse->il->next;
        if (cse->il)
            strcat(buf, ",");
    }
    strcat(buf, "\n");
    cse->il = int_list_sav;
    return buf;
}

/*
 * free_confs_entry - free malloc memory in CONFS_ENTRY
 * args: pointer to CONFS_ENTRY
 */

void
free_confs_entry(struct CONFS_ENTRY * cse)
{
    struct INT_LIST *til, *wil;

    wil = cse->il;
    while (wil) {
        til = wil->next;
        free(wil);
        wil = til;
    }
    cse->il = NULL;
    return;
}

/*
 * free_userlist - free malloc memory in USER_LIST
 * args: pointer to USER_LIST
 */

void
free_userlist(struct USER_LIST * ul)
{
    struct USER_LIST
    *tul;

    while (ul) {
        tul = ul->next;
        free(ul);
        ul = tul;
    }
}

/*
 * replace_confs - replace CONFS_ENTRY in confsfile
 * args: pointer to CONFS_ENTRY (cse), buffer to replace in (buf)
 * ret: pointer to buffer with replaced entry
 */

char *
replace_confs(struct CONFS_ENTRY * cse, char *buf)
{
    char *tbuf, *nbuf, *obuf, temp;
    HUGE_LINE tmpbuf;
    int i;
    struct CONFS_ENTRY tcse;

    obuf = buf;
    i = strlen(buf) + LINE_LEN;
    nbuf = (char *) malloc(i);  /* Jaja... */
    memset(nbuf, 0, i);

    /* Find confs-entry */

    while (buf) {
        buf = get_confs_entry(buf, &tcse);
        if (cse->num == tcse.num)
            break;
        free_confs_entry(&tcse);
    }

    if (cse->num == tcse.num) {

        /* Scan until beginning of confs-entry in buf */

        tbuf = buf;

        if (tbuf > obuf)
            tbuf--;

        while ((tbuf > obuf) && (*tbuf == '\n'))
            tbuf--;

        while ((tbuf > obuf) && (*tbuf != '\n'))
            tbuf--;

        if (tbuf > obuf)
            tbuf++;

        temp = *tbuf;
        *tbuf = '\0';           /* Set 'length' of obuf */

        strcpy(nbuf, obuf);
        *tbuf = temp;
        strcat(nbuf, stringify_confs_struct(cse, tmpbuf));
        strcat(nbuf, buf);
        free(obuf);
        free_confs_entry(&tcse);
    }
    return nbuf;
}

/*
 * replace_conf - replace CONF_ENTRY in conffile
 * args: pointer to CONF_ENTRY (ce), buffer to replace in (buf)
 * ret: pointer to a newly allocated buffer with the replaced entry (caller must free), or NULL on failure
 */

/* Major rewrite by PL August 2025 due to inconsisent behaviour in usenet meetings */
char *
replace_conf(struct CONF_ENTRY *ce, char *buf)
{
    char *tbuf, *nbuf, *obuf;
    int i;
    LONG_LINE newline;
    struct CONF_ENTRY tce = {0};   /* Temporary entry for comparison */

    if (!ce || !buf)
        return NULL;  /* Invalid input */

    obuf = buf;  /* Save original pointer */

    /* Allocate a new buffer large enough to hold replacement */
    i = strlen(buf) + LINE_LEN;
    nbuf = (char *) malloc(i);
    if (!nbuf)
        return NULL;  /* Allocation failed */

    memset(nbuf, 0, i);

    /* Search for the conf entry to replace */
    while (buf) {
        char *prev_buf = buf;
        buf = get_conf_entry(buf, &tce);
        if (buf && ce->num == tce.num)
            break;
        if (!buf) {
            /* Reached end without match */
            free(nbuf);
            return NULL;
        }
        if (buf == prev_buf) {
            /* Prevent infinite loop if get_conf_entry is buggy */
            break;
        }
    }

    /* At this point, buf points to the conf entry to be replaced */
    tbuf = buf;
    if (!tbuf) {
        free(nbuf);
        return NULL;
    }

    /* Backtrack to find start of line for the matching entry */
    if (tbuf > obuf)
        tbuf--;

    while ((tbuf > obuf) && (*tbuf == '\n'))
        tbuf--;

    while ((tbuf > obuf) && (*tbuf != '\n'))
        tbuf--;

    if (tbuf > obuf)
        tbuf++;  /* Now tbuf points to start of the line to replace */

    /* Terminate original buffer just before the matching line */
    *tbuf = '\0';

    /* Create the new replacement line */
    snprintf(newline, sizeof(newline), "%d:%ld:%d:%lld:%d:%d:%d:%s\n",
             ce->num, ce->last_text, ce->creator, (long long) ce->time,
             ce->type, ce->life, ce->comconf, ce->name);

    /* Assemble the new buffer */
    strcpy(nbuf, obuf);        /* Keep content before the replaced line */
    strcat(nbuf, newline);     /* Add new entry */
    strcat(nbuf, buf);         /* Add remaining content after replaced line */

    /* DO NOT free(obuf) here — caller owns original memory */

    return nbuf;
}

/*
 * stringify_conf_struct - make string from CONF_ENTRY
 * args: pointer to CONF_ENTRY
 * ret: pointer to static string
 */

char *
stringify_conf_struct(struct CONF_ENTRY * conf_entry, char *buf)
{
    sprintf(buf, "%d:%ld:%d:%lld:%d:%d:%d:%s\n", conf_entry->num,
        conf_entry->last_text, conf_entry->creator,
        (long long) conf_entry->time, conf_entry->type, conf_entry->life,
        conf_entry->comconf, conf_entry->name);
    return buf;
}

/*
 * set_conf - set active conference
 * args: conference to set active (conf)
 * ret: conference number
 */

int
set_conf(int conf)
{
    Current_conf = conf;
    Last_conf = conf;
    return conf;
}

/*
 * set_first_conf - set active conference at login
 * ret: conference number
 */

int
set_first_conf(void)
{
    int conf;

    conf = 0;
    set_conf(conf);
    if (!more_text()) {
        conf = more_conf();
        if (conf != -1) {
            set_conf(conf);
        } else {
            conf = 0;
        }
    }
    return conf;
}

/*
 * list_confs - list conferences for user
 * args: user (uid), list all (all)
 * ret:	ok (0) or error (-1)
 */

int
list_confs(int uid, int all)
{
    int fd, rights, count, x, xit;
    char *buf, *oldbuf, member, creator, filarea, *mbuf, *oldmbuf;
    LINE mboxname, filelist, confsname;
    LONG_LINE confname;
    struct CONF_ENTRY ce;
    struct CONFS_ENTRY cse;
    struct CEL *ce_list, *top;
    struct CEN *topconf, *conflist;
    long ftext, ntext;  /* we will now calculate the correct number of posts, as in sklaffadm 2025-08-11 PL */

    user_dir(uid, confsname);
    strcat(confsname, CONFS_FILE);

    if ((fd = open_file(confsname, 0)) == -1) {
        return -1;
    }
    if ((mbuf = read_file(fd)) == NULL) {
        return -1;
    }
    oldmbuf = mbuf;

    if (close_file(fd) == -1) {
        return -1;
    }
    /*
     *	Mailbox
     */

    mbox_dir(uid, mboxname);
    strcat(mboxname, MAILBOX_FILE);

    if ((fd = open_file(mboxname, 0)) == -1) {
        sys_error("list_confs", 1, "open_file");
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("list_confs", 2, "read_file");
        return -1;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        sys_error("list_confs", 3, "close_file");
        return -1;
    }
    output("\n");
    if ((buf = get_conf_entry(buf, &ce))) { 			/* Mailbox: show count instead of just the highest number 2025-08-11 PL */
        ftext = first_text(ce.num, uid);
        if (ftext <= 0 || ce.last_text <= 0 || ftext > ce.last_text)
            ntext = 0;
        else
            ntext = ce.last_text - ftext + 1;
        output("%6ld       %s\n", ntext, ce.name);
    }
    free(oldbuf);

    /*
     *	Conferences
     */

    ce_list = NULL;
    count = 0;
    top = NULL;
    if ((fd = open_file(CONF_FILE, 0)) == -1) {
        sys_error("list_confs", 1, "open_file");
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("list_confs", 2, "read_file");
        return -1;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        sys_error("list_confs", 3, "close_file");
        return -1;
    }
    while (buf) {
        buf = get_conf_entry(buf, &ce);
        if (ce.creator != -1) { /* Only list confs that aren't erased */
            if (buf) {
                if (ce_list) {
                    ce_list->next = (struct CEL *) malloc
                        (sizeof(struct CEL) + 1);
                    if (ce_list->next == NULL) {
                        sys_error("list_confs", 1, "malloc");
                        return -1;
                    }
                    ce_list = ce_list->next;
                    ce_list->next = NULL;
                } else {
                    ce_list = (struct CEL *) malloc
                        (sizeof(struct CEL) + 1);
                    if (ce_list == NULL) {
                        sys_error("list_conf", 1, "malloc");
                        return -1;
                    }
                    top = ce_list;
                    ce_list->next = NULL;
                }
                count++;
                strcpy(ce_list->name, ce.name);
                ce_list->unreads = ce.last_text;
		ftext = first_text(ce.num, uid); 			/* In conferences: store count of articles instead of highest number 2025-08-11 PL */
                if (ftext <= 0 || ce.last_text <= 0 || ftext > ce.last_text)
                    ntext = 0;
                else
                    ntext = ce.last_text - ftext + 1;
                ce_list->unreads = ntext;                                
                ce_list->creator = ce.creator;
                ce_list->num = ce.num;
                ce_list->type = ce.type;
            }
        }
    }
    free(oldbuf);
    conflist = sort_conf(top, count);
    topconf = conflist;
    for (x = 0; x < count; x++) {
        rights = conflist->type;
        if (rights == NEWS_CONF)
            rights = OPEN_CONF;
        if (conflist->creator == Uid) {
            creator = MSG_CONFCREATOR;
            rights = 0;
        } else {
            creator = ' ';
            if ((rights == SECRET_CONF) &&
                (can_see_conf(Uid, conflist->num, conflist->type,
                        conflist->creator)))
                rights = 0;
        }

        if (rights < 2) {
            strcpy(confname, conflist->name);
            if (conflist->type > 0) {
                if (conflist->type == 1)
                    strcat(confname, MSG_CLOSED);
                else if (conflist->type == 2)
                    strcat(confname, MSG_SECRET);
                else
                    strcat(confname, MSG_NEWS);
            }
            filarea = ' ';
            sprintf(filelist, "%s/%d%s", FILE_DB, conflist->num, INDEX_FILE);
            if (file_exists(filelist) != -1)
                filarea = MSG_FILAREA;
            xit = 0;
            mbuf = oldmbuf;
            while ((mbuf = get_confs_entry(mbuf, &cse)) != NULL) {
                free_confs_entry(&cse);
                if (cse.num == conflist->num) {
                    xit = 1;
                    break;
                }
            }
            if (!xit) {
                member = MSG_MEMBERMARK;
            } else {
                member = ' ';
            }
            if (output("%6ld  %c %c %c%s\n", conflist->unreads,
                    creator, filarea, member, confname) == -1) {
                break;
            }
        }
        conflist++;
    }
    output("\n");
    free(topconf);
    free(oldmbuf);
    return 0;
}


/*
 * conf_right - checks rights for user in conference
 * args: USER_LIST (ul), user (uid), type (type), creator (cr_num)
 * ret: all (0) or look (1) or none (2) or error (-1)
 */

int
conf_right(struct USER_LIST * ul, int uid, int type, int cr_num)
{
    int xit;

    xit = type;
    if (xit == NEWS_CONF)
        xit = OPEN_CONF;
    if (cr_num == uid) {
        xit = 0;
    } else {
        while (ul) {
            if (ul->num == uid)
                break;
            else
                ul = ul->next;
        }

        if (ul && (ul->num == uid)) {
            if ((type == OPEN_CONF) || (type == NEWS_CONF))
                xit = 1;
            else
                xit = 0;
        }
    }

    return xit;
}

/*
 * num_unread - returns number of unread texts in conference
 * args: user (uid), conference (conf), last text in conference (last)
 * ret: number of texts or error (-1L)
 */

long
num_unread(int uid, int conf, long last)
{
    long unread;
    int fd;
    char *buf, *oldbuf;
    LINE confsname;
    struct CONFS_ENTRY cse;
    struct INT_LIST *int_list_sav;

    user_dir(uid, confsname);
    strcat(confsname, CONFS_FILE);

    if ((fd = open_file(confsname, 0)) == -1) {
        sys_error("num_unread", 1, "open_file");
        return -1L;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("num_unread", 2, "read_file");
        return -1L;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        sys_error("num_unread", 3, "close_file");
        return -1L;
    }
    unread = 0;

    while ((buf = get_confs_entry(buf, &cse))) {
        if (cse.num == conf)
            break;
        free_confs_entry(&cse);
    }

    if (cse.num == conf) {
        unread = last;
        int_list_sav = cse.il;
        while (cse.il) {
            unread = unread - (cse.il->to - cse.il->from + 1L);
            cse.il = cse.il->next;
        }
        cse.il = int_list_sav;
    }
    free_confs_entry(&cse);
    free(oldbuf);
    return unread;
}

/*
 * member_of - check if user is member of conference
 * args: user (uid), conference (conf)
 * ret: yes (1) or no (0) or error (-1)
 */

int
member_of(int uid, int conf)
{
    int fd, xit;
    char *buf, *oldbuf;
    struct CONFS_ENTRY cse;
    LINE confsname;

    user_dir(uid, confsname);
    strcat(confsname, CONFS_FILE);

    if ((fd = open_file(confsname, 0)) == -1) {
        return -1L;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1L;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        return -1L;
    }
    xit = 0;
    while ((buf = get_confs_entry(buf, &cse)) != NULL) {
        free_confs_entry(&cse);
        if (cse.num == conf) {
            xit = 1;
            break;
        }
    }
    free(oldbuf);
    return xit;
}

/*
 * more_conf - check for conference with unread texts (fast version)
 * ret: conference number or -1
 */

int
more_conf(void)
{
    int fd, fd2, saveconf, i, flag, confnum;
    long text, first = 0, high, *confsiz;
    char *buf, *oldbuf, *buf2, *oldbuf2, *nbuf, *tmpbuf, saved;
    char fname[512];
    struct CONFS_ENTRY cse;
    struct CONF_ENTRY ce, mbox;


    flag = 0;
    high = 0L;

    /* get mailbox */

    strcpy(fname, Mbox);
    strcat(fname, MAILBOX_FILE);
    if ((fd = open_file(fname, 0)) == -1)
        return -1;
    if ((buf = read_file(fd)) == NULL)
        return -1;
    oldbuf = buf;
    if (close_file(fd) == -1)
        return -1;
    buf = get_conf_entry(buf, &mbox);
    free(oldbuf);

    /* get conference list */

    if ((fd = open_file(CONF_FILE, 0)) == -1)
        return -1;
    if ((buf = read_file(fd)) == NULL)
        return -1;
    oldbuf = buf;
    if (close_file(fd) == -1)
        return -1;

    /* get user conference list */

    strcpy(fname, Home);
    strcat(fname, CONFS_FILE);
    if ((fd2 = open_file(fname, 0)) == -1)
        return -1;
    if ((buf2 = read_file(fd2)) == NULL)
        return -1;
    oldbuf2 = buf2;
    if (close_file(fd2) == -1)
        return -1;


    /* check in mailbox */

    buf2 = get_confs_entry(buf2, &cse);
    if (cse.il == NULL)
        text = 1L;
    else if (cse.il->from > 1L)
        text = 1L;
    else
        text = (cse.il->to + 1L);
    if (text > mbox.last_text)
        text = 0L;
    free_confs_entry(&cse);
    if (text > 0) {
        free(oldbuf);
        free(oldbuf2);
        return 0;
    }
    /* check current number of conferences */
    buf = oldbuf;
    ce.num = 0;

    for (;;) {
        confnum = ce.num;
        buf = get_conf_entry(buf, &ce);
        if (buf == NULL)
            break;
    }
    confsiz = malloc(sizeof(*confsiz) * (confnum + 1));
    buf = oldbuf;
    for (;;) {
        buf = get_conf_entry(buf, &ce);
        if (buf == NULL)
            break;
        confsiz[ce.num] = ce.last_text;
    }


    /* check in conferences ahead */

    buf2 = oldbuf2;
    while ((buf2 = get_confs_entry(buf2, &cse))) {
        free_confs_entry(&cse);
        if (cse.num == Current_conf)
            break;
    }


    while ((buf2 = get_confs_entry(buf2, &cse))) {
        saveconf = cse.num;
        while (1) {
            if (cse.il == NULL) {
                text = 1L;
                high = text;
            } else if (cse.il->from > 1L) {
                text = 1L;
                high = 0L;
            } else {
                text = (cse.il->to + 1L);
                if (cse.il->next)
                    high = 0L;
                else
                    high = text;
            }
            free_confs_entry(&cse);
            if (text > confsiz[cse.num]) {
                text = 0L;
                break;
            } else {
                /* buf = oldbuf;
                 * 
                 * while (buf = get_conf_entry(buf, &ce)) if (ce.num == cse.num)
                 * break;
                 * 
                 * sprintf(fname, "%s/%d/%ld", SKLAFF_DB, ce.num, text); */
                sprintf(fname, "%s/%d/%ld", SKLAFF_DB, cse.num, text);
                if (file_exists(fname) == -1) {
                    if (!flag && high) {
                        flag = 1;
                        first = first_text(cse.num, Uid);
                        /* first = first_text(ce.num, Uid); */
                    }
                    if (high && (first > text)) {
                        i = strlen(oldbuf2) + 10;
                        nbuf = (char *) malloc(i);
                        if (!nbuf) {
                            return -1;
                        }
                        memset(nbuf, 0, i);
                        tmpbuf = buf2;
                        tmpbuf--;
                        while ((tmpbuf > oldbuf2) && (*tmpbuf == '\n'))
                            tmpbuf--;
                        while ((tmpbuf > oldbuf2) && (*tmpbuf != '\n'))
                            tmpbuf--;
                        if (tmpbuf > oldbuf2)
                            tmpbuf++;
                        saved = *tmpbuf;
                        *tmpbuf = 0;
                        strcpy(nbuf, oldbuf2);
                        *tmpbuf = saved;
                        snprintf(fname, sizeof(fname), "%d:1-%ld\n", cse.num, (first - 1L));
                        strcat(nbuf, fname);
                        strcat(nbuf, buf2);
                        strcpy(fname, Home);
                        strcat(fname, CONFS_FILE);
                        critical();
                        if ((fd2 = open_file(fname, 0)) == -1)
                            return -1;
                        if (write_file(fd2, nbuf) == -1)
                            return -1;
                        if (close_file(fd2) == -1)
                            return -1;
                        non_critical();
                        high = 0L;
                    } else
                        mark_as_read(text, cse.num);
                    /* mark_as_read(text, ce.num); */
                    free(oldbuf2);
                    strcpy(fname, Home);
                    strcat(fname, CONFS_FILE);
                    if ((fd2 = open_file(fname, 0)) == -1)
                        return -1;
                    if ((buf2 = read_file(fd2)) == NULL)
                        return -1;
                    oldbuf2 = buf2;
                    if (close_file(fd2) == -1)
                        return -1;
                    while ((buf2 = get_confs_entry(buf2, &cse))) {
                        free_confs_entry(&cse);
                        if (cse.num == saveconf)
                            break;
                    }
                } else {
                    free(oldbuf2);
                    free(oldbuf);
                    free(confsiz);
                    return saveconf;
                }
            }
        }
    }

    /* check in conferences behind */

    buf2 = oldbuf2;
    buf2 = get_confs_entry(buf2, &cse); /* bypass mailbox */
    free_confs_entry(&cse);

    while ((buf2 = get_confs_entry(buf2, &cse))) {
        if (cse.num == Current_conf) {
            free_confs_entry(&cse);
            break;
        }
        saveconf = cse.num;
        while (1) {
            if (cse.il == NULL) {
                text = 1L;
                high = text;
            } else if (cse.il->from > 1L) {
                text = 1L;
                high = 0L;
            } else {
                text = (cse.il->to + 1L);
                if (cse.il->next)
                    high = 0L;
                else
                    high = text;
            }
            free_confs_entry(&cse);
            if (text > confsiz[cse.num]) {
                text = 0L;
                break;
            } else {
                /*
                buf = oldbuf;
                while (buf = get_conf_entry(buf, &ce))
                  if (ce.num == cse.num) break;
                		sprintf(fname, "%s/%d/%ld", SKLAFF_DB, ce.num, text); */
                sprintf(fname, "%s/%d/%ld", SKLAFF_DB, cse.num, text);
                if (file_exists(fname) == -1) {
                    if (!flag && high) {
                        flag = 1;
                        /* first = first_text(ce.num, Uid); */
                        first = first_text(cse.num, Uid);
                    }
                    if (high && (first > text)) {
                        i = strlen(oldbuf2) + 10;
                        nbuf = (char *) malloc(i);
                        if (!nbuf) {
                            return -1;
                        }
                        memset(nbuf, 0, i);
                        tmpbuf = buf2;
                        tmpbuf--;
                        while ((tmpbuf > oldbuf2) && (*tmpbuf == '\n'))
                            tmpbuf--;
                        while ((tmpbuf > oldbuf2) && (*tmpbuf != '\n'))
                            tmpbuf--;
                        if (tmpbuf > oldbuf2)
                            tmpbuf++;
                        saved = *tmpbuf;
                        *tmpbuf = 0;
                        strcpy(nbuf, oldbuf2);
                        *tmpbuf = saved;
                        sprintf(fname, "%d:1-%ld\n", cse.num, (first - 1L));
                        strcat(nbuf, fname);
                        strcat(nbuf, buf2);
                        strcpy(fname, Home);
                        strcat(fname, CONFS_FILE);
                        critical();
                        if ((fd2 = open_file(fname, 0)) == -1)
                            return -1;
                        if (write_file(fd2, nbuf) == -1)
                            return -1;
                        if (close_file(fd2) == -1)
                            return -1;
                        non_critical();
                        high = 0L;
                    }
                    /* else mark_as_read(text, ce.num); */
                    else
                        mark_as_read(text, cse.num);
                    free(oldbuf2);
                    strcpy(fname, Home);
                    strcat(fname, CONFS_FILE);
                    if ((fd2 = open_file(fname, 0)) == -1)
                        return -1;
                    if ((buf2 = read_file(fd2)) == NULL)
                        return -1;
                    oldbuf2 = buf2;
                    if (close_file(fd2) == -1)
                        return -1;
                    while ((buf2 = get_confs_entry(buf2, &cse))) {
                        free_confs_entry(&cse);
                        if (cse.num == saveconf)
                            break;
                    }
                } else {
                    free(oldbuf2);
                    free(oldbuf);
                    free(confsiz);
                    return saveconf;
                }
            }
        }
    }

    /* nothing found */

    free(oldbuf);
    free(oldbuf2);
    free(confsiz);
    return -1;
}

/*
 * last_text - returns last text in conference
 * args: conference (conf), user (uid)
 * ret: textnumber or error (-1)
 */

long
last_text(int conf, int uid)
{
    int fd;
    char *buf, *oldbuf;
    LINE mboxname;
    struct CONF_ENTRY ce;

    if (conf == 0) {

        /*
         *	Mailbox
         */

        mbox_dir(uid, mboxname);
        strcat(mboxname, MAILBOX_FILE);

        if ((fd = open_file(mboxname, 0)) == -1) {
            sys_error("next_text", 1, "open_file");
            return -1L;
        }
        if ((buf = read_file(fd)) == NULL) {
            sys_error("next_text", 2, "read_file");
            return -1L;
        }
        oldbuf = buf;

        if (close_file(fd) == -1) {
            sys_error("next_text", 3, "close_file");
            return -1L;
        }
        if ((buf = get_conf_entry(buf, &ce))) {
            free(oldbuf);
            return ce.last_text;
        }
    }
    /*
     *	Conferences
     */

    if ((fd = open_file(CONF_FILE, 0)) == -1) {
        sys_error("next_text", 1, "open_file");
        return -1L;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("next_text", 2, "read_file");
        return -1L;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        sys_error("next_text", 3, "close_file");
        return -1L;
    }
    while ((buf = get_conf_entry(buf, &ce))) {
        if (ce.num == conf) {
            free(oldbuf);
            return ce.last_text;
        }
    }
    return -1;
}

/*
 * first_text - find first text in conference
 * args: conference (conf)
 * ret: textnumber
 */

long
first_text(int conf, int uid)
{
    LONG_LINE fname;
    LINE fhome;
    long first, last, ptr;

    if (!conf)
        mbox_dir(uid, fhome);
    last = last_text(conf, uid);
    first = 1L;
    ptr = 1L;
    while (last > first) {
        ptr = (first + last) / 2L;
        if (conf) {
            snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, conf, ptr);
        } else {
            snprintf(fname, sizeof(fname), "%s/%ld", fhome, ptr);
        }
        if (file_exists(fname) == -1) {
            if (conf) {
                snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, conf, (ptr - 1L));
            } else {
                snprintf(fname, sizeof(fname), "%s/%ld", fhome, (ptr - 1L));
            }
            if (file_exists(fname) == -1) {
                if (conf) {
                    snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, conf, (ptr - 2L));
                } else {
                    snprintf(fname, sizeof(fname), "%s/%ld", fhome, (ptr - 2L));
                }
                if (file_exists(fname) == -1) {
                    if (conf) {
                        snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, conf,
                            (ptr - 3L));
                    } else {
                        snprintf(fname, sizeof(fname), "%s/%ld", fhome, (ptr - 3L));
                    }
                    if (file_exists(fname) == -1) {
                        first = (ptr + 1L);
                        ptr = first;
                    } else {
                        last = (ptr - 3L);
                        ptr = last;
                    }
                } else {
                    last = (ptr - 2L);
                    ptr = last;
                }
            } else {
                last = (ptr - 1L);
                ptr = last;
            }
        } else {
            last = ptr;
        }
    }
    return ptr;
}

/*
 * force_unsubscribe - force people to unsubscribe at logintime
 * ret: ok (0) or error (-1)
 */

int
force_unsubscribe(void)
{
    int fd, rights;
    char *buf, *oldbuf;
    LINE confsname, confname;
    struct CONFS_ENTRY ce;
    struct CONF_ENTRY *allconfs;
    struct INT_LIST *intlist;
    struct USER_LIST *ul;

    strcpy(confsname, Home);
    strcat(confsname, CONFS_FILE);

    if ((fd = open_file(confsname, 0)) == -1) {
        return -1L;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1L;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        return -1L;
    }
    allconfs = get_all_confs();

    while ((buf = get_confs_entry(buf, &ce))) {
        while (ce.il) {
            intlist = ce.il;
            ce.il = ce.il->next;
            free(intlist);
        }
        if (ce.num) {
            ul = get_confrc_struct(ce.num);
            rights = conf_right(ul, Uid,
                allconfs[ce.num].type,
                allconfs[ce.num].creator);
            free_userlist(ul);
            if (rights != 0) {
                conf_name(ce.num, confname);
                cmd_unsubscribe(confname);
            }
        }
    }
    free(oldbuf);
    free(allconfs);
    return 0;
}

/*
 * sort_conf - sort conference list
 * args: conference list (ce_list), number of conferences (count)
 * ret: pointer to CEN struct
 */

struct CEN *
sort_conf(struct CEL * ce_list, int count)
{
    int x;
    struct CEL *saved;
    struct CEN *namelist, *topname;

    if (!ce_list) {
        return NULL;
    }
    namelist = (struct CEN *) malloc((count * sizeof(struct CEN)) + 1);
    if (namelist == NULL) {
        sys_error("sort_conf", 1, "malloc");
        return NULL;
    }
    topname = namelist;
    for (x = 0; x < count; x++) {
        namelist->unreads = ce_list->unreads;
        namelist->creator = ce_list->creator;
        namelist->num = ce_list->num;
        namelist->type = ce_list->type;
        strcpy(namelist->name, ce_list->name);
        fake_string(namelist->name);
        saved = ce_list;
        ce_list = ce_list->next;
        free(saved);
        namelist++;
    }
    qsort(topname, count, sizeof(struct CEN), (int (*) ()) strcmp);
    namelist = topname;
    for (x = 0; x < count; x++) {
        real_string(namelist->name);
        namelist++;
    }
    return topname;
}


/*
 * get_all_confs - read in all confs at once into memory
 * args: none
 * ret:	allconfs (pointer to array)
 */

struct CONF_ENTRY *
get_all_confs(void)
{
    int fd, confnum;
    char *buf, *oldbuf;
    struct CONF_ENTRY ce, *allconfs;

    /* get conference list */

    if ((fd = open_file(CONF_FILE, 0)) == -1)
        return NULL;
    if ((buf = read_file(fd)) == NULL)
        return NULL;
    oldbuf = buf;
    if (close_file(fd) == -1)
        return NULL;

    buf = oldbuf;
    ce.num = 0;

    for (;;) {
        confnum = ce.num;
        buf = get_conf_entry(buf, &ce);
        if (buf == NULL)
            break;
    }
    allconfs = (struct CONF_ENTRY *) malloc(sizeof(struct CONF_ENTRY) * (confnum + 1));
    buf = oldbuf;
    for (;;) {
        buf = get_conf_entry(buf, &ce);
        if (buf == NULL)
            break;
        memcpy(&(allconfs[ce.num]), &ce, sizeof(struct CONF_ENTRY));
        /* output("%d %ld %d %d %s\n", allconfs[ce.num].num,
         * allconfs[ce.num].last_text, allconfs[ce.num].type,
         * allconfs[ce.num].creator, allconfs[ce.num].name); */
    }

    free(oldbuf);

    return allconfs;
}

/*
 * list_news - list new texts for user
 * args: user (uid)
 * ret:	ok (0) or error (-1)
 */

int
list_news(int uid)
{
    int fd, fd2, xit;
    long unreads, left;
    char *buf, *oldbuf, *buf2, *oldbuf2;
    LONG_LINE confsname, mboxname;
    struct CONFS_ENTRY cse;
    struct CONF_ENTRY ce, mbox, *allconfs;
    struct INT_LIST *int_list_sav;

    xit = 0;
    unreads = 0L;
    user_dir(uid, confsname);
    strcat(confsname, CONFS_FILE);

    /* get mailbox */

    mbox_dir(uid, mboxname);
    strcat(mboxname, MAILBOX_FILE);
    if ((fd = open_file(mboxname, 0)) == -1)
        return -1;
    if ((buf = read_file(fd)) == NULL)
        return -1;
    oldbuf = buf;
    if (close_file(fd) == -1)
        return -1;
    buf = get_conf_entry(buf, &mbox);
    free(oldbuf);

    /* get user conference list */

    if ((fd2 = open_file(confsname, 0)) == -1)
        return -1;
    if ((buf2 = read_file(fd2)) == NULL)
        return -1;
    oldbuf2 = buf2;
    if (close_file(fd2) == -1)
        return -1;

    /* pre-read all confs into memory */

    allconfs = get_all_confs();

    output("\n");
    while ((buf2 = get_confs_entry(buf2, &cse)) && !xit) {
        if (!cse.num) {
            ce.num = mbox.num;
            ce.last_text = mbox.last_text;
            ce.type = mbox.type;
            ce.creator = mbox.creator;
            strcpy(ce.name, mbox.name);
        } else {
            memcpy(&ce, &(allconfs[cse.num]), sizeof(struct CONF_ENTRY));
        }
        left = ce.last_text;
        int_list_sav = cse.il;
        while (cse.il) {
            left = left - (cse.il->to - cse.il->from + 1L);
            cse.il = cse.il->next;
        }
        cse.il = int_list_sav;
        if (left != 0L) {
            if (can_see_conf(Uid, ce.num, ce.type, ce.creator)) {
                unreads += left;
                if (output_ansi_fmt(CYAN"%7ld"DOT  BR_RED" %s\n"DOT, "%7ld   %s\n", left, ce.name) == -1) {
                    xit = 1;
                }
            }
        }
        free_confs_entry(&cse);

    }


    if (!unreads) {
        output("%s\n\n", MSG_TOTNULL);
    } else if (unreads == 1) {
        output("\n%s\n\n", MSG_TOTONE);
    } else {
        output_ansi_fmt("\n%s "CYAN"%ld"DOT" %s\n\n","\n%s %ld %s\n\n", MSG_TOTAL, unreads, MSG_UNREADTEXTS);
    }
    free(allconfs);
    free(oldbuf2);
    return 0;
}

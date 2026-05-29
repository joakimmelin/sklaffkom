/* user.c */

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

#include "sklaff.h"
#include "ext_globals.h"
#include <pwd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>	/* for safer handling of .plan */
#include <sys/time.h>   /* for utimes() */
#if defined(LINUX) || defined(__linux__)
#include <bsd/string.h>  // Required for strlcpy on Linux via libbsd
#endif
/*
 * user_name - get username from uid
 * args: uid of user (uid), username string (name)
 * ret: pointer to username string or NULL
 */

char *
user_name(int uid, char *name)
{
    int fd;
    char *buf, *oldbuf;
    struct USER_ENTRY ue;

    if (!uid)
        return NULL;
    if (uid == -2) {
        strcpy(name, MSG_SYSOP);
        return name;
    }
    if ((fd = open_file(USER_FILE, (OPEN_QUIET | OPEN_CREATE))) == -1) {
        sys_error("user_name", 1, "open_file");
        return NULL;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("user_name", 2, "read_file");
        return NULL;
    }
    oldbuf = buf;
    if ((close_file(fd)) == -1) {
        sys_error("user_name", 3, "close_file");
        return NULL;
    }
    buf = get_user_entry(buf, &ue);
    while ((ue.num != uid) && buf)
        buf = get_user_entry(buf, &ue);
    free(oldbuf);
    if (ue.num == uid) {
        strcpy(name, ue.name);
        return name;
    } else {
        return NULL;
    }
}

/*
 * last_session - returns time for last session for uid
 * args: user (uid)
 * ret: time for last session or 0
 */

time_t
last_session(int uid)
{
    int fd;
    char *buf, *oldbuf;
    struct USER_ENTRY ue;

    if ((fd = open_file(USER_FILE, (OPEN_QUIET | OPEN_CREATE))) == -1) {
        sys_error("user_name", 1, "open_file");
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("user_name", 2, "read_file");
        return -1;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        sys_error("user_name", 3, "close_file");
        return -1;
    }
    buf = get_user_entry(buf, &ue);
    while ((ue.num != uid) && buf)
        buf = get_user_entry(buf, &ue);
    free(oldbuf);
    if (ue.num == uid)
        return ue.last_session;
    else
        return 0;
}

/*
 * user_uid - returns uid from username
 * args: username (name)
 * ret: uid or -1
 */

int
user_uid(char *name)
{
    int
     uid, xit, fd, i;

    char
    *buf, *oldbuf;

    struct USER_ENTRY
     ue;

    if (name == NULL)
        return -1;

    if ((fd = open_file(USER_FILE, 0)) == -1) {
        sys_error("user_uid", 1, "open_file");
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("user_uid", 2, "read_file");
        return -1;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        sys_error("user_uid", 3, "close_file");
        return -1;
    }
    uid = -1;
    xit = 0;
    i = strlen(name);

    do {
        if ((buf = get_user_entry(buf, &ue))) {
            if ((i == strlen(ue.name)) && (strcmp(name, ue.name) == 0)) {
                uid = ue.num;
                xit = 1;
            }
        } else {
            xit = 1;
        }
    } while (!xit);
    free(oldbuf);

    return uid;
}

/*
 * user_is_active - check if user is online
 * args: user (uid)
 * ret: yes (1) or no (0)
 */

int
user_is_active(int uid)
{
    char
    *oldbuf, *buf;
    struct ACTIVE_ENTRY
     ae;
    int
     found;

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        sys_error("user_is_active", 1, "open_file");
        return 0;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        sys_error("user_is_active", 2, "read_file");
        return 0;
    }
    oldbuf = buf;

    if (close_file(ActiveFD) == -1) {
        sys_error("user_is_active", 3, "close_file");
        free(buf);
    }
    ActiveFD = -1;

    found = 0;
    while ((buf = get_active_entry(buf, &ae))) {
        if (ae.user == uid) {
            found = 1;
            break;
        }
    }
    free(oldbuf);
    return found;
}

/*
 * user_is_avail - check if user is available
 * args: user (uid)
 * ret: yes (1) or no (0)
 */

int
user_is_avail(int uid)
{
    char
    *oldbuf, *buf;
    struct ACTIVE_ENTRY
     ae;
    int
     found;

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        sys_error("user_is_active", 1, "open_file");
        return 0;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        sys_error("user_is_active", 2, "read_file");
        return 0;
    }
    oldbuf = buf;

    if (close_file(ActiveFD) == -1) {
        sys_error("user_is_active", 3, "close_file");
        free(buf);
    }
    ActiveFD = -1;

    found = 0;
    while ((buf = get_active_entry(buf, &ae))) {
        if (ae.user == uid && (ae.avail == 0)) {
            found = 1;
            break;
        }
    }
    free(oldbuf);
    return found;
}

/*
 * replace_active - replace ACTIVE_ENTRY in activefile
 * args: pointer to ACTIVE_ENTRY (ae), buffer to replace in (buf)
 * ret: pointer to buffer with replaced entry
 */

char *
replace_active(struct ACTIVE_ENTRY *ae, char *buf)
{
    char *tbuf, *nbuf, *obuf;
    char tmp[256];  /* promoted from LINE to 256 bytes to avoid overflow, modified on 2025-07-12, PL */
    int i;
    struct ACTIVE_ENTRY tae;

    obuf = buf;
    i = strlen(buf) + LINE_LEN;
    nbuf = (char *) malloc(i);  /* Jaja... */
    memset(nbuf, 0, i);

    /* Find active-entry */

    while (buf) {
        buf = get_active_entry(buf, &tae);
        if (ae->user == tae.user)
            break;
    }

    if (ae->user == tae.user) {
        tbuf = buf;
        if (tbuf > obuf)
            tbuf--;
        while ((tbuf > obuf) && (*tbuf == '\n'))
            tbuf--;
        while ((tbuf > obuf) && (*tbuf != '\n'))
            tbuf--;
        if (tbuf > obuf)
            tbuf++;
        *tbuf = '\0';           /* Set 'length' of obuf */

        sprintf(tmp, "%d:%d:%lld:%d:%s:%s:dum:dum:dum\n", ae->user, ae->pid,
            (long long) ae->login_time, ae->avail, ae->from, ae->tty);
        strcpy(nbuf, obuf);
        strcat(nbuf, tmp);
        strcat(nbuf, buf);
        free(obuf);
    }
    return nbuf;
}

/*
 * set_avail - set avail-flag in active-file
 * args: user (uid)
 * ret: yes (1) or no (0)
 */

int
set_avail(int uid, int value)
{
    char *oldbuf, *buf, *nbuf;
    struct ACTIVE_ENTRY ae;

    critical();                 /* I'd say even here! /OR 98-07-20 */

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        sys_error("user_is_active", 1, "open_file");
        return 0;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        sys_error("user_is_active", 2, "read_file");
        return 0;
    }
    oldbuf = buf;

    while ((buf = get_active_entry(buf, &ae))) {
        if (ae.user == uid) {
            break;
        }
    }

    ae.avail = value;
    nbuf = replace_active(&ae, oldbuf);
    /* critical();  Shouldn't this be here? /OR 98-04-11 */
    if (write_file(ActiveFD, nbuf) == -1) {
        sys_error("user_is_active", 2, "write_file");
        return -1;
    }
    if (close_file(ActiveFD) == -1) {
        sys_error("user_is_active", 3, "close_file");
    }
    ActiveFD = -1;
    non_critical();             /* Shouldn't this be here? /OR 98-04-11 */
    return 0;
}

/*
 * set_from - set the 'from' string
 * args: user (uid)
 * ret: yes (1) or no (0)
 */

int
set_from(int uid, char *value)
{
    char *oldbuf, *buf, *nbuf;
    struct ACTIVE_ENTRY ae;

    critical();                 /* I'd think it's even here! /OR 98-07-20 */

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        sys_error("user_is_active", 1, "open_file");
        return 0;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        sys_error("user_is_active", 2, "read_file");
        return 0;
    }
    oldbuf = buf;

    while ((buf = get_active_entry(buf, &ae))) {
        if (ae.user == uid) {
            break;
        }
    }

    strlcpy(ae.from, value, FROM_FIELD_LEN);  /* modified on 2025-07-12, PL */
    nbuf = replace_active(&ae, oldbuf);
    /* critical();  Shouldn't this be here? /OR 98-04-11 */
    if (write_file(ActiveFD, nbuf) == -1) {
        sys_error("user_is_active", 2, "write_file");
        return -1;
    }
    if (close_file(ActiveFD) == -1) {
        sys_error("user_is_active", 3, "close_file");
    }
    ActiveFD = -1;
    non_critical();             /* Shouldn't this be here? /OR 98-04-11 */
    return 0;
}

/*
 * get_user_struct - get user struct from userfile
 * args: user (uid)
 * ret: pointer to static USER_ENTRY or NULL
 */

struct USER_ENTRY *
get_user_struct(int uid)
{
    int fd;
    char *buf, *oldbuf;
    static struct USER_ENTRY ue;

    if ((fd = open_file(USER_FILE, 0)) == -1) {
        sys_error("user_name", 1, "open_file");
        return NULL;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("user_name", 2, "read_file");
        return NULL;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        sys_error("user_name", 3, "close_file");
        return NULL;
    }
    buf = get_user_entry(buf, &ue);
    while ((ue.num != uid) && buf)
        buf = get_user_entry(buf, &ue);
    free(oldbuf);
    if (ue.num == uid)
        return &ue;
    else
        return NULL;
}

/*
 * setup_new_user - setup everything for a first time user
 * ret: ok (0) or failure (-1)
 */

int
setup_new_user()
{
    LINE fname;
    struct USER_ENTRY new_user;
    struct passwd *pw;
    int uf;
    char *buf, *nbuf, *new_user_string;
    LONG_LINE tmpbuf;

    if (!Uid) {
        output("%s\n\n", MSG_ROOTERR);
        return -1;
    }
    if (mkdir(Home, NEW_DIR_MODE) == -1) {      /* Should fail if Home already
                                                 * exists */
        output("%s\n\n", MSG_USER_ALREADY_EXIST);
        return -1;
    }
    if (mkdir(Mbox, NEW_DIR_MODE) == -1) {
        output("%s\n\n", MSG_USER_ALREADY_EXIST);
        return -1;
    }
    strcpy(fname, Home);
    strcat(fname, CONFS_FILE);
    if (copy_file(STD_CONFS, fname) == -1) {
        sys_error("setup_new_user", 5, "copy_file");
        return -1;
    }
    strcpy(fname, Home);
    strcat(fname, SKLAFFRC_FILE);
    if (copy_file(STD_SKLAFFRC, fname) == -1) {
        sys_error("setup_new_user", 6, "copy_file");
        return -1;
    }
    strcpy(fname, Mbox);
    strcat(fname, MAILBOX_FILE);
    if (copy_file(STD_MAILBOX, fname) == -1) {
        sys_error("setup_new_user", 7, "copy_file");
        return -1;
    }
    pw = getpwuid(Uid);
    new_user.num = Uid;
    new_user.last_session = 0L;
    strcpy(new_user.name, pw->pw_gecos);
    buf = new_user.name;
    while ((*buf != ',') && (*buf != 0))
        buf++;
    if (*buf == ',')
        *buf = 0;
    new_user_string = stringify_user_struct(&new_user, tmpbuf);
    if ((uf = open_file(USER_FILE,
                (OPEN_CREATE | OPEN_QUIET))) == -1) {
        return -1;
    }
    if ((buf = read_file(uf)) == NULL) {
        return -1;
    }
    nbuf = (char *) malloc(strlen(buf) + strlen(new_user_string) + 1);
    memset(nbuf, 0, strlen(buf) + strlen(new_user_string) + 1);
    strcpy(nbuf, buf);
    free(buf);
    strcat(nbuf, new_user_string);
    critical();
    if (write_file(uf, nbuf) == -1) {
        return -1;
    }
    if (close_file(uf) == -1) {
        return -1;
    }
    non_critical();
    return 0;
}

/*
 * stringify_user_struct - turn user_struct into string
 * args: pointer to USER_ENTRY
 * ret: pointer to static string
 */

char *
stringify_user_struct(struct USER_ENTRY *user_entry, char *buf)
{
    sprintf(buf, "%d:%lld:%s\n", user_entry->num,
        (long long) user_entry->last_session, user_entry->name);
    return buf;
}

/*
 * add_active - add user to active file
 * ret: ok (0) or failure (-1)
 */

int
add_active()
{
    int myuid;

    struct ACTIVE_ENTRY ae, *aep;
    char *buf, *nbuf, *oldbuf;
    char hname[FROM_FIELD_LEN];
    LINE name, tmp, tmpdir;

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        sys_error("add_active", 1, "open_file");
        return -1;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        sys_error("add_active", 2, "read_file");
        return -1;
    }
    oldbuf = buf;

    if (close_file(ActiveFD) == -1) {
        sys_error("add_active", 4, "close_file");
        return -1;
    }
    ActiveFD = -1;

    myuid = Uid;

    while ((buf = get_active_entry(buf, &ae))) {
        Uid = ae.user;
        if (kill(ae.pid, 0) == -1) {
            sprintf(tmpdir, "/tmp/%d/%d.qwk", ae.pid, ae.pid);
            if (file_exists(tmpdir) != -1) {
                unlink(tmpdir);
            }
            sprintf(tmpdir, "/tmp/%d", ae.pid);
            rmdir(tmpdir);
            remove_active();
        }
    }

    Uid = myuid;

    free(oldbuf);

    aep = check_active(Uid, &ae);
    if (aep) {
        debuglog("user already logged in", 7);
        output("\n%s %s\n\n", user_name(Uid, name), MSG_ALREADYON);
        output(MSG_QUITASK);
        input("", tmp, 4, 0, 0, 0);
        down_string(tmp);
        if (*tmp == MSG_YESANSWER) {
            debuglog("user attempts to kill other process", 5);
            output(MSG_QUITTRY);
            fflush(stdout);
            kill(ae.pid, SIGTERM);
            sleep(2L);
            if (user_is_active(Uid)) {
                debuglog("kill attempt failed, using SIGKILL", 5);
                output("%s\n\n", MSG_QUITFAIL);
                output(MSG_KILLTRY);
                fflush(stdout);
                kill(ae.pid, SIGKILL);
                sleep(2L);
                remove_active();
                output("%s\n\n", MSG_KILLOK);
            } else {
                output("%s\n\n", MSG_QUITOK);
            }
        } else {
            output("\n%s\n\n", MSG_QUITEXIT);
            sig_reset();
            tty_reset();
            exit(1);
        }
    }
    critical();                 /* Should probably be here to safeguard from
                                 * deadlock on active file in case of a quit
                                 * SIGNAL /OR 98-07-20 */

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        sys_error("add_active", 1, "open_file");
        return -1;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        sys_error("add_active", 2, "read_file");
        return -1;
    }
    hname[0] = '*';
    strncpy(hname + 1, get_hostname(), FROM_FIELD_LEN - 1);
    hname[FROM_FIELD_LEN - 1] = 0;
    sprintf(tmp, "%d:%d:%lld:0:%s:%s:dum:dum:dum\n", Uid, getpid(),
        (long long) time(0), hname, ttyname(0));

    nbuf = (char *) malloc(strlen(buf) + strlen(tmp) + 1);
    memset(nbuf, 0, strlen(buf) + strlen(tmp) + 1);
    strcpy(nbuf, buf);
    strcat(nbuf, tmp);
    free(buf);

    /* critical(); */
    if (write_file(ActiveFD, nbuf) == -1) {
        sys_error("add_active", 4, "write_file");
        return -1;
    }
    if (close_file(ActiveFD) == -1) {
        sys_error("add_active", 5, "close_file");
        return -1;
    }
    ActiveFD = -1;
    non_critical();
    return 0;
}

/*
 * remove_active - remove user from active file
 * ret: ok (0) or failure (-1)
 */

int
remove_active()
{
    char *buf, *nbuf, *tmpbuf, *oldbuf;
    int found;
    struct ACTIVE_ENTRY ae;

    critical();                 /* See comment above in add_active() /OR
                                 * 98-07-20 */

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        sys_error("remove_active", 1, "open_file");
        return -1;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        sys_error("remove_active", 2, "read_file");
        return -1;
    }
    oldbuf = buf;

    nbuf = (char *) malloc(strlen(buf) + 1);
    memset(nbuf, 0, strlen(buf) + 1);

    found = 0;
    while (buf != NULL) {
        buf = get_active_entry(buf, &ae);
        if (ae.user == Uid) {
            found = 1;
            break;
        }
    }

    if (found) {
        tmpbuf = buf;

        tmpbuf--;
        while ((tmpbuf > oldbuf) && (*tmpbuf == '\n'))
            tmpbuf--;

        while ((tmpbuf > oldbuf) && (*tmpbuf != '\n'))
            tmpbuf--;

        if (tmpbuf > oldbuf)
            tmpbuf++;
        *tmpbuf = '\0';
        strcpy(nbuf, oldbuf);
        strcat(nbuf, buf);
        /* critical(); */
        if (write_file(ActiveFD, nbuf) == -1) {
            sys_error("remove_active", 3, "write_file");
            return -1;
        }
    }
    free(oldbuf);
    if (close_file(ActiveFD) == -1) {
        sys_error("remove_active", 4, "close_file");
        return -1;
    }
    ActiveFD = -1;
    non_critical();
    return 0;
}

/*
 * check_active - check if user is active
 * args: user (uid), pointer to ACTIVE_ENTRY (ae)
 * ret: pointer to ACTIVE_ENTRY or NULL
 */

struct ACTIVE_ENTRY *
check_active(int uid, struct ACTIVE_ENTRY *ae)
{
    int found;
    char *buf, *oldbuf;

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        sys_error("check_time", 1, "open_file");
        return NULL;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        sys_error("check_active", 2, "read_file");
        return NULL;
    }
    oldbuf = buf;

    if (close_file(ActiveFD) == -1) {
        sys_error("check_active", 4, "close_file");
        return NULL;
    }
    ActiveFD = -1;

    found = 0;
    while ((buf = get_active_entry(buf, ae))) {
        if (ae->user == uid) {
            found = 1;
            break;
        }
    }

    free(oldbuf);

    if (found)
        return ae;
    else
        return NULL;
}

/*
 * active_time - returns number of minutes user has been online
 * args: user (uid)
 * ret: minutes
 */

long
active_time(int uid)
{
    long tim;

    char
    *buf, *oldbuf;

    struct ACTIVE_ENTRY
     ae;

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        sys_error("active_time", 1, "open_file");
        return -1L;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        sys_error("active_time", 2, "read_file");
        return -1L;
    }
    oldbuf = buf;

    if (close_file(ActiveFD) == -1) {
        sys_error("active_time", 3, "close_file");
        return -1L;
    }
    ActiveFD = -1;

    tim = -1L;

    while ((buf = get_active_entry(buf, &ae)) && (tim == -1)) {
        if (ae.user == uid) {
            tim = (time(0) - ae.login_time) / 60L;
        }
    }
    free(oldbuf);
    return tim;
}


/*
 * idle_time - returns number of minutes a user has been idle
 * args: uid
 * ret: minutes
 */

long
idle_time(int uid)
{
    LINE actfile;
    struct stat sb;

    user_dir(uid, actfile);
    strcat(actfile, ACT_FILE);


    if (stat(actfile, &sb) == 0)
        return (time(0) - sb.st_atime) / 60L;
    else
        return 0;
}

/*
 * make_activity_note - Touches the activity file of the user,
 * which is used by the idle_time function to determine idle time.
 */

void
make_activity_note()
{
    LINE actfile;
    int fd;
    static long lastmod = 0;
    long now;

    now = time(0);

    if (now - lastmod < IDLE_RESOLUTION)
        return;

    lastmod = now;

    strcpy(actfile, Home);
    strcat(actfile, ACT_FILE);

    if (utimes(actfile, NULL) == -1) {
        fd = open_file(actfile, OPEN_QUIET | OPEN_CREATE);
        close_file(fd);
    }
}


/*
 * disp_note - displays note for user
 * args: user (user)
 * ret: always 0
 */

int
disp_note(int user)
{
    struct SKLAFFRC *rc;

    rc = read_sklaffrc(user);
    if (rc != NULL) {
        if (strlen(rc->note) && strcmp(rc->note, "note")) {
            output("\n%s\n\n", rc->note);
        } else
            output("\n");
        free(rc);
    }
    return 0;
}

/*
 * list_user - list user by group/all groups or by last_session
 * args: type of list (type) 1=standard, 2=last_session,
 *       group (grp) -1=all groups
 * ret: ok (0) or failure (-1)
 */

int
list_user(int type, int grp, int pdate)
{
    int fd, count, x, ok, dolink;
    char *buf, *oldbuf;
    struct passwd *pwent;
    struct USER_ENTRY uetmp;
    struct UEL *ue_list, *top, *saved, *cp;
    struct UEN *namelist, *topname;
    struct UET *timelist, *toptime;
    struct SKLAFFRC *rc;
    LINE tstr;
    long realtime;

    if ((fd = open_file(USER_FILE, 0)) == -1) {
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        return -1;
    }
    output("\n");
    count = 0;
    ue_list = NULL;
    top = NULL;
    while (buf) {
        buf = get_user_entry(buf, &uetmp);
        ok = 0;
        dolink = 1;
        if (buf) {
            if (!grp) {
                ok = 1;
            } else if (grp == -1) {
                pwent = getpwuid(uetmp.num);
                if ((pwent->pw_gid != INET_GROUP) &&
                    (pwent->pw_gid != MODEM_GROUP)) {
                    ok = 1;
                }
            } else {
                pwent = getpwuid(uetmp.num);
                if (pwent->pw_gid == grp) {
                    ok = 1;
                }
            }
        }
        if (buf && ok) {
            if (ue_list) {
                ue_list->next = (struct UEL *) malloc
                    (sizeof(struct UEL) + 1);
                if (ue_list->next == NULL) {
                    sys_error("list_user", 1, "malloc");
                    return -1;
                }
                cp = ue_list;
                ue_list = ue_list->next;
                strcpy(ue_list->ue.name, uetmp.name);
                pwent = getpwuid(uetmp.num);
                if ((pwent->pw_gid == MODEM_GROUP) && (type == 1)) {
                    rc = read_sklaffrc(uetmp.num);
                    if (strstr(rc->paid, "no")) {
                        if (pdate)
                            dolink = 0;
                        else
                            strcat(ue_list->ue.name, MSG_NOPAR);
                    } else {
                        if (pdate && (atoi(rc->paydate) > pdate))
                            dolink = 0;
                        else {
                            strcat(ue_list->ue.name, " (");
                            strcat(ue_list->ue.name, rc->paydate);
                            strcat(ue_list->ue.name, ")");
                        }
                    }
                    free(rc);
                }
                if (dolink) {
                    ue_list->ue.num = uetmp.num;
                    ue_list->ue.last_session = uetmp.last_session;
                    ue_list->next = NULL;
                } else {
                    free(ue_list);
                    ue_list = cp;
                }
            } else {
                ue_list = (struct UEL *) malloc
                    (sizeof(struct UEL) + 1);
                if (ue_list == NULL) {
                    sys_error("list_user", 1, "malloc");
                    return -1;
                }
                top = ue_list;
                strcpy(ue_list->ue.name, uetmp.name);
                pwent = getpwuid(uetmp.num);
                if ((pwent->pw_gid == MODEM_GROUP) && (type == 1)) {
                    rc = read_sklaffrc(uetmp.num);
                    if (strstr(rc->paid, "no")) {
                        if (pdate)
                            dolink = 0;
                        else
                            strcat(ue_list->ue.name, MSG_NOPAR);
                    } else {
                        if (pdate && (atoi(rc->paydate) > pdate))
                            dolink = 0;
                        else {
                            strcat(ue_list->ue.name, " (");
                            strcat(ue_list->ue.name, rc->paydate);
                            strcat(ue_list->ue.name, ")");
                        }
                    }
                    free(rc);
                }
                if (dolink) {
                    ue_list->ue.num = uetmp.num;
                    ue_list->ue.last_session = uetmp.last_session;
                    ue_list->next = NULL;
                } else {
                    free(ue_list);
                    ue_list = NULL;
                    top = NULL;
                }
            }
            if (dolink)
                count++;
        }
    }
    free(oldbuf);
    if ((type == 1) && top) {
        namelist = sort_user(top, count);
        topname = namelist;
        for (x = 0; x < count; x++) {
            if (output("%s\n", namelist->name) == -1) {
                break;
            }
            namelist++;
        }
        output("\n");
        if (count == x && !grp) {
            output("%s %d %s\n\n", MSG_TOTAL, count, MSG_USERS);
        }
        free(topname);
    } else {
        timelist = (struct UET *) malloc((count * sizeof(struct UET)) + 1);
        if (timelist == NULL) {
            sys_error("list_user", 3, "malloc");
            return -1;
        }
        toptime = timelist;
        ue_list = top;
        for (x = 0; x < count; x++) {
            sprintf(timelist->last_session, "%lld",
                (long long) ue_list->ue.last_session);
            strcpy(timelist->name, ue_list->ue.name);
            saved = ue_list;
            ue_list = ue_list->next;
            free(saved);
            timelist++;
        }
        qsort(toptime, count, sizeof(struct UET), (int (*) ()) strcmp);
        timelist = toptime;
        for (x = 1; x < count; x++)
            timelist++;
        for (x = 0; x < count; x++) {
            realtime = atol(timelist->last_session);
            if (realtime) {
                time_string(realtime, tstr, 0);
                if (output("%17s  %s\n", tstr,
                        timelist->name) == -1) {
                    break;
                }
            }
            timelist--;
        }
        output("\n");
        free(toptime);
    }
    return 0;
}

/*
 * sort_user - sorts userlist
 * args: pointer to UEL (ue_list), number of users (count)
 * ret: pointer to UEN
 */

struct UEN *
sort_user(struct UEL *ue_list, int count)
{
    int x;
    struct UEL *saved;
    struct UEN *namelist, *topname;
    LINE name;

    if (!ue_list) {
        return NULL;
    }
    namelist = (struct UEN *) malloc((count * sizeof(struct UEN)) + 1);
    if (namelist == NULL) {
        sys_error("sort_user", 1, "malloc");
        return NULL;
    }
    topname = namelist;
    for (x = 0; x < count; x++) {
        reorder_name(ue_list->ue.name, name);
        fake_string(name);
        strcpy(namelist->name, name);
        saved = ue_list;
        ue_list = ue_list->next;
        free(saved);
        namelist++;
    }
    qsort(topname, count, sizeof(struct UEN), (int (*) ()) strcmp);
    namelist = topname;
    for (x = 0; x < count; x++) {
        order_name(namelist->name, name);
        real_string(name);
        strcpy(namelist->name, name);
        namelist++;
    }
    return topname;
}

/*
 * read_sklaffrc - reads sklaffrc for user
 * args: user (uid)
 * ret: pointer to SKLAFFRC struct (malloced)
 */

struct SKLAFFRC *
read_sklaffrc(int uid)
{

    int fd;
    int fusker;
    char entry[4096];
    char user_home[255];
    char *func_name = "read_sklaffrc";
    struct SKLAFFRC *kaffer;

    struct {
        char *heading;
    } headptr[100];             /* Limits maximum of headings */

    char *startptr;
    char *ptr;
    char *buf = NULL, *oldbuf = NULL;
    int state = 0, len = 0, i = 0, lastchar = 0, lasttwo = 0;   /* -Wmaybe-uninitialized warnings silenced */

    if ((kaffer = (struct SKLAFFRC *) malloc(sizeof(struct SKLAFFRC)))
        == (struct SKLAFFRC *) 0) {
        sys_error(func_name, 1, "malloc");
        return (struct SKLAFFRC *) 0;
    }
    headptr[1].heading = "adress";
    headptr[2].heading = "postnr";
    headptr[3].heading = "ort";
    headptr[4].heading = "tele1";
    headptr[5].heading = "tele2";
    headptr[6].heading = "tele3";
    headptr[7].heading = "org";
    headptr[8].heading = "note";
    headptr[9].heading = "editor";
    headptr[10].heading = "email1";
    headptr[11].heading = "email2";
    headptr[12].heading = "flags";
    headptr[13].heading = "paid";
    headptr[14].heading = "login";
    headptr[15].heading = "timeout";
    headptr[16].heading = "paydate";
    headptr[17].heading = "sig";
    headptr[18].heading = "url";
    headptr[19].heading = "Numlines"; /* 2025-08-10, PL: we now set terminal height in sklaffrc on a per-user basis */
    headptr[20].heading = "";

    memset(kaffer->user.adress, 0, 80);
    memset(kaffer->user.postnr, 0, 80);
    memset(kaffer->user.ort, 0, 80);
    memset(kaffer->user.tele1, 0, 80);
    memset(kaffer->user.tele2, 0, 80);
    memset(kaffer->user.tele3, 0, 80);
    memset(kaffer->user.email1, 0, 80);
    memset(kaffer->user.email2, 0, 80);
    memset(kaffer->user.url, 0, 80);
    memset(kaffer->user.org, 0, 80);
    memset(kaffer->note, 0, 4096);
    memset(kaffer->sig, 0, 4096);
    memset(kaffer->editor, 0, 80);
    memset(kaffer->flags, 0, 400);
    memset(kaffer->paid, 0, 80);
    memset(kaffer->login, 0, 4096);
    memset(kaffer->timeout, 0, 80);
    memset(kaffer->paydate, 0, 80);

    /* Loop two times, one time for global sklaffrc, one for users local */
    for (fusker = 0; fusker != 2; fusker++) {
        if (fusker == 0) {
            /** Read default sklaffrc **/
            if ((fd = open_file(GLOBAL_SKLAFFRC, OPEN_DEFAULT)) == -1) {
                sys_error(func_name, 2, "open_file (GLOBAL_SKLAFFRC)");
            }
        } else {
            (void) user_dir(uid, user_home);
            strcat(user_home, SKLAFFRC_FILE);
            if ((fd = open_file(user_home, OPEN_DEFAULT)) == -1) {
                sys_error(func_name, 3, "open_file (LOCAL_SKLAFFRC)");
            }
        }

        /* Check if we got a file */
        if (fd != -1) {
            if ((buf = read_file(fd)) == (char *) NULL) {
                sys_error(func_name, 3, "read_file");
            } else {
                close_file(fd);
                oldbuf = buf;
                ptr = buf;
                startptr = buf;
                state = 0;
                memset(entry, 0, 4096);
                lastchar = 0;
                lasttwo = 0;

                for (;;) {
                    if (state == 5) {
                        state = 6;
                        startptr = ptr;
                    }
                    /* Skip 'til newline */
                    if ((state == 4) && ((*ptr == 0) || (*ptr == '\n')))
                        state = 5;

                    if (state == 3)
                        state = 4;

                    /* End of heading, start looking for newline */
                    if ((*ptr == ']') && (state == 2))
                        state = 3;

                    /* state1 -- found [, start reading heading */
                    if (state == 1) {
                        startptr = ptr;
                        state = 2;
                    }
                    /* State3 -- found ], check for matching heading */
                    if (state == 3) {
                        len = ptr - startptr;
                        strncpy(entry, startptr, len);
                        for (i = 1; (strcmp(headptr[i].heading, entry) != 0) &&
                            strlen(headptr[i].heading) > 0; i++);

                        if (strlen(headptr[i].heading) <= 0) {
                            fprintf(stdout, "%%DEBUG: Heading not found %s \n\r"
                                ,entry);
                            state = 0;
                        } else {
                            state = 4;
                        }
                        fflush(stdout);
                        memset(entry, 0, 4096);
                    }
                    if (((*ptr == '[') && ((lastchar == '!') &&
                                ((lasttwo == '\n') || (lasttwo == 0))))
                        || (*ptr == 0)) {
                        if (state == 6) {
                            if (*ptr) {
                                len = ptr - startptr - 2;
                            } else {
                                len = ptr - startptr - 1;
                            }
                            if (len < 0)
                                len = 0;
                            strncpy(entry, startptr, len);
                            /* Ugly */
                            if (strcmp(headptr[i].heading, "adress") == 0)
                                strcpy(kaffer->user.adress, entry);
                            if (strcmp(headptr[i].heading, "postnr") == 0)
                                strcpy(kaffer->user.postnr, entry);
                            if (strcmp(headptr[i].heading, "ort") == 0)
                                strcpy(kaffer->user.ort, entry);
                            if (strcmp(headptr[i].heading, "tele1") == 0)
                                strcpy(kaffer->user.tele1, entry);
                            if (strcmp(headptr[i].heading, "tele2") == 0)
                                strcpy(kaffer->user.tele2, entry);
                            if (strcmp(headptr[i].heading, "tele3") == 0)
                                strcpy(kaffer->user.tele3, entry);
                            if (strcmp(headptr[i].heading, "org") == 0)
                                strcpy(kaffer->user.org, entry);
                            if (strcmp(headptr[i].heading, "note") == 0)
                                strcpy(kaffer->note, entry);
                            if (strcmp(headptr[i].heading, "sig") == 0)
                                strcpy(kaffer->sig, entry);
                            if (strcmp(headptr[i].heading, "editor") == 0)
                                strcpy(kaffer->editor, entry);
                            if (strcmp(headptr[i].heading, "flags") == 0)
                                strcpy(kaffer->flags, entry);
                            if (strcmp(headptr[i].heading, "timeout") == 0)
                                strcpy(kaffer->timeout, entry);
                            if (strcmp(headptr[i].heading, "paid") == 0)
                                strcpy(kaffer->paid, entry);
                            if (strcmp(headptr[i].heading, "paydate") == 0)
                                strcpy(kaffer->paydate, entry);
                            if (strcmp(headptr[i].heading, "login") == 0)
                                strcpy(kaffer->login, entry);
                            if (strcmp(headptr[i].heading, "email1") == 0)
                                strcpy(kaffer->user.email1, entry);
                            if (strcmp(headptr[i].heading, "email2") == 0)
                                strcpy(kaffer->user.email2, entry);
                            if (strcmp(headptr[i].heading, "url") == 0)
                                strcpy(kaffer->user.url, entry);
			    if (strcmp(headptr[i].heading, "Numlines") == 0) {
    				long v = strtol(entry, NULL, 10);
			    if (v >= 10 && v <= 200)            /* sane range limit */
			        Numlines = (int)v;
				}
                            memset(entry, 0, 4096);
                        }
                        state = 1;
                    }           /* new heading */
                    if (*ptr == 0)
                        break;
                    lasttwo = lastchar;
                    lastchar = *ptr;
                    ptr++;
                }
            }
        }
        if (oldbuf) free(oldbuf);
    }
    return kaffer;

}

/*
 * write_sklaffrc
 * args: user (uid), pointer to SKLAFFRC struct (kaffer)
 * ret: ok (0) or failure (-1)
 */

int
write_sklaffrc(int uid, struct SKLAFFRC *kaffer)
{
    char user_home[255];
    char file1[255];
    char *outbuf, *out2;
    char *function_name = "write_sklaffrc";    
    int fd;  /* fd2 moved to bottom - not used on linux PL 2025-08-10 */

    outbuf = (char *) malloc(24000);    /* write_buf frees the memory,yes? */
    out2 = (char *) malloc(24000);      /* write_buf frees the memory,yes? */

    (void) user_dir(uid, user_home);
    memcpy(file1, user_home, strlen(user_home) + 1);
    strcat(file1, SKLAFFRC_FILE);

    if ((fd = create_file(file1)) <= 0) {
        sys_error(function_name, 1, "create_file");
        free(kaffer);
        return -1;
    }
    out2[0] = 0;

#ifdef DEBUG
    printf("rename %s = %s \r\n", file1, file2);
    printf("create_file %s \r\n", file1);
#endif

    outbuf[0] = 0;

    if (strlen(kaffer->user.adress) > 0) {
        strcat(outbuf, "![adress]\n");
        strcat(outbuf, kaffer->user.adress);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->user.postnr) > 0) {
        strcat(outbuf, "![postnr]\n");
        strcat(outbuf, kaffer->user.postnr);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->user.ort) > 0) {
        strcat(outbuf, "![ort]\n");
        strcat(outbuf, kaffer->user.ort);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->user.tele1) > 0) {
        strcat(outbuf, "![tele1]\n");
        strcat(outbuf, kaffer->user.tele1);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->user.tele2) > 0) {
        strcat(outbuf, "![tele2]\n");
        strcat(outbuf, kaffer->user.tele2);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->user.tele3) > 0) {
        strcat(outbuf, "![tele3]\n");
        strcat(outbuf, kaffer->user.tele3);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->user.email1) > 0) {
        strcat(outbuf, "![email1]\n");
        strcat(outbuf, kaffer->user.email1);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->user.email2) > 0) {
        strcat(outbuf, "![email2]\n");
        strcat(outbuf, kaffer->user.email2);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->user.url) > 0) {
        strcat(outbuf, "![url]\n");
        strcat(outbuf, kaffer->user.url);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->user.org) > 0) {
        strcat(outbuf, "![org]\n");
        strcat(outbuf, kaffer->user.org);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->note) > 0) {
        strcat(outbuf, "![note]\n");
        strcat(outbuf, kaffer->note);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->sig) > 0) {
        strcat(outbuf, "![sig]\n");
        strcat(outbuf, kaffer->sig);
        strcat(outbuf, "\n");
        strcat(out2, kaffer->sig);
        strcat(out2, "\n");
    }
    if (strlen(kaffer->editor) > 0) {
        strcat(outbuf, "![editor]\n");
        strcat(outbuf, kaffer->editor);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->flags) > 0) {
        strcat(outbuf, "![flags]\n");
        strcat(outbuf, kaffer->flags);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->timeout) > 0) {
        strcat(outbuf, "![timeout]\n");
        strcat(outbuf, kaffer->timeout);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->paid) > 0) {
        strcat(outbuf, "![paid]\n");
        strcat(outbuf, kaffer->paid);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->login) > 0) {
        strcat(outbuf, "![login]\n");
        strcat(outbuf, kaffer->login);
        strcat(outbuf, "\n");
    }
    if (strlen(kaffer->paydate) > 0) {
        strcat(outbuf, "![paydate]\n");
        strcat(outbuf, kaffer->paydate);
        strcat(outbuf, "\n");
    }
#ifdef DEBUG
    output("outbuf == %s\n", outbuf);
#endif

    critical();
    if (write_file(fd, outbuf) == -1) {
        sys_error("function_name", 2, "write_file");
        free(kaffer);
        return -1;
    }
    close_file(fd);

    non_critical();


#ifndef LINUX
/* Mirror sig to ~/.plan (at our best efforts)
 * We only update if the file already exists and is writable.
 * Failures are logged and non-fatal. 2025-08-10 PL
 */
{
   /* struct passwd *p = getpwuid(Uid); */
    struct passwd *p = getpwuid(uid); /* Minor typo since ages ago 2025-05-18 PL */
    if (!p || !p->pw_dir || !*p->pw_dir) {
        debuglog("Skipping .plan: getpwuid() failed or empty home", 20);
    } else {
        char plan[PATH_MAX];
        snprintf(plan, sizeof(plan), "%s/.plan", p->pw_dir);

        /* Only proceed if file exists and is writable by sklaffuser */
        if (access(plan, F_OK) != 0) {
            debuglog("Skipping .plan: file does not exist", 20);
        } else if (access(plan, W_OK) != 0) {
            debuglog("Skipping .plan: file not writable", 20);
        } else {
            int fd2 = create_file(plan);
            if (fd2 < 0) {
                sys_error(function_name, 1, "create_file(.plan)");
            } else {
                critical();
                if (write_file(fd2, out2) == -1) {
                    sys_error(function_name, 2, "write_file(.plan)");
                }
                close_file(fd2);
                non_critical();
            }
        }
    }
}
#endif
return 0;
}

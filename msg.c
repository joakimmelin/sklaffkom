/* msg.c */

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

#include <signal.h>

#include "sklaff.h"
#include "ext_globals.h"

void
add_to_mlist(struct MSG_ENTRY * me)
{
    struct MSG_LIST *new_entry;

    new_entry = (struct MSG_LIST *) malloc(sizeof(struct MSG_LIST));
    if (new_entry == NULL) {
        sys_error("add_to_mlist", 1, "malloc");
        return;
    }
    memcpy(&new_entry->me, me, sizeof(struct MSG_ENTRY));
    new_entry->prev = mlist;
    mlist = new_entry;
}

void
list_mlist(int max, int type_filter)
{
    int count, i, r = 0; /* we must declare "l" later to silence warning when compiling for ENGLISH language 2025-08-11 PL */
    struct MSG_LIST *current;
    struct MSG_ENTRY **me;
    LINE name;

    max = max < 0 ? 0 : max > 40000 ? 40000 : max;

    output("\n");
    me = (struct MSG_ENTRY **) malloc((max ? max : 1) * sizeof(struct MSG_ENTRY *));
    if (me == NULL) {
        sys_error("list_mlist", 1, "malloc");
        return;
    }
    for (i = 0; i < max; i++)
        me[i] = NULL;
    for (count = 0, current = mlist; current && count < max;) {
        if ((1 << current->me.type) & type_filter) {
            ++count;
            me[max - count] = &current->me;
        }
        current = current->prev;
    }
    for (i = 0; i < max; i++) {
        if (!me[i])
            continue;
        user_name(me[i]->num, name);
        switch (me[i]->type) {
        case MSG_SAY:
            if (me[i]->direct == 0)
                r = output("%s: %s\n", name, me[i]->msg);       /* Sender name */
            else
                r = output("->%s: %s\n", name, me[i]->msg);     /* Receiver name */
            break;
        case MSG_YELL:
            r = output("%s %s %s\n", name, MSG_YELLS,
                me[i]->msg);
            break;
        case MSG_I:
            r = output("%s %s\n", name, me[i]->msg);
            break;
        case MSG_MY: {            
#ifdef SWEDISH
            size_t l = strlen(name);              /* declared now only for Swedish */
            if (name[l - 1] == 's')
                r = output("%s %s\n", name, me[i]->msg);
            else
                r = output("%ss %s\n", name, me[i]->msg);
#else
            r = output("%s's %s\n", name, me[i]->msg);
#endif
            break;
        }
        case MSG_SMS:
            r = output("%s\n", me[i]->msg);
            break;
        }
        if (r == -1)
            break;
    }
    free(me);
    output("\n");
}

/*
 * newmsg - set Change_msg flag at signal received
 * args: type of interrupt (tmp)
 */

void
newmsg(int tmp)
{
    sigset_t sigmask, oldsigmask;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    signal(SIGNAL_NEW_MSG, newmsg);
    Change_msg = 1;
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
}

/*
 * display_msg - display messages from other users
 * args: length of prompt (num)
 * ret: ok (0) or error (-1)
 */

int
display_msg(int num)
{
    int nl, r, fd; /* We declare l later 2025-08-11 PL */
    char *oldbuf, *buf;
    sigset_t sigmask, oldsigmask;
    LINE name, msgfile;
    struct MSG_ENTRY me;

    if (!Change_msg)
        return 0;

    sprintf(name, "enters display_msg()");
    debuglog(name, 30);

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);

    strcpy(msgfile, Home);
    strcat(msgfile, MSG_FILE);

    if ((fd = open_file(msgfile, 0)) == -1) {
        sys_error("display_msg", 1, "open_file");
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("display_msg", 2, "read_file");
        return -1;
    }
    oldbuf = buf;

    /* Truncate file */

    critical();

    if (ftruncate(fd, (off_t) 0L) == -1) {
        sys_error("display_,msg", 3, "ftruncate");
        return -1;
    }
    if (close_file(fd) == -1) {
        sys_error("display_msg", 4, "close_file");
        return -1;
    }
    non_critical();

    nl = 0;
    while ((buf = get_msg_entry(buf, &me))) {

        add_to_mlist(&me);
        if ((me.type == MSG_SAY) && Say) {
            clear_prompt_cols(num); /* modified on 2025-08-30, PL */
			nl = 1;
            if (output("\007%s: %s\n",
                    user_name(me.num, name), me.msg) == -1) {
                break;
            }
        } else if ((me.type == MSG_YELL) && Shout) {
            if (!nl)
                clear_prompt_cols(num);    /* modified on 2025-08-30, PL */
            nl = 1;
            if (output("\007%s %s %s\n", user_name(me.num, name), MSG_YELLS,
                    me.msg) == -1) {
                break;
            }
        } else if ((me.type == MSG_I) && Shout) {
            if (!nl)
                clear_prompt_cols(num);    /* modified on 2025-08-30, PL */
            nl = 1;
            if (!Presbeep || Special)
                output("\007");
            if (Special)
                output("#");
            if (output("%s %s\n", user_name(me.num, name),
                    me.msg) == -1) {
                break;
            }
        } else if ((me.type == MSG_MY) && Shout) {
            if (!nl)
                clear_prompt_cols(num);    /* modified on 2025-08-30, PL */
            nl = 1;
            if (!Presbeep || Special)
                output("\007");
            if (Special)
                output("#");
            user_name(me.num, name);
            {
#ifdef SWEDISH
            size_t l = strlen(name);
            if (name[l - 1] == 's')
                r = output("%s %s\n", name, me.msg);
            else
                r = output("%ss %s\n", name, me.msg);
#else
            r = output("%s's %s\n", name, me.msg);
#endif
            }
            if (r == -1) {
                break;
            }
        } else if ((me.type == MSG_SMS) && Shout) {
            if (!nl)
                clear_prompt_cols(num);    /* modified on 2025-08-30, PL */
            nl = 1;
            if (output("%s\n", me.msg) == -1) {
                break;
            }
        } else if ((me.type == MSG_LOGIN) && (Present || Special)) {
            if (!nl)
                clear_prompt_cols(num);    /* modified on 2025-08-30, PL */
            nl = 1;
            if (Presbeep || Special)
                output("\007");
            if (output("%s %s\n", user_name(me.num, name), MSG_LOGGEDIN) == -1) {
                break;
            }
        } else if ((me.type == MSG_LOGOUT) && (Present || Special)) {
            if (!nl)
                clear_prompt_cols(num);    /* modified on 2025-08-30, PL */
            nl = 1;
            if (Presbeep || Special)
                output("\007");
            if (output("%s %s\n", user_name(me.num, name), MSG_LOGGEDOUT) == -1) {
                break;
            }
        } else if ((me.type != MSG_SAY) &&
                (me.type != MSG_I) &&
                (me.type != MSG_MY) &&
                (me.type != MSG_YELL) &&
                (me.type != MSG_SMS) &&
                (me.type != MSG_LOGIN) &&
            (me.type != MSG_LOGOUT)) {
            if (!nl)
                clear_prompt_cols(num);    /* modified on 2025-08-30, PL */
            nl = 1;
            if (output("\007%s %s: %s\n", MSG_STRANGEMSG,
                    user_name(me.num, name), me.msg) == -1) {
                break;
            }
        }
    }

    if (nl) {
        output("\n");
    }
    free(oldbuf);
    Change_msg = 0;
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
    sprintf(name, "exits display_msg()");
    debuglog(name, 30);

    return nl;
}

/*
 * send_msg - send message to other user
 * args: user to send to (uid), type of message (type), message (msg)
 *       confirm send completed (confirm 0=no, 1=yes, 2=only warn if idle)
 * ret: ok (0) or failure (-1)
 */

int
send_msg(int uid, int type, char *msg, int confirm)
{
    int b, fd, res;
    char *newbuf, *buf;
    LINE name, tmpname, msgfile;
    LONG_LINE msgbuf;
    struct SKLAFFRC *rc;
    struct MSG_ENTRY me;
    long itime;

    rc = read_sklaffrc(uid);

    if (type == MSG_SAY) {
        res = check_flag(rc->flags, "say");
        if (res == -1)
            res = 1;
        if (!res) {
            output("%s %s\n", user_name(uid, msgfile), MSG_NOSAY);
            free(rc);
            return -1;
        }
    }
    if (type == MSG_YELL || type == MSG_I || type == MSG_SMS || type == MSG_MY) {
        res = check_flag(rc->flags, "shout");
        if (res == -1)
            res = 1;
        if (!res) {
            if (type != MSG_SMS && confirm != 3)
                output("%s %s\n", user_name(uid, msgfile), MSG_NOYELL);
            free(rc);
            return -1;
        }
    }
    free(rc);

    if (!user_is_avail(uid)) {
        if ((type != MSG_LOGIN) && (type != MSG_LOGOUT) && (type != MSG_SMS))
            output("%s %s\n", user_name(uid, msgfile), MSG_BUSY);
        return 0;
    }
    user_dir(uid, msgfile);
    strcat(msgfile, MSG_FILE);

    critical();

    if ((fd = open_file(msgfile, OPEN_QUIET)) == -1) {
        output("%s %s\n", user_name(uid, msgfile), MSG_NOACTIVE);
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("send_msg", 2, "read_file");
        return -1;
    }
    b = 0;
    while (msg[b] == ' ') {
        b++;
    }
    if (type != MSG_SMS)
        sprintf(msgbuf, "%d:%d:%s\n", Uid, type, &msg[b]);
    else
        sprintf(msgbuf, "0:%d:%s\n", type, &msg[b]);

    if (type == MSG_SAY) {
        me.num = uid;
        me.direct = 1;          /* me.num is the receiver uid */
        me.type = type;
        strcpy(me.msg, &msg[b]);
        add_to_mlist(&me);
    }
    newbuf = (char *) malloc(strlen(buf) + strlen(msgbuf) + 1);
    if (newbuf == NULL) {
        sys_error("send_msg", 3, "malloc");
        return -1;
    }
    strcpy(newbuf, buf);
    strcat(newbuf, msgbuf);
    free(buf);

    /* critical(); Moved up to safeguard from quit SIGNALS while msg-file is
     * open. */

    if (write_file(fd, newbuf) == -1) {
        sys_error("send_msg", 4, "write_file");
        return -1;
    }
    if (close_file(fd) == -1) {
        sys_error("send_msg", 5, "close_file");
        return -1;
    }
    non_critical();

    notify_user(uid, SIGNAL_NEW_MSG);

    if (((type == MSG_SAY) || (type == MSG_YELL) || (type == MSG_I) || (type == MSG_MY)) &&
        confirm > 0) {
        itime = idle_time(uid);
        if (confirm == 1) {
            output("%s %s", MSG_CONFMSG, user_name(uid, name));
            if (itime >= IDLE_LIMIT)
                output(" (%s %ld min).\n", MSG_IDLEWARNING, itime);
            else
                output(".\n");
        }
        if (confirm == 2 && itime >= IDLE_LIMIT)
            output("\n%s %s %ld min.\n", user_name(uid, name),
                MSG_IDLEWARNING, itime);
        if (confirm == 3) {
            if (itime >= IDLE_LIMIT) {
                strcpy(tmpname, user_name(uid, name));
                strcat(tmpname, ")");
                output("(%-24s ", tmpname);
            } else
                output("%-25s ", user_name(uid, name));
        }
    }
    return 0;
}

/*
 * send_msg_to_all - send message to all active users
 * args: type of message (type), message (msg)
 * ret: ok (0) or failure (-1)
 */

int
send_msg_to_all(int type, char *msg)
{
    char *buf, *oldbuf;
    struct ACTIVE_ENTRY ae;
    int b;
    struct MSG_ENTRY me;

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        return -1;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        return -1;
    }
    oldbuf = buf;

    if (close_file(ActiveFD) == -1) {
        return -1;
    }
    ActiveFD = -1;

    if (type == MSG_YELL || type == MSG_I || type == MSG_MY) {
        b = 0;
        output("%s:\n", MSG_CONFMSG);
        while ((buf = get_active_entry(buf, &ae))) {
            if (ae.user != Uid) {
                if (send_msg(ae.user, type, msg, 3) == 0) {
                    b++;
                    if ((b % 3) == 0)
                        output("\n");
                }
            }
        }
        if ((b % 3) != 0)
            output("\n");
    } else {
        while ((buf = get_active_entry(buf, &ae))) {
            if (ae.user != Uid) {
                send_msg(ae.user, type, msg, 1);
            }
        }
    }

    b = 0;
    while (msg[b] == ' ') {
        b++;
    }
    me.num = Uid;
    me.type = type;
    strcpy(me.msg, &msg[b]);
    add_to_mlist(&me);

    free(oldbuf);
    return 0;
}

/*
 * notify_all_processes - notify all processes about database change
 * args: signal to send (sig)
 */

void
notify_all_processes(int sig)
{
    char *buf, *oldbuf;

    struct ACTIVE_ENTRY ae;

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        return;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        return;
    }
    oldbuf = buf;

    if (close_file(ActiveFD) == -1) {
        free(buf);
        return;
    }
    ActiveFD = -1;

    while ((buf = get_active_entry(buf, &ae))) {
        kill(ae.pid, sig);
    }

    free(oldbuf);
    return;
}

/*
 * notify_user - notify one user about something
 * args: user (uid), signal to send (sig)
 */

void
notify_user(int uid, int sig)
{

    char *buf, *oldbuf;
    struct ACTIVE_ENTRY ae;

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        return;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        return;
    }
    if (close_file(ActiveFD) == -1) {
        return;
    }
    ActiveFD = -1;

    oldbuf = buf;

    while ((buf = get_active_entry(buf, &ae))) {
        if (ae.user == uid) {
            (void) kill(ae.pid, sig);
        }
    }

    free(oldbuf);
}

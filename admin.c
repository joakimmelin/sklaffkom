/* admin.c */

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

#include <fcntl.h>
#include <pwd.h>
#include <signal.h>

#include "sklaff.h"
#include "ext_globals.h"

/*
 * display_prompt - displays default prompt
 * args: prompt displayed (p), old prompt (oldp), type of prompt (type)
 * ret: pointer to prompt displayed
 */

char *
display_prompt(char *p, char *oldp, int type)
{
    int y;

    if (Change_prompt) {
        Nextconf = -1;
        Nexttext = -1;
        if (more_comment())
            strcpy(p, MSG_REPLYPROMPT);
        else if (more_text())
            strcpy(p, MSG_TEXTPROMPT);
        else if (more_conf() != -1)
            strcpy(p, MSG_CONFPROMPT);
        else {
            Change_prompt = 0;
            if (End_default) {
                strcpy(p, MSG_ENDPROMPT);
            } else {
                strcpy(p, MSG_TIMEPROMPT);
            }
        }
    } else {
        if (End_default) {
            strcpy(p, MSG_ENDPROMPT);
        } else {
            strcpy(p, MSG_TIMEPROMPT);
        }
    }
    if (type == 0) {
        output("%s " PROMPT , p);
    } else if (strcmp(oldp, p)) {
        y = (int)strlen(oldp) + 1 + (int)(sizeof(PROMPT) - 1);
        clear_prompt_cols(y);
        output("\007%s " PROMPT , p);
    }
    Lines = 1;
    return p;
}

/*
 * display_welcome - displays welcome message and sets up new users
 */

void
display_welcome(void)
{
    LINE name, home, fname;
    int fd;
    struct USER_ENTRY *ue;
    struct SKLAFFRC *rc;

#ifdef MODEM_POOL
#ifdef MODEM_GROUP
#ifdef INET_GROUP
    char *hostname, *buf;

#endif
#endif
#endif
    Uid = getuid();
    user_dir(Uid, Home);
    mbox_dir(Uid, Mbox);
    Warning = 0;

    ActiveFD = -1;              /* Inactive active file descriptor */

    make_activity_note();       /* Touches the activity file */

    snprintf(name, sizeof(name), "login initialized");
    debuglog(name, 10);

    if (user_name(Uid, name) == NULL) {
        if (setup_new_user() == -1) {
            output("%s\n\n", MSG_CANTCREATE);
            sig_reset();
            tty_reset();
            exit(1);
        }
    }
    /* Debugging stuff */
/*    mempointer = 0;  */
/*    for(fd = 0; fd < 2000; fd++) memstack[fd] = 0L; */
    mlist = NULL;
    restart = 0;
    Change_prompt = 1;
    Change_msg = 1;
    Cont = 0;
    for (Comtop = 0; Comtop < HISTORY_SIZE; Comtop++)
        strcpy(Comstack[Comtop], "");
    Comtop = 0;
    strcpy(Overflow, "");
    strcpy(Sub, "");
    rc = read_sklaffrc(Uid);

    set_flags(rc->flags);
    if (rc->timeout[0] != '\0') {
        Timeout = atoi(rc->timeout);
        if (Timeout) {
            alarm(60 * Timeout);
        }
    } else {
        Timeout = 0;
    }
    output_ansi_fmt(YELLOW "%s%s, %s.\n\n" DOT, "%s%s, %s.\n\n", MSG_CPY1, sklaff_version, MSG_LANG);
    output_ansi_fallback(BR_BLUE MSG_CPY2 DOT, MSG_CPY2);
    output_ansi_fallback(BR_BLUE MSG_CPY3 DOT, MSG_CPY3);
    output_ansi_fallback(BR_BLUE MSG_CPY4 DOT, MSG_CPY4);
    output_ansi_fallback(BR_BLUE MSG_CPY4a DOT,  MSG_CPY4a);;
//  output_ansi_fallback(BLUE MSG_CPY5 DOT, MSG_CPY5);
    output_ansi_fallback(BR_BLUE MSG_CPY6 DOT, MSG_CPY6);
    output_ansi_fallback(BR_BLUE MSG_CPY7 DOT, MSG_CPY7);
    output_ansi_fallback(BR_BLUE MSG_CPY8 DOT, MSG_CPY8);
    output_ansi_fallback(BR_BLUE MSG_CPY9 DOT, MSG_CPY9);

#ifdef MODEM_POOL
#ifdef MODEM_GROUP
#ifdef INET_GROUP
    hostname = get_hostname();
    if (strstr(hostname, MODEM_POOL)) {
        if (getgid() == INET_GROUP) {
            strcpy(fname, INET_FILE);
        } else {
            strcpy(fname, PAY_FILE);
        }
        if ((strstr(rc->paid, "no") && (getgid() == MODEM_GROUP))
            || (getgid() == INET_GROUP)) {

            if ((fd = open_file(fname, OPEN_QUIET)) == -1) {
                return;
            }
            if ((buf = read_file(fd)) == NULL) {
                return;
            }
            if (close_file(fd) == -1) {
                return;
            }
            output("\n%s\n", buf);
            free(buf);
            sig_reset();
            tty_reset();
            exit(1);
        }
    }
#endif
#endif
#endif
    free(rc);
    if (add_active() == -1) {
        output("%s\n", MSG_CANTADD);
        sig_reset();
        tty_reset();
        exit(1);
    }
    strcpy(home, Home);
    strcat(home, MSG_FILE);
    if ((fd = create_file(home)) == -1) {
        output("%s\n", MSG_NOTELL);
        sig_reset();
        tty_reset();
        exit(1);
    }
    if (close_file(fd) == -1) {
        output("%s\n", MSG_CLOSETELL);
        sig_reset();
        tty_reset();
        exit(1);
    }
    display_news();

    snprintf(name, sizeof(name), "display_welcome(): smta");
    debuglog(name, 20);
    send_msg_to_all(MSG_LOGIN, "");

    user_name(Uid, name);
    //output("\n%s, %s.\n", MSG_WELCOME, name);
    output_ansi_fmt("\n%s, " BR_YELLOW "%s.\n"DOT, "\n%s, %s.\n", MSG_WELCOME, name);
    ue = get_user_struct(Uid);

    if (ue->last_session) {
        time_string(ue->last_session, name, 0);
        down_string(name);
        output_ansi_fmt("\n%s" CYAN " %s\n"DOT, "\n%s %s\n", MSG_LASTHERE, name);
    }
    cstack = NULL;
    ustack = NULL;
    ustack2 = NULL;
    rstack = NULL;
}

/*
 * display_news - display news file
 * Moved to lib/helpers.c and made multilingual as it always should have been :)
 * PL 2025-09-25
*/

/*
 * check_open - check if SklaffKOM login is allowed
 */

void
check_open(void)
{
    int fd;
    char *buf;

    Rot13 = 0;

    if ((fd = open_file(DOWN_FILE, OPEN_QUIET)) == -1) {
        return;
    }
    if ((buf = read_file(fd)) == NULL) {
        return;
    }
    if (close_file(fd) == -1) {
        return;
    }
    output("\n%s\n", buf);
    free(buf);
    sig_reset();
    tty_reset();
    exit(0);
}

/*
 * strip_string - strip string of all nonalphanumeric
 * characters except those in in rmstr
 * returns: no of charachters removed
 */

int
strip_string(char *str, char *rmstr)
{
    char *s1, *s2;
    int n;

    n = 0;
    s1 = str;
    s2 = str;

    while (*s1) {
        if (isalnum(*s1) || strchr(rmstr, *s1) != NULL) {
            *s2 = *s1;
            s2++;
        } else
            n++;
        s1++;
    }
    *s2 = 0;
    return n;
}


/*
 * out_onoff - display flag status
 * args: flag status (tmp)
 */

void
out_onoff(int tmp)
{
    if (tmp)
        output("%s  ", MSG_ON);
    else
        output("%s  ", MSG_OFF);
}

int
grep(int conf, char *search)
{
    LONG_LINE dirname, lineread, tsear, greparg;
    char cmdline[512];
    LINE cwd;
    FILE *pipe;
    int found;
    long lasttext, curtext;

    lasttext = last_text(conf, Uid);

    if (conf) {
        snprintf(dirname, sizeof(dirname), "%s/%d", SKLAFF_DB, conf);
    } else {
        strlcpy(dirname, Mbox, sizeof(dirname));
    }
    if (getcwd(cwd, LINE_LEN) == NULL) {
    perror("getcwd");  /* modified on 2025-07-25, PL */
    return -1;  /* modified on 2025-07-25, PL */
    }
    found = 0;
    if (chdir(dirname) != 0) {
    perror("chdir (to dirname)");  /* modified on 2025-07-25, PL */
    return -1;  /* modified on 2025-07-25, PL */
    }

    /* All dangerous characters must be taken out of the search string before
     * passing it on to the shell. */

    strcpy(tsear, search);
    strip_string(tsear, " {}]|[:\\");

    /* I hope the space, pipe and backslash are ok. */


    /* Search 100 texts at a time. This allows user to break search after each
     * batch, instead of having to wait for ALL texts.  / OR 2000-01-14 */

    curtext = 0;

    strcpy(greparg, "[1-9] [1-9][0-9]");        /* Text 1-99 */

    while (found < 2 && curtext <= lasttext) {
        snprintf(cmdline, sizeof(cmdline), "%s %s \'%s\' %s 2>/dev/null",
            SKLAFFGREP, GREPOPT, tsear, greparg);

        if ((pipe = (FILE *) popen(cmdline, "r")) == NULL) {
            output("%s\n\n", MSG_NOGREP);
            return -1;
        } else {
            while (!feof(pipe)) {
                if (fgets(lineread, 80, pipe) == NULL) {	/* error handling */
    		break;  					/* modified on 2025-07-25, PL */
	}
                  if (!feof(pipe)) {
                    if (!found) {
                        output("\n");
                    }
                    found = 1;
                    if (output("%s", lineread)) {
                        found = 2;
                        break;
                    }
                }
            }
            pclose(pipe);
        }
        curtext += 100;
        /* Text curtext - curtext+99 */
        snprintf(greparg, sizeof(greparg), "%ld[0-9][0-9]", (long) curtext / 100);
    }

    if (chdir(cwd) != 0) {
    perror("chdir (restore cwd)");  /* compiler silencer on linux, modified on 2025-07-25, PL */
    return -1;  /* modified on 2025-07-25, PL */
    }
    tty_raw();
    sig_setup();
    return found;
}

/*
 * logout - exists SklaffKOM the right way
 * args: signal received or 0 (tmp)
 */

void
exec_logout(int tmp)
{
    LINE name, tmpdir;
    int fd, conf, i;
    long textnum, at;
    struct USER_ENTRY ue;
    char *buf, *oldbuf, *nbuf, *new_user, *tmpbuf;
    LONG_LINE tbuf;
    struct termios temp_mode;

#ifdef SIGXCPU
    if (tmp == SIGXCPU) {
        output("\n\n%s\n", MSG_CPUERR);
    }
#endif

#ifdef SIGXFSZ
    if (tmp == SIGXFSZ) {
        output("\n\n%s\n", MSG_DISKERR);
    }
#endif
    if (tmp == SIGPIPE) {
        output("\n\n%s\n", MSG_PIPEERR);
    }
    if (tmp == SIGTERM) {
        output("\n\n%s\n", MSG_OP);
    }
    if (tmp == SIGALRM) {
        output("\n\n%s\n", MSG_TIMEOUT);
    }
    if (tmp != 0) {
        snprintf(tmpdir, sizeof(tmpdir), "forced logout w/ sig %d begun", tmp);
        debuglog(tmpdir, 5);
    } else {
        snprintf(tmpdir, sizeof(tmpdir), "controlled logout begun");
        debuglog(tmpdir, 10);
    }

    sprintf(tmpdir, "/tmp/%d/%d.qwk", getpid(), getpid());
    if (file_exists(tmpdir) != -1) {
        unlink(tmpdir);
    }
    sprintf(tmpdir, "/tmp/%d", getpid());
    if (chdir(Home) != 0) {
    perror("chdir (Home)");  /* modified on 2025-07-25, PL due to compiler complaints */
    return;
    }
    rmdir(tmpdir);

    while (ustack) {
        textnum = pop_unread(&conf);
        mark_as_unread(textnum, conf);
    }

    sprintf(tmpdir, "  checking active time");
    debuglog(tmpdir, 20);

    if (tmp != SIGHUP) {
        at = active_time(Uid);
        clear_screen();
		display_logout();
		output("\n%s, %s.\n%s ",
       	     MSG_WELBACK, user_name(Uid, name), MSG_ACTIVETIME);
        if (at == 1L)
            output("%s\n\n", MSG_ONEMIN);
        else
            output("%ld %s\n\n", at, MSG_MINUTES);
        /* Modem-style logout sequence because why not PL 2025-09-25*/
        if (!restart) {
    	/* But don't show unless we're truly logging out */
		sleep(1);
        output("ATH0\n");
        fflush(stdout);
        sleep(2);
        output("NO CARRIER\n\n");
        fflush(stdout);
        sleep(1);
    	}
    }
    sprintf(tmpdir, "  removing from active list");
    debuglog(tmpdir, 20);
    if (ActiveFD != -1)
        close_file(ActiveFD);
    remove_active();

    sprintf(tmpdir, "  issuing logout msg");
    debuglog(tmpdir, 20);
    send_msg_to_all(MSG_LOGOUT, "");

    sprintf(tmpdir, "  updating user entry");
    debuglog(tmpdir, 20);
    if ((fd = open_file(USER_FILE, 0)) == -1) {
        sys_error("logout", 1, "open_file");
    }
    if ((buf = read_file(fd)) == NULL) {
        sys_error("logout", 2, "read_file");
    }
    oldbuf = buf;

    while (buf) {
        buf = get_user_entry(buf, &ue);
        if (ue.num == Uid)
            break;
    }

    if (ue.num == Uid) {
        ue.last_session = time(0);
        new_user = stringify_user_struct(&ue, tbuf);
        i = strlen(oldbuf) + LINE_LEN;
        nbuf = (char *) malloc(i);
        memset(nbuf, 0, i);

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
        strcat(nbuf, new_user);
        strcat(nbuf, buf);
        critical();
        if (write_file(fd, nbuf) == -1) {
            sys_error("logout", 3, "write_file");
        }
    }
    free(oldbuf);

    if (close_file(fd) == -1) {
        sys_error("logout", 4, "close_file");
    }
    non_critical();

    sprintf(tmpdir, "  removing msg-file");
    debuglog(tmpdir, 20);
    strcpy(name, Home);
    strcat(name, MSG_FILE);
    unlink(name);

    sprintf(tmpdir, "  resetting tty");
    debuglog(tmpdir, 20);
    sig_reset();
    tty_reset();

    sprintf(tmpdir, "logout sequence completed");
    debuglog(tmpdir, 10);

    if (restart) {
        tcgetattr(0, &temp_mode);
        temp_mode.c_lflag &= ~HUPCL;
        tcsetattr(0, TCSANOW, &temp_mode);
    } else
        exit(0);
}

/*
 * exec_login - execute user login script
 */

void
exec_login(void)
{
    struct SKLAFFRC *rc;
    LINE cmdline, args;
    char *buf, *run;
    int (*fcn) (LINE);

    Logging_in = 1;

    rc = read_sklaffrc(Uid);
    if (rc != NULL) {
        if (strlen(rc->login)) {
            buf = rc->login;
            while (*buf) {
                run = cmdline;
                while ((*buf != '\n') && *buf) {
                    *run = *buf;
                    buf++;
                    run++;
                }
                *run = '\0';
                if (*buf)
                    buf++;
                if (strlen(cmdline) && ((fcn = parse(cmdline, args))
                        != (int (*) ()) 0)) {
                    if ((*fcn) (args) == -1) {
                        break;
                    }
                }
            }
        }
        free(rc);
    }
    Logging_in = 0;
}

/*
 * timeout - called by SIGALRM
 */

void
timeout(int sig)
{
    LINE name;

    if (Warning)
        exec_logout(SIGALRM);
    else {
        Warning = 1;
        sprintf(name, "timeout(): sm");
        debuglog(name, 7);
        send_msg(Uid, MSG_SAY, MSG_WARNING, 0);
        signal(SIGALRM, timeout);
        alarm(60);
    }
}

/*
 *  debuglog()
 */

void
debuglog(char *s, int level)
{
    time_t now;
    FILE *fp;
    char logname[255], entry[512], tstr[255];

    if (level < LOGLEVEL) {
        sprintf(logname, "%s/%d.%d.log", LOGDIR, Uid, getpid());

        now = time(0);
        strcpy(tstr, ctime(&now));
        tstr[strlen(tstr) - 1] = 0;
        sprintf(entry, "%s : %d : %d : %s", tstr, Uid, getpid(), s);

        fp = fopen(logname, "a");
        if (!fp) {
            // modified on 2025-09-24, PL
            // Prevent crash if log can't be written (e.g. permission denied)
            return;
        }

        fprintf(fp, "%s\n", entry);
        fclose(fp);
    }
}


/*
 * show_status - show status for an object
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
show_status(int num, int flag, int st_type)
{
    int u_num = -1, c_num = -1, first = 0, fd;
    char *u_name, *buf, *oldbuf, *c_name;
    static LINE home, cname, uname;
    static char tmp[512];
    struct SKLAFFRC *rc;
    struct CONF_ENTRY *ce;
    struct CONFS_ENTRY cse;
    struct ACTIVE_ENTRY ae;
    struct passwd *pw;
    time_t lastsess;

    if (num >= 0 && flag == USER) {
        u_num = num;
        u_name = uname;
        user_name(u_num, u_name);
        rc = read_sklaffrc(u_num);
        if (rc != NULL) {
            pw = getpwuid(u_num);
            if (st_type == STATUS_INTERNAL)
                output("\n%s    %s (%s)\n", MSG_NAMECOL, u_name, pw->pw_name);
            if (st_type == STATUS_EXTERNAL)
                outputex("\n%s    %s (%s)\n", MSG_NAMECOL, u_name, pw->pw_name);
            if (STATUS_INTERNAL == st_type) {
                if (strlen(rc->user.adress) || strlen(rc->user.postnr) ||
                    strlen(rc->user.ort)) {
                    output(MSG_ADDRCOL);
                    first = 0;
                    if (strlen(rc->user.adress)) {
                        first = 1;
                        output("%s\n", rc->user.adress);
                    }
                    if (strlen(rc->user.adress)) {
                        if (first)
                            output("         ");
                        first = 1;
                        output("%s  %s\n", rc->user.postnr, up_string(rc->user.ort));
                    }
                }
                if (strlen(rc->user.tele1) || strlen(rc->user.tele2) ||
                    strlen(rc->user.tele3)) {
                    output(MSG_TELECOL);
                    if (strlen(rc->user.tele1)) {
                        first = 1;
                        output("%s\n", rc->user.tele1);
                    }
                    if (strlen(rc->user.tele2)) {
                        if (first)
                            output("         ");
                        first = 1;
                        output("%s\n", rc->user.tele2);
                    }
                    if (strlen(rc->user.tele3)) {
                        if (first)
                            output("         ");
                        first = 1;
                        output("%s\n", rc->user.tele3);
                    }
                }
                if (strlen(rc->user.email1) || strlen(rc->user.email2)) {
                    output(MSG_EMAILCOL);
                    if (strlen(rc->user.email1)) {
                        first = 1;
                        output("%s\n", rc->user.email1);
                    }
                    if (strlen(rc->user.email2)) {
                        if (first)
                            output("         ");
                        first = 1;
                        output("%s\n", rc->user.email2);
                    }
                }
                if (strlen(rc->user.url)) {
                    output("%s%s\n", MSG_URLCOL, rc->user.url);
                }
                if (strlen(rc->user.org)) {
                    output("%s     %s\n", MSG_ORGCOL, rc->user.org);
                }
                if (strlen(rc->timeout)) {
                    first = atoi(rc->timeout);
                    if (first)
                        output("\n%s%s\n", MSG_INACT, rc->timeout);
                }
                pw = getpwuid(u_num);
                if (pw->pw_gid == MODEM_GROUP) {
                    first = atoi(rc->paydate);
                    if (first)
                        output("%s%s\n", MSG_PDATE, rc->paydate);
                    else
                        output("%s%s\n", MSG_PDATE, MSG_NOPAY);
                }
            }
            if (user_is_active(u_num)) {
                if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
                    sys_error("cmd_show_status", 1, "open_file");
                    return -1;
                }
                if ((buf = read_file(ActiveFD)) == NULL) {
                    sys_error("cmd_show_status", 2, "read_file");
                    return -1;
                }
                if (close_file(ActiveFD) == -1) {
                    sys_error("cmd_show_status", 3, "close_file");
                    return -1;
                }
                ActiveFD = -1;
                oldbuf = buf;
                buf = get_active_entry(buf, &ae);
                while (buf) {
                    if (ae.user == u_num) {
                        time_string(ae.login_time, tmp, 0);
                        if (st_type == STATUS_INTERNAL)
                            output("\n%s %s %s %s.\n\n", MSG_USERON,
                                tmp, MSG_FROM2, ltrim(ae.from));
                        if (st_type == STATUS_EXTERNAL)
                            outputex("\n%s %s %s %s.\n\n", MSG_USERON,
                                tmp, MSG_FROM2, ltrim(ae.from));
                        break;
                    }
                    buf = get_active_entry(buf, &ae);
                }
                free(oldbuf);
            } else {
                lastsess = last_session(u_num);
                if (lastsess) {
                    time_string(lastsess, tmp, 0);
                    if (st_type == STATUS_INTERNAL)
                        output("\n%s %s.\n\n", MSG_LASTON, tmp);
                    if (st_type == STATUS_EXTERNAL)
                        outputex("\n%s %s.\n\n", MSG_LASTON, tmp);
                }
            }
            if (strlen(rc->note) && st_type == STATUS_INTERNAL) {
                output("%s\n", MSG_NOTE);
                output("%s\n", rc->note);
            }
            if (strlen(rc->sig)) {
                if (st_type == STATUS_INTERNAL) {
                    output("%s\n", MSG_SIG);
                    output("%s\n", rc->sig);
                }
                if (st_type == STATUS_EXTERNAL) {
                    outputex("%s\n", MSG_SIG);
                    outputex("%s\n", rc->sig);
                }
            }
            free(rc);
            if (st_type == STATUS_EXTERNAL)
                return 0;

/* if (u_num != Uid) return 0;*/
            output("%s\n", MSG_SUBTO);
            user_dir(u_num, home);
            snprintf(tmp, sizeof(tmp), "%s%s", home, CONFS_FILE);
            if ((fd = open_file(tmp, 0)) == -1) {
                sys_error("cmd_show_status", 1, "open_file");
                return -1;
            }
            if ((buf = read_file(fd)) == NULL) {
                sys_error("cmd_show_status", 2, "read_file");
                return -1;
            }
            oldbuf = buf;
            if (close_file(fd) == -1) {
                sys_error("cmd_show_status", 3, "close_file");
                return -1;
            }
            while ((buf = get_confs_entry(buf, &cse)) != NULL) {
                free_confs_entry(&cse);
                sprintf(tmp, "%s/%d%s", SKLAFF_DB, cse.num, CONFRC_FILE);
                if (cse.num) {  /* We don't need to check mailbox */
                    /* if ((fd = open_file(tmp, 0)) == -1) {
                     * sys_error("cmd_show_status", 4, "open_file"); return
                     * -1; } */
                    ce = get_conf_struct(cse.num);

                    if (can_see_conf(Uid, cse.num, ce->type, ce->creator)) {
                        if (output("  %s\n", ce->name) == -1) {
                            /* if (close_file(fd) == -1) {
                             * sys_error("cmd_show_status", 5, "close_file");
                                return -1; } */ break;
                        }
                    }
                    /* if (close_file(fd) == -1) {
                     * sys_error("cmd_show_status", 6, "close_file"); return
                     * -1; } */
                } else {
                    conf_name(cse.num, tmp);
                    output("  %s\n", tmp);
                }
            }
            free(oldbuf);
            output("\n");
        }
    }
    if (num > 0 && flag == CONF) {
        c_num = num;
        c_name = cname;
        conf_name(c_num, c_name);
        ce = get_conf_struct(c_num);
        if (ce != NULL) {
            output("\n%s          %s\n", MSG_NAMECOL, c_name);
            output(MSG_CONFTYPE);
            switch (ce->type) {
            case OPEN_CONF:
                output("%s\n", MSG_CONFDEFAULT);
                break;
            case CLOSED_CONF:
                output("%s\n", MSG_CLOSED2);
                break;
            case SECRET_CONF:
                output("%s\n", MSG_SECRET2);
                break;
            case NEWS_CONF:
                output("%s\n", MSG_NEWS2);
                break;
            }
            if (ce->comconf)
                output("%s%s\n", MSG_CONFCOM,
                    conf_name(ce->comconf, tmp));
            time_string(ce->time, tmp, 0);
            output("%s%s %s ", MSG_CONFCREATE, tmp, MSG_BY);
            user_name(ce->creator, tmp);
            output("%s\n", tmp);
            output("%s %d\n", MSG_NUMTEXT, last_text(c_num, Uid));
            output("\n");
        }
    } else if (c_num == 0) {
        output("\n%s\n\n", MSG_NOMBOXSTAT);
    }
    return 0;
}


static int
active_entry_cmp(const void *a, const void *b)
{
    const struct ACTIVE_ENTRY *ae1 = a;
    const struct ACTIVE_ENTRY *ae2 = b;

    int r = idle_time(ae1->user) - idle_time(ae2->user);

    if (r == 0)
        r = active_time(ae2->user) - active_time(ae1->user);

    return (r);
}


/*
 * list_who - list users online
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
list_who(int who_type)
{
    long itime;
    LINE tid, idle, namn;
    char *buf, *oldbuf;
    int nactive, nidle, i;

    struct ACTIVE_ENTRY
    *ae, ea;

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

    /* Old vilka-lista char *ptr;
     * 
     * output("\n%-25s %7s  %-12s %-8s  %s\n\n", MSG_NAME, MSG_TIME, MSG_WHEN,
     * MSG_ACT, MSG_FROM); while ((buf = get_active_entry(buf, &ae))) { if
     * (ae.avail) ptr = MSG_YES; else ptr = ""; if(output("%-25s %7ld  %-12s
     * %-6s  %s\n", user_name(ae.user, namn), active_time(ae.user),
     * time_string(ae.login_time, tid, 0), ptr, ae.from) == -1) break; }
     * output("\n"); */


    if (Old_who) {

        /* New vilka-lista (98-04-15, OR) */

        if (WHO_INTERNAL == who_type)
            output("\n%-25s %-11s %4s %4s   %s\n\n", MSG_NAME, MSG_WHEN,
                MSG_TIME, MSG_ACT, MSG_FROM);
        if (WHO_EXTERNAL == who_type)
            outputex("\n%-25s %-11s %4s %4s   %s\n\n", MSG_NAME, MSG_WHEN,
                MSG_TIME, MSG_ACT, MSG_FROM);
        while ((buf = get_active_entry(buf, &ea))) {
            if (!ea.avail) {
                user_name(ea.user, namn);
                namn[25] = 0;
            } else {
                namn[0] = '(';
                user_name(ea.user, namn + 1);
                namn[24] = 0;
                strcat(namn, ")");
            }
            itime = idle_time(ea.user);
            if (itime == 0)
                strcpy(idle, "    ");
            else
                sprintf(idle, "%4ld", itime);
            if (WHO_INTERNAL == who_type)
                if (output("%-25s %-11s %4ld %s  %s\n",
                        namn,
                        time_string(ea.login_time, tid, 0),
                        active_time(ea.user),
                        idle,
                        ea.from) == -1)
                    break;
            if (WHO_EXTERNAL == who_type)
                outputex("%-25s %-11s %4ld %s  %s\n",
                    namn,
                    time_string(ea.login_time, tid, 0),
                    active_time(ea.user),
                    idle,
                    ea.from);
        }
        if (WHO_INTERNAL == who_type)
            output("\n");
        if (WHO_EXTERNAL == who_type)
            outputex("\n");
    } else {
        nidle = 0;
        nactive = 0;
        while ((buf = get_active_entry(buf, &ea)))
            nactive++;

        ae = (struct ACTIVE_ENTRY *) malloc(nactive * sizeof(struct ACTIVE_ENTRY));

        buf = oldbuf;

        for (i = 0; i < nactive; i++) {
            buf = get_active_entry(buf, &(ae[i]));
            assert(buf);
        }

        qsort(ae, nactive, sizeof(struct ACTIVE_ENTRY), active_entry_cmp);

        if (WHO_INTERNAL == who_type)
            output("\n%-25s %-11s %4s %4s   %s\n\n", MSG_NAME, MSG_WHEN,
                MSG_TIME, MSG_ACT, MSG_FROM);
        if (WHO_EXTERNAL == who_type)
            outputex("\n%-25s %-11s %4s %4s   %s\n\n", MSG_NAME, MSG_WHEN,
                MSG_TIME, MSG_ACT, MSG_FROM);

        for (i = 0; i < nactive; i++) {
            if (!ae[i].avail) {
                user_name(ae[i].user, namn);
                namn[25] = 0;
            } else {
                namn[0] = '(';
                user_name(ae[i].user, namn + 1);
                namn[24] = 0;
                strcat(namn, ")");
            }
            itime = idle_time(ae[i].user);
            if (itime < IDLE_LIMIT)
                strcpy(idle, "    ");
            else {
                sprintf(idle, "%4ld", itime);
                nidle++;
            }
            if (WHO_INTERNAL == who_type)
                if (output("%-25s %-11s %4ld %s  %s\n",
                        namn,
                        time_string(ae[i].login_time, tid, 0),
                        active_time(ae[i].user),
                        idle,
                        ae[i].from) == -1)
                    break;
            if (WHO_EXTERNAL == who_type)
                outputex("%-25s %-11s %4ld %s  %s\n",
                    namn,
                    time_string(ae[i].login_time, tid, 0),
                    active_time(ae[i].user),
                    idle,
                    ae[i].from);
        }
        if (WHO_INTERNAL == who_type)
            output("\n");
        if (WHO_EXTERNAL == who_type)
            outputex("\n");
        free(ae);

        /* Added 99-01-28/ OR */

        if (WHO_INTERNAL == who_type) {
            output("Totalt %d inloggade, varav %d aktiva.\n", nactive, nactive - nidle);
            output("\n");
        }
        if (WHO_EXTERNAL == who_type) {
            outputex("Totalt %d inloggade, varav %d aktiva.\n", nactive, nactive - nidle);
            outputex("\n");
        }
    }

    free(oldbuf);
    return 0;
}

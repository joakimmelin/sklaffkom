/* sklaffadm.c */

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
#include <signal.h>
#include <sys/stat.h>

#include "globals.h"

int
main(int argc, char *argv[])
{
    char *name, *buf, *oldbuf, *buf2, *oldbuf2, *buf3, *oldbuf3;
    int u_num, fd, conf, fd2, fd3, pdate, ndays;
    long ftext, ntext, otext, ttext, ltext;
    char dirname[256], fname[256], udir[256], uname[256];  /* promoted to 256 to prevent truncation, modified on 2025-07-13, PL */
    LINE cname, args;
    struct ACTIVE_ENTRY ae;
    struct TEXT_HEADER th;
    struct TEXT_ENTRY te;
    struct CONF_ENTRY ce, *ce2;
    struct USER_ENTRY ue;
    struct FILE_ENTRY fe;
    struct SKLAFFRC *rc;
    struct stat fs;
    struct passwd *pw;

    tty_raw();                  /* Setup tty		   */
    if ((argc > 1) && !strcmp(argv[1], "userlist")) {
        if (argc > 2)
            pdate = atoi(argv[2]);
        else
            pdate = 0;
        output("\n%s\n", MSG_MODEMNAME);
        list_user(1, MODEM_GROUP, pdate);
        if (!pdate) {
            output("%s\n", MSG_INETNAME);
            list_user(1, INET_GROUP, 0);
        }
        if (!pdate) {
            output("%s\n", MSG_OTHERNAME);
            list_user(1, -1, 0);
        }
    } else if ((argc > 1) && !strcmp(argv[1], "paydate")) {
        if (argc < 3) {
            output("\n%s\n\n", MSG_NONAME);
        } else if (argc < 4) {
            output("\n%s\n\n", MSG_NODATE);
        } else {
            name = expand_name(argv[2], USER, 0, NULL);
            if (name && *name) {
                u_num = user_uid(name);
                if (u_num) {
                    pw = getpwuid(u_num);
                    if (pw->pw_gid != MODEM_GROUP) {
                        output("\n%s\n\n", MSG_NOMODEM);
                    } else {
                        rc = read_sklaffrc(u_num);
                        strcpy(rc->paydate, argv[3]);
                        write_sklaffrc(u_num, rc);
                        output("\n%s %s %s %s.\n\n", MSG_PAYDATE, name,
                            MSG_CHANGED, argv[3]);
                    }
                } else {
                    output("\n%s\n\n", MSG_ERRUID);
                }
            }
        }
    } else if ((argc > 1) && !strcmp(argv[1], "enable")) {
        if (argc < 3) {
            output("\n%s\n\n", MSG_NOUNAME);
        } else {
            name = expand_name(argv[2], USER, 0, NULL);
            if (name && *name) {
                u_num = user_uid(name);
                if (u_num) {
                    pw = getpwuid(u_num);
                    if (pw->pw_gid != MODEM_GROUP) {
                        output("\n%s\n\n", MSG_NONAME);
                    } else {
                        rc = read_sklaffrc(u_num);
                        strcpy(rc->paid, "yes");
                        write_sklaffrc(u_num, rc);
                        output("\n%s %s\n\n", name, MSG_ALLOW);
                    }
                } else {
                    output("\n%s\n\n", MSG_ERRUID);
                }
            }
        }
    } else if ((argc > 1) && !strcmp(argv[1], "disable")) {
        if (argc < 3) {
            output("\n%s\n\n", MSG_NONAME);
        } else {
            name = expand_name(argv[2], USER, 0, NULL);
            if (name && *name) {
                u_num = user_uid(name);
                if (u_num) {
                    pw = getpwuid(u_num);
                    if (pw->pw_gid != MODEM_GROUP) {
                        output("\n%s\n\n", MSG_NOMODEM);
                    } else {
                        rc = read_sklaffrc(u_num);
                        strcpy(rc->paid, "no");
                        write_sklaffrc(u_num, rc);
                        output("\n%s %s\n\n", name, MSG_DENY);
                    }
                } else {
                    output("\n%s\n\n", MSG_ERRUID);
                }
            }
        }
    } else if ((argc > 1) && !strcmp(argv[1], "conflist")) {
        fd = open_file(CONF_FILE, 0);
        buf = read_file(fd);
        oldbuf = buf;
        close_file(fd);
        output("\n %s\n", MSG_CONFHEAD);
        output("-------------------------------------------------\n");
        ttext = 0;
        otext = 0;
        while (buf) {
            buf = get_conf_entry(buf, &ce);
            if (buf) {
                ftext = first_text(ce.num, Uid);
                if (!ce.last_text)
                    ftext = 0;
                ntext = ce.last_text - ftext;
                if ((ftext > 0) && (ce.last_text > 0))
                    ntext++;
                ttext += ce.last_text;
                otext += ntext;
                if (output("%4d %7ld - %7ld   %5ld     %2d   %s\n", ce.num, ftext, ce.last_text, ntext, ce.life, ce.name) == -1)
                    break;
            }
        }
        free(oldbuf);
        output("\n%s %ld %s. %ld %s.\n\n", MSG_TOTAL, ttext,
            MSG_TEXTS, otext, MSG_ONLINE);
    } else if ((argc > 1) && !strcmp(argv[1], "filelist")) {
        fd = open_file(CONF_FILE, 0);
        buf = read_file(fd);
        oldbuf = buf;
        close_file(fd);
        output("\n %s\n", MSG_FILEHEAD2);
        output("--------------------------------------------------\n");
        while (buf) {
            buf = get_conf_entry(buf, &ce);
            if (buf) {
                otext = 0;
                snprintf(udir, sizeof(udir), "%s/%d%s", FILE_DB, ce.num, INDEX_FILE); /* modified on 2025-07-12, PL  */
                if ((fd = open_file(udir, OPEN_QUIET)) != -1) {
                    buf2 = read_file(fd);
                    oldbuf2 = buf2;
                    close_file(fd);
                    while (buf2) {
                        buf2 = get_file_entry(buf2, &fe);
                        if (buf2) {
                            snprintf(udir, sizeof(udir), "%s/%d/%s", FILE_DB, ce.num, /* modified on 2025-07-12, PL  */
                                fe.name);
                            if (stat(udir, &fs) == -1) {
                                fs.st_size = 0;
                            }
                            otext += fs.st_size;
                        }
                    }
                    otext = otext / 1024;
                    free(oldbuf2);
                    if (otext) {
                        output("%4d   %12ld       %s\n", ce.num,
                            otext, ce.name);
                    }
                }
            }
        }
        free(oldbuf);
        output("\n");
    } else if ((argc > 1) && !strcmp(argv[1], "maillist")) {
        fd = open_file(USER_FILE, 0);
        buf = read_file(fd);
        oldbuf = buf;
        close_file(fd);
        output("\n %s\n", MSG_CONFHEAD);
        output("-------------------------------------------------\n");
        ttext = 0;
        otext = 0;
        while (buf) {
            buf = get_user_entry(buf, &ue);
            if (buf) {
                mbox_dir(ue.num, udir);
                snprintf(uname, sizeof(uname), "%.240s%s", udir, MAILBOX_FILE); /* modified on 2025-07-12, PL  */
                fd2 = open_file(uname, 0);
                buf2 = read_file(fd2);
                oldbuf2 = buf2;
                close_file(fd2);
                buf2 = get_conf_entry(buf2, &ce);
                free(oldbuf2);
                ftext = first_text(ce.num, ue.num);
                if (!ce.last_text)
                    ftext = 0;
                ntext = ce.last_text - ftext;
                if ((ftext > 0) && (ce.last_text > 0))
                    ntext++;
                ttext += ce.last_text;
                otext += ntext;
                if (output("%4d %7ld - %7ld   %5ld     %2d   %s\n", ue.num, ftext, ce.last_text, ntext, ce.life, ue.name) == -1)
                    break;
            }
        }
        free(oldbuf);
        output("\n%s %ld %s. %ld %s.\n\n", MSG_TOTAL, ttext,
            MSG_TEXTS, otext, MSG_ONLINE);
    } else if ((argc > 1) && !strcmp(argv[1], "purge")) {
        if (argc < 4) {
            output("\n%s\n\n", MSG_CTERR);
        } else {
            conf = atoi(argv[2]);
            ntext = atol(argv[3]);
            if ((ntext >= 0) && conf) {
                if (conf > 0) {
                    if (conf_name(conf, cname) == NULL) {
                        output("\n%s\n\n", MSG_WRONGCONF);
                    } else {
                        ce2 = get_conf_struct(conf);
                        snprintf(dirname, sizeof(dirname), "%s/%d/", SKLAFF_DB, conf); /* modified on 2025-07-12, PL  */
                        ftext = first_text(conf, Uid);
                        ltext = last_text(conf, Uid);
                        ltext = ltext - ntext + 1;
                        ttext = 0;
                        while ((ftext < ltext) && ce2->life) {
                            snprintf(fname, sizeof(fname), "%.244s%ld", dirname, ftext); /* modified on 2025-07-12, PL  */
                            ftext++;
                            if (file_exists(fname) != -1) {
                                fd = open_file(fname, 0);
                                buf = read_file(fd);
                                oldbuf = buf;
                                close_file(fd);
                                buf = get_text_entry(buf, &te); /* We change the order of things here to prevent unexpected behaviours PL 2026-05-15 */
                                free(oldbuf);                             
                                if ((te.th.time + (ce2->life * 86400)) < time(0)) {
                                    if (te.th.type == TYPE_TEXT) {      /* Don't delete surveys */
                                    ttext++;
                                    unlink(fname);
                                }
                            } else {
                                free_text_entry(&te);
                                break;
                            }
                            
                            free_text_entry(&te);
                            }
                        }
                        output("\n%ld %s\n\n", ttext, MSG_DELTEXTS);
                    }
                } else {
                    if (conf != -1) {
                        output("\n%s\n\n", MSG_WRONGCONF);
                    } else {
                        fd = open_file(CONF_FILE, 0);
                        buf = read_file(fd);
                        oldbuf = buf;
                        close_file(fd);
                        ttext = 0;
                        while (buf) {
                            buf = get_conf_entry(buf, &ce);
                            if (buf) {
                                snprintf(dirname, sizeof(dirname), "%s/%d/", SKLAFF_DB, ce.num); /* modified on 2025-07-12, PL  */
                                ftext = first_text(ce.num, Uid);
                                ltext = last_text(ce.num, Uid);
                                ltext = ltext - ntext + 1;
                                while ((ftext < ltext) && ce.life) {
                                    snprintf(fname, sizeof(fname), "%.244s%ld", dirname, ftext); /* modified on 2025-07-12, PL  */
                                    ftext++;
                                    if (file_exists(fname) != -1) {
                                        fd2 = open_file(fname, 0);
                                        buf2 = read_file(fd2);
                                        oldbuf2 = buf2;
                                        close_file(fd2);
                                        buf2 = get_text_entry(buf2, &te); /* We change the order here to avoid using te after free_text_entry(). PL 2026-05-15 */
                                        free(oldbuf2);                                       
                                        if ((te.th.time + (ce.life * 86400)) < time(0)) {
                                            if (te.th.type == TYPE_TEXT) {
                                                ttext++;
                                                unlink(fname);
                                            }
                                        } else {
                                           free_text_entry(&te);
                                           break;
                                        }
                                        free_text_entry(&te);                                            
                                    }
                                }
                            }
                        }
                        free(oldbuf);
                        output("\n%ld %s\n\n", ttext, MSG_DELTEXTS);
                    }
                }
            } else {
                output("\n%s\n\n", MSG_ARGERR);
            }
    }
    } else if ((argc > 1) && !strcmp(argv[1], "forcepurge")) {
        if (argc < 4) {
        output("\n%s\n\n", MSG_CTERR);
        } else {
        conf = atoi(argv[2]);
        ntext = atol(argv[3]);

        if ((ntext >= 0) && conf) {
            if (conf > 0) {
                if (conf_name(conf, cname) == NULL) {
                    output("\n%s\n\n", MSG_WRONGCONF);
                } else {
                    snprintf(dirname, sizeof(dirname), "%s/%d/", SKLAFF_DB, conf); /* modified on 2025-07-12, PL  */
                    ftext = first_text(conf, Uid);
                    ltext = last_text(conf, Uid);
                    ltext = ltext - ntext + 1;
                    ttext = 0;

                    while (ftext < ltext) {
                        snprintf(fname, sizeof(fname), "%.244s%ld", dirname, ftext); /* modified on 2025-07-12, PL  */
                        ftext++;

                        if (file_exists(fname) != -1) {
                            fd = open_file(fname, 0);
                            buf = read_file(fd);
                            oldbuf = buf;
                            close_file(fd);

                            buf = get_text_entry(buf, &te);
                            free(oldbuf);

                            if (te.th.type == TYPE_TEXT) {      /* Don't delete surveys */
                                ttext++;
                                unlink(fname);
                            }

                            free_text_entry(&te);
                        }
                    }

                    output("\n%ld %s\n\n", ttext, MSG_DELTEXTS);
                }
            } else {
                output("\n%s\n\n", MSG_WRONGCONF);
            }
        } else {
            output("\n%s\n\n", MSG_ARGERR);
        }
    }        
    } else if ((argc > 1) && !strcmp(argv[1], "mailpurge")) {
        if (argc < 4) {
            output("\n%s\n\n", MSG_CTERR);
        } else {
            conf = atoi(argv[2]);
            ntext = atol(argv[3]);
            if ((ntext >= 0) && conf) {
                if (conf > 0) {
                    if (user_name(conf, cname) == NULL) {
                        output("%s", MSG_EUSERN);
                    } else {
                        mbox_dir(conf, dirname);
                        strlcpy(fname, dirname, sizeof(fname)); /* modified on 2025-07-12, PL  */
                        strcat(fname, MAILBOX_FILE);
                        fd = open_file(fname, 0);
                        buf = read_file(fd);
                        oldbuf = buf;
                        close_file(fd);
                        buf = get_conf_entry(buf, &ce);
                        free(oldbuf);
                        ftext = first_text(0, conf);
                        ltext = ce.last_text;
                        ltext = ltext - ntext + 1;
                        ttext = 0;
                        while ((ftext < ltext) && ce.life) {
                            snprintf(fname, sizeof(fname), "%.244s/%ld", dirname, ftext); /* modified on 2025-07-12, PL  */
                            ftext++;
                            if (file_exists(fname) != -1) {
                                fd2 = open_file(fname, 0);
                                buf2 = read_file(fd2);
                                oldbuf2 = buf2;
                                close_file(fd2);
                                buf2 = get_text_entry(buf2, &te); /* We change the order here to avoid using te after free_text_entry(). PL 2026-05-15 */
                                free(oldbuf2);

                                if ((te.th.time + (ce.life * 86400)) < time(0)) {
                                    ttext++;
                                    unlink(fname);
                                } else {
                                    free_text_entry(&te);
                                    break;
                                }

                                free_text_entry(&te);
                            }
                        }
                        output("\n%ld %s\n\n", ttext, MSG_DELTEXTS);
                    }
                } else {
                    if (conf != -1) {
                        output("%s", MSG_EUSERN);
                    } else {
                        fd = open_file(USER_FILE, 0);
                        buf = read_file(fd);
                        oldbuf = buf;
                        close_file(fd);
                        ttext = 0;
                        while (buf) {
                            buf = get_user_entry(buf, &ue);
                            if (buf) {
                                mbox_dir(ue.num, dirname);
                                strlcpy(fname, dirname, sizeof(fname)); /* modified on 2025-07-12, PL  */
                                strcat(fname, MAILBOX_FILE);
                                fd3 = open_file(fname, 0);
                                buf3 = read_file(fd3);
                                oldbuf3 = buf3;
                                close_file(fd3);
                                buf3 = get_conf_entry(buf3, &ce);
                                free(oldbuf3);
                                ftext = first_text(0, ue.num);
                                ltext = ce.last_text;
                                ltext = ltext - ntext + 1;
                                while ((ftext < ltext) && ce.life) {
                                    snprintf(fname, sizeof(fname), "%.244s/%ld", dirname, ftext);  /* modified on 2025-07-13, PL */
                                    ftext++;
                                    if (file_exists(fname) != -1) {
                                        fd2 = open_file(fname, 0);
                                        buf2 = read_file(fd2);
                                        oldbuf2 = buf2;
                                        close_file(fd2);
                                        buf2 = get_text_entry(buf2, &te); /* We change the order here to avoid using te after free_text_entry(). PL 2026-05-15 */
                                        free(oldbuf2);

                                        if ((te.th.time + (ce.life * 86400)) < time(0)) {
                                            ttext++;
                                            unlink(fname);
                                        } else {
                                            free_text_entry(&te);
                                            break;
                                        }

                                        free_text_entry(&te);
                                    }
                                }
                            }
                        }
                        free(oldbuf);
                        output("\n%ld %s\n\n", ttext, MSG_DELTEXTS);
                    }
                }
            } else {
                output("\n%s\n\n", MSG_ARGERR);
            }
        }
    } else if ((argc == 4) && !strcmp(argv[1], "life")) {
        conf = atoi(argv[2]);
        ndays = atoi(argv[3]);
        if ((ndays >= 0) && conf) {
            if (conf > 0) {
                fd = open_file(CONF_FILE, 0);
                buf = read_file(fd);
                oldbuf = buf;
                buf = get_conf_entry(buf, &ce);
                while ((ce.num != conf) && buf)
                    buf = get_conf_entry(buf, &ce);
                if (ce.num == conf) {
                    ce.life = ndays;
                    buf2 = replace_conf(&ce, oldbuf);
                    write_file(fd, buf2);
                    close_file(fd);
                    output("\n%s%s%s%d%s\n\n", MSG_LIFE1, ce.name, MSG_LIFE2,
                        ce.life, MSG_LIFE3);
                } else {
                    close_file(fd);
                    output("\n%s\n\n", MSG_WRONGCONF);
                }
            } else
                output("\n%s\n\n", MSG_WRONGCONF);
        }
    } else if ((argc == 4) && !strcmp(argv[1], "maillife")) {
        conf = atoi(argv[2]);
        ndays = atoi(argv[3]);
        if ((ndays >= 0) && conf) {
            if (conf > 0) {
                mbox_dir(conf, fname);
                strcat(fname, MAILBOX_FILE);
                fd = open_file(fname, 0);
                buf = read_file(fd);
                oldbuf = buf;
                buf = get_conf_entry(buf, &ce);
                while ((ce.num != conf) && buf)
                    buf = get_conf_entry(buf, &ce);
                ce.life = ndays;
                buf2 = replace_conf(&ce, oldbuf);
                write_file(fd, buf2);
                close_file(fd);
                output("\n%s%s%s%d%s\n\n", MSG_LIFE1, user_name(conf, fname),
                    MSG_LIFE2, ce.life, MSG_LIFE3);
            } else
                output("\n%s\n\n", MSG_WRONGCONF);
        }
    } else if ((argc > 1) && !strcmp(argv[1], "who")) {
        cmd_who(args);
    } else if ((argc > 1) && !strcmp(argv[1], "down")) {
        if (file_exists(DOWN_FILE) != -1) {
            output("\n%s\n\n", MSG_ALREADYDOWN);
        } else {
            output("\n%s (etc/down)\n\n", MSG_CAUSE);
            line_ed(DOWN_FILE, &th, 0, 0, 0, NULL, NULL);
            output("etc/down %s\n\n", MSG_CREATED);
            sleep(3);
            output("%s\n", MSG_NOTIFY);
            strlcpy(args, MSG_EXITNOW, sizeof(args)); /* modified on 2025-07-12, PL  */
            Uid = -2;
            cmd_yell(args);
            fflush(stdout);
            output(MSG_WAITMIN);
            for (u_num = 0; u_num < 60; u_num++) {
                sleep(1);
                output(".");
                fflush(stdout);
            }
            Lines = 1;
            output("\n\n");
            output("%s\n", MSG_THROWOUT);
            fflush(stdout);
            fd = open_file(ACTIVE_FILE, 0);
            buf = read_file(fd);
            oldbuf = buf;
            close_file(fd);
            while ((buf = get_active_entry(buf, &ae))) {
                kill(ae.pid, SIGTERM);
                output("\n%s %s", user_name(ae.user, args), MSG_THROW);
            }
            free(oldbuf);
            sleep(3);
            output("\n\nSklaffKOM %s\n\n", MSG_DOWNREADY);
        }
    } else if ((argc > 1) && !strcmp(argv[1], "up")) {
        if (file_exists(DOWN_FILE) != -1) {
            unlink(DOWN_FILE);
            output("\nSklaffKOM %s\n\n", MSG_UPREADY);
        } else {
            output("\nSklaffKOM %s\n\n", MSG_ALREADYUP);
        }
    } else {
        output(MSG_ADMHELP1);
        output(MSG_ADMHELP2);
        output(MSG_ADMHELP3);
        output(MSG_ADMHELP4);
        output(MSG_ADMHELP5);
        output(MSG_ADMHELP6);
        output(MSG_ADMHELP7);
        output(MSG_ADMHELP8);
        output(MSG_ADMHELP9);
        output(MSG_ADMHELP10);
        output(MSG_ADMHELP11);
        output(MSG_ADMHELP12);
        output(MSG_ADMHELP13);
        output(MSG_ADMHELP14);
        output(MSG_ADMHELP18);
        output(MSG_ADMHELP19);
        output(MSG_ADMHELP15);
        output(MSG_ADMHELP16);
        output(MSG_ADMHELP17);
    }
    tty_reset();                /* Reset tty		   */
    exit(0);
}

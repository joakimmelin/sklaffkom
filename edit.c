/* edit.c */

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

#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#ifdef LINUX                                                                    
#include <bsd/string.h>  /* for strlcat on Linux */                             
#endif
#include "sklaff.h"
#include "ext_globals.h"



/*
 * line_ed - simple line editor
 * args: file to edit (fname), pointer to text header (th), text? (edit_text),
 *       allow say/shout from editor (allow_say), allow aborts (allow_abort)
 *       conference moved to (nc) NULL = no move allowed
 */

struct TEXT_HEADER *
line_ed(char *fname, struct TEXT_HEADER * th, int edit_text, int allow_say, int allow_abort, int *nc, char *mailrec)
{
    unsigned char c, c2;
    unsigned char u8e[2] = {0,0}; int u8e_len = 0; /* UTF-8 echo buffer */
    char *space, *cptr, *j, *p, outc, lastchar, *confname;
    char *buf, *oldbuf;
    LINE waste, cmd, arg, upcasearg;
    char newname[128];  /* increased buffer size to fix truncation issue, modified on 2025-07-12, PL */
    int fd, len, count, edit_line, buflen, i, newconf, origconf, marcel, qt;
    int ll;
    long tn, numlines;
    time_t save_time;
    struct EDIT_BUF *ptr, *tmpptr, *tmpptr2, *tmpptr3;
    struct TEXT_ENTRY te;
    struct TEXT_BODY *tb;
    struct passwd *pw;

#ifndef POSTING_OK
    struct CONF_ENTRY *ce;

#endif

    origconf = edit_text;
    th->size = 0;
    if ((Start = (struct EDIT_BUF *) malloc(sizeof(struct EDIT_BUF))) == NULL) {
        sys_error("line_ed", 1, "malloc");
        return NULL;
    }
    ptr = Start;
    ptr->previous = NULL;
    ptr->next = NULL;
    ptr->line[0] = '\0';
    waste[0] = '\0';
    numlines = 1;
    Size = 1;
    c = 0;

    if ((fd = open(fname, O_RDONLY)) != -1) {
        close(fd);
        fd = open_file(fname, 0);
        if ((cptr = read_file(fd)) == NULL)
            return NULL;
        buflen = file_size(fd);
        space = ptr->line;
        while (buflen) {
            if (*cptr == '\n') {
                *space = '\0';
                numlines++;
                Size++;
                if ((ptr->next = (struct EDIT_BUF *) malloc
                        (sizeof(struct EDIT_BUF))) == NULL) {
                    sys_error("line_ed", 2, "malloc");
                    return NULL;
                }
                tmpptr = ptr;
                ptr = ptr->next;
                ptr->previous = tmpptr;
                ptr->next = NULL;
                ptr->line[0] = '\0';
                space = ptr->line;
                cptr++;
            } else {
                *space = *cptr;
                space++;
                cptr++;
            }
            buflen--;
        }
        *space = '\0';
        close_file(fd);
    }
    lastchar = '\0';
    Globalth = th;
    if (allow_abort)
        signal(SIGHUP, abort_edit);
    for (;;) {
        len = strlen(ptr->line);
        if (allow_say)
            display_msg(0);
        output("%3d:%s", numlines, ptr->line);

        for (;;) {
            lastchar = c;
            if (Timeout) {
                alarm(60 * Timeout);
            }
            do
                c = getc(stdin);
            while (c == 255);
            /* if (Strip) c &= 0x7f;  Obsolete 18/2 2000, OR */
            alarm(0);
            Warning = 0;
            outc = c;
/* 2025-08-10, PL: minimal UTF-8 mapping for Swedish å ä ö Å Ä Ö -> SF7 ] [ \ } { | (echo UTF-8) */
if (c == 0xC3) {
    int c2tmp = getc(stdin);
    if (c2tmp != EOF) {
        unsigned char u2 = (unsigned char)c2tmp;
        switch (u2) {
            case 0xA5: c = '}'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* å */
            case 0xA4: c = '{'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* ä */
            case 0xB6: c = '|'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* ö */
            case 0x85: c = ']'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* Å */
            case 0x84: c = '['; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* Ä */
            case 0x96: c = 0x5c; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* Ö -> backslash */
            default:
                ungetc(c2tmp, stdin); /* not a Swedish letter */
                break;
        }
    }
}

            if (c == 134)
                c = '}';
            else if (c == 132)
                c = '{';
            else if (c == 148)
                c = '|';
            else if (c == 143)
                c = ']';
            else if (c == 142)
                c = '[';
            else if (c == 153)
                c = 0x05c;
            if (c == 229)
                c = '}';
            else if (c == 228)
                c = '{';
            else if (c == 246)
                c = '|';
            else if (c == 197)
                c = ']';
            else if (c == 196)
                c = '[';
            else if (c == 214)
                c = 0x05c;
            if (c == 140)
                c = '}';
            else if (c == 138)
                c = '{';
            else if (c == 154)
                c = '|';
            else if (c == 129)
                c = ']';
            else if (c == 128)
                c = '[';
            else if (c == 133)
                c = 0x05c;

            Lines = 1;

            if (c >= ' ' && c <= '~') {
                if (len < 74) {
                    ptr->line[len] = (char) c;
                    if (u8e_len == 2) { fwrite(u8e, 1, 2, stdout); u8e_len = 0; } else { putc(outc, stdout); }
                    len++;
                } else {
                    ptr->line[len++] = (char) c;
                    ptr->line[len] = '\0';
                    space = strrchr(ptr->line, ' ');
                    if (!space)
                        space = ptr->line + 65;
                    *space = '\0';
                    space++;
                    strcpy(waste, space);
                    while (*space) {
                        output("\b \b");
                        len--;

                        space++;
                    }
                    output("\n");
                    break;
                }
            } else if (c == 9) {
                if (len < 72) {
                    i = 8 - (len % 8);
                    while (i) {
                        ptr->line[len] = 32;
                        putc(' ', stdout);
                        len++;
                        i--;
                    }
                }
            } else if (c == '\b' || c == 127) {
                if (len > 0) {
                    len--;
                    output("\b \b");
                } else if (len == 0) {
                    if (numlines > 1) {
                        output("\r");
                        numlines--;
                        Size--;
                        ptr = ptr->previous;
                        free(ptr->next);
                        ptr->next = NULL;
                        len = strlen(ptr->line);
                        output("%3d:%s", numlines, ptr->line);
                    } else if (edit_text || mailrec) {
                        output("\n%s", MSG_SUBJECT);
                        input(th->subject, th->subject, SUBJECT_LEN, 0, 0, 0);
                        strcpy(ptr->line, "");
                        output("%3d:%s", numlines, ptr->line);
                    }
                }
            } else if ((c == 23) && (len > 0)) {
                p = ptr->line + len;
                j = ptr->line;
                while ((*(p - 1) == ' ') && (len > 0)) {
                    p--;
                    len--;
                    output("\b \b");
                }
                while (p > j) {
                    if (*(p - 1) == ' ') {
                        break;
                    } else {
                        p--;
                        len--;
                        output("\b \b");
                    }
                }
            } else if ((c == 24 || c == 21) && len > 0) {
                for (; len > 0; len--)
                    output("\b \b");
                ptr->line[0] = '\0';
            } else if (c == '\r' || (c == '\n' && lastchar != '\r')) {
                ptr->line[len] = '\0';
                output("\n");
                Lines = 1;
                make_activity_note();
                break;
            } else if (c == 12) {
                output("\n\n");
                make_activity_note();
                ptr->line[len] = '\0';
                ptr->next = NULL;
                if (edit_text || mailrec) {
                    display_header(th, 0, edit_text, 0, mailrec);
                }
                tmpptr = Start;
                count = 1;
                while (tmpptr->next) {
                    if (output("%3d:%s\n", count++, tmpptr->line) == -1)
                        break;
                    tmpptr = tmpptr->next;
                }
                output("%3d:%s", numlines, ptr->line);
            } else if (c == 3) {
                make_activity_note();
                if (numlines > 1) {
                    output("\n\n%s ", MSG_REMTEXT);
                    input("", waste, 4, 0, 0, 0);
                    down_string(waste);
                } else {
                    strcpy(waste, MSG_YES2);
                    output("\n");
                }
                if (*waste == MSG_YESANSWER) {
                    ptr = Start;
                    while (ptr) {
                        tmpptr = ptr;
                        ptr = ptr->next;
                        free(tmpptr);
                    }
                    return NULL;
                } else {
                    ptr->line[len] = '\0';
                    output("\n%3d:%s", numlines, ptr->line);
                }
                strcpy(waste, "");
            } else if (c == 26 || c == 4) {
                make_activity_note();
                marcel = 1;
#ifndef POSTING_OK
                if (nc) {
                    ce = get_conf_struct(*nc);
                    if (ce->type == NEWS_CONF) {
                        output("\n\n%s\n", MSG_NOSAVE);
                        ptr->line[len] = '\0';
                        output("\n%3d:%s", numlines, ptr->line);
                        marcel = 0;
                    }
                }
#endif
                if (marcel) {
                    if (len == 0) {
                        output("\n");
                        if (numlines > 1) {
                            ptr = ptr->previous;
                            free(ptr->next);
                            ptr->next = NULL;
                        }
                        numlines--;
                        Size--;
                    } else {
                        output("\n");
                        ptr->line[len] = '\0';
                    }
                    goto save;
                }
            }
        }

        if (ptr->line[0] == '!') {
            space = strchr(ptr->line, ' ');
            arg[0] = '\0';
            upcasearg[0] = '\0';
            if (space) {
                *space = '\0';
                space++;
                while (*space == ' ')
                    space++;
                strcpy(arg, space);
                strcpy(upcasearg, space);
            }
            down_string(ptr->line);
            down_string(arg);
            strcpy(cmd, ptr->line);
            ptr->line[0] = '\0';
            if (strlen(cmd) > 1) {
                if ((strstr(MSG_EDREAD, cmd) && (strlen(cmd) > EDREADN))
                    || ((strstr(MSG_EDWHOLE, cmd)
                            && strlen(cmd) > EDWHOLEN))) {
                    output("\n");
                    if (edit_text || mailrec) {
                        display_header(th, 0, edit_text, 0, mailrec);
                    }
                    tmpptr = Start;
                    count = 1;
                    while (tmpptr->next) {
                        if (output("%3d:%s\n", count++, tmpptr->line) == -1)
                            break;
                        tmpptr = tmpptr->next;
                    }
                } else if ((strstr(MSG_EDINCL, cmd))
                    && (strlen(cmd) > EDINCLN)) {
                    if (*arg == '\0') {
                        output("\n%s", MSG_FNPROMPT);
                        input("", newname, 40, 0, 0, 0);
                    } else {
                        strcpy(newname, arg);
                    }
                    if (*newname == '\0' || strstr(newname, "..") != NULL) {
                        output("\n%s\n\n", MSG_BADFNAME);
                    } else {
                        output("\n");
                        strcpy(arg, newname);
                        pw = getpwuid(Uid);
                        sprintf(newname, "%s/%s", pw->pw_dir, arg);
                        if ((fd = open(newname, O_RDONLY)) == -1) {
                            output("%s\n\n", MSG_BADFNAME);
                        } else {
                            cptr = read_file(fd);
                            buflen = file_size(fd);
                            space = ptr->line;
                            ll = 0;
                            while (buflen) {
                                if (*cptr == '\n') {
                                    ll = 0;
                                    *space = '\0';
                                    numlines++;
                                    Size++;
                                    ptr->next = (struct EDIT_BUF *)
                                        malloc(sizeof(struct EDIT_BUF));
                                    if (ptr->next == NULL) {
                                        output("%s\n", MSG_OUTMEM);
                                        exec_logout(0);
                                    }
                                    tmpptr = ptr;
                                    ptr = ptr->next;
                                    ptr->previous = tmpptr;
                                    ptr->next = NULL;
                                    ptr->line[0] = '\0';
                                    space = ptr->line;
                                    cptr++;
                                } else {
                                    *space = *cptr;
                                    if (ll < 74)
                                        space++;
                                    cptr++;
                                    ll++;
                                }
                                buflen--;
                            }
                            *space = '\0';
                            close_file(fd);
                            output("%s\n\n", MSG_FILEINC);
                        }
                    }
                } else if (strstr(MSG_EDDEL, cmd) && (strlen(cmd) > EDDELN)) {
                    edit_line = atoi(arg);
                    if ((edit_line > 0) && (edit_line < numlines)) {
                        tmpptr = Start;
                        for (count = 1; count < edit_line; count++) {
                            tmpptr2 = tmpptr;
                            tmpptr = tmpptr->next;
                        }
                        if (tmpptr == Start) {
                            Start = tmpptr->next;
                            tmpptr2 = tmpptr->next;
                            tmpptr2->previous = Start;
                        } else {
                            tmpptr2->next = tmpptr->next;
                            tmpptr2 = tmpptr->next;
                            tmpptr2->previous = tmpptr->previous;
                        }
                        free(tmpptr);
                        numlines--;
                    } else
                        output("\n%s\n\n", MSG_BADLINE);
                } else if (strstr(MSG_EDINS, cmd) && (strlen(cmd) > EDINSN)) {
                    edit_line = atoi(arg);
                    if ((edit_line > 0) && (edit_line < numlines)) {
                        tmpptr = Start;
                        for (count = 1; count < edit_line; count++) {
                            tmpptr2 = tmpptr;
                            tmpptr = tmpptr->next;
                        }
                        if (tmpptr == Start) {
                            if ((tmpptr2 = (struct EDIT_BUF *) malloc
                                    (sizeof(struct EDIT_BUF))) == NULL) {
                                sys_error("line_ed", 2, "malloc");
                                return NULL;
                            }
                            strcpy(tmpptr2->line, "");
                            Start->previous = tmpptr2;
                            tmpptr2->next = Start;
                            tmpptr2->previous = NULL;
                            Start = tmpptr2;
                        } else {
                            if ((tmpptr3 = (struct EDIT_BUF *) malloc
                                    (sizeof(struct EDIT_BUF))) == NULL) {
                                sys_error("line_ed", 2, "malloc");
                                return NULL;
                            }
                            strcpy(tmpptr3->line, "");
                            tmpptr2->next = tmpptr3;
                            tmpptr->previous = tmpptr3;
                            tmpptr3->previous = tmpptr2;
                            tmpptr3->next = tmpptr;
                        }
                        numlines++;
                    } else
                        output("\n%s\n\n", MSG_BADLINE);
                } else if ((strstr(MSG_EDCHANGE, cmd))
                    && (strlen(cmd) > EDCHANGEN)) {
                    edit_line = atoi(arg);
                    if (edit_line > 0) {
                        if (edit_line < numlines) {
                            tmpptr = Start;
                            for (count = 1; count < edit_line; count++)
                                tmpptr = tmpptr->next;
                            output("%3d:%s\n", edit_line, tmpptr->line);
                            output("%3d:", edit_line);
                            input(tmpptr->line, tmpptr->line, 77, 0, 0, 0);
                        } else {
                            output("\n%s\n\n", MSG_BADLINE);
                        }
                    } else if (edit_text || mailrec) {
                        output("\n%s", MSG_SUBJECT);
                        input(th->subject, th->subject, SUBJECT_LEN, 0, 0, 0);
                        output("\n");
                    }
                } else if (strstr(MSG_EDQUOTE, cmd) && (edit_text || mailrec)
                    && (strlen(cmd) > EDQUOTEN)) {
                    if (th->comment_num) {
                        if (Current_conf)
                            sprintf(newname, "%s/%d/%ld", SKLAFF_DB,
                                Current_conf, th->comment_num);
                        else                       
			    snprintf(newname, sizeof(newname), "%.60s/%ld", Mbox, th->comment_num); /* modified on 2025-07-12, PL */
                        fd = open_file(newname, 0);
                        buf = read_file(fd);
                        oldbuf = buf;
                        close_file(fd);
                        buf = get_text_entry(buf, &te);
            		tb = te.body;
			/* Some new code below to prevent segfault when quoting Usenet posts, 2025-08-15 PL */

		    if (!tb) { /* No body found -> nothing to quote */
    			output("\n%s\n\n", MSG_NOBODYQUOTE);
    			goto done_quote;
		    }
		    if (!te.th.author) {
    			struct TEXT_BODY *scan = tb;     
    			while (scan && scan->line[0] != '\0') {
        		scan = scan->next;
    		    }
    		    if (scan && scan->next) {
        		tb = scan->next;
    		    }
		}
			qt = 0;
			output("\n%s\n\n", MSG_QUSE);

			while (tb) {
    			const char *qline = tb->line;
    			output("> %s", qline);
    			Lines = 1;

    		    if (!qt) {
        		do c2 = getc(stdin); while (c2 == 255);
    		    	} else {
        		c2 = MSG_YESANSWER;
    		        }
    		    if ((c2 > 0x40) && (c2 < 0x5e)) {
        		c2 = c2 + 0x20;
			}
    			output("\n");

    		    if (c2 == 'q')
        		break;
    		    if (c2 == 'a') {
        		qt = 1;
        		c2 = MSG_YESANSWER;
    		        }

    		    if (c2 == MSG_YESANSWER) {
        		Lines = 1;
        		strcpy(ptr->line, "> ");
        		strncat(ptr->line, qline, 71);
        		ptr->line[73] = '\0';
        		numlines++;
        		Size++;
        	    if ((ptr->next = (struct EDIT_BUF *)malloc(sizeof(struct EDIT_BUF))) == NULL) {
            		sys_error("line_ed", 2, "malloc");
            		break;
        		}
        		tmpptr = ptr;
        		ptr = ptr->next;
        		ptr->previous = tmpptr;
        		ptr->next = NULL;
        		ptr->line[0] = '\0';
    			}

    			tb = tb->next;
		    }

			done_quote: ;
			output("\n");
			free_text_entry(&te);
			free(oldbuf);
                    } else {
                        output("\n%s\n\n", MSG_NOREPLY);
                    }
                } else if (strstr(MSG_EDREVIEW, cmd) && (edit_text || mailrec)
                    && (strlen(cmd) > EDREVIEWN)) {
                    tn = atol(arg);
                    if (!tn) {
                        tn = th->comment_num;
                    }
                    if (tn) {
                        display_text(Current_conf, tn, 0, 0);
                        Cont = 0;
                    } else {
                        output("\n%s\n\n", MSG_NOREPLY);
                    }
                } else if (strstr(MSG_EDMOVE, cmd) && edit_text
                    && (strlen(cmd) > EDMOVEN) && nc) {
                    if (strlen(arg)) {
                        confname = expand_name(arg, CONF, 0, NULL);
                        if (confname) {
                            newconf = conf_num(confname);
#ifndef POSTING_OK
                            ce = get_conf_struct(newconf);
                            if (ce->type == NEWS_CONF) {
                                newconf = -1;
                            }
#endif
                            if (newconf == -1) {
                                output("\n%s\n\n", MSG_NONEWS);
                            } else if (!newconf) {
                                output("\n%s\n\n", MSG_NOPOSTMBOX);
                            } else if (member_of(Uid, newconf)) {
                                th->comment_conf = origconf;
                                edit_text = newconf;
                                *nc = newconf;
                                if (*nc == th->comment_conf) {
                                    th->comment_conf = 0;
                                }
                                output("\n%s%s\n\n", MSG_MOVED, confname);
                            } else
                                output("\n%s\n\n", MSG_NOSUB);
                        }
                    } else
                        output("\n%s\n\n", MSG_NOCONFNAME);
                } else if (strstr(MSG_EDREMOVE, cmd)
                    && (strlen(cmd) > EDREMOVEN)) {
                    if (numlines > 1) {
                        output("\n%s ", MSG_REMTEXT);
                        input("", waste, 4, 0, 0, 0);
                        down_string(waste);
                    } else {
                        strcpy(waste, MSG_YES2);
                        output("\n");
                    }
                    if (*waste == MSG_YESANSWER) {
                        ptr = Start;
                        while (ptr) {
                            tmpptr = ptr;
                            ptr = ptr->next;
                            free(tmpptr);
                        }
                        return NULL;
                    } else {
                        ptr->line[len] = '\0';
                        output("\n");
                    }
                    strcpy(waste, "");
                } else if ((strstr(MSG_EDSAVE, cmd)
                            && (strlen(cmd) > EDSAVEN))
                        || (strstr(MSG_EDPOST, cmd)
                        && (strlen(cmd) > EDPOSTN))) {
                    marcel = 1;
#ifndef POSTING_OK
                    if (nc) {
                        ce = get_conf_struct(*nc);
                        if (ce->type == NEWS_CONF) {
                            output("\n%s\n\n", MSG_NOSAVE);
                            marcel = 0;
                        }
                    }
#endif
                    if (marcel) {
                        if (numlines > 1) {
                            ptr = ptr->previous;
                            free(ptr->next);
                            ptr->next = NULL;
                        }
                        numlines--;
                        Size--;
                        goto save;
                    }
                } else if (strstr(MSG_EDTELL, cmd) && (strlen(cmd) > EDTELLN)
                    && allow_say) {
                    cmd_say(upcasearg);
                } else if (strstr(MSG_EDYELL, cmd) && (strlen(cmd) > EDYELLN)
                    && allow_say) {
                    cmd_yell(upcasearg);
                } else if (strstr(MSG_EDYELL2, cmd) && (strlen(cmd) > EDYELL2N)
                    && allow_say) {
                    cmd_yell(upcasearg);
                } else if (strstr(MSG_EDWHO, cmd) && (strlen(cmd) > EDWHO)) {
                    cmd_who(arg);
                } else if (strstr(MSG_EDI, cmd) && (strlen(cmd) > EDI)) {
                    cmd_I(upcasearg);
                } else if (strstr(MSG_EDMY, cmd) && (strlen(cmd) > EDMY)) {
                    cmd_my(upcasearg);
                } else {
                    output(MSG_EDHELP0);
                    output(MSG_EDHELP1);
                    output(MSG_EDHELP2);
                    output(MSG_EDHELP3);
                    output(MSG_EDHELP4);
                    output(MSG_EDHELP5);
                    if (allow_say)
                        output(MSG_EDHELP6);
                    if (allow_say)
                        output(MSG_EDHELP7);
                    if (edit_text) {
                        output(MSG_EDHELP8);
                        output(MSG_EDHELP9);
                        output(MSG_EDHELP9p5);
                        if (nc) {
                            output(MSG_EDHELP10);
                        }
                    }
                    output(MSG_EDHELP11);
                    output(MSG_EDHELP12);
                    output(MSG_EDHELP13);
                    output(MSG_EDHELP14);
                    output(MSG_EDHELP15);
                    output(MSG_EDHELP16);
                    output(MSG_EDHELP17);
                    output(MSG_EDHELP18);
                    if (th->type == TYPE_SURVEY) {
                        output(MSG_SURVHELP1);
                        output(MSG_SURVHELP2);
                        output(MSG_SURVHELP3);
                        output(MSG_SURVHELP4);
                        output(MSG_SURVHELP5);
                        output(MSG_SURVHELP6);
                    }
                }
            }
        } else {
            numlines++;
            Size++;
            if ((ptr->next = (struct EDIT_BUF *) malloc(sizeof(struct EDIT_BUF))) == NULL) {
                sys_error("line_ed", 2, "malloc");
                return NULL;
            }
            tmpptr = ptr;
            ptr = ptr->next;
            ptr->previous = tmpptr;
            ptr->next = NULL;
            ptr->line[0] = '\0';
            if (waste[0])
                strcpy(ptr->line, waste);
            waste[0] = '\0';
        }
    }

save:
    output("\n");
    signal(SIGHUP, exec_logout);
    critical();
    if ((fd = create_file(fname)) == -1) {
        sys_error("line_ed", 1, "create_file");
        return NULL;
    }
    ptr = Start;
    while (ptr) {
    if (numlines) {
        if (write(fd, ptr->line, strlen(ptr->line)) == -1) {
            perror("write"); /* Modified 2025-07-25 by PL to silence compiler */
            return NULL;
        }
        if (write(fd, "\n", 1) == -1) {
            perror("write"); /* Modified 2025-07-25 by PL to silence compiler */
            return NULL;
        }
    }
        tmpptr = ptr;
        ptr = ptr->next;
        free(tmpptr);
    }
    if (close_file(fd) == -1) {
        sys_error("line_ed", 1, "close_file");
        return NULL;
    }
    non_critical();
    th->time = time(&save_time);
    th->size = numlines;
    return th;
}

/*
 * abort_edit - function called if editing is aborted
 * args: signal received (tmp)
 */

void
abort_edit(int tmp)
{
    struct EDIT_BUF *ptr, *tmpptr;
    LINE fname;
    int fd;

    strcpy(fname, Home);
    strcat(fname, EDIT_FILE);

    if ((fd = create_file(fname)) == -1) {
        sys_error("abort_edit", 1, "create_file");
    }
    ptr = Start;
    while (ptr && (Size > 0)) {
        if (Size == 1) {
            strcpy(ptr->line, "");
        } else {
            if (write(fd, ptr->line, strlen(ptr->line)) == -1) {
                perror("write (abort_edit:line)"); /* Linux compiler happy now 2025-07-25, PL */
            }
            if (write(fd, "\n", 1) == -1) {
                perror("write (abort_edit:newline)"); /* Linux compiler happy now  2025-07-25, PL */
            }
            tmpptr = ptr;
            ptr = ptr->next;
            free(tmpptr);
        }
        Size--;
    }
    if (close_file(fd) == -1) {
        sys_error("abort_edit", 2, "close_file");
    }
    strcpy(fname, Home);
    strcat(fname, DEAD_FILE);
    if ((fd = create_file(fname)) == -1) {
        sys_error("abort_edit", 1, "create_file");
    }
    if (write(fd, &Current_conf, sizeof(int)) == -1) {
        perror("write (abort_edit:conf)"); /* Linux compiler happy now  2025-07-25, PL */
    }
   if (write(fd, Globalth, sizeof(struct TEXT_HEADER)) == -1) {
       perror("write (abort_edit:header)"); /* Linux compiler happy now  2025-07-25, PL */
    }

    if (close_file(fd) == -1) {
        sys_error("abort_edit", 2, "close_file");
    }
    exec_logout(tmp);
}


/*
 * resume_aborted_edit - resume aborted editing
 * ret: no session aborted (1) or ok (0) or error (-1)
 */

int
resume_aborted_edit(void)
{
    int fd, tmp, oldconf, result;
    struct TEXT_HEADER th;
    LINE fname;

    result = 0;
    oldconf = Current_conf;
    strcpy(fname, Home);
    strcat(fname, DEAD_FILE);
    if ((fd = open_file(fname, OPEN_QUIET)) == -1)
        return 1;
    if (read(fd, &tmp, sizeof(int))
        != sizeof(int)) {
        sys_error("resume_aborted_edit",
            1, "read");
        result = -1;
        goto bad;
    }
    if (read(fd, &th, sizeof(struct TEXT_HEADER))
        != sizeof(struct TEXT_HEADER)) {
        sys_error("resume_aborted_edit",
            2, "read");
        result = -1;
        goto bad;
    }
    if (set_conf(tmp) == -1)
        result = -1;

bad:
    close_file(fd);
    unlink(fname);
    strcpy(fname, Home);
    strcat(fname, EDIT_FILE);
    if (result) {
        unlink(fname);
        set_conf(oldconf);
        return result;
    } else {
        output("\n%s\n\n", MSG_EDRECOVER);
        display_header(&th, 0, tmp, 0, NULL);
        if (line_ed(fname, &th, tmp, 1, 1, NULL, NULL)) {
            save_text(fname, &th, tmp);
        } else {
            unlink(fname);
        }
        result = 0;
    }
    set_conf(oldconf);
    return result;
}

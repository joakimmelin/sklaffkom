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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sklaff.h"
#include "ext_globals.h"

#ifdef LINUX
#include <bsd/string.h>
#endif

/*
 * display_header - displays textheader
 * args: pointer to TEXT_HEADER (th), allow editing of subject (edit_subject),
 *       conf/uid (type), absolute date? (dtype)
 */

void
display_header(struct TEXT_HEADER * th, int edit_subject, int type, int dtype, char *mailrec)
{
    LINE time_val, confname; /* + confname for humanized header */
    char username[256];  /* expanded buffer to prevent overflow - 2025-09-14, PL */
	char fname[128];  /* increased from LINE to avoid overflow, modified on 2025-07-12, PL */
    int uid, right, nc, fd;
    char *tmp, *buf, *oldbuf;
    char *ptr = NULL;   /* modified on 2025-07-12, PL */
    struct CONF_ENTRY *ce = NULL;

    if (mailrec && type && (th->author == 0)) {
        strcpy(username, mailrec);
    } else {
        user_name(th->author, username);
        Current_author = th->author;
    }
  /* Humanized headers PL 2025-08-09 */
{
    LINE from_dec, disp;
    rfc2047_decode(username, from_dec, sizeof(from_dec));
    extract_display_name(from_dec, disp, sizeof(disp));
    snprintf(username, sizeof(username), "%.*s", (int)sizeof(username)-1, disp);
}
/* 2025-08-10, PL: strip surrounding quotes from display name */
{
    size_t L__ = strlen(username);
    if (L__ >= 2 && username[0] == '"' && username[L__-1] == '"') {
        username[L__-1] = '\0';
        memmove(username, username + 1, L__ - 1);
    }
}
    if (th->num == 0) {
        output("%s %s\n", MSG_WRITTENBY, username);
    } else {

int used_news_date = 0;
struct CONF_ENTRY *ce = get_conf_struct(Current_conf);
if (ce && ce->type == NEWS_CONF) {
    /* open the stored article and scan headers for "Date:" */
    LINE fname;
    int fd;
    char *buf = NULL, *oldbuf = NULL;
    snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, Current_conf, th->num);
    if ((fd = open_file(fname, OPEN_QUIET)) != -1) {
        if ((buf = read_file(fd)) != NULL) {
            oldbuf = buf;
            /* look only in the header block (up to first blank line) */
            char *hdr_end = strstr(buf, "\n\n");
            size_t hdr_len = hdr_end ? (size_t)(hdr_end - buf) : strlen(buf);
            /* crude but fast: search for "\nDate:" or "Date:" at start */
            char *d = strstr(buf, "\nDate:");
            if (!d && strncasecmp(buf, "Date:", 5) == 0) d = buf - 1; /* so d+1 points to 'D' */
            if (d && (size_t)( (d - buf) + 1 ) < hdr_len) {
                char *line_start = d + 1;
                char *line_end = memchr(line_start, '\n', hdr_len - (line_start - buf));
                if (line_end) *line_end = '\0';
                time_t utc;
                if (parse_usenet_date_utc(line_start, &utc)) {
                    se_time_string(utc, time_val, (dtype | Date));
                    used_news_date = 1;
                }
                if (line_end) *line_end = '\n';
            }
            free(oldbuf);
        }
        close_file(fd);
    }
}

if (!used_news_date) {
    /* fallback: original import timestamp */
    time_string(th->time, time_val, (dtype | Date));
}


/* 2025-08-09, PL: Human-first layout */

if (Current_conf != 0) {
    conf_name(Current_conf, confname);
    output_ansi_fmt("%s " CYAN "%d" DOT " %s " BR_RED "%s " DOT CYAN"%s\n"DOT, "%s %d %s %s %s\n",
        (th->type == TYPE_TEXT) ? MSG_TEXTNAME : MSG_SURVEYNAME,
        th->num, MSG_IN, confname, time_val);

    output_ansi_fmt("%s " BR_YELLOW "%s\n" DOT, "%s %s\n", MSG_WRITTENBY, username);
} else {
        output("%s %d %s %s %s\n",
        (th->type == TYPE_TEXT) ? MSG_TEXTNAME : MSG_SURVEYNAME,
        th->num, MSG_WRITTENBY, username, time_val);
}
    }
    switch (th->size) {
    case 0:
        if (th->num)
            output(" %s\n",
                (th->type == TYPE_TEXT) ? MSG_EMPTYTEXT : MSG_EMPTYSURVEY);
        break;
    case 1:
        //output(" %s\n", MSG_ONELINE);
        break;
    default:
	//output(" %d %s\n", th->size, MSG_LINES);
        break;
    }
    if (th->type == TYPE_SURVEY && (th->num != 0)) {
	time_string(th->sh.time, time_val, (dtype | Date));
        output("%s: %d; %s: %s\n", MSG_NQUESTIONS, th->sh.n_questions,
            MSG_REPORTRESULT, time_val);
    }
    if (th->comment_num) {
        if (th->comment_conf) {
            ce = get_conf_struct(th->comment_conf);
            right = can_see_conf(Uid, th->comment_conf, ce->type, ce->creator);
        } else {
            right = 1;
        }
        if (right) {
            output_ansi_fmt("%s " CYAN "%d " DOT, "%s %d ", MSG_REPLYTO, th->comment_num);
	    nc = th->comment_conf;
            if (!nc)
                nc = Current_conf;
            /* I put this chunk last instead to allow for display of author
             * also for text commented from other conferences. /OR 98-07-29 if
             * (th->comment_conf) { if (!nc) nc = Current_conf; conf_name(nc,
             * username); output ("%s %s\n", MSG_IN, username); } else */
            {
                if (!th->comment_author) {
                    strcpy(username, MSG_UNKNOWNU);
                    if (nc) {
                        sprintf(fname, "%s/%d/%ld", SKLAFF_DB,
                            nc, th->comment_num);
                    } else {
                        snprintf(fname, sizeof(fname), "%s/%ld", Mbox, th->comment_num);  /* modified on 2025-07-12, PL */
                    }
                    if ((fd = open_file(fname, OPEN_QUIET)) != -1) {
                        if ((buf = read_file(fd)) == NULL) {
                            output("\n%s\n\n", MSG_NOREAD);
                            return;
                        }
                        oldbuf = buf;
                        if (close_file(fd) == -1) {
                            return;
                        }
                        ptr = strstr(buf, MSG_EMFROM);
                        if (ptr) {
                            tmp = strchr(ptr, '\n');
                            *tmp = '\0';
                            //strcpy(username, (ptr + strlen(MSG_EMFROM)));
                            strlcpy(username, ptr + 6, sizeof(username));  /* fixed to prevent buffer overflow, 2025-09-14, PL */
							*tmp = '\n';
                        }
                        free(oldbuf);
                    }
                } else {
                    user_name(th->comment_author, username);
                }
		/* 2025-08-09, PL: prefer human name (no email) on follow-up line */
		{
		    char disp[256];
		    extract_display_name(username, disp, sizeof(disp));
		    snprintf(username, sizeof(username), "%.*s", (int)sizeof(username)-1, disp);
		}
		
		/* 2025-08-10, PL: strip surrounding quotes from display name */
		{
		    size_t L__ = strlen(username);
		    if (L__ >= 2 && username[0] == '"' && username[L__-1] == '"') {
		        username[L__-1] = '\0';
		        memmove(username, username + 1, L__ - 1);
		    }
		}
output_ansi_fmt("%s " BR_YELLOW "%s" DOT, "%s %s", MSG_BY, username); /* 2025-08-09, PL: print "av <name>" only once */
		if (th->comment_conf) {
                    conf_name(nc, username);
        	    sprintf(fname, "  showing MSG_IN");
                    debuglog(fname, 6);
                    output(" %s %s\n", MSG_IN, username);
                } else
                    output("\n");

            }
        }
    }
    if (!Current_conf && (th->author == Uid) &&
        (th->time > 0) && th->comment_author &&
        (th->comment_author != Uid) && (!Last_conf)) {
        user_name(th->comment_author, username);
        output("%s %s\n", MSG_COPYTO, username);
    }
    /* 2025-08-09, PL: Conference name is baked into line 1 now */

    if (Current_conf == 0) {
    /* Mailbox: keep "Mottagare:" logic unchanged */
    if (mailrec && !type) {
        output("%s %s\n", MSG_RECIPIENT, mailrec);
    } else {
        if (type < 0) {
            uid = -type;
            user_name(uid, username);
        } else {
            conf_name(type, username);
        }
        output("%s %s\n", MSG_RECIPIENT, username);
        }
    }
    //* decoded subject + trying to match underline everywhere WORK IN PROGRESS PL 2025-08-10*/
    if (edit_subject) {
    output(MSG_SUBJECT);
    input(th->subject, th->subject, SUBJECT_LEN, 0, 0, 0);
    } else {
    const char *raw_label = MSG_SUBJECT; /* 2025-08-10, PL: safe default to avoid NULL */
    LINE subj_dec, label_norm, subj_line;

    rfc2047_decode(th->subject, subj_dec, sizeof(subj_dec));
    normalize_label(raw_label, label_norm, sizeof(label_norm));

    //snprintf(subj_line, sizeof(subj_line), "%s%s", label_norm, subj_dec); //TO BE REMOVED
	subj_line[0] = '\0';
	strlcpy(subj_line, label_norm, sizeof(subj_line));        /* fixed on 2025-09-15, PL */
	strlcat(subj_line, subj_dec, sizeof(subj_line));          /* fixed on 2025-09-15, PL */
print_underlined_line(subj_line);  /* prints line + perfectly matching dashes (soon ;)) */
    }
}

/* humanized file sizes in cmd_list_files 2025-09-28 PL */
void human_size(off_t bytes, char *out, size_t outsz)
{
    const double b = (double)bytes;
    if (bytes < 1024) {
        snprintf(out, outsz, "%lldB", (long long)bytes);
    } else if (bytes < (1LL<<20)) {           // < 1 MiB
        snprintf(out, outsz, "%.1fK", b / 1024.0);
    } else if (bytes < (1LL<<30)) {           // < 1 GiB
        snprintf(out, outsz, "%.1fM", b / 1048576.0);
    } else {
        snprintf(out, outsz, "%.1fG", b / 1073741824.0);
    }
}


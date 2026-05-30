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
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <sys/wait.h>
#ifdef LINUX
#include <bsd/string.h>  /* for strlcat on Linux */
#endif
#include "sklaff.h"
#include "ext_globals.h"





/* Two Little helpers to avoid extra '(') 2025-08-26 PL */
void clear_prompt(int num) 
{
    int x;
    output("\r");
    for (x = 0; x < num; x++)
        output(" ");
    output("\r");
}

void clear_prompt_cols(int cols)
{
    output_ansi_fmt("\r\033[0K", "\r");
        if (!Ansi_output) {
        int i;
        for (i = 0; i < cols; i++)
            output(" ");
        output("\r");
    }
}

/* Count quote depth: skip leading spaces, then count '>' allowing optional single
 * space after each '>' so it matches ">>", "> >", "> > >", etc.
 */
int quote_depth(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    while (*p == ' ' || *p == '\t') p++;
    int d = 0;
    while (*p == '>') {
        d++;
        p++;
        if (*p == ' ') p++;   /* tolerate one space after each '>' */
    }
    return d;
}

/*
 * normalize_label - normalize a header label to one trailing colon and space
 * args: raw label (raw), output buffer (norm), output buffer length (nlen)
 * ret: nothing
 */
void normalize_label(const char *raw, char *norm, size_t nlen)
{
    size_t L = raw ? strlen(raw) : 0;
    int ends_with_colon = (L > 0 && raw[L-1] == ':');
    snprintf(norm, nlen, "%s%s", raw ? raw : "", ends_with_colon ? " " : ": ");
}

/*
 * get_wallclock_localtime - little helper to show correct time at all times (2025-08-16 PL)
 */

void get_wallclock_localtime(const time_t *t, struct tm *out)
{
    char *saved = NULL;
    const char *tz = getenv("TZ");
    if (tz && tz[0]) {
        saved = strdup(tz);            
    }

    unsetenv("TZ");                    
    tzset();

    {
        struct tm *tmp = localtime(t); 
        if (tmp) *out = *tmp;
    }

    if (saved) {
        setenv("TZ", saved, 1);       
        free(saved);
    } else {
        unsetenv("TZ");                
    }
    tzset();
}

int b64v(int c)  /* Base64 table */
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}









/* output_body_line - outputs a body line with ANSI color if enabled
 * 2025-09-24, PL
 */

int output_body_line(const char *line, const char *col)
{
    if (Ansi_output) {
        return output_ansi_fmt("%s%s\x1b[0m\n", "%s\n", col, line);
    } else {
        //if (output((char *)line) == -1 || output("\n") == -1)
		if (output("%s", line) == -1 || output("\n") == -1)        
    return -1;
    }
    return 0;
}

/*
 * run_external_cmd_args - runs an external program with argument list
 * argv[0] = command to run, argv[1..n] = arguments, NULL-terminated
 * use_fallback: whether to run 'which' on argv[0] if not found
 * 2025-09-17, PL
 */
int run_external_cmd_args(const char *argv[], int use_fallback)
{
    sigset_t sigmask, oldsigmask;
    char exe_path[256];
    FILE *which_fp;
    char which_buf[256];

    if (!argv || !argv[0])
        return 1;

    strlcpy(exe_path, argv[0], sizeof(exe_path));

    if (access(exe_path, X_OK) != 0 && use_fallback) {
        const char *base = strrchr(argv[0], '/');
        if (base)
            snprintf(which_buf, sizeof(which_buf), "which %s", base + 1);
        else
            snprintf(which_buf, sizeof(which_buf), "which %s", argv[0]);

        which_fp = popen(which_buf, "r");
        if (which_fp && fgets(which_buf, sizeof(which_buf), which_fp)) {
            which_buf[strcspn(which_buf, "\n")] = '\0';
            if (access(which_buf, X_OK) == 0)
                strlcpy(exe_path, which_buf, sizeof(exe_path));
        }
        if (which_fp)
            pclose(which_fp);
    }

  if (access(exe_path, X_OK) != 0) {
    output("\nFel: Kan inte starta spelet eller scriptet - meddela Sysop!\n");
    output("  Försökte köra: %s\n", exe_path);

    if (access(exe_path, F_OK) != 0) {
        output("  Filen finns inte (ENOENT).\n");
    } else {
        output("  Filen finns, men är inte körbar (EACCES eller liknande).\n");
    }

    return 1;
    }

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    signal(SIGNAL_NEW_TEXT, SIG_IGN);
    signal(SIGNAL_NEW_MSG, SIG_IGN);
    set_avail(Uid, 1);

    if (!fork()) {
        sig_reset();
        tty_reset();
        execvp(exe_path, (char *const *)argv);
        perror("execvp");
        _exit(1);
    } else {
        wait(NULL);
    }

    signal(SIGNAL_NEW_TEXT, baffo);
    signal(SIGNAL_NEW_MSG, newmsg);
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
    tty_raw();
    output("\n");
    set_avail(Uid, 0);

    return 0;
}
/*
 * display_langfile - display language-aware file
 * Updated 2025-09-15 by PL
 */
void
display_langfile(const char *base, const char *base_eng, const char *base_swe)
{
    int fd;
    char *buf = NULL;
    const char *filename = NULL;

/* Debug */
/*
output("DEBUG: language = ");

#ifdef ENGLISH
    output("ENGLISH\n");
#elif defined(SWEDISH)
    output("SWEDISH\n");
#else
    output("UNKNOWN\n");
#endif
*/
/* END OF DEBUG */

#ifdef ENGLISH
    if (file_exists(base_eng) != -1)
        filename = base_eng;
#elif defined(SWEDISH)
    if (file_exists(base_swe) != -1)
        filename = base_swe;
#endif

    if (!filename && file_exists(base) != -1)
        filename = base;

    if (filename) {
        if ((fd = open_file(filename, OPEN_QUIET)) == -1)
            return;
        if ((buf = read_file(fd)) == NULL)
            return;
        if (close_file(fd) == -1)
            return;
        output("%s", buf);
        free(buf);
    }
}


void clear_screen(void)
{
    output(ANSI_CLS);  /* or printf(ANSI_CLS); */
    fflush(stdout);
    Lines = 1;
}

void display_news(void)
{
    display_langfile(NEWS_FILE, NEWS_FILE_ENG, NEWS_FILE_SWE);
}

void display_logout(void)
{
    display_langfile(LOGOUT_FILE, LOGOUT_FILE_ENG, LOGOUT_FILE_SWE);
}

const char *
month_name(int mon)
{
    static const char *months[] = {
        "januari", "februari", "mars", "april", "maj", "juni",
        "juli", "augusti", "september", "oktober", "november", "december"
    };
    if (mon >= 0 && mon <= 11)
        return months[mon];
    return "okänd";
}


void
chomp(char *s)
{
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}
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

/* clamp_nonneg() — return v clamped to >= 0 */
/* Used in the improved "list_confs" function */
/* added 2025-10-02, PL */
long
clamp_nonneg(long v)
{
    return (v < 0) ? 0 : v;
}


















/*
 * Converts unix-time to human-time format
*/
const char *
time_string_static(time_t t)
{
    static char buf[64];
    time_string(t, buf, 0);
    return buf;
}

/*
 * get text author (small hack 2025-10-25 PL)
*/

int get_text_author(int conf, long num)
{
    char fname[PATH_MAX];
    int fd;
    char *buf;
    struct TEXT_ENTRY te;
    int author = 0;

    snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, conf, num);

    if ((fd = open_file(fname, OPEN_QUIET)) == -1)
        return 0;
    if ((buf = read_file(fd)) == NULL) {
        close_file(fd);
        return 0;
    }
    close_file(fd);

    char *p = get_text_entry(buf, &te);
    free(buf);
    if (!p)
        return 0;

    author = te.th.author;
    free_text_entry(&te);
    return author;
}

/*                                                                              
* has_file_area - checks if conference has files               
*/ 
int 
has_file_area(int confnum) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%d%s", FILE_DB, confnum, INDEX_FILE);
    return file_exists(path) != -1;
}

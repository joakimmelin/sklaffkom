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

#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sklaff.h"

struct ConfLikeSummary {
    long textnum;
    int count;
    int author;
};

/*
 * get_user_likes - build a list of texts praised by a user
 *
 * Scans all conference extra-data files and returns a linked list of
 * LikeEntry records for texts praised by the specified user.
 *
 * The returned list must be released with free_like_list().
 */
struct LikeEntry *get_user_likes(int uid)
{
    DIR *d;
    struct dirent *de;
    struct LikeEntry *head = NULL, *tail = NULL;

    d = opendir(SKLAFF_DB);
    if (!d) return NULL;

    while ((de = readdir(d))) {
        if (!isdigit(de->d_name[0])) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s%s", SKLAFF_DB, de->d_name, CONFXTRA_FILE);

        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "![hiss]", 7) == 0) continue;
            int u; long tnum; long ts;
            if (sscanf(line, "%d:%ld:%ld", &u, &tnum, &ts) == 3 && u == uid) {
                struct LikeEntry *le = malloc(sizeof(struct LikeEntry));
                if (!le) break;
                le->confnum = atoi(de->d_name);
                le->textnum = tnum;
                le->timestamp = ts;
                le->next = NULL;
                if (!head) head = le;
                else tail->next = le;
                tail = le;
            }
        }
        fclose(fp);
    }
    closedir(d);
    return head;
}

/*
 * free_like_list - release a linked list of LikeEntry records
 */
void free_like_list(struct LikeEntry *list)
{
    while (list) {
        struct LikeEntry *n = list->next;
        free(list);
        list = n;
    }
}

/*
 * get_conf_likes - build a list of praised texts in a conference
 *
 * Reads the conference extra-data file and returns a linked list of
 * LikeEntry records for all praise entries in the given conference.
 *
 * The returned list must be released with free_like_list().
 */
struct LikeEntry *get_conf_likes(int confnum)
{
    FILE *fp;
    char path[PATH_MAX];
    char line[256];
    struct LikeEntry *head = NULL, *tail = NULL;

    snprintf(path, sizeof(path), "%s/%d%s", SKLAFF_DB, confnum, CONFXTRA_FILE);

    fp = fopen(path, "r");
    if (!fp) return NULL;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "![hiss]", 7) == 0) continue;

        int u;
        long tnum, ts;
        if (sscanf(line, "%d:%ld:%ld", &u, &tnum, &ts) == 3) {
            struct LikeEntry *le = malloc(sizeof(struct LikeEntry));
            if (!le) break;
            le->confnum = confnum;
            le->textnum = tnum;
            le->timestamp = ts;
            le->next = NULL;
            if (!head)
                head = le;
            else
                tail->next = le;
            tail = le;
        }
    }

    fclose(fp);
    return head;
}

/*
 * compare_textnum - compare praise summaries by text number
 */
static int compare_textnum(const void *a, const void *b)
{
    const struct ConfLikeSummary *x = (const struct ConfLikeSummary *)a;
    const struct ConfLikeSummary *y = (const struct ConfLikeSummary *)b;

    if (x->textnum < y->textnum) return -1;
    if (x->textnum > y->textnum) return 1;
    return 0;
}

/*
 * get text author (small hack 2025-10-25 PL)
*/

static int 
get_text_author(int conf, long num)
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
 * show_conf_likes - display praised texts in a conference
 *
 * Collects all praise entries for the conference, summarizes them per
 * text, sorts the result by text number, and prints a compact list.
 */
void show_conf_likes(int confnum)
{
    struct LikeEntry *likes = get_conf_likes(confnum);
    if (!likes) return;

    struct ConfLikeSummary *summaries = NULL;

    int cap = 10, len = 0;
    summaries = malloc(cap * sizeof(*summaries));
    if (!summaries) return;

    for (struct LikeEntry *l = likes; l; l = l->next) {
        int found = 0;
        for (int i = 0; i < len; ++i) {
            if (summaries[i].textnum == l->textnum) {
                summaries[i].count++;
                found = 1;
                break;
            }
        }
        if (!found) {
            if (len == cap) {
                cap *= 2;
                summaries = realloc(summaries, cap * sizeof(*summaries));
                if (!summaries) return;
            }
            summaries[len].textnum = l->textnum;
            summaries[len].count = 1;
            summaries[len].author = get_text_author(confnum, l->textnum);
            len++;
        }
    }

    qsort(summaries, len, sizeof(*summaries), compare_textnum);

    output(MSG_CONFPRAISES"\n\n");

    LINE tmpstr;
    for (int i = 0; i < len; ++i) {
        user_name(summaries[i].author, tmpstr);
        output("Text %ld av %s — %d %s\n",
               summaries[i].textnum,
               tmpstr,
               summaries[i].count,
               summaries[i].count == 1 ? MSG_PRAISE : MSG_PRAISES);
    }
    output("\n");
    free(summaries);
    free_like_list(likes);
}

/*
 * show_likes_block - display praise count for a text
 *
 * Prints the small praise footer shown after a text, but only when the
 * text has at least one praise entry.
 */
void
show_likes_block(int conf, long num)
{
    if (conf <= 0)
        return;

    struct LikeEntry *likes = get_conf_likes(conf);
    int likecount = 0;

    for (struct LikeEntry *l = likes; l; l = l->next) {
        if (l->textnum == num)
            likecount++;
    }

    if (likecount > 0) {
         output_ansi_fmt("\n%s " CYAN "%d" DOT " %s\n",
                    "\n%s %d %s\n",
                    MSG_PRAISEDBY,
                    likecount,
                    (likecount == 1) ? MSG_PERSON : MSG_PERSONS);
    }

    free_like_list(likes);
}

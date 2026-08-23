/* buf.c */

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

/*
 * get_user_entry - gets USER_ENTRY from buffer
 * args: buffer (buf), pointer to USER_ENTRY (ue)
 * ret: next position in buffer or NULL
 */

char *
get_user_entry(char *buf, struct USER_ENTRY * ue)
{
    char *ptr, *str;

    memset(ue->name, 0, LINE_LEN);
    ue->num = (int) strtol(buf, &str, 10);
    if (str == buf) {
        return NULL;
    }
    ptr = strchr(buf, ':') + 1;
    ue->last_session = atol(ptr);
    ptr = strchr(ptr, ':') + 1;
    str = strchr(ptr, '\n');
    *str = 0;
    strcpy(ue->name, ptr);
    *str = '\n';
    rtrim(ue->name);
    buf = str;
    buf++;
    return buf;
}

/*
 * get_file_entry - gets FILE_ENTRY from buffer
 * args: buffer (buf), pointer to FILE_ENTRY (fe)
 * ret: next position in buffer or NULL
 */

/* This function was re-written by PL 2025-07-25 to prevent segfault and for increased portability */
char *
get_file_entry(char *buf, struct FILE_ENTRY *fe)
{
    char *sep, *newline;

    sep = strchr(buf, ':');
    if (!sep)
        return NULL;
    *sep = '\0'; // Temporarily null-terminate filename
    strncpy(fe->name, buf, sizeof(fe->name));
    fe->name[sizeof(fe->name)-1] = '\0'; // Ensure null-termination
    *sep = ':'; // Restore original char

    newline = strchr(sep, '\n');
    if (!newline)
        return NULL;

    *newline = '\0'; // Temporarily null-terminate desc
    strncpy(fe->desc, sep + 1, sizeof(fe->desc));
    fe->desc[sizeof(fe->desc)-1] = '\0'; // Ensure null-termination
    *newline = '\n'; // Restore newline

    buf = newline;
    while (*buf && *buf == '\n')
        buf++;
    return buf;
}

/*
 * get_conf_entry - get CONF_ENTRY from buffer
 * args: buffer (buf), pointer to CONF_ENTRY (ce)
 * ret: next position in buffer or NULL
 */

char *
get_conf_entry(char *buf, struct CONF_ENTRY * ce)
{
    char *ptr, *str;

    ce->num = strtol(buf, &str, 10);
    if (str == buf) {
        return NULL;
    }
    ptr = strchr(buf, ':') + 1;
    ce->last_text = atol(ptr);
    ptr = strchr(ptr, ':') + 1;
    ce->creator = atoi(ptr);
    ptr = strchr(ptr, ':') + 1;
    ce->time = atol(ptr);
    ptr = strchr(ptr, ':') + 1;
    ce->type = atoi(ptr);
    ptr = strchr(ptr, ':') + 1;
    ce->life = atoi(ptr);
    ptr = strchr(ptr, ':') + 1;
    ce->comconf = atoi(ptr);
    ptr = strchr(ptr, ':') + 1;
    str = strchr(ptr, '\n');
    *str = 0;
    strcpy(ce->name, ptr);
    *str = '\n';
    buf = str + 1;
    return buf;
}

/*
 * get_parse_entry - get PARSE_ENTRY from buffer
 * args: buffer (buf), pointer to PARSE_ENTRY (pe)
 * ret: next position in buffer or NULL
 */

char *
get_parse_entry(char *buf, struct PARSE_ENTRY *pe)
{
    int res;
    LINE group;

    memset(pe->cmd, 0, sizeof(pe->cmd));
    memset(pe->func, 0, sizeof(pe->func));
    memset(pe->help, 0, sizeof(pe->help));

    /*
     * Backwards compatibility:
     * old three-field parse entries default to Other.
     */
    pe->group = 'O';

    for (;;) {
        memset(group, 0, sizeof(group));

        res = sscanf(buf,
            "%[^:#\n]:%[^#:\n]:%[^#:\n]:%[^#:\n]",
            pe->cmd, pe->func, pe->help, group);

        if (res == -1)
            return NULL;

        rtrim(pe->cmd);
        rtrim(pe->func);
        rtrim(pe->help);

        /*
         * Fourth field is optional while the parse files
         * are being converted.
         */
        if (res >= 4) {
            ltrim(group);
            rtrim(group);

            if (*group)
                pe->group =
                    (char)toupper((unsigned char)group[0]);
        }

        buf = strchr(buf, '\n');

        while (buf && (*buf == '\n'))
            buf++;

        if (res || (buf == NULL))
            return buf;
    }
}

/*
 * get_confrc_users - get USER_LIST from buffer
 * args: buffer (buf)
 * ret: USER_LIST or NULL
 */

struct USER_LIST *
get_confrc_users(char *buf)
{
    int itmp;
    struct USER_LIST *tmp, *top;
    char *ptr;

    tmp = NULL;
    top = NULL;
    for (;;) {
        ptr = strchr(buf, '\n');
        if (ptr == NULL) {
            break;
        }
        itmp = atoi(buf);
        if (tmp == NULL) {
            tmp = (struct USER_LIST *) malloc(sizeof(struct USER_LIST));
            if (tmp == NULL) {
                sys_error("get_confrc_entry", 1, "malloc");
                return NULL;
            }
            tmp->num = itmp;
            tmp->next = NULL;
            top = tmp;
        } else {
            tmp->next = (struct USER_LIST *)
                malloc(sizeof(struct USER_LIST));
            if (tmp->next == NULL) {
                sys_error("get_confrc_entry", 2, "malloc");
                return NULL;
            }
            tmp = tmp->next;
            tmp->num = itmp;
            tmp->next = NULL;
        }
        buf = ptr + 1;
    }
    return top;
}

/*
 * get_text_entry - get TEXT_ENTRY from buffer
 * args: buffer (buf), pointer to TEXT_ENTRY (te)
 * ret: next position in buffer or NULL
 */

char *
get_text_entry(char *buf, struct TEXT_ENTRY * te)
{
    int i, itmp2, c;
    long itmp1;
    char *run, *str, *ptr, triplet[4], *eol1;
    struct TEXT_BODY *tmp_body;
    struct COMMENT_LIST *tmp_cl;

    te->th.num = atol(buf);
    ptr = strchr(buf, ':') + 1;
    te->th.author = atoi(ptr);
    ptr = strchr(ptr, ':') + 1;
    te->th.time = atol(ptr);
    ptr = strchr(ptr, ':') + 1;
    te->th.comment_num = atol(ptr);
    ptr = strchr(ptr, ':') + 1;
    te->th.comment_conf = atoi(ptr);
    ptr = strchr(ptr, ':') + 1;
    te->th.comment_author = atoi(ptr);
    ptr = strchr(ptr, ':') + 1;
    te->th.size = atoi(ptr);
    eol1 = strchr(ptr, '\n');
    ptr = strchr(ptr, ':');     /* Be compatible with old text format */
    te->th.type = TYPE_TEXT;
    if (ptr && ptr < eol1 && te->th.author != 0) {
        te->th.type = atoi(ptr + 1);
        if (te->th.type == TYPE_SURVEY) {
            ptr = strchr(ptr + 1, ':') + 1;
            te->th.sh.n_questions = atoi(ptr);
            ptr = strchr(ptr, ':') + 1;
            te->th.sh.time = atol(ptr);
        }
    }
    te->body = NULL;
    tmp_body = NULL;
    te->cl = NULL;
    tmp_cl = NULL;
    buf = strchr(buf, '\n');
    if (!buf)
        return buf;
    buf++;
    memset(te->th.subject, 0, LINE_LEN);
    run = te->th.subject;
    c = 1;
    while (*buf != '\n') {
        if (c < SUBJECT_LEN) {
            if (*buf == '=') {
                triplet[0] = *buf;
                buf++;
                if (*buf != '\n') {
                    triplet[1] = *buf;
                    buf++;
                    if (*buf != '\n') {
                        triplet[2] = *buf;
                        triplet[3] = '\0';
                        if (!strcmp(triplet, "=C5"))
                            *buf = ']';
                        else if (!strcmp(triplet, "=C4"))
                            *buf = '[';
                        else if (!strcmp(triplet, "=D6"))
                            *buf = '\\';
                        else if (!strcmp(triplet, "=E5"))
                            *buf = '}';
                        else if (!strcmp(triplet, "=E4"))
                            *buf = '{';
                        else if (!strcmp(triplet, "=F6"))
                            *buf = '|';
                        else if (!strcmp(triplet, "=3D"))
                            *buf = '=';
                        else if (!strcmp(triplet, "=8F"))
                            *buf = ']';
                        else if (!strcmp(triplet, "=8E"))
                            *buf = '[';
                        else if (!strcmp(triplet, "=99"))
                            *buf = '\\';
                        else if (!strcmp(triplet, "=86"))
                            *buf = '}';
                        else if (!strcmp(triplet, "=85"))
                            *buf = '{';
                        else if (!strcmp(triplet, "=94"))
                            *buf = '|';
                        else if (!strcmp(triplet, "=FC"))
                            *buf = 'u';
                        else if (!strcmp(triplet, "=DF"))
                            *buf = 's';
                        else if (!strcmp(triplet, "=91"))
                            *buf = '`';
                        else if (!strcmp(triplet, "=92"))
                            *buf = '\'';
                        else if (!strcmp(triplet, "=E9"))
                            *buf = 'e';
                        else if (!strcmp(triplet, "=20"))
                            *buf = ' ';
                        else {
                            buf--;
                            buf--;
                        }
                    } else {
                        buf--;
                        buf--;
                    }
                } else
                    buf--;
            }
            *run = *buf;
            run++;
        }
        buf++;
        c++;
    }
    *run = '\0';
    buf++;
    rtrim(te->th.subject);
    if (te->th.size > 0) {
        te->body = (struct TEXT_BODY *) malloc(sizeof(struct TEXT_BODY));
        if (te->body == NULL) {
            sys_error("get_text_entry", 1, "malloc");
            return NULL;
        }
        tmp_body = te->body;
        te->body->next = NULL;
    }
    for (i = 1; i <= te->th.size; i++) {
        run = te->body->line;
        c = 1;
        while (*buf != '\n') {
            if (c < LINE_LEN) {
                if (*buf == '=') {
                    triplet[0] = *buf;
                    buf++;
                    if (*buf != '\n') {
                        triplet[1] = *buf;
                        buf++;
                        if (*buf != '\n') {
                            triplet[2] = *buf;
                            triplet[3] = '\0';
                            if (!strcmp(triplet, "=C5"))
                                *buf = ']';
                            else if (!strcmp(triplet, "=C4"))
                                *buf = '[';
                            else if (!strcmp(triplet, "=D6"))
                                *buf = '\\';
                            else if (!strcmp(triplet, "=E5"))
                                *buf = '}';
                            else if (!strcmp(triplet, "=E4"))
                                *buf = '{';
                            else if (!strcmp(triplet, "=F6"))
                                *buf = '|';
                            else if (!strcmp(triplet, "=3D"))
                                *buf = '=';
                            else if (!strcmp(triplet, "=8F"))
                                *buf = ']';
                            else if (!strcmp(triplet, "=8E"))
                                *buf = '[';
                            else if (!strcmp(triplet, "=99"))
                                *buf = '\\';
                            else if (!strcmp(triplet, "=86"))
                                *buf = '}';
                            else if (!strcmp(triplet, "=85"))
                                *buf = '{';
                            else if (!strcmp(triplet, "=94"))
                                *buf = '|';
                            else if (!strcmp(triplet, "=FC"))
                                *buf = '^';
                            else if (!strcmp(triplet, "=DF"))
                                *buf = 's';
                            else if (!strcmp(triplet, "=91"))
                                *buf = '`';
                            else if (!strcmp(triplet, "=92"))
                                *buf = '\'';
                            else if (!strcmp(triplet, "=E9"))
                                *buf = 'e';
                            else if (!strcmp(triplet, "=20"))
                                *buf = ' ';
                            else {
                                buf--;
                                buf--;
                            }
                        } else {
                            buf--;
                            buf--;
                        }
                    } else
                        buf--;
                }
                *run = *buf;
                run++;
            }                   /* if c < LINE_LEN */
            buf++;
            c++;
        }
        *run = '\0';
        buf++;
        if (i < te->th.size) {
            te->body->next = (struct TEXT_BODY *)
                malloc(sizeof(struct TEXT_BODY));
            if (te->body->next == NULL) {
                sys_error("get_text_entry", 2, "malloc");
                return NULL;
            }
            te->body = te->body->next;
            te->body->next = NULL;
        }
    }
    te->body = tmp_body;
    if (*buf == '\0')
        return buf;
    te->cl = NULL;
    for (;;) {
        itmp1 = strtol(buf, &str, 10);
        if (str == buf) {
            break;
        }
        ptr = strchr(buf, ':') + 1;
        itmp2 = atoi(ptr);
        if (te->cl == NULL) {
            te->cl = (struct COMMENT_LIST *)
                malloc(sizeof(struct COMMENT_LIST));
            if (te->cl == NULL) {
                sys_error("get_text_entry", 3, "malloc");
                return NULL;
            }
            tmp_cl = te->cl;
            te->cl->comment_num = itmp1;
            te->cl->comment_author = itmp2;
            te->cl->next = NULL;
        } else {
            te->cl->next = (struct COMMENT_LIST *)
                malloc(sizeof(struct COMMENT_LIST));
            if (te->cl->next == NULL) {
                sys_error("get_text_entry", 4, "malloc");
                return NULL;
            }
            te->cl = te->cl->next;
            te->cl->comment_num = itmp1;
            te->cl->comment_author = itmp2;
            te->cl->next = NULL;
        }
        buf = strchr(buf, '\n');
        if (!buf)
            break;
        while (*buf == '\n')
            buf++;
    }
    te->cl = tmp_cl;
    return buf;
}

/*
 * get_active_entry - get ACTIVE_ENTRY from buffer
 * args: buffer (buf), pointer to ACTIVE_ENTRY (ae)
 * ret: next position in buffer or NULL
 */

char *
get_active_entry(char *buf, struct ACTIVE_ENTRY * ae)
{
    char *ptr, *str;

    for (;;) {
        memset(ae->from, 0, 17);    /* Ugly! But it works... */
        ae->user = (int) strtol(buf, &str, 10);
        if (buf == str) {
            return NULL;
        }
        ptr = strchr(buf, ':') + 1;
        ae->pid = atoi(ptr);
        ptr = strchr(ptr, ':') + 1;
        ae->login_time = atol(ptr);
        ptr = strchr(ptr, ':') + 1;
        ae->avail = atoi(ptr);
        ptr = strchr(ptr, ':') + 1;
/*
        str = strchr(ptr, '\n');
        *str = 0;
        strcpy(ae->from, ptr);
        *str = '\n';
*/
        str = strchr(ptr, ':');
        *str = 0;
        strcpy(ae->from, ptr);
        *str = ':';

        ptr = str + 1;
        str = strchr(ptr, ':');
        *str = 0;
        strcpy(ae->tty, ptr);
        *str = ':';

        buf = strchr(buf, '\n');
        buf++;
        return buf;
    }
}

/*
 * get_confs_entry - get CONFS_ENTRY from buffer
 * args: buffer (buf), pointer to CONFS_ENTRY (ce)
 * ret: next position in buffer or NULL
 */

char *
get_confs_entry(char *buf, struct CONFS_ENTRY * ce)
{
    char *oldbuf, *t1, *t2;
    long tmpfrom, tmpto;
    struct INT_LIST *int_list_sav;

    ce->il = NULL;
    if (!buf)
        return NULL;
    int_list_sav = NULL;
    oldbuf = buf;
    buf = strchr(buf, ':');
    if (!buf || (*buf == 0))
        return NULL;
    while ((buf > oldbuf) && (*buf != '\n'))
        buf--;
    if (buf != oldbuf)
        buf++;
    oldbuf = buf;
    ce->num = atol(buf);
    buf = strchr(buf, ':');

    buf++;
    for (;;) {
        if (*buf == '\n')
            break;
        t1 = strchr(buf, '-');
        *t1 = ' ';
        tmpfrom = atol(buf);
        tmpto = atol(t1);
        *t1 = '-';
        if (ce->il == NULL) {
            ce->il = (struct INT_LIST *) malloc(sizeof(struct INT_LIST));
            if (ce->il == NULL) {
                sys_error("get_confs_entry", 1, "malloc");
                return NULL;
            }
            int_list_sav = ce->il;
            ce->il->from = tmpfrom;
            ce->il->to = tmpto;
            ce->il->next = NULL;
        } else {
            ce->il->next = (struct INT_LIST *) malloc(sizeof(struct INT_LIST));
            if (ce->il->next == NULL) {
                sys_error("get_confs_entry", 2, "malloc");
                return NULL;
            }
            ce->il = ce->il->next;
            ce->il->from = tmpfrom;
            ce->il->to = tmpto;
            ce->il->next = NULL;
        }
        t1 = strchr(buf, '\n');
        t2 = strchr(buf, ',');
        if (t2 && (t2 < t1))
            buf = ++t2;
        else
            break;
    }
    ce->il = int_list_sav;
    buf = strchr(oldbuf, '\n');
    if (buf)
        buf++;
    return buf;
}

/*
 * get_msg_entry - get MSG_ENTRY from buffer
 * args: buffer (buf), pointer to MSG_ENTRY (me)
 * ret: next position in buffer or NULL
 */

char *
get_msg_entry(char *buf, struct MSG_ENTRY * me)
{
    char *ptr, *ptr2;

    memset(me->msg, 0, LINE_LEN);

    me->direct = 0;             /* Default is always uid=sender */

    for (;;) {

        ptr2 = buf;


        ptr = strchr(ptr2, ':');
        if (ptr == NULL) {
            return NULL;
        }
        *ptr = 0;
        me->num = atoi(ptr2);
        *ptr = ':';

        ptr2 = ptr + 1;
        ptr = strchr(ptr2, ':');
        if (ptr == NULL) {
            return NULL;
        }
        *ptr = 0;
        me->type = atoi(ptr2);
        *ptr = ':';

        ptr2 = ptr + 1;
        ptr = strchr(ptr2, '\n');
        if (ptr == NULL) {
            return NULL;
        }
        *ptr = 0;
        strcpy(me->msg, ptr2);
        *ptr = '\n';

        rtrim(me->msg);
        buf = strchr(buf, '\n');
        while (buf && (*buf == '\n')) {
            buf++;
        }
        return buf;
    }
}

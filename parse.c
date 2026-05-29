/* parse.c */

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

struct hitlist {
    int wanted;
    int matched_wanted;
    int matched_found;
};

int find_max_match(struct hitlist * hl);

/*
 * parse - parse user input and return pointer to function
 * args: pointer to user input (buf), pointer to argument returned (args)
 * ret: pointer to function or NULL
 */

cmd_func_t *
parse(char *buf, char *args)
{
    struct hitlist hl[MAX_COMMANDS];
    int i, found, full_hit, arg_hit, part_hit, max_words;
	int full_ind = -1, arg_ind = -1, part_ind = -1;
    if (atol(buf)) {
        sprintf(buf, "%s %ld", MSG_TEXTPROMPT2, atol(buf));
    }
    memset(args, 0, LINE_LEN);
    i = 0;
    found = wc(buf);
    while (Par_ent[i].func[0] != '\0') {
        hl[i].wanted = wc(Par_ent[i].cmd);
        hl[i].matched_wanted = parse_strings(Par_ent[i].cmd, buf,
            hl[i].wanted, NULL);
        hl[i].matched_found = parse_strings(Par_ent[i].cmd, buf,
            found, NULL);
        i++;
    }

    /* DBG	output("Found: %d\n", found);  */
    /* DBG	output("   Kommando             Wanted M-wanted M-found\n"); */
    /* DBG	i = 0; while (Par_ent[i].func[0] != '\0') { */
    /* DBG	output("%2d %-20.20s %6d %8d %7d\n",i,Par_ent[i].cmd,
     * hl[i].wanted, */
    /* DBG	       hl[i].matched_wanted, hl[i].matched_found); i++; } */

    i = 0;
    full_hit = 0;
    arg_hit = 0;
    part_hit = 0;
    while (Par_ent[i].func[0] != '\0') {
        if ((hl[i].matched_wanted == hl[i].wanted) &&
            (hl[i].matched_found == found)) {   /* A Roxette hit */
            full_hit++;
            full_ind = i;
        } else if ((hl[i].matched_wanted == hl[i].wanted) &&
            (hl[i].matched_found < found)) {
            arg_hit++;
            arg_ind = i;
        } else if ((hl[i].matched_wanted > 0) &&
            (hl[i].matched_wanted < hl[i].wanted)) {
            part_hit++;
            part_ind = i;
        }
        i++;
    }

    /* DBG	output("full_hit: %d arg_hit: %d part_hit: %d\n", */
    /* DBG      full_hit, arg_hit, part_hit); */

    if (full_hit == 1) {

        /* DBG		output("cmd: [%s] func: [%s] addr: [%ld]\n", */
        /* DBG	  Par_ent[full_ind].cmd, Par_ent[full_ind].func, */
        /* DBG	  Par_ent[full_ind].addr); */

        return Par_ent[full_ind].addr;
    } else if (full_hit > 1) {
        max_words = find_max_match(hl);

        /* DBG			output("max_words: %d\n", max_words); */

        output("\n%s \"%s\". %s\n", MSG_MULTICOM, buf, MSG_CHOSECOM);
        i = 0;
        while (Par_ent[i].func[0] != '\0') {
            if (parse_strings(Par_ent[i].cmd, buf, max_words,
                    NULL) == max_words) {
                output("%s\n", Par_ent[i].cmd);
            }
            i++;
        }
        output("\n");
        return (int (*) ()) 0;

    } else if (arg_hit == 1) {

        parse_strings(Par_ent[arg_ind].cmd, buf, hl[arg_ind].wanted,
            args);

        /* DBG*		output("cmd: [%s] func: [%s] addr: [%ld] args:
         * [%s]\n", Par_ent[arg_ind].cmd, Par_ent[arg_ind].func,
         * Par_ent[arg_ind].addr, args); */

        return Par_ent[arg_ind].addr;
    } else if (arg_hit > 1) {
        max_words = find_max_match(hl);

        /* DBG*		output("max_words: %d\n", max_words); */

        i = 0;
        arg_hit = 0;
        while (Par_ent[i].func[0] != '\0') {
            if (parse_strings(Par_ent[i].cmd, buf, max_words,
                    NULL) == max_words) {
                arg_hit++;
                arg_ind = i;
            }
            i++;
        }
        if (arg_hit == 1) {
            parse_strings(Par_ent[arg_ind].cmd, buf,
                hl[arg_ind].wanted, args);

            /* DBG	    			output("cmd: [%s] func: [%s]
             * addr: [%ld] args: [%s]\n", */
            /* DBG	      Par_ent[arg_ind].cmd, Par_ent[arg_ind].func, */
            /* DBG	      Par_ent[arg_ind].addr, args); */

            return Par_ent[arg_ind].addr;
        } else {
            max_words = find_max_match(hl);

            /* DBG	    			output("max_words: %d\n",
             * max_words);  */

            output("\n%s \"%s\". %s\n", MSG_MULTICOM, buf, MSG_CHOSECOM);
            i = 0;
            while (Par_ent[i].func[0] != '\0') {
                if (parse_strings(Par_ent[i].cmd, buf, max_words,
                        NULL) == max_words) {
                    output("%s\n", Par_ent[i].cmd);
                }
                i++;
            }
            output("\n");
            return (int (*) ()) 0;
        }
    } else if (part_hit == 1) {

        /* DBG			output("cmd: [%s] func: [%s] addr: [%ld] args:
         * [%s]\n", */
        /* DBG	  Par_ent[part_ind].cmd, Par_ent[part_ind].func, */
        /* DBG	  Par_ent[part_ind].addr, args);  */

        return Par_ent[part_ind].addr;
    } else if ((full_hit > 1) || (arg_hit > 1) || (part_hit > 1)) {
        max_words = find_max_match(hl);

        /* DBG			output("max_words: %d\n", max_words); */

        output("\n%s \"%s\". %s\n", MSG_MULTICOM, buf, MSG_CHOSECOM);
        i = 0;
        while (Par_ent[i].func[0] != '\0') {
            if (parse_strings(Par_ent[i].cmd, buf, max_words,
                    NULL) == max_words) {
                output("%s\n", Par_ent[i].cmd);
            }
            i++;
        }
        output("\n");
        return (int (*) ()) 0;
    } else {
        output("\n%s \"%s\". %s\n\n", MSG_COMERR, buf, MSG_HELPQ);
    }
    return (int (*) ()) 0;
}

/*
 * find_max_match - find maximum number of words matched
 * args: hitlist (hl)
 * ret: number of words matched
 */

int
find_max_match(struct hitlist * hl)
{
    int i = 0;
    static int max_words;

    max_words = 0;
    while (Par_ent[i].func[0] != '\0') {
        max_words = (max_words > hl[i].matched_wanted ?
            max_words : hl[i].matched_wanted);
        i++;
    }
    return max_words;
}

static cmd_func_t *
get_command(const char *name)
{
    unsigned int i;
    for (i = 0; command_list[i].name != NULL; i++) {
        if (strcmp(command_list[i].name, name) == 0) {
            return command_list[i].ptr;
        }
    }
    return NULL;
}

/*
 * parse_init - initialize parser
 * args: name of program (program_name)
 * ret: number of commands or failure (-1)
 */

int
parse_init(char *program_name)
{
    int i = 0, pf;
    char *fbuf, *buf;

    if ((pf = open_file(PARSE_FILE, OPEN_DEFAULT)) == -1) {
        sys_error("parse_init", 1, "open_file");
        return -1;
    }
    if ((fbuf = read_file(pf)) == NULL) {
        sys_error("parse_init", 2, "read_file");
        return -1;
    }
    close_file(pf);

    buf = fbuf;
    while ((buf = get_parse_entry(buf, &Par_ent[i])) != NULL) {
        Par_ent[i].addr = get_command(Par_ent[i].func);
        if (! Par_ent[i].addr) {
            output("%s[%s #%d] %s(): %s\n", program_name,
                "parse_init", 3, Par_ent[i].func,
                "no such function");
            free(fbuf);
            return -1;
        }
        i++;
    }
    free(fbuf);
    return i;
}

#define EXPLIST_SIZE 20

struct expand_list {
    LINE name;                  /* expanded name                  */
    int type;                   /* type of name (USER, CONF, ...) */
};

/*
 * invalid_name - outputs error message if expand_name failed
 * args: type of message
 */

static void
invalid_name(int type)
{
    switch (type) {
        case CONF:
        output(MSG_ECONFN);
        break;
    case USER:
        output(MSG_EUSERN);
        break;
    case ACTIVE:
        output(MSG_EUSERA);
        break;
    case SUBSCRIBED:
        output(MSG_ECONFS);
        break;
    case UNSUBSCRIBED:
        output(MSG_ECONFU);
        break;
    default:
        output(MSG_ECOMN);
    }
}

/*
 * expand_name - expands user or conf. name...
 * args: string to look for (name), type of object (type), don't
 *       output failures (quiet), pointer to int (expanded_type)
 * ret: name expanded or NULL
 */

char *
expand_name(char *name, int type, int quiet, int *expanded_type)
{
    int fd, i, overflow = 0, found = 0;
    char *buf, *tmpbuf;
    LINE tmp;
    struct CONF_ENTRY ce;
    struct USER_ENTRY ue;
    static struct expand_list el[EXPLIST_SIZE];

    if (!wc(name)) {
        if (!quiet)
            invalid_name(type);
        return NULL;
    }
    if ((type & CONF) || (type & SUBSCRIBED) || (type & UNSUBSCRIBED)) {
        if ((fd = open_file(CONF_FILE, 0)) == -1) {
            sys_error("conf_name", 1, "open_file");
            return NULL;
        }
        if ((tmpbuf = buf = read_file(fd)) == NULL) {
            sys_error("conf_name", 2, "read_file");
            return NULL;
        }
        if (close_file(fd) == -1) {
            sys_error("conf_name", 3, "close_file");
            return NULL;
        }
        while ((buf = get_conf_entry(buf, &ce)) && !overflow) {
            if (cmp_strings(ce.name, name) && ((ce.creator != -1) || (type & ALSOERASED))) {
                if ((type & CONF) ||
                    ((type & SUBSCRIBED) && member_of(Uid, ce.num)) ||
                    ((type & UNSUBSCRIBED) && (!member_of(Uid, ce.num)))) {
                    if (found < (EXPLIST_SIZE - 1)) {
                        if (can_see_conf(Uid, ce.num, ce.type, ce.creator)) {
                            strcpy(el[found].name, ce.name);
                            el[found].type = CONF;
                            found++;
                        }
                    } else {
                        overflow = 1;
                    }
                }
            }
        }

        free(tmpbuf);
        if (!overflow) {
            strcpy(tmp, MSG_MAILBOX);
            if (cmp_strings(tmp, name)) {
                if (found < (EXPLIST_SIZE - 1)) {
                    strcpy(el[found].name, MSG_MAILBOX);
                    el[found].type = CONF;
                    found++;
                } else {
                    overflow = 1;
                }
            }
        }
    }
    if (((type & USER) || (type & ACTIVE)) && !overflow) {
        if ((fd = open_file(USER_FILE, 0)) == -1) {
            sys_error("conf_name", 4, "open_file");
            return NULL;
        }
        if ((tmpbuf = buf = read_file(fd)) == NULL) {
            sys_error("conf_name", 5, "read_file");
            return NULL;
        }
        if (close_file(fd) == -1) {
            sys_error("conf_name", 6, "close_file");
            return NULL;
        }
        while ((buf = get_user_entry(buf, &ue)) && !overflow) {
            if ((type & USER) || ((type & ACTIVE) && user_is_active(ue.num))) {
                if (cmp_strings(ue.name, name)) {
                    if (found < (EXPLIST_SIZE - 1)) {
                        strcpy(el[found].name, ue.name);
                        if (type & ACTIVE) {
                            el[found].type = ACTIVE;
                        } else {
                            el[found].type = USER;
                        }
                        found++;
                    } else {
                        overflow = 1;
                    }
                }
            }
        }
        free(tmpbuf);
    }
    if (expanded_type != NULL) {
        *expanded_type = 0;
    }
    switch (found) {
    case 0:                    /* no match */
        if (!quiet) {
            invalid_name(type);
        }
        return NULL;
    case 1:                    /* one match */
        if (expanded_type != NULL) {
            *expanded_type = el[0].type;
        }
        return el[0].name;
    default:
        if (!quiet) {
            output("\n%s\n", MSG_MULNAM);
            for (i = 0; i < found; i++) {
                if (output(" %2d. %s\n", i + 1, el[i].name) == -1) {
                    return NULL;
                }
            }
            output("\n");
            if (overflow) {
                output("%s\n\n", MSG_MOREMUL);
            }
            output(MSG_PICKLIST);
            input("", tmp, LINE_LEN, 0, 0, 0);
            rtrim(tmp);
            if (!strlen(tmp)) {
                output("\n");
                return NULL;
            }
            i = atoi(tmp);
            if (!i) {
                return expand_name(tmp, type, quiet, expanded_type);
            } else {
                if (i && (i <= found)) {
                    if (expanded_type != NULL) {
                        *expanded_type = el[i - 1].type;
                    }
                    return el[i - 1].name;
                } else {
                    output("\n%s\n\n", MSG_BADPICK);
                    return NULL;
                }
            }
        }
    }
    return 0;
}

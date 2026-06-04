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
#include <limits.h>

#include "sklaff.h"

/*
 * get_conf_description - return description string from confxtra
 * Returns malloc'd string (caller must free), or NULL if none found
 */
char *
get_conf_description(int confnum)
{
    char path[PATH_MAX];
    FILE *fp;
    char line[256];

    snprintf(path, sizeof(path), "%s/%d%s", SKLAFF_DB, confnum, CONFXTRA_FILE);

    fp = fopen(path, "r");
    if (!fp)
        return NULL;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "![desc]", 7) == 0) {
            if (fgets(line, sizeof(line), fp)) {
                rtrim(line);
                fclose(fp);
                return strdup(line);
            }
        }
    }

    fclose(fp);
    return NULL;
}


/*
 * write_confxtra_section - write a header + content to confxtra
 */
int
write_confxtra_section(int confnum, const char *tag, const char *value)
{
    char path[PATH_MAX];
    char tmp[PATH_MAX + 5];  /* +5 for ".tmp\0" safety margin */
    FILE *in = NULL, *out = NULL;
    char line[256];
    int found = 0;

    snprintf(path, sizeof(path), "%s/%d%s", SKLAFF_DB, confnum, CONFXTRA_FILE);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    in = fopen(path, "r");
    out = fopen(tmp, "w");
    if (!out) {
        if (in) fclose(in);
        return -1;
    }

    if (in) {
        while (fgets(line, sizeof(line), in)) {
            if (!found && strncmp(line, "![desc]", 7) == 0) {
                /* Skip old value safely */
                if (fgets(line, sizeof(line), in) == NULL) {
                    /* ignore EOF */
                }
                found = 1;
                fprintf(out, "![desc]\n%s\n", value);
            } else {
                fputs(line, out);
            }
        }
        fclose(in);
    }

    if (!found)
        fprintf(out, "![%s]\n%s\n", tag, value);

    fclose(out);
    if (rename(tmp, path) != 0)
        return -1;

    return 0;
}

/*
 * remove_confxtra_section - remove a header section like ![desc]
 */
int
remove_confxtra_section(int confnum, const char *tag)
{
    char path[PATH_MAX];
    char tmp[PATH_MAX + 5];  /* extra margin for .tmp */
    FILE *in = NULL, *out = NULL;
    char line[256];
    int skip = 0, removed = 0;

    snprintf(path, sizeof(path), "%s/%d%s", SKLAFF_DB, confnum, CONFXTRA_FILE);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    in = fopen(path, "r");
    out = fopen(tmp, "w");
    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return -1;
    }

    while (fgets(line, sizeof(line), in)) {
        if (!skip && strncmp(line, "![", 2) == 0 && strstr(line, tag)) {
            skip = 2;  /* skip header + next line */
            removed = 1;
            continue;
        }
        if (skip) {
            skip--;
            continue;
        }
        fputs(line, out);
    }

    fclose(in);
    fclose(out);

    if (rename(tmp, path) != 0)
        return -1;

    return removed ? 0 : -1;
}

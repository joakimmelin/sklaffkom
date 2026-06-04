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
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "sklaff.h"

/* Render the footnote (if any)
 * 2025-10-15 PL
*/
void
show_footnote_block(int conf, long num, char *home, int has_comments)
{
    char fname[PATH_MAX], linebuf[LINE_LEN];
    FILE *fp;

    if (conf > 0)
        snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, conf, num);
    else
        snprintf(fname, sizeof(fname), "%s/%ld", home, num);

    if ((fp = fopen(fname, "r")) == NULL)
        return;

    int found = 0;
    int ends_with_newline = 1;

    while (fgets(linebuf, sizeof(linebuf), fp)) {
        if (strncmp(linebuf, "F:", 2) == 0) {
            if (!found) {
                output_ansi_fmt(BR_RED"\n%s:\n"DOT, "\n%s:\n", MSG_FOOTNOTE);  // Fotnot:
                found = 1;
            }

            char *foot = linebuf + 2;

            output_ansi_fmt(WHITE"%s"DOT, "%s", foot);

            size_t L = strlen(foot);
            if (L > 0 && foot[L - 1] != '\n')
                ends_with_newline = 0;
            else
                ends_with_newline = 1;
        }
    }
    fclose(fp);

    if (found && !ends_with_newline) {
        output("\n");  // Ensure clean separation to next block (like likes)
    }
}


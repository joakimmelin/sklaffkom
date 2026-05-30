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
* has_file_area - checks if conference has files               
*/ 
int 
has_file_area(int confnum) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%d%s", FILE_DB, confnum, INDEX_FILE);
    return file_exists(path) != -1;
}

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

#include "sklaff.h"

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




void display_news(void)
{
    display_langfile(NEWS_FILE, NEWS_FILE_ENG, NEWS_FILE_SWE);
}

void display_logout(void)
{
    display_langfile(LOGOUT_FILE, LOGOUT_FILE_ENG, LOGOUT_FILE_SWE);
}


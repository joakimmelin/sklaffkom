/* output.c */

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
#include <stdarg.h>
#include <stdio.h>

#include "sklaff.h"
#include "ext_globals.h"
/*
* Needed for ansi outputs
* PL 2025-07-31
*/
int
output_ansi_fmt(const char *ansi_fmt, const char *plain_fmt, ...)
{
    va_list args;
    char buf[HUGE_LINE_LEN];
    const char *fmt = Ansi_output ? ansi_fmt : plain_fmt;

    va_start(args, plain_fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    return output("%s", buf);   /* returns -1 on error */
}

/* Need these to avoid extra '(' in the prompt 2025-08-26 PL */

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

void clear_screen(void)
{
    output(ANSI_CLS);  /* or printf(ANSI_CLS); */
    fflush(stdout);
    Lines = 1;
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

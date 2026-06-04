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
#include <time.h>

#include "sklaff.h"

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

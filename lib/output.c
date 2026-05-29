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

#include "sklaff.h"
#include "ext_globals.h"
#include <stdarg.h>

/*
 * output - outputs string
 * args: same as for printf
 * ret:	success (0), stop output (-1)
 */

/*
int output(char * fmt,...)
{
    va_list args;

    va_start( args, fmt );
    vfprintf( stdout, fmt, args );
    va_end( args );

    return 0;
}
*/
int
output(char *fmt,...)
{
    va_list args;
    unsigned char c;
    char *tmp, *tmp2;
    char *p1, *p2;
    char fmt2[HUGE_LINE_LEN], outline[HUGE_LINE_LEN];
    char tmpline[LONG_LINE_LEN];
    //int in_ansi = Ansi_output ? 0 : -1;  /* modified on 2025-07-30, PL: disable ANSI logic unless allowed */
    int in_ansi = 0;   /* 2025‑11‑11 PL: ensure first char is translated even when ANSI is off */
	va_start(args, fmt);
    vsnprintf(fmt2, sizeof(fmt2), fmt, args); /* modified on 2025-07-25, PL: replaced vsprintf with vsnprintf for safety */
    va_end(args);
    tmp = fmt2;
    tmp2 = outline;
  while (*tmp) {
    if (Beep || Special || (*tmp != 7)) {
        unsigned char c = *tmp;
            /* modified on 2025-07-30, PL: skip character translation inside ANSI escape sequences */
        if (in_ansi != -1 && c == '\033' && *(tmp + 1) == '[')
            in_ansi = 1;
        else if (in_ansi && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
            in_ansi = 0;

        tmp++;

        if (!in_ansi) {
            if (Utf8) {
                if (c == '}') { *tmp2++ = 0xC3; *tmp2++ = 0xA5; continue; }
                if (c == '{') { *tmp2++ = 0xC3; *tmp2++ = 0xA4; continue; }
                if (c == '|') { *tmp2++ = 0xC3; *tmp2++ = 0xB6; continue; }
                if (c == ']') { *tmp2++ = 0xC3; *tmp2++ = 0x85; continue; }
                if (c == '[') { *tmp2++ = 0xC3; *tmp2++ = 0x84; continue; }
                if (c == '\\'){ *tmp2++ = 0xC3; *tmp2++ = 0x96; continue; }
            } else if (Ibm) {
                if (c == '}') c = 134;
                else if (c == '{') c = 132;
                else if (c == '|') c = 148;
                else if (c == ']') c = 143;
                else if (c == '[') c = 142;
                else if (c == '\\') c = 153;
            } else if (Iso8859) {
                if (c == '}') c = 229;
                else if (c == '{') c = 228;
                else if (c == '|') c = 246;
                else if (c == ']') c = 197;
                else if (c == '[') c = 196;
                else if (c == '\\') c = 214;
            } else if (Mac) {
                if (c == '}') c = 140;
                else if (c == '{') c = 138;
                else if (c == '|') c = 154;
                else if (c == ']') c = 129;
                else if (c == '[') c = 128;
                else if (c == '\\') c = 133;
            }
        }

        /* single newline handler */
        if (c == '\n') {
            *tmp2++ = '\r';
            *tmp2++ = '\n';
        } else {
            *tmp2++ = c;
        }
    }
    else {
        tmp++;
    }
}

    *tmp2 = '\0';

    p2 = outline;
    while (*p2 != '\0') {
        p1 = tmpline;
        while ((*p2 != '\n') && (*p2 != '\0')) {
            *p1 = *p2;
            if (Rot13 && (*p1 >= 'A') && (*p1 <= 'z')) {
                if ((*p1 >= 'A') && (*p1 <= 'Z')) {
                    *p1 = *p1 - 13;
                    if (*p1 < 'A')
                        *p1 = 'Z' - ('A' - *p1 - 1);
                } else if ((*p1 >= 'a') && (*p1 <= 'z')) {
                    *p1 = *p1 - 13;
                    if (*p1 < 'a')
                        *p1 = 'z' - ('a' - *p1 - 1);
                }
            }
            p1++;
            p2++;
        }
        if (*p2 == '\n') {
            *p1 = '\n';
            p1++;
            p2++;
            if (!Cont)
                Lines++;
        }
        *p1 = '\0';
        if ((Lines >= Numlines) && Numlines && !Cont) {
            printf(MSG_MORE);
            do {
                if (Timeout) {
                    alarm(60 * Timeout);
                }
                do
                    c = getc(stdin);
                while (c == 255);
                alarm(0);
                Warning = 0;
            } while ((c != 'q') && (c != 'Q') && (c != ' ') && (c != '\r')
                && (c != '\n') && (c != 3) && (c != 'c') && (c != 'C'));
            printf("\r       \r");
            make_activity_note();
            Lines = 1;
            if ((c == 'c') || (c == 'C'))
                Cont = 1;
            if ((c == 'q') || (c == 'Q') || (c == 3)
                || ((c == ' ') && (!Space || Special))) {
                if ((strlen(tmpline) == 2) &&
                    (tmpline[0] == '\r') &&
                    (tmpline[1] == '\n')) {
                    printf("%s", tmpline);
                }
                return -1;
            }
        }
        fputs(tmpline, stdout);
    }

    return 0;
}

/* Output for external applications */

int
outputex(char *fmt,...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    return 0;

    /* No use to add Iso8859 support. Eight bit is stripped anyway when sent
     * over network. */
}
/*
* Needed for ansi outputs with lot's of args
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


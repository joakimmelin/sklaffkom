/* input.c */

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

#include <signal.h>

#include "sklaff.h"
#include "ext_globals.h"

/*
 * haffo - handle interrupted input
 * args: type of interrupt (tmp)
 */

void
haffo(int tmp)
{
    int x, y;
    LINE oldprompt;
    sigset_t sigmask, oldsigmask;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    signal(SIGNAL_NEW_MSG, haffo);
    signal(SIGNAL_NEW_TEXT, haffo);
    if (tmp == SIGNAL_NEW_TEXT)
        Change_prompt = 1;
    else
        Change_msg = 1;
    if (Interrupt_input == 2) {
        y = strlen(MSG_MSGPROMPT);
    } else {
        y = strlen(Prompt) + 3;
    }
    x = display_msg(y);
    if (Interrupt_input == 1) {
        strcpy(oldprompt, Prompt);
        display_prompt(Prompt, oldprompt, (1 - x));
    } else {
        if (x)
            output(MSG_MSGPROMPT);
    }
    fflush(stdout);
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
}

/*
 * baffo - set Change_prompt flag at signal received
 * args: type of interrupt (tmp)
 */

void
baffo(int tmp)
{
    sigset_t sigmask, oldsigmask;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    signal(SIGNAL_NEW_TEXT, baffo);
    Change_prompt = 1;
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
}

/*
 * input - inputs string
 * args: default string (in_str), input string (out_str), max number
 *       of characters allowed (max_len), don't echo output (noecho)
 *       allow wordwrap (wrap), history (hist)
 * ret:	pointer to input string
 */

char *
input(char *in_str, char *out_str, int max_len, int noecho, int wrap, int hist)
{
    int len, hptr, ltop;
    char *p, *i, *space, *ptr;
    unsigned char c, outc;

    unsigned char u8e[2] = {0,0}; int u8e_len = 0; Lines = 1;
    hptr = Comtop;
    ltop = Comtop;
    if (strlen(in_str) >= max_len) {
        strncpy(out_str, in_str, (max_len - 1));
        p = out_str;
        p += (max_len - 1);
        *p = '\0';
    } else {
        strcpy(out_str, in_str);
    }
    if (wrap && !strlen(out_str)) {
        strcpy(out_str, Overflow);
        strcpy(Overflow, "");
    }
    len = strlen(out_str);
    if (!noecho)
        output("%s", out_str);
    for (;;) {
        if ((len == 0) && (Interrupt_input != 0)) {
            signal(SIGNAL_NEW_MSG, haffo);
            signal(SIGNAL_NEW_TEXT, haffo);
        }
        if (Timeout) {
            alarm(60 * Timeout);
        }
        do
            c = getc(stdin);
        while (c == 255);
        /* if (Strip) c &= 0x7f; Obsolete 18/2 2000, OR */

        /* added on 2025-09-25 by PL - unix like history scroll with arrow keys*/
	if (c == 27) { // ESC
	    int c1 = getc(stdin);
    	    int c2 = getc(stdin);
    	    if (c1 == '[') {
                if (c2 == 'A') {
                    c = 16;  // ↑
            } else if (c2 == 'B') {
                c = 14;  // ↓
            }
        }
    }
	alarm(0);
        Warning = 0;
        signal(SIGNAL_NEW_TEXT, baffo);
        signal(SIGNAL_NEW_MSG, newmsg);
        if (Interrupt_input != 0) {
            fflush(stdout);
        }
        outc = c;
        /* 2025-08-10, PL: minimal UTF-8 mapping for Swedish å ä ö Å Ä Ö */
        if (c == 0xC3) {
            int c2 = getc(stdin);
            if (c2 != EOF) {
                unsigned char u2 = (unsigned char)c2;
                switch (u2) {
                    case 0xA5: c = '}'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* å */
                    case 0xA4: c = '{'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* ä */
                    case 0xB6: c = '|'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* ö */
                    case 0x85: c = ']'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* Å */
                    case 0x84: c = '['; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* Ä */
                    case 0x96: c = 0x5c; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* Ö -> backslash */
                    default:
                        ungetc(c2, stdin);
                        break;
                }
            }
        }

        if (c == 134)
            c = '}';
        else if (c == 132)
            c = '{';
        else if (c == 148)
            c = '|';
        else if (c == 143)
            c = ']';
        else if (c == 142)
            c = '[';
        else if (c == 153)
            c = 0x05c;
        if (c == 229)
            c = '}';
        else if (c == 228)
            c = '{';
        else if (c == 246)
            c = '|';
        else if (c == 197)
            c = ']';
        else if (c == 196)
            c = '[';
        else if (c == 214)
            c = 0x05c;
        if (c == 140)
            c = '}';
        else if (c == 138)
            c = '{';
        else if (c == 154)
            c = '|';
        else if (c == 129)
            c = ']';
        else if (c == 128)
            c = '[';
        else if (c == 133)
            c = 0x05c;
        if ((c == '\r' || c == '\n') || (c == ' ' && !len && (Space && !Special))) {    /* Special -> Space=0 */
            out_str[len] = '\0';
            output("\n");
            break;
        } else if (c >= ' ' && c <= '~') {
            if (len < (max_len - 3)) {
                out_str[len] = (char) c;
                if (noecho) {
                    putc('*', stdout);
                } else {
                    if (u8e_len == 2) { fwrite(u8e, 1, 2, stdout); u8e_len = 0; } else { putc(outc, stdout); }
                }
                len++;
            } else {
                if (wrap) {
                    out_str[len++] = (char) c;
                    out_str[len] = '\0';
                    space = strrchr(out_str, ' ');
                    if (!space) {
                        space = out_str + max_len - 5;
                        strcpy(Overflow, space);
                        *space = '\0';
                        ptr = space;
                        space++;
                    } else {
                        *space = '\0';
                        ptr = space;
                        space++;
                        strcpy(Overflow, space);
                    }
                    while (*space) {
                        output("\b \b");
                        len--;
                        space++;
                    }
                    *ptr = '+';
                    len++;
                    ptr++;
                    *ptr = '\0';
                    output("\n");
                    break;
                }
            }
        } else if ((c == '\b' || c == 127) && (len > 0)) {
            len--;
            output("\b \b");
        } else if ((c == 23) && (len > 0)) {
            p = out_str + len;
            i = out_str;
            while ((*(p - 1) == ' ') && (len > 0)) {
                p--;
                len--;
                output("\b \b");
            }
            while (p > i) {
                if (*(p - 1) == ' ') {
                    break;
                } else {
                    p--;
                    len--;
                    output("\b \b");
                }
            }
        } else if ((c == 24 || c == 21) && len > 0)
            for (; len > 0; len--)
                output("\b \b");
        else if ((c == 16) && hist) {
            ltop = hptr - 1;
            if (ltop < 0)
                ltop = HISTORY_SIZE - 1;
            if (strlen(Comstack[ltop])) {
                hptr = ltop;
                for (; len > 0; len--)
                    output("\b \b");
                strcpy(out_str, Comstack[hptr]);
                len = strlen(out_str);
                if (!noecho)
                    output("%s", out_str);
            }
        } else if ((c == 14) && hist) {
            ltop = hptr + 1;
            if (ltop > (HISTORY_SIZE - 1))
                ltop = 0;
            if (strlen(Comstack[hptr])) {
                hptr = ltop;
                for (; len > 0; len--)
                    output("\b \b");
                strcpy(out_str, Comstack[hptr]);
                len = strlen(out_str);
                if (!noecho)
                    output("%s", out_str);
            }
        }
    }
    Lines = 1;
    if (hist && strlen(out_str)) {
        if (Comtop >= HISTORY_SIZE)
            Comtop = 0;
        strcpy(Comstack[Comtop], out_str);
        Comtop++;
        if (Comtop >= HISTORY_SIZE)
            Comtop = 0;
        strcpy(Comstack[Comtop], "");
    }
    make_activity_note();
    return out_str;
}


/*
 * input_extended - inputs string
 * args: default string (in_str), input string (out_str), max number
 *       of characters allowed (max_len), don't echo output (noecho)
 *       allow wordwrap (wrap), history (hist)
 *       only accept characters in interval (low-hi)
 * ret:	pointer to input string
 */

int
input_extended(char *in_str, char *out_str, int max_len, int noecho, int wrap, int hist, int low, int hi)
{
    int len, hptr, ltop;
    unsigned char c, outc;
    unsigned char u8e[2] = {0,0}; int u8e_len = 0; char *p, *i, *space, *ptr;

    Lines = 1;
    hptr = Comtop;
    ltop = Comtop;
    if (strlen(in_str) >= max_len) {
        strncpy(out_str, in_str, (max_len - 1));
        p = out_str;
        p += (max_len - 1);
        *p = '\0';
    } else {
        strcpy(out_str, in_str);
    }
    if (wrap && !strlen(out_str)) {
        strcpy(out_str, Overflow);
        strcpy(Overflow, "");
    }
    len = strlen(out_str);
    if (!noecho)
        output("%s", out_str);
    for (;;) {
        if ((len == 0) && (Interrupt_input != 0)) {
            signal(SIGNAL_NEW_MSG, haffo);
            signal(SIGNAL_NEW_TEXT, haffo);
        }
        if (Timeout) {
            alarm(60 * Timeout);
        }
        do
            c = getc(stdin);
        while ((c == 255 || c < low || c > hi) && (c >= 32 && c != 127));
        /* if (Strip) c &= 0x7f; Obsolete */
        alarm(0);
        Warning = 0;
        signal(SIGNAL_NEW_TEXT, baffo);
        signal(SIGNAL_NEW_MSG, newmsg);
        if (Interrupt_input != 0) {
            fflush(stdout);
        }
        outc = c;
        /* 2025-08-10, PL: minimal UTF-8 mapping for Swedish å ä ö Å Ä Ö */
        if (c == 0xC3) {
            int c2 = getc(stdin);
            if (c2 != EOF) {
                unsigned char u2 = (unsigned char)c2;
                switch (u2) {
                    case 0xA5: c = '}'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* å */
                    case 0xA4: c = '{'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* ä */
                    case 0xB6: c = '|'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* ö */
                    case 0x85: c = ']'; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* Å */
                    case 0x84: c = '['; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* Ä */
                    case 0x96: c = 0x5c; outc = c; u8e[0]=0xC3; u8e[1]=u2; u8e_len=2; break; /* Ö -> backslash */
                    default:
                        /* not a Swedish letter; push back for normal handling */
                        ungetc(c2, stdin);
                        break;
                }
            }
        }

        if (c == 134)
            c = '}';
        else if (c == 132)
            c = '{';
        else if (c == 148)
            c = '|';
        else if (c == 143)
            c = ']';
        else if (c == 142)
            c = '[';
        else if (c == 153)
            c = 0x05c;
        if (c == 229)
            c = '}';
        else if (c == 228)
            c = '{';
        else if (c == 246)
            c = '|';
        else if (c == 197)
            c = ']';
        else if (c == 196)
            c = '[';
        else if (c == 214)
            c = 0x05c;
        if (c == 140)
            c = '}';
        else if (c == 138)
            c = '{';
        else if (c == 154)
            c = '|';
        else if (c == 129)
            c = ']';
        else if (c == 128)
            c = '[';
        else if (c == 133)
            c = 0x05c;
        if ((c == '\r' || c == '\n' || c == 3) || (c == ' ' && !len && (Space && !Special))) {
            out_str[len] = '\0';
            output("\n");
            break;
        } else if (c >= ' ' && c <= '~') {
            if (len < (max_len - 3)) {
                out_str[len] = (char) c;
                if (noecho) {
                    putc('*', stdout);
                } else {
                    if (u8e_len == 2) { fwrite(u8e, 1, 2, stdout); u8e_len = 0; } else { putc(outc, stdout); }
                }
                len++;
            } else {
                if (wrap) {
                    out_str[len++] = (char) c;
                    out_str[len] = '\0';
                    space = strrchr(out_str, ' ');
                    if (!space)
                        space = out_str + max_len - 5;
                    *space = '\0';
                    ptr = space;
                    space++;
                    strcpy(Overflow, space);
                    while (*space) {
                        output("\b \b");
                        len--;
                        space++;
                    }
                    *ptr = '+';
                    len++;
                    ptr++;
                    *ptr = '\0';
                    output("\n");
                    break;
                }
            }
        } else if ((c == '\b' || c == 127) && (len > 0)) {
            len--;
            output("\b \b");
        } else if ((c == 23) && (len > 0)) {
            p = out_str + len;
            i = out_str;
            while ((*(p - 1) == ' ') && (len > 0)) {
                p--;
                len--;
                output("\b \b");
            }
            while (p > i) {
                if (*(p - 1) == ' ') {
                    break;
                } else {
                    p--;
                    len--;
                    output("\b \b");
                }
            }
        } else if ((c == 24 || c == 21) && len > 0)
            for (; len > 0; len--)
                output("\b \b");
        else if ((c == 16) && hist) {
            ltop = hptr - 1;
            if (ltop < 0)
                ltop = HISTORY_SIZE - 1;
            if (strlen(Comstack[ltop])) {
                hptr = ltop;
                for (; len > 0; len--)
                    output("\b \b");
                strcpy(out_str, Comstack[hptr]);
                len = strlen(out_str);
                if (!noecho)
                    output("%s", out_str);
            }
        } else if ((c == 14) && hist) {
            ltop = hptr + 1;
            if (ltop > (HISTORY_SIZE - 1))
                ltop = 0;
            if (strlen(Comstack[hptr])) {
                hptr = ltop;
                for (; len > 0; len--)
                    output("\b \b");
                strcpy(out_str, Comstack[hptr]);
                len = strlen(out_str);
                if (!noecho)
                    output("%s", out_str);
            }
        }
    }
    Lines = 1;
    if (hist && strlen(out_str)) {
        if (Comtop >= HISTORY_SIZE)
            Comtop = 0;
        strcpy(Comstack[Comtop], out_str);
        Comtop++;
        if (Comtop >= HISTORY_SIZE)
            Comtop = 0;
        strcpy(Comstack[Comtop], "");
    }
    make_activity_note();
    return (c == 3) ? -1 : 0;
}

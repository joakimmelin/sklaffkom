/* sklaffkom.c */

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
#include "globals.h"

int
main(int argc, char *argv[])
{
    LINE buf, args;
    int (*fcn) (LINE);

    strcpy(Program_name, prog_name(argv[0]));
    strcpy(args, "");

    do {
        restart = 0;              /* modified to make restarts actually work now 30 years later, 2025-09-25, PL */
        End_sklaff = 0;

        sig_setup();              /* Setup signals */
        tty_raw();                /* Setup tty     */
        cmd_cls(args);           /* Clear screen  */
        check_open();            /* Check if sklaffkom open */
        display_welcome();       /* Display welcome message */
        srand(time(0));          /* Initialize RNG */

        if (parse_init(Program_name) == -1) {
            exec_logout(0);
        }

        force_unsubscribe();
        exec_login();            /* Execute user login script */
        resume_aborted_edit();   /* Resume aborted editing  */
        set_first_conf();
        Current_text = last_text(Current_conf, Uid);
        Last_text = Current_text;
        Current_author = -1;
        cmd_where(args);

        End_sklaff = 0;
        while (!End_sklaff) {
            display_msg(0);
            display_prompt(Prompt, "", 0);
            Interrupt_input = 1;
            Cont = 0;
            input("", buf, LINE_LEN, 0, 0, 1);
            Interrupt_input = 0;
            ltrim(buf);
            if (*buf == '\0') {
                strcpy(buf, Prompt);
            }
            if ((fcn = parse(buf, args)) == (int (*) ()) -1) {
                break;
            } else if (fcn) {
                if ((*fcn) (args) == -1) {
                    break;
                }
            }
        }

        if (restart) {
            debuglog("User triggered session restart", 1); /* modified on 2025-09-25, PL */
        }

        exec_logout(0); /* Clean up */

    } while (restart); /* Repeat if user typed 'restart' */

    return 0;
}


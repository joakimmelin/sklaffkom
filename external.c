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
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef LINUX
#include <bsd/string.h>
#endif

#include "sklaff.h"
#include "ext_globals.h"
/*
 * run_external_cmd_args - runs an external program with argument list
 * argv[0] = command to run, argv[1..n] = arguments, NULL-terminated
 * use_fallback: whether to run 'which' on argv[0] if not found
 * 2025-09-17, PL
 */
int run_external_cmd_args(const char *argv[], int use_fallback)
{
    sigset_t sigmask, oldsigmask;
    char exe_path[256];
    FILE *which_fp;
    char which_buf[256];

    if (!argv || !argv[0])
        return 1;

    strlcpy(exe_path, argv[0], sizeof(exe_path));

    if (access(exe_path, X_OK) != 0 && use_fallback) {
        const char *base = strrchr(argv[0], '/');
        if (base)
            snprintf(which_buf, sizeof(which_buf), "which %s", base + 1);
        else
            snprintf(which_buf, sizeof(which_buf), "which %s", argv[0]);

        which_fp = popen(which_buf, "r");
        if (which_fp && fgets(which_buf, sizeof(which_buf), which_fp)) {
            which_buf[strcspn(which_buf, "\n")] = '\0';
            if (access(which_buf, X_OK) == 0)
                strlcpy(exe_path, which_buf, sizeof(exe_path));
        }
        if (which_fp)
            pclose(which_fp);
    }

  if (access(exe_path, X_OK) != 0) {
    output("\nFel: Kan inte starta spelet eller scriptet - meddela Sysop!\n");
    output("  Försökte köra: %s\n", exe_path);

    if (access(exe_path, F_OK) != 0) {
        output("  Filen finns inte (ENOENT).\n");
    } else {
        output("  Filen finns, men är inte körbar (EACCES eller liknande).\n");
    }

    return 1;
    }

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    signal(SIGNAL_NEW_TEXT, SIG_IGN);
    signal(SIGNAL_NEW_MSG, SIG_IGN);
    set_avail(Uid, 1);

    if (!fork()) {
        sig_reset();
        tty_reset();
        execvp(exe_path, (char *const *)argv);
        perror("execvp");
        _exit(1);
    } else {
        wait(NULL);
    }

    signal(SIGNAL_NEW_TEXT, baffo);
    signal(SIGNAL_NEW_MSG, newmsg);
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
    tty_raw();
    output("\n");
    set_avail(Uid, 0);

    return 0;
}

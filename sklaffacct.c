/* sklaffacct.c */

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
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>

#include "globals.h"

int
main(int argc, char *argv[])
{
    LINE name, login, passwd, epost, cmdline, fname; /* Declare inet and tele here if we want to use them again. PL 2025-07-24 */
    HUGE_LINE outbuf;
    int fd;
    char *buf;
    struct passwd *pw;

    tty_raw();
    
    if ((fd = open_file(ACCT_FILE, OPEN_QUIET)) == -1) {
	printf("[DEBUG] - Cannot open acct-file, inform Sysop!");
        goto cont;
    }
    if ((buf = read_file(fd)) == NULL) {
        printf("[DEBUG] - Cannot read acct-file, inform Sysop!");
        goto cont;
    }
    
 /* output("Closing file: %s\n", ACCT_FILE); */		/* debug */
 
    close_file(fd);
    output("\n%s\n", buf);
    free(buf);
cont:
    output(MSG_INNAME);
    input("", name, LINE_LEN, 0, 0, 0);
errlogin:
    output(MSG_INLOGIN);
    input("", login, 25, 0, 0, 0);
    pw = getpwnam(login);
    if (pw != NULL) {
        output("\n%s\n\n", MSG_UIDINUSE);
        goto errlogin;
    }
    output(MSG_INPASSWD);
    input("", passwd, 13, 0, 0, 0);
    
    /* It's 2025 so it makes sense to ask for an e-mail address, right? 2025-07-24, PL */
    output(MSG_INPOST);
    input("", epost, 70, 0, 0, 0);
    /* Modem user? Yes by default. Not sure yet why it matters. Why ask the user at all? PL */   
 /* output(MSG_INMODEM);
    input(MSG_YES, inet, 7, 0, 0, 0); */
    
    /* Let's not ask for phone number here - too long registrations tend to upset users ;) PL */
/*  output(MSG_INTELE);
    input("", tele, 70, 0, 0, 0); */
    output("\n");

    strcpy(outbuf, "\n");
    strcpy(outbuf, "------------------------------------------------\n");
    strcat(outbuf, MSG_ACCAPP);
    time_string(time(0), cmdline, 1);
    strcat(outbuf, cmdline);
    strcat(outbuf, "\n------------------------------------------------\n");
    strcat(outbuf, MSG_INSNAME);
    strcat(outbuf, name);
    strcat(outbuf, MSG_INSLOGIN);
    strcat(outbuf, login);
    strcat(outbuf, MSG_INSPASSWD);
    strcat(outbuf, passwd);
    strcat(outbuf, MSG_INSPOST);
    strcat(outbuf, epost);
    /*
    strcat(outbuf, MSG_INSMODEM);
    strcat(outbuf, inet);
    strcat(outbuf, MSG_INSTELE);
    strcat(outbuf, tele);
    */
    strcat(outbuf, "\n");
    sprintf(fname, "/tmp/%d", getpid());
    fd = open_file(fname, OPEN_QUIET | OPEN_CREATE);
    {
    ssize_t n__ = write(fd, outbuf, strlen(outbuf));
    if (n__ < 0) {
        /* TODO: perror("write"); */
    }
}
    
 /* printf("Closing file: %s\n", fname); */		/* debug */
    close_file(fd);
    if (fork()) {
        (void) wait(&fd);
    } else {
        close(0);
        close(1);
        close(2);
        (void) open(fname, O_RDONLY);
        (void) open("/dev/null", O_WRONLY);
        {						/* Small tweak to keep linux compiler happy PL 2025-08-10 */
        int d__ = dup(1);
        if (d__ >= 0) close(d__);
    }
        execl(MAILPRGM, MAILPRGM, SKLAFF_ACCT, (char *) 0);
    }
    unlink(fname);
    fd = open_file(ACCT_LOG, OPEN_QUIET | OPEN_CREATE);
    lseek(fd, 0L, 2);
    {
    ssize_t n__ = write(fd, outbuf, strlen(outbuf));
    if (n__ < 0) {
        /* TODO perror("write"); in the future PL 2025-08-10 */
    }
}
 /* printf("Closing file: %s\n", ACCT_LOG); */		/* debug */
    close_file(fd);
    output(MSG_APPLIED);
    sleep(1);
    output("ATH0\n");
    fflush(stdout);
    sleep(2);
    output("NO CARRIER\n\n");
    fflush(stdout);
    sleep(1);
    tty_reset();
    exit(0);
}

/* rawio.c */

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
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * write_file - write file to disk
 * args: file to write (filedes), buffer to write (buf)
 * ret: failure (-1), ok (0, 1)
 */

int
write_file(int fildes, char *buf)
{
    char *func_name = "write_file";
    long length;

    if (fildes == -1)
        return -1;

    if ((off_t) lseek(fildes, 0L, 0) == -1) {
        sys_error(func_name, 1, "lseek");
        return -1;
    }
    length = strlen(buf);
    if (write(fildes, buf, length) == -1) {
        sys_error(func_name, 2, "write");
        return -1;
    }
    if (ftruncate(fildes, (off_t) length) == -1) {
        sys_error(func_name, 3, "ftruncate");
        return 1;
    }
    free(buf);

    return 0;
}

/*
 * read_file - read file from disk
 * args: file to read (filedes)
 * ret: buffer containing file or NULL
 */

char *
read_file(int fildes)
{
    struct stat s;
    char *buf;

    fstat(fildes, &s);
    buf = (char *) malloc(s.st_size + 1);
    memset(buf, 0, s.st_size + 1);
    lseek(fildes, 0L, 0);
    if (read(fildes, buf, s.st_size) == s.st_size) {
        return buf;
    } else {
        sys_error("read_file", 1, "read");
        return NULL;
    }
}


/*
 * open_file - open file and lock it (modernized 2025-09-15, PL)
 * args: const char *filename, int flag (OPEN_CREATE, OPEN_QUIET)
 * ret : file descriptor or -1 on failure
 */

int
open_file(const char *filename, int flag)
{
    int mode = O_RDWR;
    int fd;
    int count = 0;
    char dentry[180];
    const char *function_name = "open_file";

    if (flag & OPEN_CREATE) {
        mode |= O_CREAT;
    }

    fd = open(filename, mode, NEW_FILE_MODE);
    if (fd == -1) {
        if (!(flag & OPEN_QUIET)) {
            sys_error(function_name, 1, "open");
        }
        return -1;
    }

    lseek(fd, 0L, SEEK_SET);

    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        snprintf(dentry, sizeof dentry, "file lock on [%s]", filename);
        debuglog(dentry, 10);

        while (flock(fd, LOCK_EX | LOCK_NB) == -1 && count < 30) {
            sleep(1);
            count++;
        }

        if (count >= 30) {
            snprintf(dentry, sizeof dentry, "file [%s] reached lock timeout", filename);
            debuglog(dentry, 10);
            close(fd);
            return -1;
        }

        snprintf(dentry, sizeof dentry, "file [%s] lock acquired after %d sec", filename, count);
        debuglog(dentry, 10);
    }

    return fd;
}
int
close_file(int filedesc)
{
    /* "Error handling" added by PL 2025-07-07 */
    /* only unlock if file descriptor is still valid */
    
    if (filedesc >= 0 && fcntl(filedesc, F_GETFD) != -1) {
        unlock(filedesc);
    }
    return close(filedesc);
}



/*
 * create_file - create file and lock it
 * args: filename (filename)
 * ret: ok (0), failure (-1)
 */

int
create_file(char *filename)
{

    int tmp_filedesc;
    char *function_name = "open_file";

    /** O_SYNC might be used for some files?**/
    if ((tmp_filedesc = open(filename, O_RDWR | O_CREAT | O_TRUNC, NEW_FILE_MODE)) == -1)
        sys_error(function_name, 1, "open");
    else
        lock(tmp_filedesc);     /** Error checking ! **/

    return tmp_filedesc;

}

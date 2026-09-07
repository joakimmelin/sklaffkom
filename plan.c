/* plan.c */

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
 */

#define PLAN_MAX_SIZE (64 * 1024)

#if defined(LINUX) || defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#define LOGTAG "plan"

#include "sklaff.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#ifndef O_NOFOLLOW
#error "plan.c requires O_NOFOLLOW"
#endif


enum plan_child_status {
    PLAN_CHILD_OK = 0,
    PLAN_CHILD_DROP_PRIVS = 20,
    PLAN_CHILD_OPEN = 21,
    PLAN_CHILD_STAT = 22,
    PLAN_CHILD_UNSAFE = 23,
    PLAN_CHILD_CHMOD = 24,
    PLAN_CHILD_TRUNCATE = 25,
    PLAN_CHILD_WRITE = 26,
    PLAN_CHILD_CLOSE = 27,
    PLAN_CHILD_READ = 28
};


static const char *
plan_child_status_text(int status)
{
    switch (status) {
    case PLAN_CHILD_DROP_PRIVS:
        return "could not drop privileges";
    case PLAN_CHILD_OPEN:
        return "could not open/create .plan";
    case PLAN_CHILD_STAT:
        return "could not inspect .plan";
    case PLAN_CHILD_UNSAFE:
        return "unsafe .plan file";
    case PLAN_CHILD_CHMOD:
        return "could not set .plan mode";
    case PLAN_CHILD_TRUNCATE:
        return "could not truncate .plan";
    case PLAN_CHILD_WRITE:
        return "could not write .plan";
    case PLAN_CHILD_CLOSE:
        return "could not close .plan";
    case PLAN_CHILD_READ:
        return "could not read .plan";
    default:
        return "unknown .plan error";
    }
}


/*
 * Build ~/.plan for the real Unix login user.
 *
 * SklaffKOM is setuid to the shared sklaff account, but .plan belongs
 * to the Unix account which actually started the session.
 *
 * Return:
 *   0  path ready
 *   1  deliberately skipped (uid is not the real login uid)
 *  -1  error
 */
static int
plan_path_for_current_user(int uid, char *plan, size_t plansz)
{
    struct passwd *pw;
    uid_t ruid;
    int n;

    if (plan == NULL || plansz == 0)
        return -1;

    plan[0] = '\0';

    if (uid <= 0)
        return -1;

    ruid = getuid();

    /*
     * Never allow the setuid SklaffKOM process to use this helper to
     * touch another Unix user's home directory.
     */
    if ((uid_t)uid != ruid) {
        dlog(8, "skipping .plan for uid %d; real uid is %ld",
            uid, (long)ruid);
        return 1;
    }

    pw = getpwuid((uid_t)uid);

    if (pw == NULL || pw->pw_dir == NULL || pw->pw_dir[0] != '/') {
        dlog(3, "cannot resolve Unix home for uid %d", uid);
        return -1;
    }

    n = snprintf(plan, plansz, "%s/.plan", pw->pw_dir);

    if (n < 0 || (size_t)n >= plansz) {
        dlog(3, ".plan path too long for uid %d", uid);
        return -1;
    }

    return 0;
}


/*
 * Permanently drop the child process to the real Unix user.
 *
 * The parent SklaffKOM process keeps its normal setuid identity.
 * Only the short-lived child loses the sklaff credentials.
 */
static int
plan_drop_to_real_user(uid_t uid)
{
#if defined(LINUX) || defined(__linux__) || defined(__FreeBSD__)

    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    /*
     * SklaffKOM is not normally setgid, but discard any saved setgid
     * state too in case that ever changes.
     */
    if (getresgid(&rgid, &egid, &sgid) != 0)
        return -1;

    if (setresgid(rgid, rgid, rgid) != 0)
        return -1;

    if (getresgid(&rgid, &egid, &sgid) != 0 ||
        rgid != egid ||
        rgid != sgid)
        return -1;

    /*
     * uid is the process' real uid.  Replace real, effective and saved
     * uid with it.  The child can no longer regain the setuid sklaff uid.
     */
    if (setresuid(uid, uid, uid) != 0)
        return -1;

    if (getresuid(&ruid, &euid, &suid) != 0 ||
        ruid != uid ||
        euid != uid ||
        suid != uid)
        return -1;

#else

    /*
     * Conservative fallback for older Unix variants.
     * The child performs no untrusted work and exits immediately.
     */
    if (setuid(uid) != 0)
        return -1;

    if (getuid() != uid || geteuid() != uid)
        return -1;

#endif

    return 0;
}


/*
 * Open ~/.plan safely.
 *
 * If it does not exist, create it mode 0600.
 *
 * Existing files keep their current mode and ACL.  This is intentional:
 * a SysOp may independently configure fingerd access to the file.
 */
static int
plan_open_safe(const char *path, uid_t uid, int *created)
{
    struct stat st;
    int fd;
    int flags;

    if (path == NULL || created == NULL)
        return -PLAN_CHILD_OPEN;

    *created = 0;

    flags = O_WRONLY | O_NOFOLLOW | O_CLOEXEC;

    /*
     * First try the existing file without changing anything.
     */
    fd = open(path, flags);

    /*
     * Missing file: create it as the real Unix user.
     *
     * O_EXCL prevents us from accidentally following something that
     * appears between the first open() and this one.
     */
    if (fd == -1 && errno == ENOENT) {
        fd = open(path,
            flags | O_CREAT | O_EXCL,
            0600);

        if (fd != -1) {
            *created = 1;
        } else if (errno == EEXIST) {
            /*
             * Someone created the file between our two calls.
             * Re-open it normally, still with O_NOFOLLOW.
             */
            fd = open(path, flags);
        }
    }

    if (fd == -1)
        return -PLAN_CHILD_OPEN;

    if (fstat(fd, &st) != 0) {
        close(fd);
        return -PLAN_CHILD_STAT;
    }

    /*
     * Only a normal file owned by this Unix user is acceptable.
     *
     * st_nlink == 1 also prevents using a hard link to trick
     * SklaffKOM into overwriting some other file owned by the user.
     */
    if (!S_ISREG(st.st_mode) ||
        st.st_uid != uid ||
        st.st_nlink != 1) {
        close(fd);
        return -PLAN_CHILD_UNSAFE;
    }

    /*
     * New .plan files start private.
     *
     * Do NOT chmod an existing file: the SysOp may deliberately have
     * configured its read permissions or ACL for fingerd.
     */
    if (*created && fchmod(fd, 0600) != 0) {
        close(fd);
        return -PLAN_CHILD_CHMOD;
    }

    return fd;
}


static int
plan_child_do(const char *path, uid_t uid,
    const char *text, size_t textlen, int do_write)
{
    const char *p;
    size_t left;
    int created;
    int fd;

    if (plan_drop_to_real_user(uid) != 0)
        return PLAN_CHILD_DROP_PRIVS;

    fd = plan_open_safe(path, uid, &created);

    if (fd < 0)
        return -fd;

    /*
     * plan_ensure() stops here.  The file now exists and has passed
     * all safety checks.
     */
    if (!do_write) {
        if (close(fd) != 0)
            return PLAN_CHILD_CLOSE;

        return PLAN_CHILD_OK;
    }

    /*
     * Verify first, truncate second.  We never truncate an object until
     * fstat() has established that it is a safe .plan belonging to uid.
     */
    if (ftruncate(fd, 0) != 0) {
        close(fd);
        return PLAN_CHILD_TRUNCATE;
    }

    p = text;
    left = textlen;

    while (left > 0) {
        ssize_t n;

        n = write(fd, p, left);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            close(fd);
            return PLAN_CHILD_WRITE;
        }

        if (n == 0) {
            close(fd);
            return PLAN_CHILD_WRITE;
        }

        p += n;
        left -= (size_t)n;
    }

    if (close(fd) != 0)
        return PLAN_CHILD_CLOSE;

    return PLAN_CHILD_OK;
}

/*
 * Read ~/.plan as the real Unix user and send its contents to the
 * parent through pipefd.
 */
static int
plan_child_read(const char *path, uid_t uid, int pipefd)
{
    struct stat st;
    char buf[4096];
    ssize_t n;
    int fd;

    if (plan_drop_to_real_user(uid) != 0)
        return PLAN_CHILD_DROP_PRIVS;

    fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);

    if (fd < 0)
        return PLAN_CHILD_OPEN;

    if (fstat(fd, &st) != 0) {
        close(fd);
        return PLAN_CHILD_STAT;
    }

    if (!S_ISREG(st.st_mode) ||
        st.st_uid != uid ||
        st.st_nlink != 1) {
        close(fd);
        return PLAN_CHILD_UNSAFE;
    }

    for (;;) {
        n = read(fd, buf, sizeof(buf));

        if (n < 0) {
            if (errno == EINTR)
                continue;

            close(fd);
            return PLAN_CHILD_READ;
        }

        if (n == 0)
            break;

        {
            char *p = buf;
            ssize_t left = n;

            while (left > 0) {
                ssize_t written;

                written = write(pipefd, p, (size_t)left);

                if (written < 0) {
                    if (errno == EINTR)
                        continue;

                    close(fd);
                    return PLAN_CHILD_WRITE;
                }

                if (written == 0) {
                    close(fd);
                    return PLAN_CHILD_WRITE;
                }

                p += written;
                left -= written;
            }
        }
    }

    if (close(fd) != 0)
        return PLAN_CHILD_CLOSE;

    return PLAN_CHILD_OK;
}

/*
 * Run .plan filesystem work in a child.
 *
 * This is important for a setuid SklaffKOM:
 *
 *   parent: real uid = user, effective uid = sklaff
 *   child : real/effective/saved uid = user
 *
 * The parent never gives up its SklaffKOM credentials.
 */
static int
plan_run(int uid, const char *text, int do_write)
{
    char plan[PATH_MAX];
    size_t textlen;
    pid_t pid;
    int status;
    int rc;

    rc = plan_path_for_current_user(uid, plan, sizeof(plan));

    if (rc == 1)
        return 0;       /* deliberately skipped */

    if (rc != 0)
        return -1;

    textlen = 0;

    if (do_write) {
        if (text == NULL)
            text = "";

        textlen = strlen(text);
    }

    pid = fork();

    if (pid < 0) {
        dlog_errno(3, "fork for .plan");
        return -1;
    }

    if (pid == 0) {
        rc = plan_child_do(plan,
            (uid_t)uid,
            text,
            textlen,
            do_write);

        _exit(rc);
    }

    do {
        rc = waitpid(pid, &status, 0);
    } while (rc < 0 && errno == EINTR);

    if (rc < 0) {
        dlog_errno(3, "waitpid for .plan");
        return -1;
    }

    if (!WIFEXITED(status)) {
        if (WIFSIGNALED(status)) {
            dlog(3, ".plan child for uid %d died on signal %d",
                uid, WTERMSIG(status));
        } else {
            dlog(3, ".plan child for uid %d ended abnormally",
                uid);
        }

        return -1;
    }

    rc = WEXITSTATUS(status);

    if (rc != PLAN_CHILD_OK) {
        dlog(3,
            ".plan operation failed for uid %d: %s (status %d)",
            uid,
            plan_child_status_text(rc),
            rc);

        return -1;
    }

    return 0;
}


/*
 * Ensure that ~/.plan exists.
 *
 * New files are created as the real Unix user with mode 0600.
 * Existing files are verified but otherwise left untouched.
 */
int
plan_ensure(int uid)
{
    return plan_run(uid, NULL, 0);
}


/*
 * Write text to ~/.plan.
 *
 * Missing files are created automatically as the real Unix user.
 */
 
int
plan_write(int uid, const char *text)
{
    if (text == NULL)
        text = "";

    if (strlen(text) > PLAN_MAX_SIZE) {
        dlog(3, ".plan too large for uid %d", uid);
        return -1;
    }

    return plan_run(uid, text, 1);
}

/*
 * Read ~/.plan.
 *
 * The returned buffer is malloced and must be freed by the caller.
 * An empty .plan returns an allocated empty string.
 */
int
plan_read(int uid, char **text)
{
    char plan[PATH_MAX];
    char tmp[4096];
    char *buf;
    char *newbuf;
    size_t used;
    size_t size;
    ssize_t n;
    pid_t pid;
    int pipefd[2];
    int status;
    int rc;

    if (text == NULL)
        return -1;

    *text = NULL;

    rc = plan_path_for_current_user(uid, plan, sizeof(plan));

    if (rc != 0)
        return -1;

    if (pipe(pipefd) != 0) {
        dlog_errno(3, "pipe for .plan");
        return -1;
    }

    pid = fork();

    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        dlog_errno(3, "fork for .plan read");
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);

        rc = plan_child_read(plan,
            (uid_t)uid,
            pipefd[1]);

        close(pipefd[1]);
        _exit(rc);
    }

    close(pipefd[1]);

    size = 4096;
    used = 0;

    buf = malloc(size);

    if (buf == NULL) {
        close(pipefd[0]);

        do {
            rc = waitpid(pid, &status, 0);
        } while (rc < 0 && errno == EINTR);

        return -1;
    }

    for (;;) {
        n = read(pipefd[0], tmp, sizeof(tmp));
        
        if (n < 0) {
            if (errno == EINTR)
                continue;

            free(buf);
            close(pipefd[0]);

            do {
                rc = waitpid(pid, &status, 0);
            } while (rc < 0 && errno == EINTR);

            return -1;
        }

        if (n == 0)
            break;

        if (used + (size_t)n > PLAN_MAX_SIZE) {
            free(buf);
            close(pipefd[0]);

            do {
                rc = waitpid(pid, &status, 0);
            } while (rc < 0 && errno == EINTR);

            dlog(3, ".plan too large for uid %d", uid);
            return -1;
        }

        if (used + (size_t)n + 1 > size) {
            size_t newsize = size;

            while (used + (size_t)n + 1 > newsize)
                newsize *= 2;

            newbuf = realloc(buf, newsize);

            if (newbuf == NULL) {
                free(buf);
                close(pipefd[0]);

                do {
                    rc = waitpid(pid, &status, 0);
                } while (rc < 0 && errno == EINTR);

                return -1;
            }

            buf = newbuf;
            size = newsize;
        }

        memcpy(buf + used, tmp, (size_t)n);
        used += (size_t)n;
    }

    close(pipefd[0]);

    do {
        rc = waitpid(pid, &status, 0);
    } while (rc < 0 && errno == EINTR);

    if (rc < 0) {
        free(buf);
        dlog_errno(3, "waitpid for .plan read");
        return -1;
    }

    if (!WIFEXITED(status)) {
        free(buf);
        dlog(3, ".plan read child for uid %d ended abnormally", uid);
        return -1;
    }

    rc = WEXITSTATUS(status);

    if (rc != PLAN_CHILD_OK) {
        free(buf);

        dlog(3,
            ".plan read failed for uid %d: %s (status %d)",
            uid,
            plan_child_status_text(rc),
            rc);

        return -1;
    }

    buf[used] = '\0';
    *text = buf;

    return 0;
}

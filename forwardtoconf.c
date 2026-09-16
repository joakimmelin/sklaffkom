#include "sklaff.h"
#include "globals.h"
#include <pwd.h>
#include <fcntl.h>
#include <signal.h>

static int send_mail(int conf, char *mbuf, int ouid, int ogrp);

int
main(int argc, char *argv[])
{
    struct passwd *pw;
    char *ptr, *ptr2, *buf, *oldbuf;
    int conf, fd, count = 0;

    if (argc != 3) {
        fprintf(stderr, "\n%s\n\n", MSG_FTCINFO);
        exit(1);
    }

    conf = conf_num(argv[2]);
    if (conf == -1) {
        fprintf(stderr, "\n");
        fprintf(stderr, MSG_FTCNOCONF, argv[2]);
        fprintf(stderr, "\n\n");
        exit(1);
    }

    pw = getpwnam(SKLAFF_ACCT);
    if (pw == NULL) {
        fprintf(stderr, "\n");
        fprintf(stderr, MSG_FTCNOACCOUNT, SKLAFF_ACCT);
        fprintf(stderr, "\n\n");
        exit(1);
    }

    if ((fd = open_file(argv[1], 0)) == -1) {
        fprintf(stderr, "\n");
        fprintf(stderr, MSG_FTCOPEN, argv[1]);
        fprintf(stderr, "\n\n");
        exit(1);
    }

    if ((buf = read_file(fd)) == NULL) {
        fprintf(stderr, "\n");
        fprintf(stderr, MSG_FTCREAD, argv[1]);
        fprintf(stderr, "\n\n");
        exit(1);
    }

    oldbuf = buf;

    if (close_file(fd) == -1) {
        fprintf(stderr, "\n");
        fprintf(stderr, MSG_FTCCLOSE, argv[1]);
        fprintf(stderr, "\n\n");
        free(oldbuf);
        exit(1);
    }

    if (strlen(buf) < 50) {
        fprintf(stderr, "\n");
        fprintf(stderr, MSG_FTCEMPTY, argv[1]);
        fprintf(stderr, "\n\n");
        free(oldbuf);
        exit(1);
    }

    while (1) {
        ptr = strstr(buf, "\nFrom ");
        if (ptr) {
            *ptr = '\0';

            if (send_mail(conf, buf, pw->pw_uid, pw->pw_gid) == -1) {
                fprintf(stderr, "\n");
                fprintf(stderr, MSG_FTCPOSTFAIL, argv[2]);
                fprintf(stderr, "\n\n");
                free(oldbuf);
                exit(1);
            }

            count++;
            *ptr = '\n';
            buf = ptr + 1;
        } else
            break;
    }

    ptr = buf;
    while (1) {
        ptr2 = ptr;
        ptr++;
        ptr = strchr(ptr, '\n');
        if (!ptr)
            break;
    }
    *ptr2 = '\0';

    if (send_mail(conf, buf, pw->pw_uid, pw->pw_gid) == -1) {
        fprintf(stderr, "\n");
        fprintf(stderr, MSG_FTCPOSTFAIL, argv[2]);
        fprintf(stderr, "\n\n");
        free(oldbuf);
        exit(1);
    }

    count++;
    free(oldbuf);

    notify_all_processes(SIGNAL_NEW_TEXT);

    if (unlink(argv[1]) == -1) {
        fprintf(stderr, "\n");
        fprintf(stderr, MSG_FTCUNLINK, argv[1]);
        fprintf(stderr, "\n\n");
        exit(1);
    }

    printf("\n");
    if (count == 1)
        printf(MSG_FTCOKONE, argv[2]);
    else
        printf(MSG_FTCOKMANY, count, argv[2]);
    printf("\n\n");

    exit(0);
}

static int
send_mail(int conf, char *mbuf, int ouid, int ogrp)
{
    char conffile[256], confdir[256], textfile[512];
    struct CONF_ENTRY ce;
    struct TEXT_HEADER th;
    int fd, fdo;
    int found = 0;
    char *buf, *oldbuf, *nbuf, *ptr, *tmp, *fbuf;

    snprintf(conffile, sizeof(conffile), "%s", CONF_FILE);
    snprintf(confdir, sizeof(confdir), "%s/%d/", SKLAFF_DB, conf);

    if ((fd = open_file(conffile, 0)) == -1)
        return -1;
    if ((buf = read_file(fd)) == NULL)
        return -1;
    oldbuf = buf;

    while ((buf = get_conf_entry(buf, &ce))) {
        if (ce.num == conf) {
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("\n%s\n\n", MSG_CONFMISSING);
        return -1;
    }

    ce.last_text++;

    nbuf = replace_conf(&ce, oldbuf);
    if (!nbuf) {
        printf("\n%s\n\n", MSG_CONFMISSING);
        return -1;
    }
    snprintf(textfile, sizeof(textfile), "%s%ld", confdir, ce.last_text);
    if ((fdo = open_file(textfile, OPEN_QUIET | OPEN_CREATE)) == -1) {
        printf("\n%s\n\n", MSG_ERRCREATET);
        return -1;
    }
    ptr = mbuf;
    while (*ptr) {
        if ((unsigned char) *ptr == 134)
            *ptr = '}';
        else if ((unsigned char) *ptr == 132)
            *ptr = '{';
        else if ((unsigned char) *ptr == 148)
            *ptr = '|';
        else if ((unsigned char) *ptr == 143)
            *ptr = ']';
        else if ((unsigned char) *ptr == 142)
            *ptr = '[';
        else if ((unsigned char) *ptr == 153)
            *ptr = 0x05c;
        if ((unsigned char) *ptr == 229)
            *ptr = '}';
        else if ((unsigned char) *ptr == 228)
            *ptr = '{';
        else if ((unsigned char) *ptr == 246)
            *ptr = '|';
        else if ((unsigned char) *ptr == 197)
            *ptr = ']';
        else if ((unsigned char) *ptr == 196)
            *ptr = '[';
        else if ((unsigned char) *ptr == 214)
            *ptr = 0x05c;
        if ((unsigned char) *ptr == 140)
            *ptr = '}';
        else if ((unsigned char) *ptr == 138)
            *ptr = '{';
        else if ((unsigned char) *ptr == 154)
            *ptr = '|';
        else if ((unsigned char) *ptr == 129)
            *ptr = ']';
        else if ((unsigned char) *ptr == 128)
            *ptr = '[';
        else if ((unsigned char) *ptr == 133)
            *ptr = 0x05c;
        ptr++;
    }

    ptr = strstr(mbuf, MSG_EMSUB);
    if (ptr) {
        ptr = ptr + strlen(MSG_EMSUB);
        tmp = strchr(ptr, '\n');
        *tmp = '\0';
        strncpy(th.subject, ptr, (SUBJECT_LEN - 2));
        th.subject[SUBJECT_LEN - 1] = 0;
        *tmp = '\n';
    } else
        strcpy(th.subject, "");

    /* Count lines without modifying mbuf */

    th.size = 1;
    ptr = mbuf;

    while ((ptr = strchr(ptr, '\n')) != NULL) {
        th.size++;
        ptr++;
    }

    th.time = time(0);

    {
        char header[256];
        size_t fbuf_len;

        snprintf(header, sizeof(header),
            "%ld:%d:%ld:%ld:%d:%d:%d\n",
            ce.last_text, 0, th.time, 0L, 0, 0, th.size);

        fbuf_len = strlen(header)
                 + strlen(th.subject) + 1
                 + strlen(mbuf) + 1
                 + 1;

        fbuf = malloc(fbuf_len);
        if (fbuf == NULL) {
            sys_error("send_mail", 1, "malloc");
            return -1;
        }

        snprintf(fbuf, fbuf_len, "%s%s\n%s\n",
            header, th.subject, mbuf);
    }

    if (write_file(fdo, fbuf) == -1)
        return -1;
    if (close_file(fdo) == -1)
        return -1;

    if (chown(textfile, ouid, ogrp) == -1) {
        sys_error("send_mail", 1, "chown");
        return -1;
    }

    if (write_file(fd, nbuf) == -1)
        return -1;
    if (close_file(fd) == -1)
        return -1;

    return 0;
}

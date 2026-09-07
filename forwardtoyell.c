#include "sklaff.h"
#include "globals.h"
#include <pwd.h>
#include <fcntl.h>
#include <signal.h>

void
get_phoneno(char *inbuf, char *utbuf)
{
    int c, d, i;

    utbuf[0] = 0;

    c = strlen(inbuf) - 1;

    while (c > 0) {

        while (c > 0 && (inbuf[c] < '0' || inbuf[c] > '9'))
            c--;
        if (c == 0)
            return;
        d = c;
        while (d > 0 && (inbuf[d] >= '0' && inbuf[d] <= '9'))
            d--;

        if (c - d <= 12 && c - d >= 6) {
            for (i = d + 1; i <= c; i++)
                utbuf[i - d - 1] = inbuf[i];
            utbuf[c - d] = 0;
            return;
        }
        c = d;
    }
}

int
main(int argc, char *argv[])
{
    char *ptr, *ptr2, *buf, *oldbuf;
    unsigned char *mpt;
    int len, fd, c, plen;
    LINE msg, phone, prefix;

    if (argc != 2) {
        printf("\nsyntax: forwardtoyell <file>s\n\n");
        exit(1);
    }
    if ((fd = open_file(argv[1], 0)) == -1)
        exit(1);
    if ((buf = read_file(fd)) == NULL)
        exit(1);
    oldbuf = buf;
    if (close_file(fd) == -1)
        exit(1);
    /* unlink(argv[1]);  */


    ptr = strstr(buf, "\n\n");

    if (ptr) {
        ptr = ptr + 2;
        get_phoneno(ptr, phone);
        strcpy(prefix, "SMS: ");
        if (strlen(phone) > 0)
            sprintf(prefix, "SMS (%s): ", phone);
        plen = strlen(prefix);

        ptr2 = strstr(ptr, "!s");
        if (!ptr2)
            ptr2 = strstr(ptr, "!S");
        if (ptr2)
            *ptr2 = 0;

        c = 0;
        do {
            ptr2 = strchr(ptr, '\n');
            if (!ptr2)
                len = strlen(ptr);
            else
                len = ptr2 - ptr;
            ptr2 = ptr + len + 1;
            if (len > 78 - plen) {
                len = 78 - plen;
                ptr2 = ptr + len;
            }
            strcpy(msg, prefix);
            strncat(msg, ptr, len);
            msg[len + plen] = 0;
            if (len > 0) {

                /* Safeguard from unwanted control characters */

                for (mpt = msg; *mpt; mpt++)
                    if (*mpt < 32)
                        *mpt = '*';

                if (send_msg_to_all(MSG_SMS, msg) == -1)
                    exit(1);
                /* printf("%d %ld %ld [%s]\n", len, ptr, ptr2,msg);   */
                c++;
            }
            ptr = ptr2;
        } while (*(ptr2 - 1) != 0 && c < 5);
    }
    free(oldbuf);

    exit(0);
}

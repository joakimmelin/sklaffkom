/*
 * ftnmsgdump.c - debug dumper for Fido/FTN .MSG files
 *
 * Build:
 *   cc -Wall -Wextra -O2 -o ftnmsgdump ftnmsgdump.c ftnmsg.c
 *
 * modified on 2026-06-09, PL
 */

#include "ftnmsg.h"

#include <stdio.h>

static void
print_visible_ctrl(const char *s)
{
    if (s == NULL)
        return;

    while (*s) {
        if ((unsigned char)*s == '\001')
            fputs("^A", stdout);
        else
            putchar((unsigned char)*s);
        s++;
    }
}

int
main(int argc, char **argv)
{
    struct fido_msg msg;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file.msg\n", argv[0]);
        return 1;
    }

    if (read_fido_msg(argv[1], &msg) != 0)
        return 1;

    printf("File:    %s\n", argv[1]);
    printf("From:    %s\n", msg.from);
    printf("To:      %s\n", msg.to);
    printf("Subject: %s\n", msg.subject);
    printf("Date:    %s\n", msg.date);

    if (msg.chrs[0] != '\0')
        printf("CHRS:    %s\n", msg.chrs);
    if (msg.msgid[0] != '\0')
        printf("MSGID:   %s\n", msg.msgid);
    if (msg.reply[0] != '\0')
        printf("REPLY:   %s\n", msg.reply);
    if (msg.tid[0] != '\0')
        printf("TID:     %s\n", msg.tid);
    if (msg.tzutc[0] != '\0')
        printf("TZUTC:   %s\n", msg.tzutc);

    printf("\n--- CLEAN BODY ---\n");
    if (msg.clean_body != NULL)
        printf("%s", msg.clean_body);

    printf("\n--- RAW BODY ---\n");
    if (msg.raw_body != NULL)
    	print_visible_ctrl(msg.raw_body);

    free_fido_msg(&msg);

    return 0;
}

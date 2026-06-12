#ifndef FTNMSG_H
#define FTNMSG_H

#include <stddef.h>

#define FIDO_FROM_LEN    36
#define FIDO_TO_LEN      36
#define FIDO_SUBJ_LEN    72
#define FIDO_DATE_LEN    20
#define FIDO_HDR_SIZE    190

struct fido_msg {
    char from[FIDO_FROM_LEN + 1];
    char to[FIDO_TO_LEN + 1];
    char subject[FIDO_SUBJ_LEN + 1];
    char date[FIDO_DATE_LEN + 1];

    char chrs[128];
    char msgid[256];
    char reply[256];
    char tid[256];
    char tzutc[64];

    /*
     * raw_body:
     *   Body after the 190-byte .MSG header, converted from CR to LF,
     *   with FTN kludges preserved as lines beginning with '\001'.
     *
     * clean_body:
     *   Display/debug version with FTN kludges, AREA, SEEN-BY and PATH hidden,
     *   and pipe color codes stripped.
     */
    char *raw_body;
    char *clean_body;
};

int read_fido_msg(const char *path, struct fido_msg *msg);
void free_fido_msg(struct fido_msg *msg);

#endif /* FTNMSG_H */

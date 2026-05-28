/* struct.h */

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

struct SURVEY_HEADER {
    int n_questions;
    time_t time;
};

struct TEXT_HEADER {
    long num;
    int author;
    time_t time;
    long comment_num;
    int comment_conf;
    int comment_author;
    int size;
    LINE subject;
    int type;
    struct SURVEY_HEADER sh;    /* Only used for surveys */
};


struct EDIT_BUF {
    LINE line;
    struct EDIT_BUF *next;
    struct EDIT_BUF *previous;
};

struct USER_ENTRY {
    int num;                    /* user's UID */
    time_t last_session;        /* time of last session (logout) */
    LINE name;                  /* user's name */
};

struct FILE_ENTRY {
    LINE name;                  /* filename */
    LINE desc;                  /* file description */
};

struct CONF_ENTRY {
    int num;                    /* number of conf. */
    long last_text;             /* number of last text in conf. */
    int creator;                /* creator of conference */
    time_t time;                /* time conf. was created */
    int type;                   /* type of conf. */
    int life;                   /* Number of days before texts expire */
    int comconf;                /* Move comments here */
    LINE name;                  /* name of conf. */
};

typedef int (cmd_func_t)(char *);
struct COMMAND_ENTRY {
    char *name;                 /* name of function */
    cmd_func_t *ptr;            /* pointer to function */
};

struct PARSE_ENTRY {
    LINE func;                  /* name of function to call */
    LINE cmd;                   /* specification of the command */
    LINE help;                  /* a short description of what it does */
    cmd_func_t *addr;           /* address of the function to call */
};

struct USER_LIST {
    int num;                    /* number of user */
    struct USER_LIST *next;     /* link to next entry in the list */
};

struct TEXT_BODY {
    LINE line;                  /* a row of text */
    struct TEXT_BODY *next;     /* link to next entry in the list */
};

struct COMMENT_LIST {
    long comment_num;           /* number of comment */
    int comment_author;         /* number of comment's author */
    struct COMMENT_LIST *next;  /* link to next entry in the list */
};

struct TEXT_ENTRY {
    struct TEXT_HEADER th;      /* the text header */
    struct TEXT_BODY *body;     /* the text body */
    struct COMMENT_LIST *cl;    /* the list of comments */
};

struct ACTIVE_ENTRY {
    int user;                   /* uid of user */
    int pid;                    /* process identification */
    time_t login_time;          /* time of login */
    int avail;                  /* available or busy */
    char from[FROM_FIELD_LEN];  /* logged in from */
    char tty[50];               /* terminal */
};

struct INT_LIST {               /* Confs-interval-list */
    long from;
    long to;
    struct INT_LIST *next;      /* Next entry */
};

struct CONFS_ENTRY {
    int num;                    /* Number of conf. */
    struct INT_LIST *il;        /* List with confs-intervals */
};

struct COM_STACK {
    long num;
    struct COM_STACK *next;
};

struct UR_STACK {
    int conf;
    long num;
    struct UR_STACK *next;
};

struct MSG_ENTRY {
    int type;                   /* What kind of message is this? */
    int num;                    /* Users uid */
    int direct;                 /* direct=0 -> uid=sender, direct=1 ->
                                 * uid=receiver */
    LINE msg;                   /* Message */
};

struct MSG_LIST {
    struct MSG_LIST *prev;
    struct MSG_ENTRY me;
};

struct UEL {
    struct USER_ENTRY ue;
    struct UEL *next;
};

struct CEL {
    LONG_LINE name;
    long unreads;
    int creator;
    int num;
    int type;
    struct CEL *next;
};

struct UEN {
    LINE name;
};

struct UET {
    LINE last_session;
    LINE name;
};

struct CEN {
    LONG_LINE name;
    long unreads;
    int creator;
    int num;
    int type;
};

struct SKLAFFRC {
    struct {
        char adress[80];
        char postnr[80];
        char ort[80];
        char tele1[80];
        char tele2[80];
        char tele3[80];
        char email1[80];
        char email2[80];
        char url[80];
        char org[80];
    } user;
    char note[4096];
    char sig[4096];
    char editor[80];
    char flags[400];
    char timeout[80];
    char paid[80];
    char login[4096];
    char paydate[80];
};

/* LikeEntry - one liked post 2025-10-24 PL */
struct LikeEntry {
    int confnum;                /* Conference number */
    long textnum;               /* Text number */
    time_t timestamp;           /* Unix timestamp of like */
    struct LikeEntry *next;     /* Linked list */
};

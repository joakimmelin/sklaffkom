/* ext_globals.h */

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

#include <signal.h>
#include <termios.h>
#ifndef LINUX
# include <sys/ttycom.h>
#endif
#include <sys/ioctl.h>

#ifdef LINUX
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

extern int Uid;                 /* Uid of current user	 */
extern LINE Home;               /* home_dir of current user */
extern LINE Mbox;               /* mbox_dir of current user */
extern int End_sklaff;          /* End sklaffKOM?	 */
extern int Num_commands;        /* Number of commands	 */
extern int Numlines;            /* Lines on terminal	 */
extern int Lines;               /* Lines displayed	 */
extern int Globalfd;            /* Used by line_ed	 */
extern int Rot13;               /* Rot13 enabled	 */
extern long Current_text;       /* Current text		 */
extern long Last_text;          /* Last text read	 */
extern int Current_author;      /* Uid of author of current */
extern int Last_author;         /* Uid of author of last text */
extern int Logging_in;          /* 1 during exec_login  */
extern int Current_conf;        /* Current conference	 */
extern int Last_conf;           /* Last conference	 */
extern long Size;               /* size of editing buf  */
extern struct EDIT_BUF *Start;  /* start of editing buf */
extern struct TEXT_HEADER *Globalth;    /* Used by line_ed	 */
extern const struct COMMAND_ENTRY command_list[];       /* Exported commands  */
extern struct PARSE_ENTRY Par_ent[MAX_COMMANDS];        /* Parse entries      */
extern struct COM_STACK *cstack;/* Comment stack	 */
extern struct UR_STACK *ustack, *rstack;        /* Unread/read stack	 */
extern struct UR_STACK *ustack2;/* Temp text stack      */
extern struct MSG_LIST *mlist;  /* Last msg received    */
extern int Interrupt_input;     /* Allow interrupts (talk, new_texts) */
extern int Cont;                /* Cont. output         */
extern char *sklaff_version;
extern char *sklaff_build_date;
extern char *sklaff_build_time;
extern char Program_name[80];   /* The name of the program */
extern LINE Prompt;             /* Last prompt written	   */
extern LINE Overflow;           /* Overflow from last line */
extern LINE Comstack[HISTORY_SIZE];     /* Command history         */
extern LINE Sub;                /* Last subject read	   */
extern int Comtop;              /* Top of command stack    */
extern int restart;             /* Restart program at logout */
extern int Nextconf;            /* Number of next conference */
extern long Nexttext;           /* Number of next text      */
extern int Change_prompt;       /* Prompt has been changed? */
extern int Change_msg;          /* Message has arrived?     */
extern int Timeout;             /* Inactive timeout (minutes) */
extern int Warning;             /* Timeout warning flag    */
extern int Say;                 /* Allow says		   */
extern int Shout;               /* Allow shouts		   */
extern int Present;             /* Allow present-messages  */
extern int Ibm;                 /* IBM mode		   */
extern int Iso8859;             /* ISO-8859 mode	   */
extern int Mac;                 /* Macintosh mode   	   */
extern int Old_who;             /* Old-style who list      */
extern int Subject_change;      /* Subject change wanted   */
extern int End_default;         /* Logout as default	   */
extern int Space;               /* Conservative space      */
extern int Copy;                /* Copy of mail sent	   */
extern int Author;              /* Author after text       */
extern int Special;             /* Special use, e.g. GUI client */
extern int Date;                /* Show date in textheader */
extern int Beep;                /* Allow beeps 		   */
extern int Clear;               /* Clear screen		   */
extern int Header;              /* email header		   */
extern int Presbeep;            /* Beep at present msg     */
extern int Ansi_output;		/* Allows ANSI colors      */
extern int Utf8;		/* UTF-8 mode		   */
extern struct termios Tty_mode;
extern sigset_t Oldmask;

extern int ActiveFD;            /* Global file descriptor used for the active
                                 * file.         */

/* Calles stuff for debugging */

/* extern long memstack[30000]; */
/* extern int mempointer;       */

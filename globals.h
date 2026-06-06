/* globals.h */

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

#include <termios.h>
#include <signal.h>

int Uid;                        /* Uid of current user	 */
LINE Home;                      /* home_dir of current user */
LINE Mbox;                      /* mbox_dir of current user */
int End_sklaff;                 /* End sklaffKOM?	 */
int Num_commands;               /* Number of commands	 */
int Numlines;                   /* Lines on terminal	 */
int Lines;                      /* Lines displayed	 */
int Globalfd;                   /* Used by line_ed	 */
int Rot13;                      /* Rot13 enabled	 */
long Current_text;              /* Current text		 */
int Current_author;             /* Uid of author of current text */
long Last_text;                 /* Last text read	 */
int Last_author;                /* Uid of author of last text */
int Logging_in;                 /* 1 during exec_login  */
int Current_conf;               /* Current conference	 */
int Last_conf;                  /* Last conference	 */
long Size;                      /* size of editing buf  */
struct EDIT_BUF *Start;         /* start of editing buf */
struct TEXT_HEADER *Globalth;   /* Used by line_ed	 */
struct PARSE_ENTRY Par_ent[MAX_COMMANDS];       /* Parse entries      */
struct COM_STACK *cstack;       /* Comment stack	 */
struct UR_STACK *ustack, *rstack;       /* Unread/read stack	 */
struct UR_STACK *ustack2;       /* Temp text stack      */
struct MSG_LIST *mlist;         /* Last msg received    */
int Interrupt_input = 0;        /* Allow interrupts (talk, new_texts) */
int Cont;                       /* Cont. output         */
extern char *sklaff_version;
extern char *sklaff_build_date;	/* Attempting to make a new "Version" command inside SklaffKOM */
extern char *sklaff_build_time; /* PL 2026-06-02 */
char Program_name[80];          /* The name of the program file */
LINE Prompt;                    /* Last prompt written		 */
LINE Overflow;                  /* Overflow from last line	 */
LINE Comstack[HISTORY_SIZE];    /* Command history              */
LINE Sub;                       /* Last subject read		 */
int Comtop;                     /* Top of command stack         */
int Nextconf;                   /* Number of next conference    */
long Nexttext;                  /* Number of next text          */
int restart;                    /* Restart program at logout    */
int Change_prompt;              /* Prompt has been changed? 	 */
int Change_msg;                 /* Message has arrived? 	 */
int Timeout;                    /* Inactive timeout (minutes)   */
int Warning;                    /* Timeout warning flag    	 */
int Say;                        /* Allow says			 */
int Shout;                      /* Allow shouts		        */
int Present;                    /* Allow present-messages	 */
int Ibm;                        /* IBM mode			 */
int Iso8859;                    /* ISO-8859  mode		 */
int Mac;                        /* Macintosh mode		 */
int Old_who;                    /* Old-style who list            */
int Subject_change;             /* Subject change wanted	 */
int End_default;                /* Logout as default		 */
int Space;                      /* Conservative space-handling   */
int Copy;                       /* Copy of mail sent		 */
int Author;                     /* Author after text		 */
int Date;                       /* Show date in textheader	 */
int Beep;                       /* Allow beeps 		   	 */
int Clear;                      /* Clear screen		   	 */
int Header;                     /* email header		   	 */
int Special;                    /* Special use, e.g. GUI client  */
int Presbeep;                   /* Beep at present msg        	 */
int Ansi_output;		/* Allow display of ANSI colors  */
int Utf8;                	/* UTF-8 mode			 */
struct termios Tty_mode;
sigset_t Oldmask;

int ActiveFD;                   /* Global file descriptor used for the active
                                 * file.         */

/* Calles stuff for debugging */

/* long memstack[30000];      */
/* int mempointer;            */

/* Work in progress PL 2025-07-31 */
/*
extern char last_opened_filename[PATH_MAX];
extern int last_opened_fd;
*/

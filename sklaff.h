/* sklaff.h */

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

#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

/* Define language desired, one of: SWEDISH or ENGLISH */
#define SWEDISH
//#define ENGLISH

/* Define group ids and terminaltypes for modem_pool */

#define MODEM_POOL 	"silly"
#define MODEM_GROUP	50
#define INET_GROUP	60

/* Allow news postings in news-conferences */

/* #undef POSTING_OK */
/* #define POSTING_OK */

/* Machine name for news postings */

#define MACHINE_NAME	"skom.se"

/* User to mail for new accounts */

#define SKLAFF_ACCT	"sklaff"

/* For "batch" command */

#define SKLAFF_ID	"SKOM II"
#define SKLAFF_LOC	"Stockholm, Sweden"
#define SKLAFF_NUM	"+46-8-7021174"
#define SKLAFF_SYSOP	"Carl Sundbom"

/* User ID of survey reporter account */

#define SKLAFF_SURVEY_REPORTER 5000

/* Default terminal used for clear screen if TERM not correct */

#define NO_TERM	        "unknown"
#define SKLAFF_TERM	"vt100"

/* Default number of days before texts expire */

#define EXP_DEF		10      /* Ordinary confs */
#define EXP_DEF_NEWS	 3      /* News confs	  */

/* System files used by SklaffKOM */

#define UTMP_REC 	"/var/run/utmp"
#define MAIL_LIB	"/var/mail"
#define NEWS_SPOOL	"/usr/local/news/spool/articles"
#define NEWS_GROUPS	"/usr/local/news/db/active"

/* Programs used by SklaffKOM */

#define SKLAFFSHELL	"/bin/sh"
#define SKLAFFPASSWD 	"/usr/bin/passwd"
#define SKLAFFGREP 	"/usr/bin/grep"
#define GREPOPT		"-i -F" /* option to grep */
#define SKLAFFAT 	"/usr/bin/at"
#define SKLAFFECHO 	"/bin/echo"
#define DEFAULT_NETHACK_PATH 	"/usr/local/bin/nethack"  /* default Nethack path */


/* Everything related to Frotz and Zork */
#define FROTZ_BIN       "/usr/local/bin/frotz"        /* Path to frotz binary */
#define FROTZ_ARGS      "-D 0 -Z 0 -R "               /* Disable colors on top, report 0 errors, saves in ./data dir */
#define FROTZ_HOME      DOORDIR "/frotz"              /* -> /doors/frotz on a standard install */
#define FROTZ_ZORK1     "zork1.z3"
#define FROTZ_ZORK2     "zork2.z3"
#define FROTZ_ZORK3     "zork3.z3"

/* OTHER NEW (2025) external commands */
#define CP437_WRAPPER "/usr/local/bin/cp437"
#define DEFAULT_BBSLINK_PATH    "/doors/bbslink"
#define BBSLINK_INTRO           SKLAFFDIR "/etc/bbslink"

/* Software for uploads and downloads. 
Please read item 18 for instructions here: 
https://github.com/joakimmelin/sklaffkom/wiki/Install-Instructions */

#define UPLOADPRGM      "/usr/local/bin/lrz"
#define ULOPT1          "-Eq"
#define DOWNLOADPRGM    "/usr/local/bin/lsz"
#define DLOPT1          "-Eq"
#define DLOPT2          ""
#define DLOPT3          ""
#define ZIPPRGM		"/usr/bin/zip -9"
#define LSPRGM		"/bin/ls"
#define LSOPT		"-1"    /* option to make one column */
#define MVPRGM		"/bin/mv"
#define MAILPRGM	"/usr/sbin/sendmail --"
#define NEWSPRGM	"/usr/local/bin/inews -h" /*-h needed!  */
#define SURVREPORT 	SKLAFFBIN "/srep"

/* SklaffKOM-files */

#define SKLAFF_DB 	SKLAFFDIR "/db"
#define USER_DB		SKLAFFDIR "/user"
#define FILE_DB		SKLAFFDIR "/files"
#define MBOX_DB		SKLAFFDIR "/mbox"
#define LOGDIR   	SKLAFFDIR "/log"
#define DOORDIR		"/doors"
#define USER_FILE 	SKLAFFDIR "/etc/user"
#define	ACTIVE_FILE 	SKLAFFDIR "/etc/active"
#define CONF_FILE 	SKLAFFDIR "/etc/conf"
#define NEWS_FILE 	SKLAFFDIR "/etc/news"
#define NEWS_FILE_ENG	SKLAFFDIR "/etc/news.eng"
#define NEWS_FILE_SWE	SKLAFFDIR "/etc/news.swe"
#define LOGOUT_FILE     "/usr/local/sklaff/etc/logout"
#define LOGOUT_FILE_ENG     "/usr/local/sklaff/etc/logout.eng"
#define LOGOUT_FILE_SWE "/usr/local/sklaff/etc/logout.swe"
#define INFO_FILE	SKLAFFDIR "/etc/info"
#define LICENS_FILE	SKLAFFDIR "/etc/COPYING"
#define DOWN_FILE 	SKLAFFDIR "/etc/down"
#define PAY_FILE	SKLAFFDIR "/etc/pay"
#define INET_FILE	SKLAFFDIR "/etc/inet"
#define ACCT_FILE	SKLAFFDIR "/etc/newacct"
#define ACCT_LOG	SKLAFFDIR "/etc/acctlog"
#define STD_CONFS 	SKLAFFDIR "/etc/stdconfs"
#define GLOBAL_SKLAFFRC	SKLAFFDIR "/etc/sklaffrc"
#define STD_SKLAFFRC 	SKLAFFDIR "/etc/stdsklaffrc"
#define POST_INFO	SKLAFFDIR "/etc/postnews"

#ifdef SWEDISH
#define PARSE_FILE 	SKLAFFDIR "/etc/parse.swe"
#define STD_MAILBOX 	SKLAFFDIR "/etc/stdmailbox.swe"
#define HELP_DIR	SKLAFFDIR "/etc/help.swe"
#define HELP_FILE	SKLAFFDIR "/etc/help.swe/general.help"
#else
#define PARSE_FILE 	SKLAFFDIR "/etc/parse.eng"
#define STD_MAILBOX 	SKLAFFDIR "/etc/stdmailbox.eng"
#define HELP_DIR	SKLAFFDIR "/etc/help.eng"
#define HELP_FILE	SKLAFFDIR "/etc/help.eng/general.help"
#endif

#define CONFRC_FILE 	"/confrc"

#define INDEX_FILE	"/.index"
#define NEWINDEX_FILE	"/.newindex"

#define MSG_FILE 	"/msg"
#define CONFS_FILE 	"/confs"
#define SKLAFFRC_FILE 	"/sklaffrc"
#define MAILBOX_FILE 	"/mailbox"
#define EDIT_FILE 	"/text.sklaff"
#define DEAD_FILE 	"/dead.sklaff"
#define TMP_NOTE 	"/tmplapp"
#define ACT_FILE        "/act"

/* Log level (lower means less logs, 0 = no logs) */
/*
0 : No logs
1–2: Errors only
3–4: Warnings
5–6: High-level actions (command start/stop)
7–8: Inputs/outputs (sanitized), decisions taken
9–10: Verbose (command lines, env, sizes, timing)
12+: Very verbose (bodies/snippets, hexdumps) — opt-in only
*/
#define LOGLEVEL 6

/* Misc settings */
#define PROMPT		"-> " /* one whitespace at the end is a good idea PL 2025-07-30*/
#define ANSI_CLS    "\033[H\033[J"

/* Macros */

#define ansi_output(...) do { if (Ansi_output) output(__VA_ARGS__); } while (0) /* 2025-07-30 PL */
#define output_ansi_fallback(ansi_str, plain_str) \
        do { output(Ansi_output ? (ansi_str) : (plain_str)); } while (0)  	/* 2025-07-30 PL */

/* ANSI colors */ 								/* 2025-07-30 PL */
#define RED             "\e[31m"
#define GREEN		"\e[32m"
#define YELLOW		"\e[33m"
#define BLUE		"\e[34m"
#define PURPLE		"\e[35m"
#define CYAN		"\e[36m"
#define WHITE		"\e[37m"
#define BR_WHITE	"\e[97m"
#define BR_RED		"\e[91m"
#define BR_YELLOW	"\e[93m"
#define BR_BLUE		"\e[94m"
#define DOT		"\e[0m" /* Reset to default */
#define RESET		"\e[0m" /* Alias for the above */

/* Change nothing below this line (unless it won't compile) */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* Misc defines */

#define LINE_LEN 	79
#define SUBJECT_LEN 	70
#define LONG_LINE_LEN 	2 * LINE_LEN
#define HUGE_LINE_LEN 	32768
#define MAX_COMMANDS 	256
#define HISTORY_SIZE    20
#define NEW_FILE_MODE 	0600
#define NEW_DIR_MODE 	0700
#define SIGNAL_NEW_TEXT SIGUSR1
#define SIGNAL_NEW_MSG  SIGUSR2
#define FROM_FIELD_LEN  28
#define IDLE_RESOLUTION 60
#define IDLE_LIMIT      3

/* Defines for messages */

#define	MSG_SAY		1
#define	MSG_YELL	2
#define	MSG_LOGIN	3
#define MSG_LOGOUT	4
#define	MSG_I           5
#define	MSG_SMS         6
#define	MSG_MY          7

/* Defines for open_file */

#define OPEN_CREATE	1
#define OPEN_QUIET	2
#define OPEN_DEFAULT	0

/* Defines for conf_type */

#define OPEN_CONF	0
#define CLOSED_CONF	1
#define SECRET_CONF	2
#define NEWS_CONF	3

/* Defines for text types */

#define TYPE_TEXT		0
#define TYPE_SURVEY		1

/* Defines for expand_name */

#define USER		0x01    /* to search among users */
#define CONF   		0x02    /* to search among confs. */
#define ACTIVE 		0x04    /* to search among logged in users */
#define SUBSCRIBED	0x08    /* to search among subscribed confs. */
#define UNSUBSCRIBED	0x10    /* to search among unsubscribed confs. */
#define ALSOERASED      0x20    /* to search also among erased confs */

/* Defines for surveys */

#define SURVEY_SINGLECHOICE    0
#define SURVEY_MULTIPLECHOICE  1
#define SURVEY_FREETEXT        2
#define SURVEY_INTERVAL        3
#define SURVEY_FREENUMBER      4

#define TAKE_SURVEY        1
#define SHOW_SURVEY_RESULT 2

/* Other defines */

#define STATUS_INTERNAL   0
#define STATUS_EXTERNAL   1
#define WHO_INTERNAL 0
#define WHO_EXTERNAL 1


/* Typedefs */

typedef char LINE[LINE_LEN + 1];
typedef char LONG_LINE[LONG_LINE_LEN + 1];
typedef char HUGE_LINE[HUGE_LINE_LEN];

#include "struct.h"
#include "lang.h"

/* lib-funktioner */

int cmp_strings(char *, char *);
int copy_file(char *, char *);
void critical(void);
char *down_string(char *);
int file_exists(const char *); /* 2025-09-25 PL */
off_t file_size(int);
char *user_dir(int, char *);
char *in_string(char *, char *);
char *get_hostname(void);
void haffo(int);
void baffo(int);
char *input(char *, char *, int, int, int, int);
int input_extended(char *, char *, int, int, int, int, int, int);
void lock(int);
void non_critical(void);
int output(char *,...);
int outputex(char *,...);
int close_file(int);
int create_file(char *);
int open_file(const char *filename, int flag);
int parse_strings(char *, char *, int, char *);
char *read_file(int);
int write_file(int, char *);
char *rtrim(char *);
char *ltrim(char *);
void sig_reset(void);
void sig_setup(void);
void sys_error(const char *, int, const char *);
char *time_string(time_t, char *, int);
void tty_raw(void);
void tty_reset(void);
void unlock(int);
char *up_string(char *);
int wc(char *);
char *fake_string(char *);
char *order_name(char *, char *);
char *real_string(char *);
char *reorder_name(char *, char *);
char *prog_name(char *);
char *mbox_dir(int, char *);
int  output_ansi_fmt(const char *ansi_fmt, const char *plain_fmt, ...); 				/* 2025-07-30 PL */
int  detect_terminal_lines(void);														/* 2025-08-10 PL */
void se_time_string(time_t utc, LINE out, int flags);  									/* 2025-08-30 PL */
int  parse_usenet_date_utc(const char *date_line, time_t *out_utc);						/* 2025-08-30 PL */
int  parse_tz_token(const char *p, int *out_sec);										/* 2025-08-30 PL */
int  month3_to_num(const char *m);														/* 2025-08-30 PL */
time_t timegm_compat(struct tm *t); 													/* 2025-08-30 PL */
void clear_prompt(int num); 															/* Little helper to avoid extra '(') 2025-08-26 PL */
void clear_prompt_cols(int cols);														/* 2025-08-30 PL */
int quote_depth(const char *s);  														/* Usenet quotes in colors 2025-08-31 PL */
void get_wallclock_localtime(const time_t *t, struct tm *out);                          /* 2025-08-16 PL little helper used by (Se) tiden > */
void extract_display_name(const char *from, char *out, size_t outlen);                  /* Humanizing usenet posts */
int is_blank_line(const char *line);                                                    /* Blank line detection for usenet posts */
size_t utf8_disp_len(const char *s);                                                    /* UTF8 related PL */
void utf8_trunc_cols(const char *in, size_t max_cols, char *out, size_t outlen);        /* 2025-08-10, PL: truncate by display columns (UTF-8 safe, 1 col/codepoint) */
void print_underlined_line(const char *line);                                           /* Build underline matching a printed line */
int b64v(int c);                                                                        /* Base64 table */
size_t qp_decode_bytes(const char *in, size_t inlen, unsigned char *out, size_t outlen);    /* Decode a single encoded-word's bytes with quoted-printable (Q) */
size_t b64_decode_bytes(const char *in, size_t inlen, unsigned char *out, size_t outlen);   /* Decode a single encoded-word's bytes with base64 (B) */
size_t latin1_to_utf8(const unsigned char *in, size_t inlen, char *out, size_t outlen);     /* Minimal charset -> UTF-8: utf-8 (pass), us-ascii (pass), iso-8859-1 (map) */
size_t bytes_to_utf8(const char *charset, const unsigned char *in, size_t inlen, char *out, size_t outlen); /* Counting bytes when converting char tables (?) */
void rfc2047_decode(const char *in, char *out, size_t outlen);                              /* RFC 2047 decoder: decodes any number of encoded-words in a header field */
void normalize_label(const char *raw, char *norm, size_t nlen);                              /* Normalize a label to ensure exactly one trailing ": " */
int output_body_line(const char *line, const char *col);                                    /* ANSI support fort text body in articles 2025-09-24 PL */
int run_external_cmd_args(const char *argv[], int use_fallback);                            /* Better support for external commands 2025-09-24 PL */
void display_langfile(const char *base, const char *base_eng, const char *base_swe);        /* Support for multilingual display of files (news etc) 2025-09-24 PL */
void display_news(void);
void display_logout(void);
void clear_screen(void);                                                                     /* More sophisticated cls to use outside of commands.c */
const char *month_name(int mon);                                                                /* "Se tiden"-stuff 2025-09.25 */
void chomp(char *s);                                                                            /* "Se tiden"-stuff 2025-09.25 */
void display_header(struct TEXT_HEADER *th, int edit_subject, int type, int dtype, char *mailrec);


/* Debugging */
/* void *my_malloc (size_t); */
/* void my_free (void *);    */

/* commands.c */

int cmd_add_rights(char *);
int cmd_change_conf(char *);
int cmd_comment(char *);
int cmd_create_conf(char *);
int cmd_delete_conf(char *);
int cmd_delete_text(char *);
int cmd_display_commented(char *);
int cmd_display_last(char *);
int cmd_display_rot13(char *);
int cmd_display_time(char *);
int cmd_end_session(char *);
int cmd_restart(char *);
int cmd_on_flag(char *);
int cmd_off_flag(char *);
int cmd_goto_text(char *);
int cmd_help(char *);
int cmd_info(char *);
int cmd_jump_over(char *);
int cmd_jump_tree(char *);
int cmd_licens(char *);
int cmd_list_confs(char *);
int cmd_list_flags(char *);
int cmd_list_last(char *);
int cmd_list_member(char *);
int cmd_list_news(char *);
int cmd_list_rights(char *);
int cmd_list_subj(char *);
int cmd_list_users(char *);
int cmd_long_help(char *);
int cmd_mail(char *);
int cmd_mod_note(char *);
int cmd_mod_sig(char *);
int cmd_mod_login(char *);
int cmd_mod_pinfo(char *);
int cmd_mod_timeout(char *);
int cmd_next_comment(char *);
int cmd_next_conf(char *);
int cmd_next_text(char *);
int cmd_only(char *);
int cmd_personal(char *);
int cmd_post_text(char *);
int cmd_read_text(char *);
int cmd_whole_text(char *);
int cmd_say(char *);
int cmd_show_status(char *);
int cmd_sub_rights(char *);
int cmd_subscribe(char *);
int cmd_unread_text(char *);
int cmd_unread_tree(char *);
int cmd_unsubscribe(char *);
int cmd_where(char *);
int cmd_who(char *);
int cmd_yell(char *);
int cmd_change_cname(char *);
int cmd_change_comc(char *);
int cmd_change_passwd(char *);
int cmd_cls(char *);
int cmd_grep(char *);
int cmd_global_grep(char *);
int cmd_list_files(char *);
int cmd_download(char *);
int cmd_upload(char *);
int cmd_unlink(char *);
int cmd_describe(char *);
int cmd_I(char *);
int cmd_alias(char *);
int cmd_my(char *);
int cmd_prio(char *);
int cmd_deprio(char *);
int cmd_back_text(char *);
int cmd_readall(char *);
int cmd_readsome(char *);
int cmd_sendbatch(char *);
int cmd_survey_result(char *);
int cmd_unreadsub(char *);
int cmd_jumpsub(char *);
int cmd_jumpuser(char *);
int cmd_answermsg(char *);
int cmd_from(char *);
int cmd_list_says(char *);
int cmd_list_yells(char *);
int cmd_post_survey(char *);
int cmd_read_last_text(char *);
int cmd_reclaim_unread(char *);
int cmd_nethack(char *);
int cmd_mod_numlines(char *args); /* 2025-08-10 PL */
int cmd_zork(char *args); /* 2025-08-17 PL */
int cmd_bbslink(char *args); /* 2025-09-24 PL */

/* admin.c */

char *display_prompt(char *, char *, int);
void display_welcome(void);
void display_news(void);
void check_open(void);
void out_onoff(int);
int grep(int, char *);
void exec_logout(int);
void exec_login(void);
void timeout(int);
void debuglog(char *, int);
int strip_string(char *, char *);
int show_status(int, int, int);
int list_who(int);

/* buf.c */


char *get_active_entry(char *, struct ACTIVE_ENTRY *);
char *get_conf_entry(char *, struct CONF_ENTRY *);
struct USER_LIST *get_confrc_users(char *);
char *get_confs_entry(char *, struct CONFS_ENTRY *);
char *get_msg_entry(char *, struct MSG_ENTRY *);
char *get_parse_entry(char *, struct PARSE_ENTRY *);
char *get_text_entry(char *, struct TEXT_ENTRY *);
char *get_user_entry(char *, struct USER_ENTRY *);
char *get_file_entry(char *, struct FILE_ENTRY *);

/* conf.c */

char *conf_name(int, char *);
int conf_num(char *);
int can_see_conf(int, int, int, int);
int is_conf_creator(int, int);
int set_conf(int);
int set_first_conf();
int member_of(int, int);
void free_confs_entry(struct CONFS_ENTRY *);
void free_userlist(struct USER_LIST *);
struct CONF_ENTRY *get_conf_struct(int);
struct USER_LIST *get_confrc_struct(int);
char *replace_conf(struct CONF_ENTRY *, char *);
char *replace_confs(struct CONFS_ENTRY *, char *);
char *stringify_confs_struct(struct CONFS_ENTRY *, char *);
char *stringify_conf_struct(struct CONF_ENTRY *, char *);
long num_unread(int, int, long);
int conf_right(struct USER_LIST *, int, int, int);
int list_confs(int, int);
int more_conf(void);
int force_unsubscribe(void);
long last_text(int, int);
long first_text(int, int);
struct CEN *sort_conf(struct CEL *, int);
int list_news(int);
struct CONF_ENTRY *get_all_confs(void);

/* edit.c */

void abort_edit(int);
int resume_aborted_edit(void);
struct TEXT_HEADER *
line_ed(char *, struct TEXT_HEADER *, int, int,
    int, int *, char *);

/* file.c */

int rebuild_index_file(void);

/* flag.c */

void set_flags(char *);
int check_flag(char *, char *);
int turn_flag(int, char *);

/* msg.c */

void add_to_mlist(struct MSG_ENTRY *);
void list_mlist(int, int);
void newmsg(int);
int display_msg(int);
int send_msg(int, int, char *, int);
int send_msg_to_all(int, char *);
void notify_all_processes(int);
void notify_user(int, int);

/* parse.c */

int (*parse(char *, char *)) ();
void buggy_sunOS_fix(int);
int parse_init(char *);
char *expand_name(char *, int, int, int *);

/* text.c */

int check_if_read(long, int);
void clear_comment(void);
//void display_header(struct TEXT_HEADER *, int, int, int, char *); /* Moved to lib-functioner 2025-09-25 */
int display_text(int, long, int, int);
int display_survey_result(int, long);
int list_subj(char *);
int mark_as_read(long, int);
int mark_as_unread(long, int);
long next_text(int);
long parse_text(char *);
long pop_comment(void);
int push_comment(long);
long pop_unread(int *);
int push_unread(int, long);
long pop_unread2(int *);
int push_unread2(int, long);
long pop_read(int *);
int push_read(int, long);
int stack_text(long);
int tree_top(long);
long save_text(char *, struct TEXT_HEADER *, int);
int more_comment(void);
int more_text(void);
void free_text_entry(struct TEXT_ENTRY *);
long save_mailcopy(char *, char *, char *);
void cnvnat(char *, int);
void int2ms(int, char[4]);  /* modified on 2025-07-12, PL */
long age_to_textno(long);

/* user.c */

struct USER_ENTRY *get_user_struct(int);
time_t last_session(int);
int user_is_active(int);
int user_is_avail(int);
char *replace_active(struct ACTIVE_ENTRY *, char *);
int set_avail(int, int);
int set_from(int, char *);
char *user_name(int, char *);
int user_uid(char *);
int setup_new_user(void);
char *stringify_user_struct(struct USER_ENTRY *, char *);
int add_active(void);
int remove_active(void);
struct ACTIVE_ENTRY *check_active(int, struct ACTIVE_ENTRY *);
long active_time(int);
long idle_time(int);
void make_activity_note();
int disp_note(int);
int list_user(int, int, int);
struct UEN *sort_user(struct UEL *, int);
struct SKLAFFRC *read_sklaffrc(int);
int write_sklaffrc(int, struct SKLAFFRC *);

/* survey.c */

int make_survey(char *, int *, LINE, int);
int save_survey_result(long, int, char *, int);
char *show_survey_result(long, int, struct TEXT_BODY *, int);
int mark_survey_as_taken(long, int);
int check_if_survey_taken(long, int);
time_t get_survey_time(time_t);
int get_no_survey_questions(char *);

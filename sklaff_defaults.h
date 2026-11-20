/*
 * sklaff_defaults.h
 *
 * Default site and system settings for SklaffKOM.
 *
 * Local installations should normally override these values in
 * sklaff_local.h rather than editing this file.
 *
 * Every setting in this file is guarded by #ifndef so that a value
 * defined earlier in sklaff_local.h takes precedence.
 */

#ifndef SKLAFF_DEFAULTS_H
#define SKLAFF_DEFAULTS_H


/* Login character-set negotiation. */
#ifndef ENABLE_CHARSET_HANDSHAKE
#define ENABLE_CHARSET_HANDSHAKE 1
#endif


/* InterBBS One-Liner (IBOL). */
#ifndef IBOL_LOGOUT_COUNT
#define IBOL_LOGOUT_COUNT 0
#endif

#ifndef IBOL_ONELINER_MAX
#define IBOL_ONELINER_MAX 65
#endif

#ifndef IBOL_FTN_DOMAIN
#define IBOL_FTN_DOMAIN "fsxnet"
#endif

#ifndef IBOL_FTN_AREA
#define IBOL_FTN_AREA "FSX_DAT"
#endif


/* CrashMail II. */
#ifndef CRASHMAIL_PREFS_FILE
#define CRASHMAIL_PREFS_FILE "/ftn/etc/crashmail.prefs"
#endif


/* Legacy modem/login settings. */
#ifndef MODEM_POOL
#define MODEM_POOL "silly"
#endif

#ifndef MODEM_GROUP
#define MODEM_GROUP 50
#endif

#ifndef INET_GROUP
#define INET_GROUP 60
#endif


/* Site identity. */
#ifndef MACHINE_NAME
#define MACHINE_NAME "bbs.example.org"
#endif

#ifndef SKLAFF_ACCT
#define SKLAFF_ACCT "sklaff"
#endif

#ifndef SKLAFF_ID
#define SKLAFF_ID "SklaffKOM BBS"
#endif

#ifndef SKLAFF_LOC
#define SKLAFF_LOC "Stockholm, Sweden"
#endif

#ifndef SKLAFF_NUM
#define SKLAFF_NUM "+46-8-12345678"
#endif

#ifndef SKLAFF_SYSOP
#define SKLAFF_SYSOP "John Doe"
#endif

#ifndef SKLAFF_SURVEY_REPORTER
#define SKLAFF_SURVEY_REPORTER 5000
#endif


/* Terminal defaults. */
#ifndef NO_TERM
#define NO_TERM "unknown"
#endif

#ifndef SKLAFF_TERM
#define SKLAFF_TERM "vt100"
#endif


/* Default number of days before texts expire. */
#ifndef EXP_DEF
#define EXP_DEF 10
#endif

#ifndef EXP_DEF_NEWS
#define EXP_DEF_NEWS 10
#endif

#ifndef EXP_DEF_FTN
#define EXP_DEF_FTN 10
#endif


/* System files and external spools. */
#ifndef UTMP_REC
#define UTMP_REC "/var/run/utmp"
#endif

#ifndef MAIL_SPOOL
#define MAIL_SPOOL "/var/mail"
#endif

#ifndef NEWS_SPOOL
#define NEWS_SPOOL "/usr/local/news/spool/articles"
#endif

#ifndef NEWS_GROUPS
#define NEWS_GROUPS "/usr/local/news/db/active"
#endif

#endif /* SKLAFF_DEFAULTS_H */

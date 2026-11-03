#ifndef ZMODEM_GLOBAL_H
#define ZMODEM_GLOBAL_H

/* zglobal.h - prototypes etcetera for lrzsz

  Copyright (C) until 1998 Chuck Forsberg (OMEN Technology Inc)
  Copyright (C) 1994 Matt Porter
  Copyright (C) 1996, 1997 Uwe Ohse

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
  02111-1307, USA.
*/

#include "config.h"
#include <sys/types.h>

#include <stdio.h> /* putchar, fflush, stdout: sendline/flushmo helpers */
#include <stdarg.h>
#include <stdbool.h>

#  include <sys/time.h>
#  include <time.h>
#  include <sys/select.h>

#include <string.h>

#include <sys/stat.h>

#include <fcntl.h>
#include <termios.h>

/* Take care of NLS matters.  */
#include <locale.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define bindtextdomain(Domain, Directory) /* empty */
# define textdomain(Domain) /* empty */
# define _(Text) Text
#endif

#include <syslog.h>
extern int enable_syslog;

#include <unistd.h>
#include <limits.h>

#include "stats.h"
#include "util.h"

#define OK 0
#define ERROR (-1)

/* Ward Christensen / CP/M parameters - Don't change these! */
#define ENQ 0x05
#define CAN ('X'&0x1f)
#define XOFF ('s'&0x1f)
#define XON ('q'&0x1f)
#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CPMEOF 0x1a
#define WANTCRC 0x43    /* send C not NAK to get crc not checksum */
#define WANTG 0x47  /* Send G not NAK to get nonstop batch xmsn */
#define TIMEOUT (-2)
#define RCDO (-3)
#define WCEOT (-10)

#define RETRYMAX 10

#define DEFBYTL 2000000000L	/* default rx file size */

enum zm_type_enum {
	ZM_XMODEM,
	ZM_YMODEM,
	ZM_ZMODEM
};

struct zm_fileinfo {
	char *fname;
	time_t modtime;
	mode_t mode;
	size_t bytes_total;
	size_t bytes_sent;
	size_t bytes_received;
	size_t bytes_skipped; /* crash recovery */
	int    eof_seen;
};

/* zero every field (fname=NULL, counters 0, eof_seen=0) */
static inline void zi_init(struct zm_fileinfo *zi)
{
	zi->fname = NULL;
	zi->modtime = 0;
	zi->mode = 0;
	zi->bytes_total = 0;
	zi->bytes_sent = 0;
	zi->bytes_received = 0;
	zi->bytes_skipped = 0;
	zi->eof_seen = 0;
}

#define R_BYTESLEFT(x) ((x)->bytes_total-(x)->bytes_received)

extern enum zm_type_enum protocol;

extern int Verbose;
extern int errors;
extern bool no_timeout;
 extern bool Zctlesc;    /* Encode control characters */
 extern bool under_rsh;
extern bool turbo_escape;
extern bool zmodem_requested;

void bibi (int n);

/* Output primitives for the protocol stream, including stat-accounting */
static inline void sendline(int c)
{
	putchar((c) & 0377);
	stat_for_file.net_bytes_out++;
}
static inline void xsendline(int c)
{
	putchar(c);
	stat_for_file.net_bytes_out++;
}
static inline void flushmo(void)
{
	fflush(stdout);
	stat_for_file.net_write_ops++;
}

/* zreadline.c */
extern unsigned char *readline_ptr; /* pointer for removing chars from linbuf */
extern int readline_left; /* number of buffered chars left to read */
#define READLINE_PF(timeout) \
    (--readline_left >= 0? (*readline_ptr++ & 0377) : readline_internal(timeout))

int readline_internal (unsigned int timeout);
void readline_purge (void);
void readline_setup (int fd, size_t readnum, size_t buffer_size);


/* rbsb.c */
extern bool Fromcu;
extern bool Twostop;
extern unsigned int Baudrate;

LRZSZ_FORMAT_PRINTF(1,2) void zperr (const char *fmt, ...);
LRZSZ_FORMAT_PRINTF(1,2) void zpfatal (const char *fmt, ...);
#define vchar(x) putc(x,stderr)
#define vstring(x) fputs(x,stderr)

LRZSZ_FORMAT_PRINTF(1,2) void vstringf (const char *format, ...);

/* Debug modules, selected with --debug=NAME[,NAME...] (see util.c).
 * DPRINTF prints to stderr when any of the modules in mask is enabled. */
#define DEBUG_PROTOCOL        0x01  /* ZMODEM frame state machine (zm.c) */
#define DEBUG_READLINE        0x02  /* line reader: select/read/timeouts/hex (zreadline.c) */
#define DEBUG_TRANSFER        0x04  /* file transfer state machine (lrz.c/lsz.c) */
#define DEBUG_WINDOWHANDLING  0x08  /* adaptive blklen/window (lsz.c) */
#define DEBUG_TTY             0x10  /* tty setup (rbsb.c) */
#define DEBUG_ALL (DEBUG_PROTOCOL|DEBUG_READLINE|DEBUG_TRANSFER|DEBUG_WINDOWHANDLING|DEBUG_TTY)

extern unsigned int debugmode;
#define DPRINTF(mask, ...) do { if ((debugmode) & (mask)) \
	vstringf(__VA_ARGS__); } while (0)

/* rbsb.c */
int from_cu (void);
int rdchk (int fd);
/* io_mode returns OK or ERROR. ERROR means the line mode could not be
 * determined/changed; the next io_mode call with n != 0 retries. The
 * return value is advisory: most callers are best-effort cleanups, the
 * startup call sites warn on ERROR and continue. */
int io_mode (int fd, int n);
void sendbrk (int fd);
void purgeline (int fd);
void canit (int fd);


#include "crctab.h"

/* zm.c */
#include "zmodem.h"
extern unsigned int Rxtimeout;        /* Tenths of seconds to wait for something */

/* Globals used by ZMODEM functions */
extern unsigned int Zrwindow;       /* RX window size (controls garbage count) */
/* extern int Rxcount; */       /* Count of data bytes received */
extern unsigned char Rxhdr[4];      /* Received header */
extern unsigned char Txhdr[4];      /* Transmitted header */
extern int Txfcs32;        /* TURE means send binary frames with 32 bit FCS */
extern int Crc32t;     /* Display flag indicating 32 bit CRC being sent */
extern int Znulls;     /* Number of nulls to send at beginning of ZDATA hdr */
extern char Attn[ZATTNLEN+1];  /* Attention string rx sends to tx on err */

extern void zsendline_init (void);
void zsbhdr (int type, unsigned char *hdr);
void zshhdr (int type, unsigned char *hdr);
void zsdata (const char *buf, size_t length, int frameend);
void zsda32 (const char *buf, size_t length, int frameend);
int zrdata (char *buf, int length, size_t *received);
int zgethdr (unsigned char *hdr, int eflag, size_t *);
void stohdr (size_t pos);
unsigned long rclhdr (const unsigned char *hdr);

const char * protname (void);
LRZSZ_FORMAT_PRINTF(2,3) void lsyslog (int, const char *,...);

/* gate + call lsyslog */
#define DO_SYSLOG(...) do { \
	if (enable_syslog) lsyslog(__VA_ARGS__); \
} while (0)

unsigned getspeed(speed_t code);

const char * frameend_to_string(unsigned int type);
const char * frametype_to_string(unsigned int type);


#endif

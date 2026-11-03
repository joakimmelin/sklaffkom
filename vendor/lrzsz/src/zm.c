/*
  zm.c - zmodem protocol handling lowlevelstuff
  Copyright (C) until 1988 Chuck Forsberg (OMEN Technology Inc)
  Copyright (C) 1996-1998,2026+ Uwe Ohse

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

  originally written by Chuck Forsberg
*/

#include "zglobal.h"

#include <stdio.h>

unsigned int Rxtimeout = 100;		/* Tenths of seconds to wait for something */

/* Globals used by ZMODEM functions */
static int Rxframeind;	/* ZBIN ZBIN32, or ZHEX type of frame received */
static int Rxtype;		/* Type of header received */
unsigned char Rxhdr[4];		/* Received header */
unsigned char Txhdr[4];		/* Transmitted header */
int Txfcs32;		/* true means send binary frames with 32 bit FCS */
int Crc32t;		/* Display flag indicating 32 bit CRC being sent */
static int Crc32;		/* Display flag indicating 32 bit CRC being received */
int Znulls;		/* Number of nulls to send at beginning of ZDATA hdr */
char Attn[ZATTNLEN+1];	/* Attention string rx sends to tx on err */

static char lastsent;	/* Last char we sent */
bool turbo_escape;

static const char *frametypes[] = {
	"Carrier Lost",		/* -3 */
	"TIMEOUT",		/* -2 */
	"ERROR",		/* -1 */
	"ZRQINIT",
	"ZRINIT",
	"ZSINIT",
	"ZACK",
	"ZFILE",
	"ZSKIP",
	"ZNAK",
	"ZABORT",
	"ZFIN",
	"ZRPOS",
	"ZDATA",
	"ZEOF",
	"ZFERR",
	"ZCRC",
	"ZCHALLENGE",
	"ZCOMPL",
	"ZCAN",
	"ZFREECNT",
	"ZCOMMAND",
	"ZSTDERR"
};
#define FTOFFSET 3
#define FRMAX (sizeof(frametypes) / sizeof(frametypes[0]))

const char *
frametype_to_string(unsigned int type)
{
	static char unknown[32];
	unsigned int idx = (unsigned int)type + FTOFFSET;
	if (idx < FRMAX)
		return frametypes[idx];
	snprintf(unknown, sizeof(unknown), "unknown frame type %x", type);
	return unknown;
}

#define badcrc _("Bad CRC")
/* static char *badcrc = "Bad CRC"; */
static inline LRZSZ_ALWAYS_INLINE int noxrd7 (void);
static inline LRZSZ_ALWAYS_INLINE int zdlread (void);
static int zdlread2 (int);
static inline LRZSZ_ALWAYS_INLINE int zgeth1 (void);
static void zputhex (unsigned int c, unsigned char *pos);
static int zgethex (void);
static int zrbhdr (unsigned char *hdr);
static int zrbhdr32 (unsigned char *hdr);
static int zrhhdr (unsigned char *hdr);
static unsigned char zsendline_tab[256];
static void zsbh32 (unsigned char *hdr, int type);
static inline void zsendline_s (const char *s, size_t count);

/*
 * Read a character from the modem line with timeout.
 *  Eat parity, XON and XOFF characters.
 */
static inline int
noxrd7(void)
{
	int c;

	for (;;) {
		if ((c = READLINE_PF(Rxtimeout)) < 0)
			return c;
		switch (c &= 0x7f) {
		case XON:
		case XOFF:
			continue;
		default:
			if (Zctlesc && !(c & 0x60))
				continue;
                        return c;
		case '\r':
		case '\n':
		case ZDLE:
			return c;
		}
	}
}

static inline int
zgeth1(void)
{
	int c, n;

	if ((c = noxrd7()) < 0)
		return c;
	n = c - '0';
	if (n > 9)
		n -= ('a' - ':');
	if (n & ~0xF)
		return ERROR;
	if ((c = noxrd7()) < 0)
		return c;
	c -= '0';
	if (c > 9)
		c -= ('a' - ':');
	if (c & ~0xF)
		return ERROR;
	c += (n<<4);
	return c;
}

/* Decode two lower case hex digits into an 8 bit byte value */
static int
zgethex(void)
{
	int c;

	c = zgeth1();
	DPRINTF(DEBUG_PROTOCOL, "zgethex: %02X", (unsigned int) c);
	return c;
}

/*
 * Read a byte, checking for ZMODEM escape encoding
 *  including CAN*5 which represents a quick abort
 */
static inline int
zdlread(void)
{
	int c;
	if ((c = READLINE_PF(Rxtimeout)) < 0)
		return c;
	/* Quick check for non control characters */
	if (c & 0x60)
		return c;
	return zdlread2(c);
}
static int
zdlread2(int c)
{
	bool have_byte = true;	/* zdlread() passes the first byte in */
	bool after_zdle = false;	/* set once a ZDLE has been seen */

	for (;;) {
		if (!after_zdle) {
			/* unescaped context: a plain byte passes through */
			if (!have_byte) {
				if ((c = READLINE_PF(Rxtimeout)) < 0)
					return c;
				if (c & 0x60)
					return c;
			}
			have_byte = false;
			switch (c) {
			case ZDLE:
				after_zdle = true;
				continue;
			case XON:
			case (XON|0x80):
			case XOFF:
			case (XOFF|0x80):
				/* flow control: skipped */
				continue;
			default:
				if (Zctlesc && !(c & 0x60))
					continue;	/* skip control garbage */
				return c;
			}
		}
		/* escaped context: read + CAN*5 chain + escaped switch */
		if ((c = READLINE_PF(Rxtimeout)) < 0)
			return c;
		if (c == CAN && (c = READLINE_PF(Rxtimeout)) < 0)
			return c;
		if (c == CAN && (c = READLINE_PF(Rxtimeout)) < 0)
			return c;
		if (c == CAN && (c = READLINE_PF(Rxtimeout)) < 0)
			return c;
		switch (c) {
		case CAN:
			return GOTCAN;
		case ZCRCE:
		case ZCRCG:
		case ZCRCQ:
		case ZCRCW:
			return (c | GOTOR);
		case ZRUB0:
			return 0x7f;
		case ZRUB1:
			return 0xff;
		case XON:
		case (XON|0x80):
		case XOFF:
		case (XOFF|0x80):
			/* flow control: skipped, also in the escaped
			 * context - the classic rule (see below) can
			 * only be reached for encodings of controls. */
			continue;
		default:
			if (Zctlesc && !(c & 0x60))
				continue;	/* skip and stay escaped */
			break;
		}
		/* ZDLE-escaped data: the classic rule accepts the escape
		 * encodings of 0x40-0x5f/0xc0-0xdf ((c & 0x60) == 0x40) -
		 * the escapes of the always-escaped controls 0x00-0x1f
		 * and 0x80-0x9f. Any other byte after ZDLE is a format
		 * error (the ESC8 dialect, whose escape encodings
		 * include 0xa0-0xbf/0xe0-0xff, is deprecated). */
		if ((c & 0x60) == 0x40)
			return (c ^ 0x40);
		zperr(_("Bad escape sequence 0x%x"), (unsigned int) c);
		return ERROR;
	}
}



/*
 * Send character c with ZMODEM escape sequence encoding.
 *  Escape XON, XOFF. Escape CR following @ (Telenet net escape)
 */
static inline void 
zsendline(int c)
{

	switch(zsendline_tab[(unsigned) (c&=0xff)])
	{
        default: /* can't happen */
	case 0: 
		xsendline(lastsent = c); 
		break;
	case 1:
		xsendline(ZDLE);
		c ^= 0x40;
		xsendline(lastsent = c);
		break;
	case 2:
		if ((lastsent & 0x7f) != 0x40) {
			xsendline(lastsent = c);
		} else {
			xsendline(ZDLE);
			c ^= 0x40;
			xsendline(lastsent = c);
		}
		break;
	}
}

/* is this really still useful in 2026? */
static inline void 
zsendline_s(const char *s, size_t count) 
{
	const char *end=s+count;
	while(s!=end) {
		int last_esc=0;
		const char *t=s;
		while (t!=end) {
			last_esc=zsendline_tab[(unsigned) ((*t) & 0xff)];
			if (last_esc) 
				break;
			t++;
		}
		if (t!=s) {
			fwrite(s,(size_t)(t-s),1,stdout);
                        stats_net_write((size_t)(t-s));
			lastsent=t[-1];
			s=t;
		}
		if (last_esc) {
			int c=*s;
			switch(last_esc) {
                        default: /* can't happen */
			case 0: 
				xsendline(lastsent = c); 
				break;
			case 1:
				xsendline(ZDLE);
				c ^= 0x40;
				xsendline(lastsent = c);
				break;
			case 2:
				if ((lastsent & 0x7f) != 0x40) {
					xsendline(lastsent = c);
				} else {
					xsendline(ZDLE);
					c ^= 0x40;
					xsendline(lastsent = c);
				}
				break;
			}
			s++;
		}
	}
}


/* Send ZMODEM binary header hdr of type type */
void 
zsbhdr(int type, unsigned char *hdr)
{
	int n;
	unsigned short crc;

	DPRINTF(DEBUG_PROTOCOL, "zsbhdr: %s %lx", frametype_to_string(type), rclhdr(hdr));
	if (type == ZDATA)
		for (n = Znulls; --n >=0; )
			xsendline(0);

	xsendline(ZPAD); xsendline(ZDLE);

	Crc32t=Txfcs32;
	if (Crc32t)
		zsbh32(hdr, type);
	else {
		xsendline(ZBIN); zsendline(type); crc = updcrc(type, 0);

		for (n=4; --n >= 0; ++hdr) {
			zsendline(*hdr);
			crc = updcrc((0xff& *hdr), crc);
		}
		crc = updcrc(0,updcrc(0,crc));
		zsendline(crc>>8);
		zsendline(crc);
	}
	if (type != ZDATA)
		flushmo();
}


/* Send ZMODEM binary header hdr of type type */
static void
zsbh32(unsigned char *hdr, int type)
{
	int n;
	unsigned long crc;

	xsendline(ZBIN32);  zsendline(type);
	crc = 0xFFFFFFFFL; crc = UPDC32(type, crc);

	for (n=4; --n >= 0; ++hdr) {
		crc = UPDC32((0xff & *hdr), crc);
		zsendline(*hdr);
	}
	crc = ~crc;
	for (n=4; --n >= 0;) {
		zsendline((int)crc);
		crc >>= 8;
	}
}

/* Send ZMODEM HEX header hdr of type type */
void 
zshhdr(int type, unsigned char *hdr)
{
	int n;
	unsigned short crc;
	unsigned char s[30];
	size_t len;

	DPRINTF(DEBUG_PROTOCOL, "zshhdr: %s %lx", frametype_to_string(type & 0x7f), rclhdr(hdr));
	s[0]=ZPAD;
	s[1]=ZPAD;
	s[2]=ZDLE;
	s[3]=ZHEX;
	zputhex(type & 0x7f ,s+4);
	len=6;
	Crc32t = 0;

	crc = updcrc((type & 0x7f), 0);
	for (n=4; --n >= 0; ++hdr) {
		zputhex(*hdr,s+len); 
		len += 2;
		crc = updcrc((0xff & *hdr), crc);
	}
	crc = updcrc(0,updcrc(0,crc));
	zputhex(crc>>8,s+len); 
	zputhex(crc,s+len+2);
	len+=4;

	/* Make it printable on remote machine */
	s[len++]='\r';
	s[len++]='\n'|0x80;
	/*
	 * Uncork the remote in case a fake XOFF has stopped data flow
	 */
	if (type != ZFIN && type != ZACK)
	{
		s[len++]=XON;
	}
	fwrite(s, 1, len, stdout);
	stats_net_write(len);
	flushmo();
}

/*
 * Send binary array buf of length length, with ending ZDLE sequence frameend
 */
static const char *Zendnames[] = { "ZCRCE", "ZCRCG", "ZCRCQ", "ZCRCW"};
const char *
frameend_to_string(unsigned int type)
{
        switch(type) {
        case ZCRCE: return "ZCRCE";
        case ZCRCG: return "ZCRCG";
        case ZCRCQ: return "ZCRCQ";
        case ZCRCW: return "ZCRCW";
        default: return "unknown";
	}
}
void
zsdata(const char *buf, size_t length, int frameend)
{
	unsigned short crc;

	DPRINTF(DEBUG_PROTOCOL, "zsdata: %lu %s", (unsigned long) length,
		Zendnames[(frameend-ZCRCE)&3]);
	crc = 0;

        stats_msg_valid(length);
	for( ; length; length--) {
	  zsendline(*buf); crc = updcrc((0xff & *buf), crc);
	  buf++;
	}

	xsendline(ZDLE); xsendline(frameend);
	crc = updcrc(frameend, crc);

	crc = updcrc(0,updcrc(0,crc));
	zsendline(crc>>8); zsendline(crc);
	if (frameend == ZCRCW) {
		xsendline(XON);  flushmo();
	}
}

void
zsda32(const char *buf, size_t length, int frameend)
{
	int c;
	unsigned long crc;
	int i;
	DPRINTF(DEBUG_PROTOCOL, "zsdat32: %lu %s", length, Zendnames[(frameend-ZCRCE)&3]);

	crc = 0xFFFFFFFFL;
        stats_msg_valid(length);
	zsendline_s(buf,length);
	for (; length; length--) {
		c = *buf & 0xff;
		crc = UPDC32(c, crc);
		buf++;
	}
	xsendline(ZDLE); xsendline(frameend);
	crc = UPDC32(frameend, crc);

	crc = ~crc;
	for (i=4; --i >= 0;) {
		c=(int) crc;
		if (c & 0x60)
			xsendline(lastsent = c);
		else
			zsendline(c);
		crc >>= 8;
	}
	if (frameend == ZCRCW) {
		xsendline(XON);  flushmo();
	}
}

/*
 * Receive array buf of max length with ending ZDLE sequence
 *  and CRC.  Returns the ending character or error code.
 *  NB: on the too-long error path (a data byte arriving when the buffer
 *  is already full) the offending byte is stored at buf[length] - buf
 *  must provide length+1 bytes. Callers allocate exactly that (secbuf:
 *  1+LRZSZ_MAX_SUBPACKET_LEN; Attn: ZATTNLEN+1).
 *
 * zrdata1 is the shared body of the 16-bit and 32-bit CRC variants: the
 * two differ only in the CRC chain (init, update, byte count in the
 * frame-end cases, final compare) and the debug name. The 16-bit chain
 * is emulated bit-exactly: the original code stored each updcrc() result
 * into an unsigned short, truncating to 16 bits on every update, so the
 * merged function masks each update with 0xFFFF. CRC32 is loop-invariant,
 * so the per-byte branch costs nothing measurable. The GOTCAN/TIMEOUT/
 * default verdict cases are shared; the only order change (stats call vs
 * zperr in the invalid-CRC and default cases) is unobservable outside a
 * STATS_TRACE build.
 */
static int
zrdata1(char *buf, int length, size_t *bytes_received, bool crc32)
{
	int c;
	unsigned long crc32v;
	unsigned short crc16;
	char *end;
	int d;
	unsigned int i;

	*bytes_received=0;
	if (crc32)
		crc32v = 0xFFFFFFFFL;
	else
		crc16 = 0;
	end = buf + length;
	while (buf <= end) {
		if ((c = zdlread()) & ~0xff) {
crcfoo:
			switch (c) {
			case GOTCRCE:
			case GOTCRCG:
			case GOTCRCQ:
			case GOTCRCW:
				{
					d = c;
					c &= 0xff;
					if (crc32)
						crc32v = UPDC32(c, crc32v);
					else
						crc16 = updcrc(c, crc16) & 0xFFFF;
					for (i = 0; i < (crc32 ? 4 : 2); i++) {
						if ((c = zdlread()) & ~0xff)
							goto crcfoo;
						if (crc32)
							crc32v = UPDC32(c, crc32v);
						else
							crc16 = updcrc(c, crc16) & 0xFFFF;
					}

                                        stats_msg_bytes(length - (end - buf));
					if (crc32) {
						if (crc32v != 0xDEBB20E3) {
							stats_msg_invalid();
							zperr(badcrc);
							return ERROR;
						}
					} else if (crc16 != 0) {
						/* CRC-16/XMODEM: the sender sends
						 * the ones-complement, so the
						 * accumulator over data plus the
						 * received CRC bytes ends at 0 for
						 * good data; nonzero means corruption. */
						stats_msg_invalid();
						zperr(badcrc);
						return ERROR;
					}
                                        stats_msg_valid(0);
					*bytes_received = length - (end - buf);
					if (crc32)
						DPRINTF(DEBUG_PROTOCOL, "zrdat32: %lu %s", (unsigned long) *bytes_received,
							Zendnames[(d-GOTCRCE)&3]);
					else
						DPRINTF(DEBUG_PROTOCOL, "zrdata: %lu  %s", (unsigned long) (*bytes_received),
								Zendnames[(d-GOTCRCE)&3]);
					return d;
				}
			case GOTCAN:
				zperr(_("Sender Canceled"));
                                stats_cancel();
				return ZCAN;
			case TIMEOUT:
				zperr(_("TIMEOUT"));
                                stats_timeout();
				return c;
			case RCDO:
				/* the line died (peer closed, carrier lost):
				 * a net-death verdict, not protocol garbage */
				zperr(_("Line dropped"));
				stats_net_error();
				return c;
			default:
				zperr(_("Bad data subpacket"));
                                stats_msg_format_error();
				return c;
			}
		}
		if (buf == end) break;
		*buf++ = c;
		if (crc32)
			crc32v = UPDC32(c, crc32v);
		else
			crc16 = updcrc(c, crc16) & 0xFFFF;
	}
	zperr(_("Data subpacket too long"));
        stats_msg_format_error();
	return ERROR;
}

int
zrdata(char *buf, int length, size_t *bytes_received)
{
	return zrdata1(buf, length, bytes_received, Rxframeind == ZBIN32);
}

/*
 * Read a ZMODEM header to hdr, either binary or hex.
 *  eflag controls local display of non zmodem characters:
 *	0:  no display
 *	1:  display printing characters only
 *	2:  display all non ZMODEM characters
 *  On success, set Zmodem to 1, set Rxpos and return type of header.
 *   Otherwise return negative on error.
 *   Return ERROR instantly if ZCRCW sequence, for fast error recovery.
 */
int
zgethdr(unsigned char *hdr, int eflag, size_t *Rxpos)
{
	/* The byte-level resync parser is a state machine, was a goto hell:
	 *   ST_GARBAGE  - read + classify a byte (former startover/again/agn2)
	 *   ST_HUNT_CAN - CAN-hunt with eager 1-tenths-timeout reads (former gotcan)
	 *   ST_ZPADS    - skip consecutive ZPADs via noxrd7() (former splat)
	 *   ST_FRAME    - frame type read + ZBIN/ZBIN32/ZHEX dispatch
	 *   ST_TAIL     - former fifi: common epilogue
	 * reset_cancount: cancount resets only on the garbage-tail loop (the old
	 *   goto startover), not when the CAN-hunt returns to the scan via TIMEOUT
	 *   or more-CAN (the old goto again).
         * have_byte: the CAN-hunt's default
	 *   byte, the ST_ZPADS default and the ST_FRAME default flow directly into
	 *   the garbage block with the byte already read (the old goto agn2).
         *
	 * rxpos/rclhdr and stats_msg_bytes(4) run only on the dispatched-header
	 * arms (as in the original), NOT in the epilogue: error paths must leave
	 * *Rxpos untouched and print rxpos=0.
         *
         * this is still not good, but much better.
	 */
	enum zgethdr_state { ST_GARBAGE, ST_HUNT_CAN, ST_ZPADS, ST_FRAME, ST_TAIL };
	int state = ST_GARBAGE;
	bool have_byte = false;
	bool reset_cancount = true;

	int c, cancount;
	unsigned int max_garbage; /* Max bytes before start of frame */
	size_t rxpos=0; /* keep gcc happy */
	int garbage_count = 0;

	max_garbage = Zrwindow + Baudrate;
	Rxframeind = Rxtype = 0;

	while (state != ST_TAIL) {
		switch (state) {
		case ST_GARBAGE:
			if (reset_cancount) {
				cancount = 5;
				reset_cancount = false;
			}
			if (!have_byte) {
				/* Return immediate ERROR if ZCRCW sequence seen */
				c = READLINE_PF(Rxtimeout);
				if (c == RCDO || c == TIMEOUT)
					state = ST_TAIL;
				if (state == ST_TAIL)
					continue;
			}
			have_byte = false;
			if (c == CAN) {
				state = ST_HUNT_CAN;
				continue;
			}
			if (c == ZPAD || c == (ZPAD|0x80)) {
				/* This is what we want. */
				state = ST_ZPADS;
				continue;
			}
			/* garbage byte: count it, maybe display it */
			garbage_count++;
			if ((debugmode & DEBUG_PROTOCOL)
			    && (garbage_count <= 3 || garbage_count % 1000 == 0))
				DPRINTF(DEBUG_PROTOCOL,
					"zgethdr: garbage[%d]=0x%02x, max_garbage_left=%u",
					garbage_count, (unsigned int) (c & 0xff), max_garbage);
			if ( --max_garbage == 0) {
				if (debugmode & DEBUG_PROTOCOL)
					DPRINTF(DEBUG_PROTOCOL,
						"zgethdr: garbage count exceeded after %d bytes",
						garbage_count);
				zperr(_("Garbage count exceeded"));
				return(ERROR);
			}
			if (eflag && ((c &= 0x7f) & 0x60) && (debugmode & DEBUG_PROTOCOL))
				vchar(c);
			else if (eflag > 1 && (debugmode & DEBUG_PROTOCOL))
				vchar(c);
			reset_cancount = true;	/* the old goto startover */
			continue;
		case ST_HUNT_CAN:
			if (--cancount <= 0) {
				c = ZCAN;
				state = ST_TAIL;
				continue;
			}
			c = READLINE_PF(1);
			if (c == TIMEOUT) {
				/* the old goto again: an Rxtimeout read next, no reset */
				state = ST_GARBAGE;
				reset_cancount = false;
				continue;
			}
			if (c == ZCRCW) {
				/* fast error recovery */
				c = ERROR;
				state = ST_TAIL;
				continue;
			}
			if (c == RCDO) {
				state = ST_TAIL;
				continue;
			}
			if (c == CAN) {
				if (--cancount <= 0) {
					c = ZCAN;
					state = ST_TAIL;
					continue;
				}
				/* the old goto again: back to the Rxtimeout read;
				 * if that byte is a CAN too, we re-enter the hunt.
				 * Reads alternate Rxtimeout/1 exactly as before. */
				state = ST_GARBAGE;
				reset_cancount = false;
				continue;
			}
			/* the hunted byte is not a CAN: process it as garbage */
			state = ST_GARBAGE;
			reset_cancount = false;
			have_byte = true;
			continue;
		case ST_ZPADS:
			c = noxrd7();
			if (c == ZPAD)
				continue;		/* skip more ZPADs */
			if (c == RCDO || c == TIMEOUT) {
				state = ST_TAIL;
				continue;
			}
			if (c == ZDLE) {
				/* This is what we want. */
				state = ST_FRAME;
				continue;
			}
			state = ST_GARBAGE;		/* the old goto agn2 */
			reset_cancount = false;
			have_byte = true;
			continue;
		case ST_FRAME:
			switch (c = noxrd7()) {
			case RCDO:
			case TIMEOUT:
				state = ST_TAIL;
				continue;
			case ZBIN:
				Rxframeind = ZBIN;
				Crc32 = false;
				c =  zrbhdr(hdr);
				if (c==ERROR) {
					stats_msg_invalid();
				} else {
					stats_msg_valid(0);
				}
				rxpos = rclhdr(hdr);
				if (Rxpos)
					*Rxpos = rxpos;
				stats_msg_bytes(4);
				state = ST_TAIL;
				continue;
			case ZBIN32:
				Rxframeind = ZBIN32;
				Crc32 = true;
				c =  zrbhdr32(hdr);
				if (c==ERROR) {
					stats_msg_invalid();
				} else {
					stats_msg_valid(0);
				}
				rxpos = rclhdr(hdr);
				if (Rxpos)
					*Rxpos = rxpos;
				stats_msg_bytes(4);
				state = ST_TAIL;
				continue;
			case ZHEX:
				Rxframeind = ZHEX;
				Crc32 = false;
				c =  zrhhdr(hdr);
				if (c==ERROR) {
					stats_msg_invalid();
				} else {
					stats_msg_valid(0);
				}
				rxpos = rclhdr(hdr);
				if (Rxpos)
					*Rxpos = rxpos;
				stats_msg_bytes(4);
				state = ST_TAIL;
				continue;
			case CAN:
				state = ST_HUNT_CAN;	/* the old goto gotcan */
				reset_cancount = false;
				continue;
			default:
				stats_msg_format_error();
				state = ST_GARBAGE;	/* the old goto agn2 */
				reset_cancount = false;
				have_byte = true;
				continue;
			}
		default:
			/* unreachable: state always holds one of the enum values */
			state = ST_TAIL;
			continue;
		}
	}
	/* ex fifi label: the common epilogue. rxpos is 0 here unless a header
	 * was dispatched (the success arms set it above). */
	DPRINTF(DEBUG_PROTOCOL,
		"zgethdr: returning %d (%s) after %d garbage bytes, rxpos=%lx, readline_left=%d",
		c, frametype_to_string(c),
		garbage_count, (unsigned long)rxpos, readline_left);
        /* error accounting */
	switch (c) {
	case GOTCAN:
	case ZCAN:
            stats_cancel();
            break;
	case TIMEOUT:
            stats_timeout();
            break;
        default:
            /* nothing to do */
            ;
        }

	switch (c) {
	case GOTCAN:
		c = ZCAN;
	/* FALLTHROUGH */
	case ZNAK:
	case ZCAN:
	case ERROR:
	case TIMEOUT:
	case RCDO:
	zperr(_("Got %s"), frametype_to_string(c));
	/* FALLTHROUGH */
    default:
		DPRINTF(DEBUG_PROTOCOL, "zgethdr: %s %lx", frametype_to_string(c), (unsigned long) rxpos);
	}
	return c;
}

/* Receive a binary style header (type and position) */
static int 
zrbhdr(unsigned char *hdr)
{
	int c, n;
	unsigned short crc;

	if ((c = zdlread()) & ~0xff)
		return c;
	Rxtype = c;
	crc = updcrc(c, 0);

	for (n=4; --n >= 0; ++hdr) {
		if ((c = zdlread()) & ~0xff)
			return c;
		crc = updcrc(c, crc);
		*hdr = c;
	}
	if ((c = zdlread()) & ~0xff)
		return c;
	crc = updcrc(c, crc);
	if ((c = zdlread()) & ~0xff)
		return c;
	crc = updcrc(c, crc);
	if (crc != 0) {
		zperr(badcrc);
		return ERROR;
	}
	protocol = ZM_ZMODEM;
	zmodem_requested=true;
	return Rxtype;
}

/* Receive a binary style header (type and position) with 32 bit FCS */
static int
zrbhdr32(unsigned char *hdr)
{
	int c, n;
	unsigned long crc;

	if ((c = zdlread()) & ~0xff)
		return c;
	Rxtype = c;
	crc = 0xFFFFFFFFL; crc = UPDC32(c, crc);

	for (n=4; --n >= 0; ++hdr) {
		if ((c = zdlread()) & ~0xff)
			return c;
		crc = UPDC32(c, crc);
		*hdr = c;
	}
	for (n=4; --n >= 0;) {
		if ((c = zdlread()) & ~0xff)
			return c;
		crc = UPDC32(c, crc);
	}
	if (crc != 0xDEBB20E3) {
		zperr(badcrc);
		return ERROR;
	}
	protocol = ZM_ZMODEM;
	zmodem_requested=true;
	return Rxtype;
}


/* Receive a hex style header (type and position) */
static int 
zrhhdr(unsigned char *hdr)
{
	int c;
	unsigned short crc;
	int n;

	if ((c = zgethex()) < 0)
		return c;
	Rxtype = c;
	crc = updcrc(c, 0);

	for (n=4; --n >= 0; ++hdr) {
		if ((c = zgethex()) < 0)
			return c;
		crc = updcrc(c, crc);
		*hdr = c;
	}
	if ((c = zgethex()) < 0)
		return c;
	crc = updcrc(c, crc);
	if ((c = zgethex()) < 0)
		return c;
	crc = updcrc(c, crc);
	if (crc != 0) {
		zperr(badcrc); return ERROR;
	}
	switch (READLINE_PF(1)) {
	case '\r'|0x80:
		/* **** FALL THRU TO **** */
	case '\r':
	 	/* Throw away possible cr/lf */
		READLINE_PF(1);
		break;
        default:
                ;
	}
	protocol = ZM_ZMODEM;
	zmodem_requested=true;
	return Rxtype;
}

/* Write a byte as two hex digits */
static void 
zputhex(unsigned int c, unsigned char *pos)
{
	static char	digits[]	= "0123456789abcdef";

	DPRINTF(DEBUG_PROTOCOL, "zputhex: %02X", c);
	pos[0]=digits[(c&0xF0)>>4];
	pos[1]=digits[c&0x0F];
}

void
zsendline_init(void)
{
	int i;
	for (i=0;i<256;i++) {	
		if (i & 0x60)
			zsendline_tab[i]=0;
		else {
			switch(i)
			{
			case ZDLE:
			case XOFF: /* ^Q */
			case XON: /* ^S */
			case (XOFF | 0x80):
			case (XON | 0x80):
				zsendline_tab[i]=1;
				break;
			case 0x10: /* ^P */
			case 0x10|0x80:
				if (turbo_escape)
					zsendline_tab[i]=0;
				else
					zsendline_tab[i]=1;
				break;
			case '\r':
			case '\r'|0x80:
				if (Zctlesc)
					zsendline_tab[i]=1;
				else if (!turbo_escape)
					zsendline_tab[i]=2;
				else 
					zsendline_tab[i]=0;
				break;
			default:
 				if (Zctlesc)
 					zsendline_tab[i]=1;
 				else
 					zsendline_tab[i]=0;
 			}
 		}
 	}
}



/* Store pos in Txhdr */
void 
stohdr(size_t pos)
{
	unsigned long lpos=(unsigned long) pos;
	Txhdr[ZP0] = lpos;
	Txhdr[ZP1] = lpos>>8;
	Txhdr[ZP2] = lpos>>16;
	Txhdr[ZP3] = lpos>>24;
}

/* Recover a long integer from a header */
unsigned long
rclhdr(const unsigned char *hdr)
{
	unsigned long l;

	l = (hdr[ZP3] & 0xff);
	l = (l << 8) | (hdr[ZP2] & 0xff);
	l = (l << 8) | (hdr[ZP1] & 0xff);
	l = (l << 8) | (hdr[ZP0] & 0xff);
	return l;
}

/* End of zm.c */

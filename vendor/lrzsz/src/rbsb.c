/*
  rbsb.c - terminal handling stuff for lrzsz
  Copyright (C) until 1988 Chuck Forsberg (Omen Technology INC)
  Copyright (C) 1994 Matt Porter, Michael D. Black
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

  originally written by Chuck Forsberg
*/

/*
 *  Rev 05-05-1988
 *  ============== (not quite, but originated there :-). -- uwe 
 */
#include "zglobal.h"

#include <stdio.h>
#include <errno.h>

static const struct {
	unsigned baudr;
	speed_t speedcode;
} speeds[] = {
	{110,	B110},
	{300,	B300},
	{600,	B600},
	{1200,	B1200},
	{2400,	B2400},
	{4800,	B4800},
#ifdef B7200
        {7200,  B7200},
#endif
	{9600,	B9600},
#ifdef B14400
        {14400,  B14400},
#endif
#ifdef B19200
    {19200,  B19200},
#endif
#ifdef B28800
        {28800,  B28800},
#endif
#ifdef B38400
    {38400,  B38400},
#endif
#ifdef B57600
    {57600,  B57600},
#endif
#ifdef B76800
    {76800,  B76800},
#endif
#ifdef B115200
    {115200,  B115200},
#endif
#ifdef B230400
    {230400,  B230400},
#endif
#ifdef B460800
    {460800,  B460800},
#endif
#ifdef B500000
    {500000,  B500000},
#endif
#ifdef B576000
    {576000,  B576000},
#endif
#ifdef B921600
    {921600,  B921600},
#endif
#ifdef B1000000
    {1000000,  B1000000},
#endif
#ifdef B1152000
    {1152000,  B1152000},
#endif
#ifdef B1500000
    {1500000,  B1500000},
#endif
#ifdef B2000000
    {2000000,  B2000000},
#endif

#ifdef EXTA
	{19200,	EXTA},
#endif
#ifdef EXTB
	{38400,	EXTB},
#endif
	{0, 0}
};

unsigned
getspeed(speed_t code)
{
	int n;
	for (n=0; speeds[n].baudr; ++n)
		if (speeds[n].speedcode == code)
			return speeds[n].baudr;
	return 38400;	/* fallback when the speed code is unknown */
}

/*
 * return 1 if stdout and stderr are different devices
 *  indicating this program operating with a modem on a
 *  different line
 */
bool Fromcu;		/* Were called from cu or yam */
int
from_cu(void)
{
	struct stat a, b;

	/* in case fstat fails */
	a.st_rdev=b.st_rdev=a.st_dev=b.st_dev=0;

	fstat(1, &a); fstat(2, &b);

	/* check only for device equality only. decomposing into major/minor 
         * like before seems useless. */
	Fromcu = (a.st_rdev != b.st_rdev) || (a.st_dev != b.st_dev);
	return Fromcu;
}



bool Twostop;		/* Use two stop bits */

static struct termios oldtty, tty;

static int not_a_tty = false;	/* tcgetattr can never succeed on this fd */

/*
 * mode(n)
 *  3: save old tty stat, set raw mode with flow control
 *  2: set XON/XOFF for sb/sz with ZMODEM or YMODEM-g
 *  1: save old tty stat, set raw mode 
 *  0: restore original tty mode
 */
int 
io_mode(int fd, int n)
{
	static int did0 = 0;
	static int have_oldtty = false;

	DPRINTF(DEBUG_TTY, "mode:%d\n", n);

	if(!did0) {
		did0 = 1;
		if (tcgetattr(fd,&oldtty) == 0)
			have_oldtty = true;
		else if (errno == EINTR) {
			/* transient failure: don't latch did0 and don't
			 * change the line now, so the next call can still
			 * capture the UNMODIFIED state. (Setting a mode here
			 * would make the later tcgetattr save the changed
			 * settings, and the exit would "restore" those.) */
			did0 = 0;
			return ERROR;
		} else {
			/* not a terminal (pipe, pty master...): tcgetattr can
			 * never succeed on this fd. Whatever the errno is
			 * (ENOTTY on Linux, EOPNOTSUPP on OpenBSD fifos),
			 * don't retry it forever. */
			did0 = 2;
			not_a_tty = true;
			DPRINTF(DEBUG_TTY, "tcgetattr failed: not a tty (errno %d)\n",
				errno);
		}
	}

	switch(n) {
	case 2:		/* Un-raw mode used by sz, sb when -g detected */
		if (!have_oldtty)
			return OK; /* not a tty: nothing to set */
		tty = oldtty;

		tty.c_iflag = BRKINT|IXON;

		tty.c_oflag = 0;	/* Transparent output */

		tty.c_cflag &= ~ (unsigned int) PARENB;	/* Disable parity */
		tty.c_cflag |= CS8;	/* Set character size = 8 */
		if (Twostop)
			tty.c_cflag |= CSTOPB;	/* Set two stop bits */

		tty.c_lflag = protocol==ZM_ZMODEM ? 0 : ISIG;
		tty.c_cc[VINTR] = protocol==ZM_ZMODEM ? _POSIX_VDISABLE : CAN;	/* Interrupt char */
                tty.c_cc[VQUIT] = _POSIX_VDISABLE;		/* Quit char */
		tty.c_cc[VMIN] = 1; /* 1 char satisfied reads */
		tty.c_cc[VTIME] = 1; /* or in this many tenths of seconds */

		if (tcsetattr(fd,TCSADRAIN,&tty) < 0)
			DPRINTF(DEBUG_TTY, "tcsetattr failed: %s\n", strerror(errno));

		return OK;
	case 1:
	case 3:
		if (!have_oldtty) {
			/* not a terminal (pipe etc.): there is nothing to set,
			 * and cfgetospeed() on a zeroed struct would just hit
			 * getspeed()'s fallback. Preserve that fallback's value
			 * so pipe-based setups keep their block-size behavior. */
			Baudrate = 38400;
			return OK;
		}
		tty = oldtty;

		tty.c_iflag = IGNBRK;
		if (n==3) /* with flow control */
			tty.c_iflag |= IXOFF;

		/* Setup raw mode: no echo, noncanonical (no edit chars),
		 * no signal generating chars, and no extended chars (^V, 
		 * ^O, ^R, ^W).
		 */
		tty.c_lflag &= ~(unsigned int) (ECHO | ICANON | ISIG | IEXTEN);
		tty.c_oflag = 0;	/* Transparent output */

		tty.c_cflag &= ~((unsigned int) PARENB);	/* Same baud rate, disable parity */
		/* Set character size = 8 */
		tty.c_cflag &= ~((unsigned int) CSIZE);
		tty.c_cflag |= CS8;	
		if (Twostop)
			tty.c_cflag |= CSTOPB;	/* Set two stop bits */
		tty.c_cc[VMIN] = 1; /* This many chars satisfies reads */
		tty.c_cc[VTIME] = 1;	/* or in this many tenths of seconds */
		if (tcsetattr(fd,TCSADRAIN,&tty) < 0)
			DPRINTF(DEBUG_TTY, "tcsetattr failed: %s\n", strerror(errno));
		Baudrate = getspeed(cfgetospeed(&tty));
		return OK;
	case 0:
		if(!did0)
			return ERROR;
		if (have_oldtty) {
			if (tcsetattr(fd,TCSANOW,&oldtty) < 0)
				lrzsz_warning(0, _("tcsetattr failed: %s"), strerror(errno));
		}
		tcflush(fd,TCIOFLUSH); /* flush input queue */
		tcflow(fd,TCOON); /* restart output */

		return OK;
	default:
		return ERROR;
	}
}

void
sendbrk(int fd)
{
	/* remember that the fd cannot do breaks (e.g. ENOTTY on a pipe);
	 * don't retry forever on a line that lies about it either */
	static int nobreak;

	if (nobreak)
		return;
	if (not_a_tty) {
		nobreak = 1;
		return;
	}
	if (tcsendbreak(fd,0) < 0) {
		if (errno == ENOTTY)
			nobreak = 1;
	}
}

void
purgeline(int fd)
{
	readline_purge();
	tcflush(fd, TCIFLUSH);
}

/* End of rbsb.c */

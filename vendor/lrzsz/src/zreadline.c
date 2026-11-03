/*
  zreadline.c - line reading stuff for lrzsz
  Copyright (C) until 1998 Chuck Forsberg (OMEN Technology Inc)
  Copyright (C) 1994 Matt Porter
  Copyright (C) 1996, 1997, 2026+ Uwe Ohse

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
/* once part of lrz.c, taken out to be useful to lsz.c too */

#include "zglobal.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>


/* Ward Christensen / CP/M parameters - Don't change these! */
#define TIMEOUT (-2)

static size_t readline_readnum;
static int readline_fd;
static unsigned char *readline_buffer;
int readline_left=0;
unsigned char *readline_ptr;

/*
 * This version of readline is reasonably well suited for
 * reading many characters.
 *
 * timeout is in tenths of seconds
 */
int
readline_internal(unsigned int timeout)
{
	fd_set fds;
	struct timeval tv;
	struct timeval *tvp;
	int rc;

	DPRINTF(DEBUG_READLINE, "Calling read: timeout=%u/10s  readnum=%lu ",
	  timeout, readline_readnum);

	if (no_timeout) {
		tvp = NULL;
	} else {
		tv.tv_sec = timeout / 10;
		tv.tv_usec = (long)(timeout % 10) * 100000L;
		tvp = &tv;
	}

	FD_ZERO(&fds);
	FD_SET(readline_fd, &fds);
	rc = select(readline_fd + 1, &fds, NULL, NULL, tvp);
	if (rc < 0) {
		if (errno == EINTR)
			return TIMEOUT;
		if (debugmode & DEBUG_READLINE)
			DPRINTF(DEBUG_READLINE, "select error: errno=%d:%s\n", errno, strerror(errno));
		stats_net_error();
		return ERROR;
	}
	if (rc == 0)
		return TIMEOUT;

	readline_ptr=readline_buffer;
	readline_left=read(readline_fd, readline_ptr, readline_readnum);
	stats_net_read(readline_left);
	if (debugmode & DEBUG_READLINE) {
		DPRINTF(DEBUG_READLINE, "Read returned %d bytes\n", readline_left);
		if (readline_left==-1)
			DPRINTF(DEBUG_READLINE, "errno=%d:%s\n", errno,strerror(errno));
		if (readline_left>0) {
			int i,j;
			j=readline_left > 48 ? 48 : readline_left;
			vstring("    ");
			for (i=0;i<j;i++) {
				if (i%24==0 && i)
					vstring("\n    ");
				vstringf("%02x ", readline_ptr[i]);
			}
			vstringf("\n");
		}
	}
	if (readline_left == 0) {
		/* EOF: carrier lost, peer closed the line */
		if (debugmode & DEBUG_READLINE)
			DPRINTF(DEBUG_READLINE, "read: EOF on the line\n");
		stats_net_error();
		return RCDO;
	}
	if (readline_left < 0) {
		if (errno == EINTR)
			return TIMEOUT;
		if (debugmode & DEBUG_READLINE)
			DPRINTF(DEBUG_READLINE, "read error: errno=%d:%s\n", errno, strerror(errno));
		stats_net_error();
		return ERROR;
	}
	--readline_left;
	return (*readline_ptr++ & 0377);
}



void
readline_setup(int fd, size_t readnum, size_t bufsize)
{
	readline_fd=fd;
	readline_readnum=readnum;
	readline_buffer=malloc(bufsize > readnum ? bufsize : readnum);
	if (!readline_buffer)
		fatal_error(1,0,_("out of memory"));
}

void
readline_purge(void)
{
	readline_left=0;
}

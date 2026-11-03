/*
  canit - cancel zmodem connection 
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
*/
#include "zglobal.h"

#include <errno.h>

/* send cancel string to get the other end to shut up */
static void
write_all(int fd, const char *buf, size_t len)
{
	size_t off;

	off = 0;
	while (off < len) {
		ssize_t n = write(fd, buf + off, len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			/* no much we can do: ignore and hope, or exit
			 * in any other program i'd opt for exit, but serials
			 * ports play strange games.
			 */
			return;
		}
		off += (size_t) n; /* n > 0: short writes are completed */
	}
}

void
canit (int fd)
{
	static const char canistr[] =
	{
		24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
	};
	purgeline(fd);
	write_all(fd, canistr, sizeof canistr);
	if (fd==0) {
		purgeline(1);
		write_all(1, canistr, sizeof canistr);
        }
}


/*
  timing.c - Timing routines for computing elapsed wall time
  Copyright (C) 1994 Michael D. Black
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

  originally written by Michael D. Black, mblack@csihq.com
*/

#include "zglobal.h"

#include "timing.h"

static double starttime;
static int initialized = 0;

static double
timing_now(void)
{
  /* CLOCK_MONOTONIC: durations (bps, ETA, min_bps watchdog) must not be
   * affected by NTP steps or date changes (zmodem-wip.txt uses wall
   * time only for the user-specified -s stop deadline, which timing.c
   * callers obtain via time(2) separately). */
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    /* unreachable on POSIX-2008 systems */
    return (double) time(NULL);
  }
  return ts.tv_sec + ts.tv_nsec/1000000000.0;
}

void
timing_reset(void)
{
  starttime = timing_now();
  initialized = 1;
}

double
timing_elapsed(void)
{
  double yet = timing_now();
  if (!initialized) {
    starttime = yet;
    initialized = 1;
  }
  return yet - starttime;
}

long
timing_bps(double bytes)
{
  double d = timing_elapsed();
  if (d == 0) /* can happen on the very first call (zero elapsed) */
    d = 0.5;
  return (long) (bytes / d);
}


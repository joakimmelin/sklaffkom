/* ibolview.h */

/*
 *   SklaffKOM, a simple conference system for UNIX.
 *
 *   Copyright (C) 1993-1994  Torbjörn Bååth, Peter Forsberg, Peter Lindberg,
 *                            Odd Petersson, Carl Sundbom
 *   Copyright (C) 2026       Peter London
 *
 *   Program dedicated to the memory of Staffan Bergström.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#ifndef IBOLVIEW_H
#define IBOLVIEW_H

/*
 * Display up to count cached IBOL entries, newest -> oldest.
 *
 * Returns the number of entries displayed, 0 when count is zero or there
 * are no usable entries, and -1 if the cache could not be read or output
 * was interrupted.
 */
int display_ibol_entries(int count);

/* Same renderer, but useful for diagnostics/tests with another cache file. */
int display_ibol_entries_from(const char *path, int count);

/* Ask the current user for one physical oneliner and queue it for FTN export. */
int prompt_ibol_oneliner(void);

#endif /* IBOLVIEW_H */

/* iboldb.h */

/*
 *   SklaffKOM, a simple conference system for UNIX.
 *
 *   Copyright (C) 1993-1994  Torbjörn Bååth, Peter Forsberg, Peter Lindberg,
 *                            Odd Petersson, Carl Sundbom
 *   Copyright (C) 2026       Peter London
 *
 *   Program dedicated to the memory of Staffan Bergström.
 *
 *   For questions about this program, mail sklaff@sklaffkom.se
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#ifndef IBOLDB_H
#define IBOLDB_H

#include "ibol.h"

#include <stddef.h>
#include <stdio.h>

#ifndef SKLAFFDIR
#define SKLAFFDIR "/usr/local/sklaff"
#endif

#define IBOL_DB_DEFAULT    SKLAFFDIR "/db/ibol/entries"
#define IBOL_STATE_DEFAULT SKLAFFDIR "/db/ibol/state"

#define IBOL_DB_MSGID_MAX 256

struct ibol_record {
    long long timestamp;
    char msgid[IBOL_DB_MSGID_MAX];
    char author[IBOL_AUTHOR_MAX];
    char source[IBOL_SOURCE_MAX];
    char text[IBOL_TEXT_MAX];
};

void ibol_record_init(struct ibol_record *record);

/*
 * Cache format, one physical line per entry:
 *
 *   unix_time:MSGID:Author:Source:text
 *
 * The characters '\\', ':', '\n', '\r' and '\t' are escaped with a leading
 * backslash.  Newlines in a multi-part IBOL message therefore remain inside
 * one physical cache line.
 */
int ibol_db_write_record(FILE *fp, const struct ibol_record *record);
int ibol_db_parse_line(const char *line, struct ibol_record *record,
    char *error, size_t errorsz);

/* Append one record to an existing cache file. */
int ibol_db_append(const char *path, const struct ibol_record *record);

/* Read the last valid physical record without scanning the whole cache. */
int ibol_db_read_latest(const char *path, struct ibol_record *record,
    char *error, size_t errorsz);

/*
 * Read up to max_records valid records from the end of the cache.
 * Returned records are ordered oldest -> newest, suitable for display.
 * Damaged physical lines are skipped in the same way as read_latest().
 */
int ibol_db_read_recent(const char *path, struct ibol_record *records,
    size_t max_records, size_t *count_out, char *error, size_t errorsz);

#endif /* IBOLDB_H */

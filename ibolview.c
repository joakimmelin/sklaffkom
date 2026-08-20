/* ibolview.c */

#define _POSIX_C_SOURCE 200809L

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

#include "sklaff.h"
#include "ext_globals.h"
#include "iboldb.h"
#include "ibolview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int format_ibol_date(long long timestamp, char *datebuf,
    size_t datebufsz);
static const char *display_text_start(const struct ibol_record *record,
    const char *datebuf);
static int output_ibol_text(const char *text);
static int output_ibol_record(const struct ibol_record *record);

static int
format_ibol_date(long long timestamp, char *datebuf, size_t datebufsz)
{
    time_t when;
    struct tm tmv;

    if (datebuf == NULL || datebufsz == 0)
        return -1;

    when = (time_t)timestamp;
    if ((long long)when != timestamp || localtime_r(&when, &tmv) == NULL)
        return -1;

    if (strftime(datebuf, datebufsz, "%Y-%m-%d %H:%M", &tmv) == 0)
        return -1;

    return 0;
}

/*
 * Some IBOL clients put YYYY-MM-DD at the start of the text itself.
 * If it matches the FTN/cache date, suppress only that display-time prefix.
 * The cache is never modified.
 */
static const char *
display_text_start(const struct ibol_record *record, const char *datebuf)
{
    const char *text;

    if (record == NULL)
        return "";

    text = record->text;
    if (datebuf != NULL && strlen(datebuf) >= 10 &&
        strncmp(text, datebuf, 10) == 0 && text[10] == ' ' &&
        text[11] != '\0')
        return text + 11;

    return text;
}

/* Prefix every physical Oneliner line with a deliberately plain "> ". */
static int
output_ibol_text(const char *text)
{
    const char *p;

    if (text == NULL)
        text = "";

    p = text;
    for (;;) {
        const char *nl;
        size_t len;

        nl = strchr(p, '\n');
        len = nl != NULL ? (size_t)(nl - p) : strlen(p);

        if (output_raw("> ") == -1)
            return -1;

        if (len > 0 && output_raw("%.*s", (int)len, p) == -1)
            return -1;

        if (output_raw("\n") == -1)
            return -1;

        if (nl == NULL)
            break;

        p = nl + 1;
        if (*p == '\0')
            break;
    }

    return 0;
}

static int
output_ibol_record(const struct ibol_record *record)
{
    char datebuf[32];
    const char *author;
    const char *source;
    const char *text;

    if (record == NULL)
        return -1;

    if (format_ibol_date(record->timestamp, datebuf, sizeof(datebuf)) != 0)
        snprintf(datebuf, sizeof(datebuf), "0000-00-00 00:00");

    author = record->author[0] != '\0' ? record->author : "?";
    source = record->source[0] != '\0' ? record->source : "?";
    text = display_text_start(record, datebuf);

    /*
     * Author/source/body are external BBS data, not SklaffKOM SF7 strings.
     * Keep them on the raw output path while still allowing ANSI colour.
     */
    if (output_ansi_raw_fmt(BR_YELLOW "%s" DOT, "%s", author) == -1)
        return -1;
    if (output(" %s ", MSG_IBOL_AT) == -1)
        return -1;
    if (output_ansi_raw_fmt(PURPLE "%s" DOT, "%s", source) == -1)
        return -1;
    if (output_raw(", ") == -1)
        return -1;
    if (output_ansi_raw_fmt(CYAN "%s" DOT, "%s", datebuf) == -1)
        return -1;
    if (output_raw(":\n") == -1)
        return -1;

    if (output_ibol_text(text) != 0)
        return -1;

    return 0;
}

int
display_ibol_entries_from(const char *path, int count)
{
    struct ibol_record *records;
    size_t nrecords;
    size_t i;
    char error[256];

    if (count <= 0)
        return 0;
    if (path == NULL || *path == '\0')
        return -1;

    records = calloc((size_t)count, sizeof(*records));
    if (records == NULL)
        return -1;

    nrecords = 0;
    error[0] = '\0';
    if (ibol_db_read_recent(path, records, (size_t)count, &nrecords,
            error, sizeof(error)) != 0) {
        free(records);
        return -1;
    }

    if (nrecords == 0) {
        free(records);
        return 0;
    }

    if (output("\n%s\n\n", MSG_IBOL_HEADER) == -1) {
        free(records);
        return -1;
    }

    for (i = nrecords; i > 0; i--) {
        if (output_ibol_record(&records[i - 1]) != 0) {
            free(records);
            return -1;
        }

        if (i > 1 && output("\n") == -1) {
            free(records);
            return -1;
        }
    }

    if (output("\n") == -1) {
        free(records);
        return -1;
    }

    free(records);
    return (int)nrecords;
}

int
display_ibol_entries(int count)
{
    return display_ibol_entries_from(IBOL_DB_DEFAULT, count);
}

/*
 * prompt_ibol_oneliner - optionally queue one local InterBBS Oneliner
 * ret: 1 if queued, 0 if declined/empty, -1 on local error
 *
 * SklaffKOM deliberately emits exactly one physical Oneliner line even
 * though the importer accepts multi-line posts from other BBS software.
 *
 * modified on 2026-08-13, PL
 */
int
prompt_ibol_oneliner(void)
{
    LINE answer;
    LINE text;
    LINE author;

    if (IBOL_LOGOUT_COUNT <= 0)
        return 0;

    if (output("%s", MSG_IBOL_WRITE_PROMPT) == -1)
        return -1;

    input("", answer, 4, 0, 0, 0);
    down_string(answer);

    if (answer[0] != MSG_YESANSWER)
        return 0;

    if (output("%s", MSG_IBOL_LINE_PROMPT) == -1)
        return -1;

    input("", text, IBOL_ONELINER_MAX, 0, 0, 0);
    rtrim(text);

    while (text[0] == ' ' || text[0] == '\t')
        memmove(text, text + 1, strlen(text));

    if (text[0] == '\0') {
        output("\n%s\n\n", MSG_IBOL_EMPTY);
        return 0;
    }

    if (strchr(text, '\n') != NULL || strchr(text, '\r') != NULL) {
        output("\n%s\n\n", MSG_IBOL_SEND_ERROR);
        return -1;
    }

    if (user_name(Uid, author) == NULL || author[0] == '\0') {
        output("\n%s\n\n", MSG_IBOL_SEND_ERROR);
        return -1;
    }

    if (queue_ibol_oneliner(Uid, author, text) != 0) {
        output("\n%s\n\n", MSG_IBOL_SEND_ERROR);
        return -1;
    }

    output("\n%s\n\n", MSG_IBOL_SENT);
    return 1;
}


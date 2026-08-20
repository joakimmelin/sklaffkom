/* iboldb.c */

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
 *   For questions about this program, mail sklaff@sklaffkom.se
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "iboldb.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void set_error(char *error, size_t errorsz, const char *fmt, ...);
static int write_escaped(FILE *fp, const char *s);
static int append_decoded(char *dst, size_t dstsz, size_t *used, char ch);
static int parse_decoded_field(const char **pp, int final_field,
    char *dst, size_t dstsz, char *error, size_t errorsz);
static int sync_file(FILE *fp);

void
ibol_record_init(struct ibol_record *record)
{
    if (record != NULL)
        memset(record, 0, sizeof(*record));
}

static void
set_error(char *error, size_t errorsz, const char *fmt, ...)
{
    va_list ap;

    if (error == NULL || errorsz == 0)
        return;

    va_start(ap, fmt);
    vsnprintf(error, errorsz, fmt, ap);
    va_end(ap);
}

static int
write_escaped(FILE *fp, const char *s)
{
    const unsigned char *p;

    if (fp == NULL)
        return -1;
    if (s == NULL)
        s = "";

    for (p = (const unsigned char *)s; *p != '\0'; p++) {
        switch (*p) {
        case '\\':
            if (fputs("\\\\", fp) == EOF)
                return -1;
            break;
        case ':':
            if (fputs("\\:", fp) == EOF)
                return -1;
            break;
        case '\n':
            if (fputs("\\n", fp) == EOF)
                return -1;
            break;
        case '\r':
            if (fputs("\\r", fp) == EOF)
                return -1;
            break;
        case '\t':
            if (fputs("\\t", fp) == EOF)
                return -1;
            break;
        default:
            if (fputc(*p, fp) == EOF)
                return -1;
            break;
        }
    }

    return 0;
}

int
ibol_db_write_record(FILE *fp, const struct ibol_record *record)
{
    if (fp == NULL || record == NULL)
        return -1;

    if (fprintf(fp, "%lld:", record->timestamp) < 0)
        return -1;
    if (write_escaped(fp, record->msgid) != 0 || fputc(':', fp) == EOF)
        return -1;
    if (write_escaped(fp, record->author) != 0 || fputc(':', fp) == EOF)
        return -1;
    if (write_escaped(fp, record->source) != 0 || fputc(':', fp) == EOF)
        return -1;
    if (write_escaped(fp, record->text) != 0 || fputc('\n', fp) == EOF)
        return -1;

    return ferror(fp) ? -1 : 0;
}

static int
append_decoded(char *dst, size_t dstsz, size_t *used, char ch)
{
    if (*used + 1 >= dstsz)
        return -1;
    dst[(*used)++] = ch;
    dst[*used] = '\0';
    return 0;
}

static int
parse_decoded_field(const char **pp, int final_field,
    char *dst, size_t dstsz, char *error, size_t errorsz)
{
    const char *p;
    size_t used;

    if (pp == NULL || *pp == NULL || dst == NULL || dstsz == 0)
        return -1;

    p = *pp;
    used = 0;
    dst[0] = '\0';

    while (*p != '\0' && *p != '\n' && *p != '\r') {
        char ch;

        if (!final_field && *p == ':') {
            p++;
            *pp = p;
            return 0;
        }

        if (*p != '\\') {
            if (append_decoded(dst, dstsz, &used, *p++) != 0) {
                set_error(error, errorsz, "cache field is too long");
                return -1;
            }
            continue;
        }

        p++;
        if (*p == '\0' || *p == '\n' || *p == '\r') {
            set_error(error, errorsz, "dangling escape at end of cache field");
            return -1;
        }

        switch (*p) {
        case 'n': ch = '\n'; break;
        case 'r': ch = '\r'; break;
        case 't': ch = '\t'; break;
        case ':': ch = ':'; break;
        case '\\': ch = '\\'; break;
        default:
            /* Be conservative with future/unknown escapes: preserve them. */
            if (append_decoded(dst, dstsz, &used, '\\') != 0) {
                set_error(error, errorsz, "cache field is too long");
                return -1;
            }
            ch = *p;
            break;
        }
        p++;

        if (append_decoded(dst, dstsz, &used, ch) != 0) {
            set_error(error, errorsz, "cache field is too long");
            return -1;
        }
    }

    if (!final_field) {
        set_error(error, errorsz, "cache line has too few fields");
        return -1;
    }

    *pp = p;
    return 0;
}

int
ibol_db_parse_line(const char *line, struct ibol_record *record,
    char *error, size_t errorsz)
{
    const char *p;
    char stamp[64];
    char *end;
    long long timestamp;

    if (line == NULL || record == NULL) {
        set_error(error, errorsz, "line or record is NULL");
        return -1;
    }

    ibol_record_init(record);
    p = line;

    if (parse_decoded_field(&p, 0, stamp, sizeof(stamp), error, errorsz) != 0)
        return -1;

    errno = 0;
    timestamp = strtoll(stamp, &end, 10);
    if (errno != 0 || end == stamp || *end != '\0' || timestamp < 0) {
        set_error(error, errorsz, "invalid unix timestamp '%s'", stamp);
        return -1;
    }
    record->timestamp = timestamp;

    if (parse_decoded_field(&p, 0, record->msgid, sizeof(record->msgid),
            error, errorsz) != 0 ||
        parse_decoded_field(&p, 0, record->author, sizeof(record->author),
            error, errorsz) != 0 ||
        parse_decoded_field(&p, 0, record->source, sizeof(record->source),
            error, errorsz) != 0 ||
        parse_decoded_field(&p, 1, record->text, sizeof(record->text),
            error, errorsz) != 0)
        return -1;

    if (record->msgid[0] == '\0') {
        set_error(error, errorsz, "cache record has empty MSGID");
        return -1;
    }

    if (error != NULL && errorsz > 0)
        error[0] = '\0';
    return 0;
}

static int
sync_file(FILE *fp)
{
    if (fflush(fp) != 0)
        return -1;
    if (fsync(fileno(fp)) != 0)
        return -1;
    return 0;
}

int
ibol_db_append(const char *path, const struct ibol_record *record)
{
    FILE *fp;
    int rc;

    if (path == NULL || record == NULL)
        return -1;

    fp = fopen(path, "a");
    if (fp == NULL)
        return -1;

    rc = ibol_db_write_record(fp, record);
    if (rc == 0)
        rc = sync_file(fp);
    if (fclose(fp) != 0)
        rc = -1;

    return rc;
}

static int
read_previous_physical_line(FILE *fp, long *scanp, char **line_out,
    char *error, size_t errorsz)
{
    long scan;
    long line_end;
    long line_start;
    long len;
    char *line;

    if (fp == NULL || scanp == NULL || line_out == NULL)
        return -1;

    *line_out = NULL;
    scan = *scanp;

    /* Ignore CR/LF between physical records and at EOF. */
    while (scan > 0) {
        int ch;

        if (fseek(fp, scan - 1, SEEK_SET) != 0) {
            set_error(error, errorsz, "cannot seek cache");
            return -1;
        }
        ch = fgetc(fp);
        if (ch != '\n' && ch != '\r')
            break;
        scan--;
    }

    if (scan == 0) {
        *scanp = 0;
        return 0;
    }

    line_end = scan;
    line_start = scan;
    while (line_start > 0) {
        int ch;

        if (fseek(fp, line_start - 1, SEEK_SET) != 0) {
            set_error(error, errorsz, "cannot seek cache");
            return -1;
        }
        ch = fgetc(fp);
        if (ch == '\n' || ch == '\r')
            break;
        line_start--;
    }

    len = line_end - line_start;
    *scanp = line_start;

    if (len <= 0)
        return 0;

    line = malloc((size_t)len + 1);
    if (line == NULL) {
        set_error(error, errorsz, "out of memory");
        return -1;
    }

    if (fseek(fp, line_start, SEEK_SET) != 0 ||
        fread(line, 1, (size_t)len, fp) != (size_t)len) {
        free(line);
        set_error(error, errorsz, "cannot read cache");
        return -1;
    }

    line[len] = '\0';
    *line_out = line;
    return 1;
}

int
ibol_db_read_recent(const char *path, struct ibol_record *records,
    size_t max_records, size_t *count_out, char *error, size_t errorsz)
{
    FILE *fp;
    long scan;
    long end;
    size_t found;

    if (count_out != NULL)
        *count_out = 0;

    if (path == NULL || count_out == NULL ||
        (max_records > 0 && records == NULL)) {
        set_error(error, errorsz, "invalid argument to ibol_db_read_recent");
        return -1;
    }

    if (max_records == 0) {
        if (error != NULL && errorsz > 0)
            error[0] = '\0';
        return 0;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        set_error(error, errorsz, "cannot open '%s': %s", path,
            strerror(errno));
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0 || (end = ftell(fp)) < 0) {
        set_error(error, errorsz, "cannot seek '%s'", path);
        fclose(fp);
        return -1;
    }

    scan = end;
    found = 0;

    while (scan > 0 && found < max_records) {
        char *line;
        struct ibol_record record;
        char parse_error[256];
        int rc;

        rc = read_previous_physical_line(fp, &scan, &line,
            error, errorsz);
        if (rc < 0) {
            fclose(fp);
            return -1;
        }
        if (rc == 0)
            break;

        parse_error[0] = '\0';
        rc = ibol_db_parse_line(line, &record,
            parse_error, sizeof(parse_error));
        free(line);

        if (rc == 0)
            records[found++] = record; /* newest -> oldest for now */
    }

    fclose(fp);

    if (found == 0) {
        *count_out = 0;
        if (error != NULL && errorsz > 0)
            error[0] = '\0';
        return 0;
    }

    /* Present callers with natural reading order: oldest -> newest. */
    {
        size_t i;

        for (i = 0; i < found / 2; i++) {
            struct ibol_record tmp;
            tmp = records[i];
            records[i] = records[found - 1 - i];
            records[found - 1 - i] = tmp;
        }
    }

    *count_out = found;
    if (error != NULL && errorsz > 0)
        error[0] = '\0';
    return 0;
}

int
ibol_db_read_latest(const char *path, struct ibol_record *record,
    char *error, size_t errorsz)
{
    size_t count;

    if (record == NULL) {
        set_error(error, errorsz, "record is NULL");
        return -1;
    }

    count = 0;
    if (ibol_db_read_recent(path, record, 1, &count,
            error, errorsz) != 0)
        return -1;

    if (count != 1) {
        set_error(error, errorsz, "cache contains no valid IBOL records");
        return -1;
    }

    return 0;
}

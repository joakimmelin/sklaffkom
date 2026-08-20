/* ibolimport.c */

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

#include "ftnmsg.h"
#include "ibol.h"
#include "iboldb.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef SKLAFFDIR
#define SKLAFFDIR "/usr/local/sklaff"
#endif

#define DEFAULT_IBOL_DB    SKLAFFDIR "/db/ibol/entries"
#define DEFAULT_IBOL_STATE SKLAFFDIR "/db/ibol/state"

struct msg_file {
    unsigned long number;
    char *name;
};

struct id_set {
    char **items;
    size_t count;
    size_t capacity;
};

struct import_stats {
    unsigned long files_seen;
    unsigned long files_selected;
    unsigned long ftn_failures;
    unsigned long non_ibol;
    unsigned long ibol_messages;
    unsigned long valid_entries;
    unsigned long empty_entries;
    unsigned long malformed_entries;
    unsigned long duplicate_msgids;
    unsigned long imported_entries;
    unsigned long synthetic_msgids;
    unsigned long fallback_dates;
};

enum import_mode {
    MODE_NONE = 0,
    MODE_REBUILD,
    MODE_UPDATE,
    MODE_LATEST
};

static void usage(FILE *fp, const char *prog);
static int parse_msg_number(const char *name, unsigned long *number);
static int collect_msg_files(const char *dirpath, struct msg_file **files_out,
    size_t *count_out, unsigned long *highest_out);
static void free_msg_files(struct msg_file *files, size_t count);
static int compare_msg_files(const void *a, const void *b);
static int ensure_parent_dirs(const char *path);
static int read_state(const char *path, unsigned long *last_msg);
static int write_state_atomic(const char *path, unsigned long last_msg);
static int id_set_contains(const struct id_set *set, const char *id);
static int id_set_add(struct id_set *set, const char *id);
static void id_set_free(struct id_set *set);
static int load_cache_ids(const char *path, struct id_set *set,
    unsigned long *bad_lines);
static int ftn_date_to_timestamp(const char *date, long long *timestamp);
static void copy_string(char *dst, size_t dstsz, const char *src);
static int make_record(const char *filepath, unsigned long msgno,
    const struct fido_msg *msg, const struct ibol_entry *entry,
    struct ibol_record *record, struct import_stats *stats);
static int cache_ends_with_newline(const char *path, int *ends_with_newline);
static int flush_and_sync(FILE *fp);
static int run_import(enum import_mode mode, const char *area,
    const char *dbpath, const char *statepath, int dry_run, int verbose);
static int show_latest(const char *dbpath);
static void print_summary(enum import_mode mode, const char *area,
    const char *dbpath, const char *statepath, unsigned long old_state,
    unsigned long new_state, const struct import_stats *stats, int dry_run);

static void
usage(FILE *fp, const char *prog)
{
    fprintf(fp,
        "Usage:\n"
        "  %s --rebuild [options] DIRECTORY\n"
        "  %s --update  [options] DIRECTORY\n"
        "  %s --latest  [--db FILE]\n"
        "\n"
        "Options:\n"
        "  --rebuild       rebuild the complete IBOL cache atomically\n"
        "  --update        import only .MSG files newer than the checkpoint\n"
        "  --latest        decode and display the latest cached entry\n"
        "  --db FILE       cache file (default %s)\n"
        "  --state FILE    checkpoint file (default %s)\n"
        "  --dry-run       scan and report without changing cache/state\n"
        "  -v, --verbose   show each imported/skipped IBOL message\n"
        "  -h, --help      show this help\n"
        "\n"
        "Cache format:\n"
        "  unix_time:MSGID:Author:Source:text\n"
        "  Colons, backslashes and embedded newlines are escaped.\n"
        "\n"
        "Build example:\n"
        "  cc -Wall -Wextra -O2 -o ibolimport \\\n"
        "     ibolimport.c iboldb.c ibol.c ftnmsg.c\n",
        prog, prog, prog, DEFAULT_IBOL_DB, DEFAULT_IBOL_STATE);
}

static int
parse_msg_number(const char *name, unsigned long *number)
{
    char *end;
    unsigned long n;

    if (name == NULL || !isdigit((unsigned char)name[0]))
        return 0;

    errno = 0;
    n = strtoul(name, &end, 10);
    if (errno != 0 || end == name || *end != '.')
        return 0;
    if (strcasecmp(end, ".msg") != 0)
        return 0;

    if (number != NULL)
        *number = n;
    return 1;
}

static int
compare_msg_files(const void *a, const void *b)
{
    const struct msg_file *ma = a;
    const struct msg_file *mb = b;

    if (ma->number < mb->number)
        return -1;
    if (ma->number > mb->number)
        return 1;
    return strcmp(ma->name, mb->name);
}

static int
collect_msg_files(const char *dirpath, struct msg_file **files_out,
    size_t *count_out, unsigned long *highest_out)
{
    DIR *dir;
    struct dirent *de;
    struct msg_file *files;
    size_t count;
    size_t capacity;
    unsigned long highest;

    if (files_out == NULL || count_out == NULL || highest_out == NULL)
        return -1;

    *files_out = NULL;
    *count_out = 0;
    *highest_out = 0;

    dir = opendir(dirpath);
    if (dir == NULL) {
        fprintf(stderr, "[ERROR] Could not open '%s': %s\n",
            dirpath, strerror(errno));
        return -1;
    }

    files = NULL;
    count = 0;
    capacity = 0;
    highest = 0;

    while ((de = readdir(dir)) != NULL) {
        unsigned long n;
        char *copy;

        if (!parse_msg_number(de->d_name, &n))
            continue;

        if (count == capacity) {
            size_t new_capacity = capacity == 0 ? 256 : capacity * 2;
            struct msg_file *tmp = realloc(files,
                new_capacity * sizeof(*files));
            if (tmp == NULL) {
                fprintf(stderr, "[ERROR] Out of memory\n");
                free_msg_files(files, count);
                closedir(dir);
                return -1;
            }
            files = tmp;
            capacity = new_capacity;
        }

        copy = strdup(de->d_name);
        if (copy == NULL) {
            fprintf(stderr, "[ERROR] Out of memory\n");
            free_msg_files(files, count);
            closedir(dir);
            return -1;
        }

        files[count].number = n;
        files[count].name = copy;
        count++;
        if (n > highest)
            highest = n;
    }

    closedir(dir);
    qsort(files, count, sizeof(*files), compare_msg_files);

    *files_out = files;
    *count_out = count;
    *highest_out = highest;
    return 0;
}

static void
free_msg_files(struct msg_file *files, size_t count)
{
    size_t i;

    if (files == NULL)
        return;
    for (i = 0; i < count; i++)
        free(files[i].name);
    free(files);
}

static int
ensure_parent_dirs(const char *path)
{
    char tmp[PATH_MAX];
    char *p;
    size_t len;

    if (path == NULL)
        return -1;
    len = strlen(path);
    if (len == 0 || len >= sizeof(tmp))
        return -1;

    memcpy(tmp, path, len + 1);
    p = strrchr(tmp, '/');
    if (p == NULL)
        return 0;
    if (p == tmp)
        return 0;
    *p = '\0';

    for (p = tmp + 1; *p != '\0'; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

static int
read_state(const char *path, unsigned long *last_msg)
{
    FILE *fp;
    char buf[128];
    char *end;
    unsigned long n;

    if (last_msg == NULL)
        return -1;
    *last_msg = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT)
            return 0;
        fprintf(stderr, "[ERROR] Could not read state '%s': %s\n",
            path, strerror(errno));
        return -1;
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        if (ferror(fp)) {
            fprintf(stderr, "[ERROR] Could not read state '%s'\n", path);
            fclose(fp);
            return -1;
        }
        fclose(fp);
        return 0;
    }
    fclose(fp);

    errno = 0;
    n = strtoul(buf, &end, 10);
    if (errno != 0 || end == buf) {
        fprintf(stderr, "[ERROR] Invalid state file '%s'\n", path);
        return -1;
    }
    while (*end != '\0' && isspace((unsigned char)*end))
        end++;
    if (*end != '\0') {
        fprintf(stderr, "[ERROR] Invalid state file '%s'\n", path);
        return -1;
    }

    *last_msg = n;
    return 0;
}

static int
write_state_atomic(const char *path, unsigned long last_msg)
{
    char tmppath[PATH_MAX];
    FILE *fp;
    int rc;

    if (ensure_parent_dirs(path) != 0) {
        fprintf(stderr, "[ERROR] Could not create parent directory for '%s': %s\n",
            path, strerror(errno));
        return -1;
    }

    if (snprintf(tmppath, sizeof(tmppath), "%s.tmp.%ld", path,
            (long)getpid()) >= (int)sizeof(tmppath))
        return -1;

    fp = fopen(tmppath, "w");
    if (fp == NULL)
        return -1;

    rc = fprintf(fp, "%lu\n", last_msg) < 0 ? -1 : 0;
    if (rc == 0)
        rc = flush_and_sync(fp);
    if (fclose(fp) != 0)
        rc = -1;

    if (rc != 0) {
        unlink(tmppath);
        return -1;
    }

    if (rename(tmppath, path) != 0) {
        unlink(tmppath);
        return -1;
    }
    return 0;
}

static int
id_set_contains(const struct id_set *set, const char *id)
{
    size_t i;

    if (set == NULL || id == NULL)
        return 0;
    for (i = 0; i < set->count; i++) {
        if (strcmp(set->items[i], id) == 0)
            return 1;
    }
    return 0;
}

static int
id_set_add(struct id_set *set, const char *id)
{
    char *copy;

    if (set == NULL || id == NULL)
        return -1;
    if (id_set_contains(set, id))
        return 0;

    if (set->count == set->capacity) {
        size_t new_capacity = set->capacity == 0 ? 128 : set->capacity * 2;
        char **tmp = realloc(set->items, new_capacity * sizeof(*set->items));
        if (tmp == NULL)
            return -1;
        set->items = tmp;
        set->capacity = new_capacity;
    }

    copy = strdup(id);
    if (copy == NULL)
        return -1;
    set->items[set->count++] = copy;
    return 0;
}

static void
id_set_free(struct id_set *set)
{
    size_t i;

    if (set == NULL)
        return;
    for (i = 0; i < set->count; i++)
        free(set->items[i]);
    free(set->items);
    memset(set, 0, sizeof(*set));
}

static int
load_cache_ids(const char *path, struct id_set *set, unsigned long *bad_lines)
{
    FILE *fp;
    char *line;
    size_t cap;
    ssize_t n;

    if (bad_lines != NULL)
        *bad_lines = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT)
            return 0;
        fprintf(stderr, "[ERROR] Could not open cache '%s': %s\n",
            path, strerror(errno));
        return -1;
    }

    line = NULL;
    cap = 0;
    while ((n = getline(&line, &cap, fp)) >= 0) {
        struct ibol_record record;
        char error[128];

        (void)n;
        if (ibol_db_parse_line(line, &record, error, sizeof(error)) != 0) {
            if (bad_lines != NULL)
                (*bad_lines)++;
            continue;
        }
        if (id_set_add(set, record.msgid) != 0) {
            free(line);
            fclose(fp);
            return -1;
        }
    }

    free(line);
    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int
ftn_date_to_timestamp(const char *date, long long *timestamp)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    struct tm tmv;
    char mon[8];
    int day, year, hour, min, sec, month, i;
    time_t t;

    if (date == NULL || timestamp == NULL)
        return -1;

    memset(&tmv, 0, sizeof(tmv));
    mon[0] = '\0';
    if (sscanf(date, " %d %7s %d %d:%d:%d",
            &day, mon, &year, &hour, &min, &sec) != 6)
        return -1;

    month = -1;
    for (i = 0; i < 12; i++) {
        if (strcasecmp(mon, months[i]) == 0) {
            month = i;
            break;
        }
    }
    if (month < 0 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
        min < 0 || min > 59 || sec < 0 || sec > 60)
        return -1;

    if (year < 70)
        year += 2000;
    else if (year < 100)
        year += 1900;

    tmv.tm_year = year - 1900;
    tmv.tm_mon = month;
    tmv.tm_mday = day;
    tmv.tm_hour = hour;
    tmv.tm_min = min;
    tmv.tm_sec = sec;
    tmv.tm_isdst = -1;

    /* FTN dates carry no timezone.  Interpret them in the importer timezone. */
    t = mktime(&tmv);
    if (t == (time_t)-1)
        return -1;

    *timestamp = (long long)t;
    return 0;
}

static void
copy_string(char *dst, size_t dstsz, const char *src)
{
    size_t n;

    if (dst == NULL || dstsz == 0)
        return;
    if (src == NULL)
        src = "";

    n = strlen(src);
    if (n >= dstsz)
        n = dstsz - 1;
    if (n > 0)
        memcpy(dst, src, n);
    dst[n] = '\0';
}

static int
make_record(const char *filepath, unsigned long msgno,
    const struct fido_msg *msg, const struct ibol_entry *entry,
    struct ibol_record *record, struct import_stats *stats)
{
    struct stat st;

    ibol_record_init(record);

    if (ftn_date_to_timestamp(msg->date, &record->timestamp) != 0) {
        if (stat(filepath, &st) != 0)
            return -1;
        record->timestamp = (long long)st.st_mtime;
        stats->fallback_dates++;
    }

    if (msg->msgid[0] != '\0') {
        copy_string(record->msgid, sizeof(record->msgid), msg->msgid);
    } else {
        snprintf(record->msgid, sizeof(record->msgid), "FILE:%lu", msgno);
        stats->synthetic_msgids++;
    }

    copy_string(record->author, sizeof(record->author),
        entry->author[0] ? entry->author : msg->from);
    copy_string(record->source, sizeof(record->source), entry->source);
    copy_string(record->text, sizeof(record->text), entry->text);
    return 0;
}

static int
cache_ends_with_newline(const char *path, int *ends_with_newline)
{
    FILE *fp;
    long end;
    int ch;

    *ends_with_newline = 1;
    fp = fopen(path, "rb");
    if (fp == NULL) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0 || (end = ftell(fp)) < 0) {
        fclose(fp);
        return -1;
    }
    if (end == 0) {
        fclose(fp);
        return 0;
    }
    if (fseek(fp, -1, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    ch = fgetc(fp);
    fclose(fp);
    *ends_with_newline = (ch == '\n' || ch == '\r');
    return 0;
}

static int
flush_and_sync(FILE *fp)
{
    if (fflush(fp) != 0)
        return -1;
    if (fsync(fileno(fp)) != 0)
        return -1;
    return 0;
}

static int
run_import(enum import_mode mode, const char *area,
    const char *dbpath, const char *statepath, int dry_run, int verbose)
{
    struct msg_file *files;
    size_t file_count;
    unsigned long highest;
    unsigned long old_state;
    unsigned long new_state;
    struct id_set ids;
    struct import_stats stats;
    FILE *out;
    char tmppath[PATH_MAX];
    int write_failed;
    int parse_failure_seen;
    size_t i;

    files = NULL;
    file_count = 0;
    highest = 0;
    old_state = 0;
    new_state = 0;
    memset(&ids, 0, sizeof(ids));
    memset(&stats, 0, sizeof(stats));
    out = NULL;
    tmppath[0] = '\0';
    write_failed = 0;
    parse_failure_seen = 0;

    if (collect_msg_files(area, &files, &file_count, &highest) != 0)
        return 1;
    stats.files_seen = (unsigned long)file_count;

    if (mode == MODE_UPDATE) {
        struct stat dbst;
        struct stat stst;
        unsigned long bad_cache_lines;

        if (read_state(statepath, &old_state) != 0)
            goto fail;

        if (old_state != 0 && highest < old_state) {
            fprintf(stderr,
                "[ERROR] Messagebase highest file is %lu.msg, below checkpoint %lu.msg; use --rebuild\n",
                highest, old_state);
            goto fail;
        }

        if (stat(statepath, &stst) == 0 && stat(dbpath, &dbst) != 0 &&
            errno == ENOENT) {
            fprintf(stderr,
                "[ERROR] State exists but cache '%s' is missing; use --rebuild\n",
                dbpath);
            goto fail;
        }

        if (load_cache_ids(dbpath, &ids, &bad_cache_lines) != 0) {
            fprintf(stderr, "[ERROR] Could not load existing cache IDs\n");
            goto fail;
        }
        if (verbose && bad_cache_lines != 0)
            printf("[WARN] Ignored %lu malformed cache line(s)\n",
                bad_cache_lines);
    }

    if (!dry_run) {
        if (ensure_parent_dirs(dbpath) != 0) {
            fprintf(stderr, "[ERROR] Could not create parent directory for '%s': %s\n",
                dbpath, strerror(errno));
            goto fail;
        }

        if (mode == MODE_REBUILD) {
            if (snprintf(tmppath, sizeof(tmppath), "%s.tmp.%ld", dbpath,
                    (long)getpid()) >= (int)sizeof(tmppath)) {
                fprintf(stderr, "[ERROR] Cache path too long\n");
                goto fail;
            }
            out = fopen(tmppath, "w");
        } else {
            int has_newline;
            if (cache_ends_with_newline(dbpath, &has_newline) != 0) {
                fprintf(stderr, "[ERROR] Could not inspect cache '%s'\n", dbpath);
                goto fail;
            }
            out = fopen(dbpath, "a");
            if (out != NULL && !has_newline && fputc('\n', out) == EOF)
                write_failed = 1;
        }

        if (out == NULL) {
            fprintf(stderr, "[ERROR] Could not open cache output: %s\n",
                strerror(errno));
            goto fail;
        }
    }

    for (i = 0; i < file_count; i++) {
        char filepath[PATH_MAX];
        struct fido_msg msg;
        struct ibol_entry entry;
        struct ibol_record record;
        char error[256];
        int rc;

        if (mode == MODE_UPDATE && files[i].number <= old_state)
            continue;
        stats.files_selected++;

        if (snprintf(filepath, sizeof(filepath), "%s/%s", area,
                files[i].name) >= (int)sizeof(filepath)) {
            fprintf(stderr, "[ERROR] Path too long: %s/%s\n",
                area, files[i].name);
            stats.ftn_failures++;
            parse_failure_seen = 1;
            continue;
        }

        if (read_fido_msg(filepath, &msg) != 0) {
            stats.ftn_failures++;
            parse_failure_seen = 1;
            if (verbose)
                printf("[FTN ERROR] %s\n", files[i].name);
            continue;
        }

        if (!ibol_is_message(&msg)) {
            stats.non_ibol++;
            free_fido_msg(&msg);
            continue;
        }

        stats.ibol_messages++;
        rc = ibol_parse_message(&msg, &entry, error, sizeof(error));
        if (rc == IBOL_PARSE_EMPTY) {
            stats.empty_entries++;
            if (verbose)
                printf("[EMPTY] %-12s %s\n", files[i].name, error);
            free_fido_msg(&msg);
            continue;
        }
        if (rc != IBOL_PARSE_OK) {
            stats.malformed_entries++;
            if (verbose)
                printf("[MALFORMED] %-12s %s\n", files[i].name, error);
            free_fido_msg(&msg);
            continue;
        }
        stats.valid_entries++;

        if (make_record(filepath, files[i].number, &msg, &entry,
                &record, &stats) != 0) {
            fprintf(stderr, "[ERROR] Could not create cache record for %s\n",
                files[i].name);
            free_fido_msg(&msg);
            write_failed = 1;
            break;
        }

        if (id_set_contains(&ids, record.msgid)) {
            stats.duplicate_msgids++;
            if (verbose)
                printf("[DUP] %-12s %s\n", files[i].name, record.msgid);
            free_fido_msg(&msg);
            continue;
        }

        if (!dry_run && ibol_db_write_record(out, &record) != 0) {
            fprintf(stderr, "[ERROR] Could not write cache record for %s\n",
                files[i].name);
            free_fido_msg(&msg);
            write_failed = 1;
            break;
        }

        if (id_set_add(&ids, record.msgid) != 0) {
            fprintf(stderr, "[ERROR] Out of memory while recording MSGID\n");
            free_fido_msg(&msg);
            write_failed = 1;
            break;
        }

        stats.imported_entries++;
        if (verbose)
            printf("[ADD] %-12s %s @ %s: %s\n", files[i].name,
                record.author, record.source, entry.text_flat);

        free_fido_msg(&msg);
    }

    new_state = highest;

    if (!dry_run && out != NULL) {
        if (!write_failed && flush_and_sync(out) != 0)
            write_failed = 1;
        if (fclose(out) != 0)
            write_failed = 1;
        out = NULL;
    }

    if (write_failed)
        goto fail;

    if (!dry_run && mode == MODE_REBUILD) {
        if (rename(tmppath, dbpath) != 0) {
            fprintf(stderr, "[ERROR] Could not install rebuilt cache '%s': %s\n",
                dbpath, strerror(errno));
            goto fail;
        }
        tmppath[0] = '\0';
    }

    /*
     * Never advance the checkpoint past an FTN file we failed to parse.
     * A later run will retry the range; MSGID de-duplication keeps that safe.
     */
    if (!dry_run && !parse_failure_seen) {
        if (write_state_atomic(statepath, new_state) != 0) {
            fprintf(stderr, "[ERROR] Could not update state '%s': %s\n",
                statepath, strerror(errno));
            goto fail;
        }
    } else if (parse_failure_seen) {
        new_state = old_state;
    }

    print_summary(mode, area, dbpath, statepath, old_state, new_state,
        &stats, dry_run);

    id_set_free(&ids);
    free_msg_files(files, file_count);
    return parse_failure_seen ? 1 : 0;

fail:
    if (out != NULL)
        fclose(out);
    if (tmppath[0] != '\0')
        unlink(tmppath);
    id_set_free(&ids);
    free_msg_files(files, file_count);
    return 1;
}

static void
print_summary(enum import_mode mode, const char *area,
    const char *dbpath, const char *statepath, unsigned long old_state,
    unsigned long new_state, const struct import_stats *stats, int dry_run)
{
    printf("\nIBOL import summary\n");
    printf("===================\n");
    printf("Mode:                      %s%s\n",
        mode == MODE_REBUILD ? "rebuild" : "update",
        dry_run ? " (dry-run)" : "");
    printf("Source:                    %s\n", area);
    printf("Cache:                     %s\n", dbpath);
    printf("State:                     %s\n", statepath);
    if (mode == MODE_UPDATE)
        printf("Previous checkpoint:       %lu.msg\n", old_state);
    printf("New checkpoint:            %lu.msg\n", new_state);
    printf(".MSG files seen:           %lu\n", stats->files_seen);
    printf(".MSG files selected:       %lu\n", stats->files_selected);
    printf("FTN parse failures:        %lu\n", stats->ftn_failures);
    printf("Non-IBOL messages:         %lu\n", stats->non_ibol);
    printf("IBOL messages:             %lu\n", stats->ibol_messages);
    printf("Valid IBOL entries:        %lu\n", stats->valid_entries);
    printf("Empty IBOL entries:        %lu\n", stats->empty_entries);
    printf("Malformed IBOL entries:    %lu\n", stats->malformed_entries);
    printf("Duplicate MSGIDs skipped:  %lu\n", stats->duplicate_msgids);
    printf("Entries imported:          %lu\n", stats->imported_entries);
    printf("Synthetic MSGIDs:          %lu\n", stats->synthetic_msgids);
    printf("Date fallbacks (mtime):    %lu\n", stats->fallback_dates);
}

static int
show_latest(const char *dbpath)
{
    struct ibol_record record;
    char error[256];
    time_t t;
    struct tm tmv;
    char datebuf[64];

    if (ibol_db_read_latest(dbpath, &record, error, sizeof(error)) != 0) {
        fprintf(stderr, "[ERROR] %s\n", error);
        return 1;
    }

    datebuf[0] = '\0';
    t = (time_t)record.timestamp;
    if (localtime_r(&t, &tmv) != NULL)
        strftime(datebuf, sizeof(datebuf), "%Y-%m-%d %H:%M:%S", &tmv);

    printf("Latest cached IBOL entry\n");
    printf("========================\n\n");
    if (datebuf[0])
        printf("Timestamp: %lld (%s)\n", record.timestamp, datebuf);
    else
        printf("Timestamp: %lld\n", record.timestamp);
    printf("MSGID:     %s\n", record.msgid);
    printf("Author:    %s\n", record.author);
    printf("Source:    %s\n", record.source);
    printf("Text:\n%s\n", record.text);
    return 0;
}

int
main(int argc, char **argv)
{
    enum import_mode mode;
    const char *area;
    const char *dbpath;
    const char *statepath;
    char derived_state[PATH_MAX];
    int db_was_set;
    int state_was_set;
    int dry_run;
    int verbose;
    int i;

    mode = MODE_NONE;
    area = NULL;
    dbpath = DEFAULT_IBOL_DB;
    statepath = DEFAULT_IBOL_STATE;
    derived_state[0] = '\0';
    db_was_set = 0;
    state_was_set = 0;
    dry_run = 0;
    verbose = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rebuild") == 0) {
            if (mode != MODE_NONE) {
                fprintf(stderr, "[ERROR] Choose exactly one mode\n");
                return 2;
            }
            mode = MODE_REBUILD;
        } else if (strcmp(argv[i], "--update") == 0) {
            if (mode != MODE_NONE) {
                fprintf(stderr, "[ERROR] Choose exactly one mode\n");
                return 2;
            }
            mode = MODE_UPDATE;
        } else if (strcmp(argv[i], "--latest") == 0) {
            if (mode != MODE_NONE) {
                fprintf(stderr, "[ERROR] Choose exactly one mode\n");
                return 2;
            }
            mode = MODE_LATEST;
        } else if (strcmp(argv[i], "--db") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "[ERROR] --db requires a path\n");
                return 2;
            }
            dbpath = argv[i];
            db_was_set = 1;
        } else if (strcmp(argv[i], "--state") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "[ERROR] --state requires a path\n");
                return 2;
            }
            statepath = argv[i];
            state_was_set = 1;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = 1;
        } else if (strcmp(argv[i], "--verbose") == 0 ||
            strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--help") == 0 ||
            strcmp(argv[i], "-h") == 0) {
            usage(stdout, argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "[ERROR] Unknown option: %s\n", argv[i]);
            usage(stderr, argv[0]);
            return 2;
        } else if (area == NULL) {
            area = argv[i];
        } else {
            fprintf(stderr, "[ERROR] Too many source directories\n");
            return 2;
        }
    }

    if (mode == MODE_NONE) {
        usage(stderr, argv[0]);
        return 2;
    }

    if (db_was_set && !state_was_set) {
        if (snprintf(derived_state, sizeof(derived_state), "%s.state",
                dbpath) >= (int)sizeof(derived_state)) {
            fprintf(stderr, "[ERROR] Derived state path is too long\n");
            return 2;
        }
        statepath = derived_state;
    }

    if (mode == MODE_LATEST) {
        if (area != NULL || dry_run || verbose || state_was_set) {
            fprintf(stderr, "[ERROR] --latest only accepts --db FILE\n");
            return 2;
        }
        return show_latest(dbpath);
    }

    if (area == NULL) {
        fprintf(stderr, "[ERROR] Source DIRECTORY is required\n");
        usage(stderr, argv[0]);
        return 2;
    }

    return run_import(mode, area, dbpath, statepath, dry_run, verbose);
}

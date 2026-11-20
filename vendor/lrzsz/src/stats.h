#ifndef LRZSZ_STATS_H
#define LRZSZ_STATS_H

#include <sys/types.h> /* size_t */

struct lrzsz_stat {
    char *filename;
    double start_time;

    /* a message is a sector/subframe/packet, something with an own checksum. */
    unsigned long valid_messages;
    unsigned long invalid_messages;
    unsigned long out_of_sync_messages; /* a valid message out of sync */
    /* we were reading a message, had identified it as such, but could not find its end */
    unsigned long format_errors;

    /* what happened to files: */
    unsigned long files_ok;
    unsigned long files_err;
    unsigned long local_errors;
    unsigned long policy_rejects;
    unsigned long skipped;
    unsigned long cancels; /* a transfer was cancelled after one side began to send data. */

    /* general error handling */
    unsigned long timeouts; /* timeout */
    unsigned long too_many_errors; /* some operation failed because of that */

    /* counters */
    unsigned long long total_message_bytes;
    unsigned long long net_bytes_in;
    unsigned long net_read_ops;
    unsigned long long net_bytes_out;
    unsigned long net_write_ops;
    unsigned long net_errors; /* read errors and hangups on the transfer line */

    char direction; /* 'r' = receive (lrz), 's' = send (lsz) */
};
extern struct lrzsz_stat stat_for_file;
extern struct lrzsz_stat stat_for_all;

void stats_account_one(void);
void stats_account_finalize_file(void);
void stats_start_one(void);
void stats_log(struct lrzsz_stat *st, bool forone);
void stats_set_filename(const char *);
void stats_journal_open(const char *path);
void stats_journal_close(void);

/*
 * Event accounting helpers: the single write barrier for stat_for_file
 * counters, so protocol code states what happened without touching the
 * struct. One call per event; see the matching comments in struct
 * lrzsz_stat for the field semantics.
 *
 * All counters are per file (stat_for_file); stats_account_one() rolls
 * them into stat_for_all at batch boundaries.
 *
 * Debug aid: compile with -DSTATS_TRACE to log every accounting event to
 * stderr (useful for tracking where counters originate; far more readable
 * than a raw struct hexdump).
 */
void stats_msg_valid(size_t bytes);  /* valid message carrying bytes payload */
void stats_msg_invalid(void);        /* message failed its checksum */
void stats_msg_out_of_sync(void);    /* valid message, but out of sync */
void stats_msg_format_error(void);   /* message start seen, end not found */
void stats_msg_bytes(size_t bytes);  /* message volume without a verdict */

void stats_file_ok(void);            /* file transferred successfully */
void stats_file_error(void);         /* file transfer failed */
void stats_file_local_error(void);   /* local (non-protocol) problem */
void stats_file_policy_reject(void); /* refused by policy, not an error */
void stats_file_skipped(void);       /* file skipped by sender or receiver */

void stats_cancel(void);             /* a side sent ZCAN/CAN CAN */
void stats_timeout(void);            /* a read timed out */
void stats_too_many_errors(void);    /* giving up: retry budget exhausted */

void stats_net_read(long n);         /* one read op; n < 0: failed read */
void stats_net_write(size_t bytes);  /* bytes handed to the line */
void stats_net_error(void);          /* read error or hangup on the line */

#endif

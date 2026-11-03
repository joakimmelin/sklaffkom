/*
 * linesimulator - rate-limiting and error-injecting relay for serial protocols.
 *
 * Starts a subprocess (SUB) connected via pipes. Relays stdin->SUB and
 * SUB->stdout, optionally rate-limiting throughput and/or injecting errors.
 *
 * Usage: linesimulator [OPTIONS] SUB [args...]
 *
 * Build: cc -o linesimulator linesimulator.c
 *
 * Deliberately independent of the lrzsz codebase, like zmodemsnif.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>

#define BUFSZ 16384
#define MAX_RANGES 32

/* ------------------------------------------------------------------ */
/* Byte ranges for error windowing                                    */
/* ------------------------------------------------------------------ */

struct range {
    size_t lo;
    size_t hi;
};

struct range_list {
    struct range ranges[MAX_RANGES];
    int count;
};

static int
in_ranges(const struct range_list *rl, size_t pos)
{
    int i;
    if (rl->count == 0)
        return 1;
    for (i = 0; i < rl->count; i++) {
        if (pos >= rl->ranges[i].lo && pos < rl->ranges[i].hi)
            return 1;
    }
    return 0;
}

/* Parse a size string like "128k", "1m", "4096" */
static size_t
parse_size(const char *s, const char **endp)
{
    char *ep;
    unsigned long long v = strtoull(s, &ep, 0);
    if (ep != s) {
        switch (*ep) {
        case 'k': case 'K': v <<= 10; ep++; break;
        case 'm': case 'M': v <<= 20; ep++; break;
        case 'g': case 'G': v <<= 30; ep++; break;
        default: break;
        }
    }
    if (endp)
        *endp = ep;
    return (size_t)v;
}

/* Parse a comma-separated list of ranges like "128k-256k,768k-1m" */
static int
parse_ranges(const char *s, struct range_list *rl)
{
    const char *p = s;
    rl->count = 0;
    while (*p && rl->count < MAX_RANGES) {
        const char *endp;
        size_t lo, hi;
        lo = parse_size(p, &endp);
        p = endp;
        if (*p == '-') {
            p++;
            hi = parse_size(p, &endp);
            p = endp;
        } else {
            hi = lo + 1;
        }
        if (lo >= hi) {
            fprintf(stderr, "linesimulator: bad range near '%s'\n", s);
            return -1;
        }
        rl->ranges[rl->count].lo = lo;
        rl->ranges[rl->count].hi = hi;
        rl->count++;
        if (*p == ',')
            p++;
        else if (*p != '\0') {
            fprintf(stderr, "linesimulator: bad range separator near '%s'\n", p);
            return -1;
        }
    }
    if (rl->count == 0) {
        fprintf(stderr, "linesimulator: no ranges parsed from '%s'\n", s);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* RNG: xorshift64                                                    */
/* ------------------------------------------------------------------ */

static unsigned long
xorshift64(unsigned long *state)
{
    unsigned long x = *state;
    if (x == 0)
        x = 0x9E3779B97F4A7C15UL;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/* ------------------------------------------------------------------ */
/* Buffer                                                             */
/* ------------------------------------------------------------------ */

struct buf {
    unsigned char data[BUFSZ];
    size_t len;
};

/* ------------------------------------------------------------------ */
/* Rate limiter                                                       */
/* ------------------------------------------------------------------ */

enum rl_mode {
    RL_NONE,
    RL_BITRATE,
    RL_BLOCK,
};

struct rate_limit {
    enum rl_mode mode;
    long bps;            /* bits per second (RL_BITRATE) */
    long tokens;         /* available tokens (in bits) */
    struct timespec last_refill;
    /* block mode */
    size_t block_size;   /* bytes per block (RL_BLOCK) */
    long delay_ms;       /* delay between blocks (RL_BLOCK) */
    size_t block_accum;  /* bytes accumulated in current block */
    struct timespec block_deadline; /* when current block can be sent */
    int block_has_deadline;
    /* bit-rate mode latency */
    long latency_ms;     /* per-message latency (RL_BITRATE) */
};

static void
rl_init(struct rate_limit *rl, enum rl_mode mode, long bps,
        size_t block_size, long delay_ms, long latency_ms)
{
    memset(rl, 0, sizeof(*rl));
    rl->mode = mode;
    rl->bps = bps;
    rl->tokens = 0; /* start empty: rate limit applies from the first byte */
    rl->block_size = block_size;
    rl->delay_ms = delay_ms;
    rl->latency_ms = latency_ms;
    clock_gettime(CLOCK_MONOTONIC, &rl->last_refill);
}

static long
ms_remaining(const struct timespec *deadline)
{
    struct timespec now;
    long ms;
    clock_gettime(CLOCK_MONOTONIC, &now);
    ms = (deadline->tv_sec - now.tv_sec) * 1000
            + (deadline->tv_nsec - now.tv_nsec) / 1000000;
    return ms < 0 ? 0 : ms;
}

/* Compute poll timeout (ms) for this rate limiter, or -1 if unlimited. */
static int
rl_timeout(struct rate_limit *rl)
{
    if (rl->mode == RL_BITRATE) {
        int lat_ms = 0;
        long ms, need;
        if (rl->block_has_deadline) {
            lat_ms = (int)ms_remaining(&rl->block_deadline);
            if (lat_ms > 0)
                return lat_ms;
        }
        if (rl->tokens >= 8)
            return 0;
        /* time to get at least 8 bits (1 byte) */
        need = 8 - rl->tokens;
        ms = need * 1000 / rl->bps;
        return (int)(ms < 1 ? 1 : ms);
    }
    if (rl->mode == RL_BLOCK && rl->block_has_deadline) {
        return (int)ms_remaining(&rl->block_deadline);
    }
    return -1;
}

/* Refill token bucket based on elapsed time. */
static void
rl_refill(struct rate_limit *rl)
{
    struct timespec now;
    long elapsed_us, add;
    if (rl->mode != RL_BITRATE)
        return;
    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed_us = (now.tv_sec - rl->last_refill.tv_sec) * 1000000
                    + (now.tv_nsec - rl->last_refill.tv_nsec) / 1000;
    if (elapsed_us <= 0)
        return;
    add = elapsed_us * rl->bps / 1000000;
    if (add <= 0)
        return;
    rl->tokens += add;
    if (rl->tokens > rl->bps)
        rl->tokens = rl->bps; /* cap at 1 second worth */
    rl->last_refill = now;
}

/* How many bytes can we write right now? */
static size_t
rl_allow(struct rate_limit *rl, size_t want)
{
    size_t avail;
    if (rl->mode == RL_NONE)
        return want;
    if (rl->mode == RL_BITRATE) {
        if (rl->block_has_deadline) {
            if (ms_remaining(&rl->block_deadline) > 0)
                return 0;
            rl->block_has_deadline = 0;
        }
        rl_refill(rl);
        avail = (size_t)(rl->tokens / 8);
        return avail < want ? avail : want;
    }
    /* RL_BLOCK */
    if (rl->block_has_deadline) {
        if (ms_remaining(&rl->block_deadline) > 0)
            return 0;
        rl->block_has_deadline = 0;
    }
    return want;
}

/* Record that we wrote n bytes. */
static void
rl_consumed(struct rate_limit *rl, size_t n)
{
    struct timespec now;
    if (rl->mode == RL_BITRATE) {
        rl->tokens -= (long)n * 8;
        if (rl->tokens < 0)
            rl->tokens = 0;
    } else if (rl->mode == RL_BLOCK) {
        rl->block_accum += n;
        if (rl->block_accum >= rl->block_size) {
            rl->block_accum = 0;
            clock_gettime(CLOCK_MONOTONIC, &now);
            rl->block_deadline.tv_sec = now.tv_sec
                + (now.tv_nsec + rl->delay_ms * 1000000L) / 1000000000L;
            rl->block_deadline.tv_nsec = (now.tv_nsec
                + rl->delay_ms * 1000000L) % 1000000000L;
            rl->block_has_deadline = 1;
        }
    }
}

/* Set latency deadline for a newly arrived message (RL_BITRATE mode). */
static void
rl_arrived(struct rate_limit *rl)
{
    struct timespec now;
    if (rl->mode == RL_BITRATE && rl->latency_ms > 0) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        rl->block_deadline.tv_sec = now.tv_sec
            + (now.tv_nsec + rl->latency_ms * 1000000L) / 1000000000L;
        rl->block_deadline.tv_nsec = (now.tv_nsec
            + rl->latency_ms * 1000000L) % 1000000000L;
        rl->block_has_deadline = 1;
    }
}

/* ------------------------------------------------------------------ */
/* Error injection                                                    */
/* ------------------------------------------------------------------ */

enum err_type {
    ERR_FLIP = 0,
    ERR_DROP,
    ERR_GARBLE,
    NERR,
};

struct error_state {
    unsigned long rng;
    unsigned long seed;
    int flip_bits;      /* max bits to flip per event (0=disabled) */
    int drop_bytes;     /* max bytes to drop per event (0=disabled) */
    int garble_bytes;   /* max bytes to garble per event (0=disabled) */
    int drop_packet;    /* drop exactly this many bytes per event (0=disabled) */
    int strip_bit8;     /* clear bit 7 of every byte */
    int strip_ctrl;     /* remove control chars */
    size_t error_freq;  /* nominal bytes between error events */
    int deterministic;  /* if true, errors at exact error_freq intervals */
    size_t byte_count;  /* bytes processed since last check */
    size_t next_error[NERR]; /* jittered threshold per error type */
    struct range_list ranges; /* byte ranges where errors are active */
};

static void
es_init(struct error_state *es)
{
    memset(es, 0, sizeof(*es));
    es->error_freq = 1024;
}

static void
es_seed(struct error_state *es, unsigned long seed)
{
    es->seed = seed;
    es->rng = seed;
    if (es->rng == 0)
        es->rng = 0x9E3779B97F4A7C15UL;
}

static void
es_schedule(struct error_state *es, enum err_type t)
{
    if (es->deterministic) {
        es->next_error[t] = es->byte_count + es->error_freq;
    } else {
        unsigned long r = xorshift64(&es->rng);
        es->next_error[t] = es->byte_count + 1
            + (size_t)(r % (2 * es->error_freq));
    }
}

static void
es_setup(struct error_state *es)
{
    if (es->flip_bits > 0)
        es_schedule(es, ERR_FLIP);
    if (es->drop_bytes > 0 || es->drop_packet > 0)
        es_schedule(es, ERR_DROP);
    if (es->garble_bytes > 0)
        es_schedule(es, ERR_GARBLE);
}

/* Garble: flip all bits in up to n bytes at position pos in the buffer. */
static void
do_garble(struct error_state *es, unsigned char *buf, size_t buflen,
          size_t pos)
{
    int maxn = es->garble_bytes;
    unsigned long r = xorshift64(&es->rng);
    int n = 1 + (int)(r % (unsigned long)maxn);
    for (int i = 0; i < n && pos + i < buflen; i++)
        buf[pos + i] ^= 0xFF;
}

/* Flip: flip up to maxn random bits near position pos. */
static void
do_flip(struct error_state *es, unsigned char *buf, size_t buflen,
        size_t pos)
{
    int maxn = es->flip_bits;
    unsigned long r = xorshift64(&es->rng);
    int n = 1 + (int)(r % (unsigned long)maxn);
    for (int i = 0; i < n; i++) {
        unsigned long r2 = xorshift64(&es->rng);
        size_t byte_off = r2 % 8; /* spread within 8 bytes */
        if (byte_off >= buflen - pos)
            byte_off = 0; /* clamp to current position if near end */
        if (pos + byte_off < buflen) {
            unsigned char bit = 1 << (xorshift64(&es->rng) % 8);
            buf[pos + byte_off] ^= bit;
        }
    }
}

/*
 * Transform a buffer in-place.
 * Returns the new length (may be shorter due to byte drops or control-char
 * stripping).
 */
static size_t
apply_transform(struct error_state *es, unsigned char *buf, size_t len)
{
    if (len == 0)
        return 0;

    /* Phase 1: per-byte always-on transforms */
    if (es->strip_bit8 || es->strip_ctrl) {
        size_t j = 0;
        for (size_t i = 0; i < len; i++) {
            unsigned char c = buf[i];
            if (es->strip_bit8)
                c &= 0x7F;
            if (es->strip_ctrl) {
                if (c < 0x20 || c == 0x7F)
                    continue;
            }
            buf[j++] = c;
        }
        len = j;
    }

    if (len == 0)
        return 0;

    /* Phase 2: error events */
    if (es->flip_bits <= 0 && es->drop_bytes <= 0 && es->drop_packet <= 0
        && es->garble_bytes <= 0)
        return len;

    {
    size_t pos = 0;
    while (pos < len) {
        es->byte_count++;

        if (!in_ranges(&es->ranges, es->byte_count)) {
            pos++;
            continue;
        }

        if (es->flip_bits > 0 && es->byte_count >= es->next_error[ERR_FLIP]) {
            do_flip(es, buf, len, pos);
            es_schedule(es, ERR_FLIP);
        }
        if (es->garble_bytes > 0
            && es->byte_count >= es->next_error[ERR_GARBLE]) {
            do_garble(es, buf, len, pos);
            es_schedule(es, ERR_GARBLE);
        }
        if (es->drop_packet > 0
            && es->byte_count >= es->next_error[ERR_DROP]) {
            /* silent packet drop: exactly drop_packet bytes at the event
             * position (packet boundaries are ours, not the protocol's) */
            int n = es->drop_packet;
            if ((size_t)n > len - pos)
                n = (int)(len - pos);
            memmove(buf + pos, buf + pos + n, len - pos - n);
            len -= n;
            es_schedule(es, ERR_DROP);
            /* don't advance pos: next byte is now at pos */
            continue;
        }
        if (es->drop_bytes > 0
            && es->byte_count >= es->next_error[ERR_DROP]) {
            unsigned long r = xorshift64(&es->rng);
            int maxn = es->drop_bytes;
            int n = 1 + (int)(r % (unsigned long)maxn);
            if ((size_t)n > len - pos)
                n = (int)(len - pos);
            memmove(buf + pos, buf + pos + n, len - pos - n);
            len -= n;
            es_schedule(es, ERR_DROP);
            /* don't advance pos: next byte is now at pos */
            continue;
        }
        pos++;
    }

    return len;
    }
}

/* ------------------------------------------------------------------ */
/* Direction                                                          */
/* ------------------------------------------------------------------ */

struct direction {
    struct buf to_buf;      /* buffered data waiting to be written */
    struct rate_limit rl;
    struct error_state es;
    int has_error;          /* whether error injection is active */
    int eof;                /* input side reached EOF */
};

static void
dir_init(struct direction *d)
{
    memset(d, 0, sizeof(*d));
    rl_init(&d->rl, RL_NONE, 0, 0, 0, 0);
    es_init(&d->es);
}

/* ------------------------------------------------------------------ */
/* Utility                                                            */
/* ------------------------------------------------------------------ */

static void
set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void
usage(FILE *out)
{
    fprintf(out,
"Usage: linesimulator [OPTIONS] SUB [args...]\n"
"\n"
"Rate limiting (bit rate):\n"
"  -r BPS       bits per second, both directions\n"
"  -R BPS       bits per second, stdin->SUB only\n"
"  -T BPS       bits per second, SUB->stdout only\n"
"  -L MS        per-message latency in ms (both directions)\n"
"\n"
"Rate limiting (block mode):\n"
"  -b SIZE      block size in bytes\n"
"  -p N         blocks per second (both directions)\n"
"  -d MS        delay in ms between blocks (both directions)\n"
"\n"
"Error injection (both directions unless noted):\n"
"  -7           strip bit 8 (zero the 8th bit of every byte)\n"
"  -c           strip all ASCII control chars (< 0x20 and 0x7f)\n"
"  -e N         flip up to N random bits per error event\n"
"  -x N         drop up to N random bytes per error event\n"
"  -P N         silently drop exactly N bytes (a packet) per event;\n"
"               overrides -x\n"
"  -g N         garble up to N bytes (flip all bits) per error event\n"
"  -f N         error frequency: ~1 event per N bytes (jittered 0..2N)\n"
"               (default: 1024)\n"
"  -F N         error frequency: exactly 1 event per N bytes (no jitter)\n"
"  -w RANGES    only inject errors within byte ranges, e.g.\n"
"               -w 128k-256k,768k-1m  (default: all bytes)\n"
"  -s SEED      random seed (default: random, printed to stderr)\n"
"  -1           apply errors only to stdin->SUB direction\n"
"  -2           apply errors only to SUB->stdout direction\n"
"               (default: both directions)\n"
"\n"
"  -h           print this help\n"
"\n");
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int
main(int argc, char **argv)
{
    /* Defaults */
    long bps_tx = 0, bps_rx = 0;     /* 0 = unlimited */
    long bps_both = 0;
    size_t block_size = 0;
    long block_delay_ms = 0;
    long latency_ms = 0;
    int blocks_per_sec = 0;

    int strip_bit8 = 0;
    int strip_ctrl = 0;
    int flip_bits = 0;
    int drop_bytes = 0;
    int drop_packet = 0;
    int garble_bytes = 0;
    int deterministic = 0;
    size_t error_freq = 1024;
    long seed = -1;          /* -1 = random */
    int err_dir = 3;         /* 1=tx only, 2=rx only, 3=both */
    struct range_list err_ranges;  /* empty = all bytes */

    /* Variables used later */
    int c;
    char **sub_argv;
    enum rl_mode mode = RL_NONE;
    long rl_bps = 0;
    unsigned long actual_seed;
    struct direction dir[2];
    int has_errors;
    int sub_stdin[2], sub_stdout[2];
    pid_t pid;
    int stdin_eof = 0, sub_stdout_eof = 0;
    int d;
    long tx_bps, rx_bps;

    memset(&err_ranges, 0, sizeof(err_ranges));

    while ((c = getopt(argc, argv, "r:R:T:b:p:d:L:7ce:x:P:g:f:F:w:s:12h")) != -1) {
        switch (c) {
        case 'r': bps_both = (long)parse_size(optarg, NULL); break;
        case 'R': bps_tx = (long)parse_size(optarg, NULL); break;
        case 'T': bps_rx = (long)parse_size(optarg, NULL); break;
        case 'b': block_size = parse_size(optarg, NULL); break;
        case 'p': blocks_per_sec = atoi(optarg); break;
        case 'd': block_delay_ms = atol(optarg); break;
        case 'L': latency_ms = atol(optarg); break;
        case '7': strip_bit8 = 1; break;
        case 'c': strip_ctrl = 1; break;
        case 'e': flip_bits = atoi(optarg); break;
        case 'x': drop_bytes = atoi(optarg); break;
        case 'P': drop_packet = atoi(optarg); break;
        case 'g': garble_bytes = atoi(optarg); break;
        case 'f': error_freq = (size_t)atol(optarg); break;
        case 'F': error_freq = (size_t)atol(optarg); deterministic = 1; break;
        case 'w':
            if (parse_ranges(optarg, &err_ranges) < 0)
                return 1;
            break;
        case 's': seed = atol(optarg); break;
        case '1': err_dir = 1; break;
        case '2': err_dir = 2; break;
        case 'h': usage(stdout); return 0;
        default:  usage(stderr); return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "linesimulator: no SUB command given\n");
        usage(stderr);
        return 1;
    }

    sub_argv = &argv[optind];

    /* Resolve rate-limit mode */
    if (block_size > 0) {
        mode = RL_BLOCK;
        if (blocks_per_sec > 0)
            block_delay_ms = 1000 / blocks_per_sec;
        if (block_delay_ms <= 0) {
            fprintf(stderr, "linesimulator: block mode requires -p or -d\n");
            return 1;
        }
    } else if (bps_both > 0 || bps_tx > 0 || bps_rx > 0) {
        mode = RL_BITRATE;
        if (bps_both > 0) {
            if (bps_tx == 0) bps_tx = bps_both;
            if (bps_rx == 0) bps_rx = bps_both;
        }
        rl_bps = bps_both; /* used as default if one dir is 0 */
    }

    /* Seed RNG */
    if (seed >= 0) {
        /* Reproducible Number Generator */
        actual_seed = (unsigned long)seed;
    } else {
        /* Random Number Generator */
        FILE *urand = fopen("/dev/urandom", "rb");
        if (urand) {
            if (fread(&actual_seed, sizeof(actual_seed), 1, urand) != 1)
                actual_seed = (unsigned long)time(NULL);
            fclose(urand);
        } else {
            actual_seed = (unsigned long)time(NULL);
        }
        fprintf(stderr, "linesimulator: seed=%lu\n", actual_seed);
    }

    /* Directions: 0=stdin->SUB (tx), 1=SUB->stdout (rx) */
    dir_init(&dir[0]);
    dir_init(&dir[1]);

    /* Configure rate limiters */
    if (mode == RL_BITRATE) {
        tx_bps = bps_tx > 0 ? bps_tx : rl_bps;
        rx_bps = bps_rx > 0 ? bps_rx : rl_bps;
        if (tx_bps > 0)
            rl_init(&dir[0].rl, RL_BITRATE, tx_bps, 0, 0, latency_ms);
        if (rx_bps > 0)
            rl_init(&dir[1].rl, RL_BITRATE, rx_bps, 0, 0, latency_ms);
    } else if (mode == RL_BLOCK) {
        rl_init(&dir[0].rl, RL_BLOCK, 0, block_size, block_delay_ms, 0);
        rl_init(&dir[1].rl, RL_BLOCK, 0, block_size, block_delay_ms, 0);
    }

    /* Configure error injection */
    has_errors = (strip_bit8 || strip_ctrl || flip_bits > 0
                      || drop_bytes > 0 || drop_packet > 0
                      || garble_bytes > 0);
    if (has_errors) {
        for (d = 0; d < 2; d++) {
            if (err_dir == 3 || err_dir == d + 1) {
                dir[d].has_error = 1;
                dir[d].es.strip_bit8 = strip_bit8;
                dir[d].es.strip_ctrl = strip_ctrl;
                dir[d].es.flip_bits = flip_bits;
                dir[d].es.drop_bytes = drop_bytes;
                dir[d].es.drop_packet = drop_packet;
                dir[d].es.garble_bytes = garble_bytes;
                dir[d].es.error_freq = error_freq;
                dir[d].es.deterministic = deterministic;
                dir[d].es.ranges = err_ranges;
                es_seed(&dir[d].es, actual_seed + (unsigned long)d);
                es_setup(&dir[d].es);
            }
        }
    }

    /* Create pipes and fork SUB aka lrz… most often. */

    if (pipe(sub_stdin) < 0 || pipe(sub_stdout) < 0) {
        perror("pipe");
        return 1;
    }
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        /* Child: exec SUB */
        close(sub_stdin[1]);
        close(sub_stdout[0]);
        dup2(sub_stdin[0], STDIN_FILENO);
        dup2(sub_stdout[1], STDOUT_FILENO);
        close(sub_stdin[0]);
        close(sub_stdout[1]);
        execvp(sub_argv[0], sub_argv);
        perror("execvp");
        _exit(127);
    }

    /* Parent: relay */
    close(sub_stdin[0]);
    close(sub_stdout[1]);

    /* A peer going away (SUB exited, or the process feeding us) is a
     * normal end-of-session condition on a half-duplex teardown: the
     * last buffered frame may still be forwarded after the other side
     * already left. Dying of SIGPIPE there would abort this process
     * before waitpid() below can reap SUB, so the caller would see our
     * death (exit 141) instead of SUB's real exit status. Ignore
     * SIGPIPE and let the write errors unwind the loop. Do this AFTER
     * the fork: setting it before would leak SIG_IGN through execvp
     * into SUB, and lrzsz_install_signal() refuses to install a handler
     * when the signal is ignored, silently changing SUB's semantics. */
    signal(SIGPIPE, SIG_IGN);

    set_nonblock(STDIN_FILENO);
    set_nonblock(STDOUT_FILENO);
    set_nonblock(sub_stdin[1]);
    set_nonblock(sub_stdout[0]);

    for (;;) {
        struct pollfd fds[4];
        int nfds = 0;
        int pi_stdin = -1, pi_subin = -1, pi_subout = -1, pi_stdout = -1;
        int timeout, t, ret;
        ssize_t n;
        size_t avail, can;

        if (!stdin_eof && dir[0].to_buf.len < BUFSZ) {
            fds[nfds].fd = STDIN_FILENO;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            pi_stdin = nfds++;
        }
        if (dir[0].to_buf.len > 0) {
            fds[nfds].fd = sub_stdin[1];
            fds[nfds].events = POLLOUT;
            fds[nfds].revents = 0;
            pi_subin = nfds++;
        }
        if (!sub_stdout_eof && dir[1].to_buf.len < BUFSZ) {
            fds[nfds].fd = sub_stdout[0];
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            pi_subout = nfds++;
        }
        if (dir[1].to_buf.len > 0) {
            fds[nfds].fd = STDOUT_FILENO;
            fds[nfds].events = POLLOUT;
            fds[nfds].revents = 0;
            pi_stdout = nfds++;
        }

        if (nfds == 0)
            break;

        /* Compute poll timeout from rate limiters */
        timeout = -1;
        for (d = 0; d < 2; d++) {
            if (dir[d].to_buf.len > 0) {
                t = rl_timeout(&dir[d].rl);
                if (t >= 0) {
                    if (timeout < 0 || t < timeout)
                        timeout = t;
                }
            }
        }

        ret = poll(fds, nfds, timeout);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        /* stdin -> SUB (direction 0) */
        if (pi_stdin >= 0 && (fds[pi_stdin].revents & (POLLIN | POLLHUP))) {
            unsigned char tmp[BUFSZ];
            avail = BUFSZ - dir[0].to_buf.len;
            n = read(STDIN_FILENO, tmp,
                              avail < sizeof(tmp) ? avail : sizeof(tmp));
            if (n > 0) {
                if (dir[0].has_error)
                    n = (ssize_t)apply_transform(&dir[0].es, tmp, (size_t)n);
                if (n > 0) {
                    if (dir[0].to_buf.len == 0)
                        rl_arrived(&dir[0].rl);
                    memcpy(dir[0].to_buf.data + dir[0].to_buf.len,
                           tmp, (size_t)n);
                    dir[0].to_buf.len += (size_t)n;
                }
            } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                stdin_eof = 1;
                if (dir[0].to_buf.len == 0)
                    close(sub_stdin[1]);
            }
        }

        /* Drain to SUB (direction 0) */
        if (pi_subin >= 0 && (fds[pi_subin].revents & POLLOUT)) {
            can = rl_allow(&dir[0].rl, dir[0].to_buf.len);
            if (can > 0) {
                n = write(sub_stdin[1], dir[0].to_buf.data, can);
                if (n > 0) {
                    rl_consumed(&dir[0].rl, (size_t)n);
                    if ((size_t)n < dir[0].to_buf.len) {
                        memmove(dir[0].to_buf.data,
                                dir[0].to_buf.data + n,
                                dir[0].to_buf.len - n);
                        dir[0].to_buf.len -= n;
                    } else {
                        dir[0].to_buf.len = 0;
                    }
                } else if (n < 0 && errno != EINTR && errno != EAGAIN) {
                    close(sub_stdin[1]);
                    dir[0].to_buf.len = 0;
                }
            }
            if (stdin_eof && dir[0].to_buf.len == 0)
                close(sub_stdin[1]);
        }

        /* SUB stdout -> stdout (direction 1) */
        if (pi_subout >= 0
            && (fds[pi_subout].revents & (POLLIN | POLLHUP))) {
            unsigned char tmp[BUFSZ];
            avail = BUFSZ - dir[1].to_buf.len;
            n = read(sub_stdout[0], tmp,
                              avail < sizeof(tmp) ? avail : sizeof(tmp));
            if (n > 0) {
                if (dir[1].has_error)
                    n = (ssize_t)apply_transform(&dir[1].es, tmp, (size_t)n);
                if (n > 0) {
                    if (dir[1].to_buf.len == 0)
                        rl_arrived(&dir[1].rl);
                    memcpy(dir[1].to_buf.data + dir[1].to_buf.len,
                           tmp, (size_t)n);
                    dir[1].to_buf.len += (size_t)n;
                }
            } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                sub_stdout_eof = 1;
                close(sub_stdout[0]);
            }
        }

        /* Drain to stdout (direction 1) */
        if (pi_stdout >= 0 && (fds[pi_stdout].revents & POLLOUT)) {
            can = rl_allow(&dir[1].rl, dir[1].to_buf.len);
            if (can > 0) {
                n = write(STDOUT_FILENO, dir[1].to_buf.data, can);
                if (n > 0) {
                    rl_consumed(&dir[1].rl, (size_t)n);
                    if ((size_t)n < dir[1].to_buf.len) {
                        memmove(dir[1].to_buf.data,
                                dir[1].to_buf.data + n,
                                dir[1].to_buf.len - n);
                        dir[1].to_buf.len -= n;
                    } else {
                        dir[1].to_buf.len = 0;
                    }
                } else if (n < 0 && errno != EINTR && errno != EAGAIN) {
                    dir[1].to_buf.len = 0;
                }
            }
        }

        if (sub_stdout_eof && dir[1].to_buf.len == 0)
            break;
    }

    /* Wait for SUB to exit */
    {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 1;
    }
}

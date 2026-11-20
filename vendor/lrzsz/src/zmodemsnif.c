/*
 * zmodemsnif - ZMODEM/XMODEM/YMODEM protocol sniffer/proxy/debugger/tracer.
 *
 * Calls a subprocess, relays stdin/stdout bidirectionally in nonblocking
 * mode, parses passing frames on both channels, and dumps info about them to stderr.
 *
 * Usage: zmodemsnif SUB [args...]
 *
 * Build: cc -o zmodemsnif zmodemsnif.c
 *
 * deliberately totally independent of the lrzsz code.
 *
 * in memoriam of recordxyz, a similar thing i wrote in the late 1990s
 * and rewrote in the early 200x years.
 */

/* #define _POSIX_C_SOURCE 200809L */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

/* --- ZMODEM protocol constants (copied from zmodem.h) */

#define ZPAD    '*'
#define ZDLE    030
#define ZBIN    'A'
#define ZHEX    'B'
#define ZBIN32  'C'

#define ZRQINIT 0
#define ZRINIT  1
#define ZSINIT  2
#define ZACK    3
#define ZFILE   4
#define ZSKIP   5
#define ZNAK    6
#define ZABORT  7
#define ZFIN    8
#define ZRPOS   9
#define ZDATA   10
#define ZEOF    11
#define ZFERR   12
#define ZCRC    13
#define ZCHALLENGE 14
#define ZCOMPL  15
#define ZCAN    16
#define ZFREECNT 17
#define ZCOMMAND 18
#define ZSTDERR 19

#define ZCRCE   'h'
#define ZCRCG   'i'
#define ZCRCQ   'j'
#define ZCRCW   'k'

#define ZF0     3
#define ZF1     2
#define ZF2     1
#define ZF3     0
#define ZP0     0
#define ZP1     1
#define ZP2     2
#define ZP3     3

#define CANFDX  0x01
#define CANOVIO 0x02
#define CANBRK  0x04
#define CANCRY  0x08
#define CANLZW  0x10
#define CANFC32 0x20
#define ESCCTL  0x40
#define ESC8    0x80

#define TESCCTL 0100
#define TESC8   0200

#define ZCBIN   1
#define ZCNL    2
#define ZCRESUM 3

#define ZF1_ZMNEWL   1
#define ZF1_ZMCRC    2
#define ZF1_ZMAPND   3
#define ZF1_ZMCLOB   4
#define ZF1_ZMNEW    5
#define ZF1_ZMDIFF   6
#define ZF1_ZMPROT   7
#define ZF1_ZMCHNG   8

/* XMODEM/YMODEM constants */
#define SOH     0x01
#define STX     0x02
#define EOT     0x04
#define ACK     0x06
#define NAK     0x15
#define CAN     0x18
#define CPMEOF  0x1a

/* --- Frame type names --- */

static const char *frame_names[] = {
    "ZRQINIT", "ZRINIT", "ZSINIT", "ZACK",
    "ZFILE",   "ZSKIP",  "ZNAK",   "ZABORT",
    "ZFIN",    "ZRPOS",  "ZDATA",  "ZEOF",
    "ZFERR",   "ZCRC",   "ZCHALLENGE", "ZCOMPL",
    "ZCAN",    "ZFREECNT", "ZCOMMAND", "ZSTDERR",
};

static const char *
frame_name(int type)
{
    if (type >= 0 && type <= 19)
        return frame_names[type];
    return "?UNKNOWN?";
}

/* --- CRC tables */

static unsigned short crctab[256];
static unsigned long cr3tab[256];

static void
init_crc_tables(void)
{
    /* CRC-16: polynomial 0x1021 (CCITT) */
    for (int i = 0; i < 256; i++) {
        unsigned short crc = i << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
        crctab[i] = crc;
    }
    /* CRC-32: polynomial 0xedb88320 */
    for (int i = 0; i < 256; i++) {
        unsigned long crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xedb88320UL;
            else
                crc >>= 1;
        }
        cr3tab[i] = crc;
    }
}

static unsigned short
updcrc(unsigned char cp, unsigned short crc)
{
    return (crctab[(crc >> 8) & 0xff] ^ (crc << 8) ^ cp);
}

static unsigned long
updc32(unsigned char b, unsigned long c)
{
    return (cr3tab[(c ^ b) & 0xff] ^ ((c >> 8) & 0x00FFFFFF));
}

/* --- Buffer sizes */

#define BUFSZ 65536

struct buf {
    unsigned char data[BUFSZ];
    size_t len;
};

/* --- Parser state machine */

enum pstate {
    PS_IDLE,
    PS_ZPAD,
    PS_ZDLE,
    PS_ZHEX,
    PS_ZBIN,
    PS_ZBIN32,
    PS_DATA,
    PS_DATA_CRC,
    PS_X_BLOCK,
    PS_X_BLOCK_CRC,
};

struct parser {
    enum pstate state;
    int dir;            /* 0 = stdin->SUB (tx), 1 = SUB->stdout (rx) */
    unsigned char hdr[4];
    int hdr_type;
    int frame_ind;      /* ZBIN, ZBIN32, or ZHEX */
    int byte_idx;
    int data_escaping;
    int crc_escaping;    /* ZDLE-escaping flag for CRC bytes */
    int data_term;      /* ZCRCE/ZCRCG/ZCRCQ/ZCRCW */
    int crc_len;        /* 2 or 4 depending on ZBIN vs ZBIN32 */
    size_t data_len;    /* bytes of data in current subframe */
    int x_block_num;
    int x_block_size;   /* 128 or 1024 */
    /* CRC tracking */
    unsigned short crc16;       /* running 16-bit CRC */
    unsigned long  crc32;       /* running 32-bit CRC */
    unsigned char  crc_recv[4]; /* received CRC bytes */
    int            crc_recv_len;
    /* ZFILE subframe data capture */
    int            is_zfile;
    unsigned char  data_buf[2048];
};

static void process_byte(struct parser *p, unsigned char c);

/* --- Timestamp --- */

static void
print_ts(void)
{
    struct timespec ts;
    struct tm tm;
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    fprintf(stderr, "%02d:%02d:%02d.%03ld ",
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            (long)(ts.tv_nsec / 1000000));
}

static void
print_dir(int dir)
{
    fprintf(stderr, "%s ", dir == 0 ? "\xe2\x86\x92" : "\xe2\x86\x90");
}

/* --- Flag decoding */

static void
print_zrinit_flags(unsigned char f0, const unsigned char *hdr)
{
    int first = 1;
    fprintf(stderr, "flags=");
#define PF(flag, name) \
    do { if (f0 & flag) { fprintf(stderr, "%s%s", first ? "" : "|", name); first = 0; } } while(0)
    PF(CANFDX, "CANFDX");
    PF(CANOVIO, "CANOVIO");
    PF(CANBRK, "CANBRK");
    PF(CANCRY, "CANCRY");
    PF(CANLZW, "CANLZW");
    PF(CANFC32, "CANFC32");
    PF(ESCCTL, "ESCCTL");
    PF(ESC8, "ESC8");
#undef PF
    if (first)
        fprintf(stderr, "0x%02x", f0);
    {
        /* 
            9.5  Segmented Streaming

            If the receiver cannot overlap serial and disk I/O, it uses the
            ZRINIT frame to specify a buffer length which the sender will not
            overflow.

            11.2 ZRINIT
            ZP0 and ZP1 contain the size of the receiver's buffer in bytes, or 0
            if nonstop I/O is allowed.
          
        */
        unsigned int buflen = ((unsigned int)(unsigned char)hdr[ZP0]) |
           ((unsigned int)(unsigned char)hdr[ZP1] << 8);
        fprintf(stderr, " Rxbuflen=%u %x", buflen, buflen);
        fprintf(stderr, " ZP[0-3]=[%x", (unsigned int) (unsigned char) hdr[ZP0]);
        fprintf(stderr, " ,%x", (unsigned int) (unsigned char) hdr[ZP1]);
        fprintf(stderr, " ,%x", (unsigned int) (unsigned char) hdr[ZP2]);
        fprintf(stderr, " ,%x]", (unsigned int) (unsigned char) hdr[ZP3]);
    }
}

static void
print_zsinit_flags(unsigned char f0)
{
    int first = 1;
    fprintf(stderr, "flags=");
#define PF(flag, name) \
    do { if (f0 & flag) { fprintf(stderr, "%s%s", first ? "" : "|", name); first = 0; } } while(0)
    PF(TESCCTL, "TESCCTL");
    PF(TESC8, "TESC8");
#undef PF
    if (first)
        fprintf(stderr, "0x%02x", f0);
}

static void
print_zfile_flags(unsigned char f0, unsigned char f1)
{
    const char *conv = NULL;
    const char *mgmt = NULL;

    switch (f0) {
    case ZCBIN:  conv = "BIN"; break;
    case ZCNL:   conv = "CNL"; break;
    case ZCRESUM: conv = "RESUM"; break;
    default:     conv = NULL; break;
    }
    if (conv)
        fprintf(stderr, "conv=%s ", conv);
    else
        fprintf(stderr, "conv=0x%02x ", f0);

    switch (f1 & 0x1f) {
    case ZF1_ZMNEWL: mgmt = "NEWL"; break;
    case ZF1_ZMCRC:  mgmt = "CRC"; break;
    case ZF1_ZMAPND: mgmt = "APND"; break;
    case ZF1_ZMCLOB: mgmt = "CLOB"; break;
    case ZF1_ZMNEW:  mgmt = "NEW"; break;
    case ZF1_ZMDIFF: mgmt = "DIFF"; break;
    case ZF1_ZMPROT: mgmt = "PROT"; break;
    case ZF1_ZMCHNG: mgmt = "CHNG"; break;
    default:         mgmt = NULL; break;
    }
    if (mgmt)
        fprintf(stderr, "mgmt=%s", mgmt);
    else
        fprintf(stderr, "mgmt=0x%02x", f1);
    if (f1 & 0x80)
        fprintf(stderr, "|NOLOC");
}

static unsigned long
hdr_to_pos(const unsigned char *hdr)
{
    return ((unsigned long)(unsigned char)hdr[ZP0]) |
           ((unsigned long)(unsigned char)hdr[ZP1] << 8) |
           ((unsigned long)(unsigned char)hdr[ZP2] << 16) |
           ((unsigned long)(unsigned char)hdr[ZP3] << 24);
}

static void
print_header_fields(int type, const unsigned char *hdr)
{
    switch (type) {
    case ZRINIT:
        print_zrinit_flags(hdr[ZF0], hdr);
        break;
    case ZSINIT:
        print_zsinit_flags(hdr[ZF0]);
        break;
    case ZFILE:
        print_zfile_flags(hdr[ZF0], hdr[ZF1]);
        break;
    case ZDATA:
    case ZEOF:
    case ZRPOS:
    case ZACK:
    case ZCRC:
    case ZCHALLENGE:
    case ZCOMPL:
    case ZFREECNT:
        fprintf(stderr, "offset=%lu", hdr_to_pos(hdr));
        break;
    case ZCOMMAND:
        fprintf(stderr, "flags=0x%02x", hdr[ZF0]);
        break;
    default:
        fprintf(stderr, "hdr=%02x%02x%02x%02x",
                hdr[0], hdr[1], hdr[2], hdr[3]);
        break;
    }
}

static void
parser_init(struct parser *p, int dir)
{
    memset(p, 0, sizeof(*p));
    p->state = PS_IDLE;
    p->dir = dir;
}

static void
emit_frame(struct parser *p)
{
    /* Verify header CRC */
    int crc_bad = 0;

    if (p->frame_ind == ZBIN32) {
        unsigned long crc = 0xFFFFFFFFUL;
        unsigned long recv;
        crc = updc32((unsigned char)p->hdr_type, crc);
        for (int i = 0; i < 4; i++)
            crc = updc32(p->hdr[i], crc);
        crc = (~crc) & 0xFFFFFFFFUL;
        recv = (unsigned long)p->crc_recv[0] |
                             ((unsigned long)p->crc_recv[1] << 8) |
                             ((unsigned long)p->crc_recv[2] << 16) |
                             ((unsigned long)p->crc_recv[3] << 24);
        if (crc != recv) {
            crc_bad = 1;
            print_ts();
            print_dir(p->dir);
            fprintf(stderr, "  (info) bad 32bit header CRC: received %lx, calculated %lx\n",recv,crc);
        }
    } else {
        unsigned long crc = updcrc((unsigned char)p->hdr_type, 0);
        unsigned long recv;
        for (int i = 0; i < 4; i++)
            crc = updcrc(p->hdr[i], crc);
        crc = updcrc(0, updcrc(0, crc));
        recv = ((unsigned short)p->crc_recv[0] << 8) |
                              p->crc_recv[1];
        if (crc != recv) {
            crc_bad = 1;
            print_ts();
            print_dir(p->dir);
            fprintf(stderr, "  (info) bad 16bit header CRC: received %lx, calculated %lx\n",recv,crc);
        }
    }

    print_ts();
    print_dir(p->dir);
    fprintf(stderr, "%s ", frame_name(p->hdr_type));
    print_header_fields(p->hdr_type, p->hdr);
    if (crc_bad)
        fprintf(stderr, " [BAD CRC]");
    fprintf(stderr, "\n");

    if (p->hdr_type == ZDATA || p->hdr_type == ZFILE ||
        p->hdr_type == ZSINIT || p->hdr_type == ZCOMMAND) {
        p->state = PS_DATA;
        p->data_escaping = 0;
        p->data_term = 0;
        p->data_len = 0;
        p->is_zfile = (p->hdr_type == ZFILE);
        p->crc_len = (p->frame_ind == ZBIN32) ? 4 : 2;
        /* Initialize data subframe CRC */
        if (p->frame_ind == ZBIN32)
            p->crc32 = 0xFFFFFFFFUL;
        else
            p->crc16 = 0;
    } else {
        p->state = PS_IDLE;
    }
}

static void
emit_xmodem_block(struct parser *p, int block_num, int block_size)
{
    print_ts();
    print_dir(p->dir);
    fprintf(stderr, "XMODEM: %s block=%d %d bytes\n",
            block_size == 128 ? "SOH" : "STX",
            block_num, block_size);
    p->state = PS_X_BLOCK_CRC;
    p->crc_len = 2;
    p->byte_idx = 0;
}

static void
emit_xmodem_byte(struct parser *p, unsigned char c, const char *desc)
{
    print_ts();
    print_dir(p->dir);
    fprintf(stderr, "XMODEM: %s (0x%02x)\n", desc, c);
}


static void
process_byte(struct parser *p, unsigned char c)
{
        switch (p->state) {

        default:
        case PS_IDLE:
            if (c == ZPAD) {
                p->state = PS_ZPAD;
            } else if (c == SOH) {
                p->state = PS_X_BLOCK;
                p->x_block_size = 128;
                p->byte_idx = 0;
            } else if (c == STX) {
                p->state = PS_X_BLOCK;
                p->x_block_size = 1024;
                p->byte_idx = 0;
            } else if (c == EOT) {
                emit_xmodem_byte(p, c, "EOT");
            } else if (c == ACK) {
                emit_xmodem_byte(p, c, "ACK");
            } else if (c == NAK) {
                emit_xmodem_byte(p, c, "NAK");
            } else if (c == CAN) {
                emit_xmodem_byte(p, c, "CAN");
            } else if (c == 'r') {
                /* Could be start of "rz\r" trigger - pass through */
            }
            break;

        case PS_ZPAD:
            if (c == ZPAD) {
                /* Stay in PS_ZPAD for repeated ZPADs */
            } else if (c == ZDLE) {
                p->state = PS_ZDLE;
            } else {
                p->state = PS_IDLE;
            }
            break;

        case PS_ZDLE:
            if (c == ZBIN) {
                p->frame_ind = ZBIN;
                p->state = PS_ZBIN;
                p->byte_idx = 0;
                p->crc_escaping = 0;
            } else if (c == ZBIN32) {
                p->frame_ind = ZBIN32;
                p->state = PS_ZBIN32;
                p->byte_idx = 0;
                p->crc_escaping = 0;
            } else if (c == ZHEX) {
                p->frame_ind = ZHEX;
                p->state = PS_ZHEX;
                p->byte_idx = 0;
            } else {
                p->state = PS_IDLE;
            }
            break;

        case PS_ZHEX: {
            /* Hex frame: 2 hex digits for type, 8 for data, 4 for CRC
             * = 14 hex chars, then CR + 0x8a (and optional XON 0x11) */
            static char hexbuf[16];
            if (p->byte_idx < 14) {
                if ((c >= '0' && c <= '9') ||
                    (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F')) {
                    hexbuf[p->byte_idx++] = c;
                } else {
                    p->state = PS_IDLE;
                    break;
                }
                if (p->byte_idx == 14) {
                    /* Parse: type(2) + data[0..3](8) + crc(4) = 14 hex chars */
                    unsigned int type, d0, d1, d2, d3, crc;
                    sscanf(hexbuf, "%2x%2x%2x%2x%2x%4x",
                           &type, &d0, &d1, &d2, &d3, &crc);
                    p->hdr_type = type & 0xff;
                    p->hdr[0] = d0 & 0xff;
                    p->hdr[1] = d1 & 0xff;
                    p->hdr[2] = d2 & 0xff;
                    p->hdr[3] = d3 & 0xff;
                    p->crc_recv[0] = (crc >> 8) & 0xff;
                    p->crc_recv[1] = crc & 0xff;
                    p->crc_recv_len = 2;
                    /* Wait for line ending chars */
                }
            } else {
                /* Skip CR(0x0d) + 0x8a + optional XON(0x11) after hex data.
                 * Once we see a non-line-ending char or enough chars, emit. */
                if (c == '\r' || c == 0x8a || c == 0x11 || c == '\n' || c == '\0') {
                    /* Line ending bytes - keep skipping until we see 2 */
                    p->byte_idx++;
                    if (p->byte_idx >= 16) {
                        emit_frame(p);
                    }
                } else {
                    /* Something else - emit and reprocess in IDLE */
                    emit_frame(p);
                    if (c == ZPAD) {
                        p->state = PS_ZPAD;
                    } else {
                        p->state = PS_IDLE;
                    }
                }
            }
            break;
        }

        case PS_ZBIN:
            /* Binary 16-bit CRC: type(1) + data(4) + crc(2) = 7 bytes.
             * All bytes except ZBIN itself are sent via zsendline() and
             * may be ZDLE-escaped. */
            if (p->crc_escaping) {
                p->crc_escaping = 0;
                c = c ^ 0x40;
            } else if (c == ZDLE) {
                p->crc_escaping = 1;
                break;
            }
            if (p->byte_idx == 0) {
                p->hdr_type = c;
                p->byte_idx = 1;
            } else if (p->byte_idx <= 4) {
                p->hdr[p->byte_idx - 1] = c;
                p->byte_idx++;
            } else if (p->byte_idx <= 6) {
                p->crc_recv[p->byte_idx - 5] = c;
                p->byte_idx++;
                if (p->byte_idx == 7) {
                    p->crc_recv_len = 2;
                    emit_frame(p);
                }
            }
            break;

        case PS_ZBIN32:
            /* Binary 32-bit CRC: type(1) + data(4) + crc(4) = 9 bytes.
             * All bytes except ZBIN32 itself are sent via zsendline() and
             * may be ZDLE-escaped. */
            if (p->crc_escaping) {
                p->crc_escaping = 0;
                c = c ^ 0x40;
            } else if (c == ZDLE) {
                p->crc_escaping = 1;
                break;
            }
            if (p->byte_idx == 0) {
                p->hdr_type = c;
                p->byte_idx = 1;
            } else if (p->byte_idx <= 4) {
                p->hdr[p->byte_idx - 1] = c;
                p->byte_idx++;
            } else if (p->byte_idx <= 8) {
                p->crc_recv[p->byte_idx - 5] = c;
                p->byte_idx++;
                if (p->byte_idx == 9) {
                    p->crc_recv_len = 4;
                    emit_frame(p);
                }
            }
            break;

        case PS_DATA:
            if (p->data_escaping) {
                p->data_escaping = 0;
                if (c == ZCRCE || c == ZCRCG ||
                    c == ZCRCQ || c == ZCRCW) {
                    /* Terminator: update CRC with terminator byte */
                    if (p->frame_ind == ZBIN32)
                        p->crc32 = updc32(c, p->crc32);
                    else
                        p->crc16 = updcrc(c, p->crc16);
                    p->data_term = c;
                    p->state = PS_DATA_CRC;
                    p->byte_idx = 0;
                    p->crc_escaping = 0;
                } else {
                    /* Escaped data byte: decode and update CRC */
                    unsigned char raw = c ^ 0x40;
                    if (p->frame_ind == ZBIN32)
                        p->crc32 = updc32(raw, p->crc32);
                    else
                        p->crc16 = updcrc(raw, p->crc16);
                    if (p->is_zfile && p->data_len < sizeof(p->data_buf))
                        p->data_buf[p->data_len] = raw;
                    p->data_len++;
                }
            } else if (c == ZDLE) {
                p->data_escaping = 1;
            } else {
                /* Normal data byte */
                if (p->frame_ind == ZBIN32)
                    p->crc32 = updc32(c, p->crc32);
                else
                    p->crc16 = updcrc(c, p->crc16);
                if (p->is_zfile && p->data_len < sizeof(p->data_buf))
                    p->data_buf[p->data_len] = c;
                p->data_len++;
            }
            break;

        case PS_DATA_CRC: {
            /* Collect CRC bytes after data subframe.
             * CRC bytes may be ZDLE-escaped by the sender. */
            if (p->crc_escaping) {
                p->crc_escaping = 0;
                c = c ^ 0x40;
            } else if (c == ZDLE) {
                p->crc_escaping = 1;
                break;
            }
            p->crc_recv[p->byte_idx] = c;
            p->byte_idx++;
            if (p->byte_idx >= p->crc_len) {
                int crc_bad = 0;
                if (p->frame_ind == ZBIN32) {
                    unsigned long crc = (~p->crc32) & 0xFFFFFFFFUL;
                    unsigned long recv = (unsigned long)p->crc_recv[0] |
                                         ((unsigned long)p->crc_recv[1] << 8) |
                                         ((unsigned long)p->crc_recv[2] << 16) |
                                         ((unsigned long)p->crc_recv[3] << 24);
                    if (crc != recv) {
                        print_ts();
                        print_dir(p->dir);
                        fprintf(stderr, "  (info) bad 32bit data crc: received %lx, calculated %lx\n",recv,crc);
                        crc_bad = 1;
                    }
                } else {
                    unsigned long crc = updcrc(0, updcrc(0, p->crc16));
                    unsigned long recv = ((unsigned short)p->crc_recv[0] << 8) |
                                          p->crc_recv[1];
                    if (crc != recv) {
                        print_ts();
                        print_dir(p->dir);
                        fprintf(stderr, "  (info) bad 16bit data CRC: received %lx, calculated %lx\n",recv,crc);
                        crc_bad = 1;
                    }
                }

                print_ts();
                print_dir(p->dir);
                fprintf(stderr, "  data subframe %zu bytes (%s)%s\n",
                        p->data_len,
                        p->data_term == ZCRCE ? "ZCRCE" :
                        p->data_term == ZCRCG ? "ZCRCG" :
                        p->data_term == ZCRCQ ? "ZCRCQ" :
                        p->data_term == ZCRCW ? "ZCRCW" : "?",
                        crc_bad ? " [BAD CRC]" : "");
                if (p->is_zfile && p->data_len > 0 && !crc_bad) {
                    /* Parse ZFILE metadata: filename\0 size mtime mode ... */
                    size_t namelen = strnlen((char *)p->data_buf, p->data_len);
                    const char *fname = (char *)p->data_buf;
                    long size_val = 0;
                    unsigned long mtime_val = 0, mode_val = 0;
                    if (namelen < p->data_len) {
                        const char *rest = (char *)p->data_buf + namelen + 1;
                        sscanf(rest, "%ld %lo %lo",
                               &size_val, &mtime_val, &mode_val);
                    }
                    print_ts();
                    print_dir(p->dir);
                    fprintf(stderr, "    file=\"%s\" size=%ld mtime=%lu mode=0%lo\n",
                            fname, size_val, mtime_val, mode_val);
                }
                if (p->data_term == ZCRCE || p->data_term == ZCRCW) {
                    p->state = PS_IDLE;
                } else {
                    p->state = PS_DATA;
                    p->data_escaping = 0;
                    p->data_term = 0;
                    p->data_len = 0;
                    if (p->frame_ind == ZBIN32)
                        p->crc32 = 0xFFFFFFFFUL;
                    else
                        p->crc16 = 0;
                }
            }
            break;
        }

        case PS_X_BLOCK:
            /* X/YMODEM: block_num(1) + ~block_num(1) + data + crc */
            if (p->byte_idx == 0) {
                p->x_block_num = c;
                p->byte_idx = 1;
            } else if (p->byte_idx == 1) {
                /* ~block_num */
                p->byte_idx = 2;
                p->crc16 = 0;
                p->crc32 = 0;  /* reused as checksum accumulator */
            } else if (p->byte_idx < p->x_block_size + 2) {
                /* Data bytes - accumulate CRC and checksum */
                p->crc16 = updcrc(c, p->crc16);
                p->crc32 = (p->crc32 + c) & 0xff;
                p->byte_idx++;
            } else {
                /* Done with data - c is the first CRC/checksum byte */
                emit_xmodem_block(p, p->x_block_num, p->x_block_size);
                p->crc_recv[0] = c;
                p->byte_idx = 1;
            }
            break;

        case PS_X_BLOCK_CRC:
            /* Collect 2 bytes after XMODEM block data.
             * Try both CRC-16 (2 bytes) and checksum (1 byte).
             * In checksum mode, the 2nd byte is the start of the
             * next frame and must be pushed back. */
            p->crc_recv[p->byte_idx] = c;
            p->byte_idx++;
            if (p->byte_idx >= 2) {
                unsigned short crc16_val = updcrc(0, updcrc(0, p->crc16));
                unsigned short recv16 = ((unsigned short)p->crc_recv[0] << 8) |
                                        p->crc_recv[1];
                int crc16_ok = (crc16_val == recv16);
                int checksum_ok = (p->crc32 == p->crc_recv[0]);

                if (crc16_ok) {
                    print_ts(); print_dir(p->dir);
                    fprintf(stderr, "  (CRC-16 ok)\n");
                    p->state = PS_IDLE;
                } else if (checksum_ok) {
                    print_ts(); print_dir(p->dir);
                    fprintf(stderr, "  (checksum ok)\n");
                    /* byte 1 is the start of the next frame - process it now */
                    p->state = PS_IDLE;
                    process_byte(p, p->crc_recv[1]);
                } else {
                    print_ts(); print_dir(p->dir);
                    fprintf(stderr, "  [BAD CRC]\n");
                    p->state = PS_IDLE;
                }
            }
            break;
        }
}

static void
parser_feed(struct parser *p, const unsigned char *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        process_byte(p, buf[i]);
    }
}

/* --- Relay */

static int
set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void
flush_buf(struct buf *b)
{
    b->len = 0;
}

static ssize_t
buf_write(int fd, struct buf *b)
{   
    ssize_t n;
    if (b->len == 0)
        return 0;
    n = write(fd, b->data, b->len);
    if (n > 0) {
        if ((size_t)n < b->len)
            memmove(b->data, b->data + n, b->len - n);
        b->len -= n;
    }
    return n;
}

int
main(int argc, char *argv[])
{
    int status;
    int stdin_eof = 0;
    int sub_stdout_eof = 0;

    struct buf to_sub;    /* data going to SUB's stdin */
    struct buf to_stdout; /* data coming from SUB's stdout */
    struct parser tx_parser, rx_parser;
    int sub_stdin[2], sub_stdout[2];
    pid_t pid;

    init_crc_tables();

    /* put here so i'll not have to do some preprocessor magic to compile it with compilers
     * insisting on noreturn
     */
    if (argc < 2) {
        fprintf(stderr, "Usage: zmodemsnif SUB [args...]\n");
        fprintf(stderr, "       SUB typically is an (l)rz/sz/rb/sb/rx/sx command.\n");
        fprintf(stderr, "       zmodemsnif then will dump the protocol exchange to stderr\n");
        exit(1);
    }

    /* Create pipes for SUB's stdin and stdout */
    if (pipe(sub_stdin) < 0) {
        perror("pipe");
        exit(1);
    }
    if (pipe(sub_stdout) < 0) {
        perror("pipe");
        exit(1);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* Child: exec SUB */
        close(sub_stdin[1]);
        close(sub_stdout[0]);
        dup2(sub_stdin[0], STDIN_FILENO);
        dup2(sub_stdout[1], STDOUT_FILENO);
        close(sub_stdin[0]);
        close(sub_stdout[1]);
        execvp(argv[1], &argv[1]);
        perror("execvp");
        _exit(127);
    }

    /* Parent: relay */
    close(sub_stdin[0]);
    close(sub_stdout[1]);

    set_nonblock(STDIN_FILENO);
    set_nonblock(STDOUT_FILENO);
    set_nonblock(sub_stdin[1]);
    set_nonblock(sub_stdout[0]);

    parser_init(&tx_parser, 0);
    parser_init(&rx_parser, 1);

    flush_buf(&to_sub);
    flush_buf(&to_stdout);


    while (1) {
        int ret;
        struct pollfd fds[4];
        int nfds = 0;
        int pi_stdin = -1, pi_subin = -1, pi_subout = -1, pi_stdout = -1;

        if (!stdin_eof && to_sub.len < BUFSZ) {
            fds[nfds].fd = STDIN_FILENO;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            pi_stdin = nfds;
            nfds++;
        }
        if (to_sub.len > 0) {
            fds[nfds].fd = sub_stdin[1];
            fds[nfds].events = POLLOUT;
            fds[nfds].revents = 0;
            pi_subin = nfds;
            nfds++;
        }
        if (!sub_stdout_eof && to_stdout.len < BUFSZ) {
            fds[nfds].fd = sub_stdout[0];
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            pi_subout = nfds;
            nfds++;
        }
        if (to_stdout.len > 0) {
            fds[nfds].fd = STDOUT_FILENO;
            fds[nfds].events = POLLOUT;
            fds[nfds].revents = 0;
            pi_stdout = nfds;
            nfds++;
        }

        if (nfds == 0)
            break;

        ret = poll(fds, nfds, -1);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        /* stdin → SUB */
        if (pi_stdin >= 0 && (fds[pi_stdin].revents & (POLLIN | POLLHUP))) {
            size_t avail = BUFSZ - to_sub.len;
            unsigned char tmp[BUFSZ];
            ssize_t n = read(STDIN_FILENO, tmp, avail < sizeof(tmp) ? avail : sizeof(tmp));
            if (n > 0) {
                parser_feed(&tx_parser, tmp, (size_t)n);
                memcpy(to_sub.data + to_sub.len, tmp, (size_t)n);
                to_sub.len += (size_t)n;
            } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                stdin_eof = 1;
                if (to_sub.len == 0)
                    close(sub_stdin[1]);
            }
        }

        /* Drain to SUB */
        if (pi_subin >= 0 && (fds[pi_subin].revents & POLLOUT)) {
            ssize_t n = buf_write(sub_stdin[1], &to_sub);
            if (n < 0 && errno != EINTR && errno != EAGAIN) {
                /* SUB stdin broken */
                close(sub_stdin[1]);
                to_sub.len = 0;
            }
            if (stdin_eof && to_sub.len == 0)
                close(sub_stdin[1]);
        }

        /* SUB stdout → stdout */
        if (pi_subout >= 0 && (fds[pi_subout].revents & (POLLIN | POLLHUP))) {
            size_t avail = BUFSZ - to_stdout.len;
            unsigned char tmp[BUFSZ];
            ssize_t n = read(sub_stdout[0], tmp, avail < sizeof(tmp) ? avail : sizeof(tmp));
            if (n > 0) {
                parser_feed(&rx_parser, tmp, (size_t)n);
                memcpy(to_stdout.data + to_stdout.len, tmp, (size_t)n);
                to_stdout.len += (size_t)n;
            } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                sub_stdout_eof = 1;
                close(sub_stdout[0]);
            }
        }

        /* Drain to stdout */
        if (pi_stdout >= 0 && (fds[pi_stdout].revents & POLLOUT)) {
            ssize_t n = buf_write(STDOUT_FILENO, &to_stdout);
            if (n < 0 && errno != EINTR && errno != EAGAIN) {
                to_stdout.len = 0;
            }
        }

        if (sub_stdout_eof && to_stdout.len == 0)
            break;
    }

    /* Wait for SUB to exit */
    waitpid(pid, &status, 0);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 1;
}

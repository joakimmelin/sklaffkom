/*
 * zmodemfuzz - ZMODEM protocol fuzzer / peer driver.
 *
 * Runs a target binary (typically lrz or lsz) and plays the opposing
 * ZMODEM peer with malformed or hostile traffic, checking invariants:
 * bounded termination, no death-by-signal, and (receive mode) that
 * nothing is written outside the receiving directory. Failures leave
 * a fuzz-fail-*.log with the seed and the complete byte transcript,
 * so every run is reproducible:  zmodemfuzz -s SEED -m receive ...
 *
 * Modes:
 *   -m receive   target is a receiver (lrz); the fuzzer plays a sender
 *                whose byte stream is mutated (phase 1).
 *   -m send      target is a sender (lsz); the fuzzer plays a receiver
 *                whose frames are drawn from a hostile sequence engine
 *                (phase 2).
 *
 * Build: cc -o zmodemfuzz zmodemfuzz.c
 * Deliberately independent of the lrzsz code (like zmodemsnif).
 *
 * Deterministic: xorshift64* PRNG; -s SEED replays a run exactly.
 * Each iteration is alarm-bounded (-t, default 20s); a target killed
 * by that alarm counts as a hang failure.
 *
 * Written by glm-5.3-flash, because this needed to be independent of
 * my own logic.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <dirent.h>

/* --- protocol constants (copied, as in zmodemsnif) --- */

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

#define ZP0     0
#define ZP1     1
#define ZP2     2
#define ZP3     3

#define CANFDX  0x01
#define CANOVIO 0x02
#define CANBRK  0x04
#define CANFC32 0x20
#define ESCCTL  0x40
#define ESC8    0x80

#define ZCBIN   1

#define XON     ('q' & 0x1f)
#define XOFF    ('s' & 0x1f)
#define CAN     0x18

/* the receiver-side subpacket cap (mirrors LRZSZ_MAX_SUBPACKET_LEN;
 * other zmodem implementations accept up to 64k) */
#define SUBPKT_MAX 8192

/* --- CRC --- */

static unsigned short crctab[256];
static unsigned long cr3tab[256];

static void
init_crc_tables(void)
{
	int i;
	for (i = 0; i < 256; i++) {
		unsigned short crc = (unsigned short)(i << 8);
		int j;
		for (j = 0; j < 8; j++) {
			if (crc & 0x8000)
				crc = (unsigned short)((crc << 1) ^ 0x1021);
			else
				crc = (unsigned short)(crc << 1);
		}
		crctab[i] = crc;
	}
	for (i = 0; i < 256; i++) {
		unsigned long crc = (unsigned long)i;
		int j;
		for (j = 0; j < 8; j++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xedb88320UL;
			else
				crc >>= 1;
		}
		cr3tab[i] = crc;
	}
}

static unsigned short
updcrc(unsigned char b, unsigned short crc)
{
	return (unsigned short)(crctab[(crc >> 8) & 0xff] ^ (crc << 8) ^ b);
}

static unsigned long
updc32(unsigned char b, unsigned long c)
{
	return (cr3tab[(c ^ b) & 0xff] ^ ((c >> 8) & 0x00FFFFFFUL));
}

/* --- deterministic PRNG: xorshift64* --- */

static unsigned long long rng_state = 88172645463325252ULL;

static unsigned long long
rng_next(void)
{
	unsigned long long x = rng_state;
	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	rng_state = x;
	return x * 0x2545F4914F6CDD1DULL;
}

static unsigned int
rng_below(unsigned int n)
{
	return n ? (unsigned int)(rng_next() % n) : 0;
}

/* --- frame output buffer --- */

static unsigned char outbuf[262144];
static size_t outpos;

static void
ob(unsigned char c)
{
	if (outpos < sizeof(outbuf) - 8)
		outbuf[outpos++] = c;
}

static void
out_reset(void)
{
	outpos = 0;
}

/* ZDLE escaping as lrzsz's zsendline() does it (without TESCCTL/TESC8):
 * every byte with (c & 0x60) == 0 (controls and their 0x80-0x9f,
 * 0xc0-0xdf mirrors), ZDLE itself, XON/XOFF and their high-bit forms;
 * CR only when control-char escaping was negotiated. */
static int
needs_escape(unsigned char c, int ctlesc)
{
	if (!(c & 0x60))
		return 1;
	if (c == ZDLE || c == XON || c == XOFF)
		return 1;
	if (c == (XON | 0x80) || c == (XOFF | 0x80))
		return 1;
	if (c == '\r')
		return ctlesc ? 1 : 0;
	return 0;
}

static void
ob_esc(unsigned char c, int ctlesc)
{
	if (needs_escape(c, ctlesc)) {
		ob(ZDLE);
		ob((unsigned char)(c ^ 0x40));
	} else {
		ob(c);
	}
}

/* binary header, 16-bit CRC */
static void
build_binhdr16(int type, const unsigned char hdr[4], int ctlesc)
{
	unsigned short crc = 0;
	int n;

	ob(ZPAD);
	ob(ZPAD);
	ob(ZDLE);
	ob(ZBIN);
	ob_esc((unsigned char)type, ctlesc);
	crc = updcrc((unsigned char)type, 0);
	for (n = 0; n < 4; n++) {
		ob_esc(hdr[n], ctlesc);
		crc = updcrc(hdr[n], crc);
	}
	crc = updcrc(0, updcrc(0, crc));
	ob_esc((unsigned char)(crc >> 8), ctlesc);
	ob_esc((unsigned char)(crc & 0xff), ctlesc);
	if (getenv("ZFUZZ_DEBUG"))
		fprintf(stderr, "binhdr16 type=%d crc=%04x\n", type, crc);
}

/* binary header, 32-bit CRC */
static void
build_binhdr32(int type, const unsigned char hdr[4])
{
	unsigned long crc = 0xFFFFFFFFUL;
	int n;

	ob(ZPAD);
	ob(ZPAD);
	ob(ZDLE);
	ob(ZBIN32);
	ob_esc((unsigned char)type, 0);
	crc = updc32((unsigned char)type, crc);
	for (n = 0; n < 4; n++) {
		crc = updc32(hdr[n], crc);
		ob_esc(hdr[n], 0);
	}
	crc = ~crc;
	for (n = 0; n < 4; n++) {
		ob_esc((unsigned char)(crc & 0xff), 0);
		crc >>= 8;
	}
}

/* hex header */
static void
build_hexhdr(int type, const unsigned char hdr[4])
{
	unsigned short crc = 0;
	char hex[20];
	int n;

	crc = updcrc((unsigned char)type, 0);
	for (n = 0; n < 4; n++)
		crc = updcrc(hdr[n], crc);
	crc = updcrc(0, updcrc(0, crc));

	ob(ZPAD);
	ob(ZPAD);
	ob(ZDLE);
	ob(ZHEX);
	snprintf(hex, sizeof hex, "%02x%02x%02x%02x%02x%04x",
		 type & 0xff, hdr[0], hdr[1], hdr[2], hdr[3],
		 crc & 0xffff);
	if (getenv("ZFUZZ_DEBUG"))
		fprintf(stderr, "build_hexhdr type=%d hex=%s\n", type, hex);
	for (n = 0; hex[n]; n++)
		ob((unsigned char)hex[n]);
	ob('\r');
	ob((unsigned char)('\n' | 0x80));
	if (type != ZFIN && type != ZACK)
		ob(XON);
}

/* data subpacket with 16-bit CRC, appended to a byte buffer */
static size_t
append_data16(unsigned char *dst, size_t dstcap, size_t pos,
	      const unsigned char *data, size_t len, int term, int ctlesc)
{
	unsigned short crc = 0;
	size_t i;

	if (pos + len * 2 + 10 >= dstcap)
		return pos;
	for (i = 0; i < len; i++) {
		unsigned char c = data[i];
		crc = updcrc(c, crc);
		if (needs_escape(c, ctlesc)) {
			dst[pos++] = ZDLE;
			dst[pos++] = (unsigned char)(c ^ 0x40);
		} else
			dst[pos++] = c;
	}
	dst[pos++] = ZDLE;
	dst[pos++] = (unsigned char)term;
	crc = updcrc((unsigned char)term, crc);
	crc = updcrc(0, updcrc(0, crc));
	/* the CRC bytes are escaped like data bytes: ZDLE plus the
	 * byte XORed with 0x40 */
	{
		unsigned char cb[2];
		int k;
		cb[0] = (unsigned char)((crc >> 8) & 0xff);
		cb[1] = (unsigned char)(crc & 0xff);
		for (k = 0; k < 2; k++) {
			if (needs_escape(cb[k], ctlesc)) {
				dst[pos++] = ZDLE;
				dst[pos++] = (unsigned char)(cb[k] ^ 0x40);
			} else
				dst[pos++] = cb[k];
		}
	}
	return pos;
}

/* data subpacket with 32-bit CRC, appended to a byte buffer */
static size_t
append_data32(unsigned char *dst, size_t dstcap, size_t pos,
	      const unsigned char *data, size_t len, int term, int ctlesc)
{
	unsigned long crc = 0xFFFFFFFFUL;
	size_t i;

	if (pos + len * 2 + 12 >= dstcap)
		return pos;
	for (i = 0; i < len; i++) {
		unsigned char c = data[i];
		crc = updc32(c, crc);
		if (needs_escape(c, ctlesc)) {
			dst[pos++] = ZDLE;
			dst[pos++] = (unsigned char)(c ^ 0x40);
		} else
			dst[pos++] = c;
	}
	dst[pos++] = ZDLE;
	dst[pos++] = (unsigned char)term;
	crc = updc32((unsigned char)term, crc);
	crc = ~crc;
	{
		int k;
		for (k = 0; k < 4; k++) {
			unsigned char cb = (unsigned char)(crc & 0xff);
			crc >>= 8;
			if (needs_escape(cb, ctlesc)) {
				dst[pos++] = ZDLE;
				dst[pos++] = (unsigned char)(cb ^ 0x40);
			} else
				dst[pos++] = cb;
		}
	}
	return pos;
}

/* --- configuration / iteration context --- */

static void usage(void) __attribute__((noreturn));

static const char *progname = "zmodemfuzz";
static char *target_argv[64];
static int target_argc;
static long seed = 0;
static long iterations = 200;
static long start_iteration = 0;
static long current_iteration; /* for helpers outside the main loop */
static int alarm_seconds = 25;
static int do_selftest = 0;
static int keep_dirs = 0;
static int show_progress = 0;
static int mode_receive = -1; /* 1 = receive fuzz, 0 = send fuzz */

static void build_payload(void);

static char fuzzdir[512];       /* working root */
static char recvdir[768];       /* receiving directory */
static char canary[768];        /* parent-dir canary */
static char canary_pristine[768];
static char canary2[768];       /* subdir-symlink canary target */
static char canary2_pristine[768];

static unsigned char payload[131072];
static size_t payload_len;
static char payload_name[128];
static int longpkt_done;	/* M_LONGPACKET: exchange ran to completion */

static void
build_payload(void)
{
	size_t i;
	payload_len = 1 + rng_below(20000);
	for (i = 0; i < payload_len; i++)
		payload[i] = (unsigned char)rng_next();
}

__attribute__((noreturn))
__attribute__((format(printf, 1, 2))) static void
die(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

/* --- transcript --- */

static FILE *transcript;

static void
tr_buf(int dir, const unsigned char *b, size_t n)
{
	size_t i;
	if (!transcript)
		return;
	for (i = 0; i < n; i++) {
		if (dir) {
			fprintf(transcript, "<%02x", b[i] & 0xff);
		} else {
			fprintf(transcript, ">%02x", b[i] & 0xff);
		}
	}
}

static time_t fuzz_start;

__attribute__((format(printf, 1, 2))) static void
tr_mark(const char *fmt, ...)
{
	va_list ap;
	if (!transcript)
		return;
	va_start(ap, fmt);
	fprintf(transcript, "[%lds] ", (long)(time(NULL) - fuzz_start));
	vfprintf(transcript, fmt, ap);
	va_end(ap);
	fputc('\n', transcript);
}

/* --- target management --- */

static pid_t target_pid = -1;
static int to_target[2];   /* parent -> target stdin */
static int from_target[2]; /* target stdout -> parent */
static int target_stderr[2];
static volatile sig_atomic_t alarm_fired;
#ifdef RLIMIT_AS
/* 256MB guards the parent against mutation-driven memory bombs in
 * plain targets, but it is fatal for sanitizer-built targets: MSan
 * must mmap ~16TB of virtual shadow memory at startup and dies with
 * "failed to allocate shadow memory (errno 12)" under any RLIMIT_AS.
 * The harness itself is msan-built when the tree is (--enable-msan),
 * so its targets are msan-built too - skip the limit there and rely
 * on the per-iteration -t deadline to bound runaway targets. */
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define FUZZ_UNDER_MSAN 1
#endif
#elif defined(__SANITIZE_MEMORY__)
#define FUZZ_UNDER_MSAN 1
#endif
# ifndef FUZZ_UNDER_MSAN
static unsigned long mem_limit = 256UL * 1024 * 1024;
# endif
#endif

/* run a shell command, discarding its status (mkdir/ln/rm plumbing).
 * system() is deliberately NOT used: its /bin/sh child resets signal
 * dispositions and glibc's save/restore dance around SIGALRM made a
 * pending harness alarm lethal (observed against targets that ignore
 * reads, e.g. zmtx-zmrx-1.02: harness killed by its own SIGALRM).
 * Here the child gets a known-clean signal state; the parent's
 * on_alarm stays installed and the target-kill alarm semantics in
 * wait_target() are untouched. */
static void
run_cmd(const char *cmd)
{
	pid_t pid = fork();

	if (pid < 0)
		die("fork: %s", strerror(errno));
	if (pid == 0) {
		/* plumbing child: default dispositions, no alarm - the
		 * harness's wait_target() deadline must not be shortened
		 * by an alarm armed in the forked shell */
		signal(SIGALRM, SIG_DFL);
		alarm(0);
		execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
		_exit(127);
	}
	{
		int status;
		pid_t r;
		do {
			r = waitpid(pid, &status, 0);
		} while (r < 0 && errno == EINTR);
		/* status is deliberately ignored: the plumbing (mkdir/ln/
		 * rm) is best-effort; failures surface as invariant
		 * violations instead */
		(void)r;
		(void)status;
	}
}

static void
on_alarm(int sig)
{
	(void)sig;
	alarm_fired = 1;
	/* nothing must rely on the main loop checking the flag: kill the
	 * wedged target immediately so wait_target() reaps it; the flag
	 * stays set for the deadline bookkeeping */
	if (target_pid > 0)
		kill(target_pid, SIGKILL);
}

static void
spawn_target(void)
{
	/* a leftover alarm from a previous iteration's early exit (target
	 * died / receiver aborted) must not fire mid-iteration where no
	 * alarm_fired check exists */
	alarm(0);
	target_pid = fork();
	if (target_pid < 0)
		die("fork: %s", strerror(errno));
	if (target_pid == 0) {
		char *argv[68];
		int i;
		sigset_t alrm;
		/* the parent keeps SIGALRM blocked between waits so a
		 * stray alarm cannot kill the harness; the child must
		 * receive it default-style (its own bounded alarm) */
		sigemptyset(&alrm);
		sigaddset(&alrm, SIGALRM);
		sigprocmask(SIG_UNBLOCK, &alrm, NULL);
		signal(SIGALRM, SIG_DFL);
		alarm((unsigned)alarm_seconds);
		/* receive mode: the child must chdir into the receiving
		 * directory so received files land there (and the escape
		 * canaries around it are meaningful).
		 * send mode: the target only READS files (its file
		 * arguments are relative to the caller's cwd) and writes
		 * nothing to disk, so keep the caller's cwd - otherwise
		 * relative file arguments like ./test/4k.bin break. */
		if (mode_receive && chdir(recvdir) != 0)
			_exit(126);
#ifdef RLIMIT_AS
# ifndef FUZZ_UNDER_MSAN
		if (mem_limit
		    && setrlimit(RLIMIT_AS,
				 &(struct rlimit){ mem_limit, mem_limit }) != 0)
			_exit(126);
# endif
#endif
		if (dup2(to_target[0], 0) < 0
		    || dup2(from_target[1], 1) < 0
		    || dup2(target_stderr[1], 2) < 0)
			_exit(126);
		close(to_target[0]);
		close(to_target[1]);
		close(from_target[0]);
		close(from_target[1]);
		close(target_stderr[0]);
		close(target_stderr[1]);
		for (i = 0; i < target_argc && i < 66; i++)
			argv[i] = target_argv[i];
		argv[i] = NULL;
                if (!argv[0]) {
		    _exit(127); /* ECANTHAPPEN */
                }
		execvp(argv[0], argv);
		_exit(127);
	}
	/* parent ends; the pipes must be non-blocking for the poll-driven
	 * pump/flush paths */
	close(to_target[0]);
	close(from_target[1]);
	close(target_stderr[1]);
	{
		int flags = fcntl(to_target[1], F_GETFL, 0);
		fcntl(to_target[1], F_SETFL, flags | O_NONBLOCK);
		flags = fcntl(from_target[0], F_GETFL, 0);
		fcntl(from_target[0], F_SETFL, flags | O_NONBLOCK);
	}
}

static void
close_pipes(void)
{
	if (to_target[1] >= 0)
		close(to_target[1]);
	to_target[1] = -1;
	close(to_target[0]);
	close(from_target[0]);
	close(from_target[1]);
	close(target_stderr[0]);
	close(target_stderr[1]);
}

static struct {
	unsigned char data[65536];
	size_t len;
} to_target_buf;

/* set when the target's pipe broke: the target is gone, stop feeding it */
static int target_dead;

static int poll_one_out(void);

static unsigned char stderr_capture[262144];
static size_t stderr_len;

/* target stdout bytes not yet consumed by the frame parser */
static struct {
	unsigned char data[131072];
	size_t len;
	size_t pos;
} rxbuf;

static void
rxbuf_reset(void)
{
	rxbuf.len = 0;
	rxbuf.pos = 0;
}


/* drain target stdout/stderr; returns -1 when the target's stdout closed */
static int
drain_target(void)
{
	struct pollfd fds[2];
	int nfds = 0;
	int pi_tout = -1, pi_terr = -1;

	if (from_target[0] >= 0) {
		fds[nfds].fd = from_target[0];
		fds[nfds].events = POLLIN;
		fds[nfds].revents = 0;
		pi_tout = nfds++;
	}
	if (target_stderr[0] >= 0) {
		fds[nfds].fd = target_stderr[0];
		fds[nfds].events = POLLIN;
		fds[nfds].revents = 0;
		pi_terr = nfds++;
	}
	if (!nfds)
		return 0;
	if (poll(fds, nfds, 50) < 0) {
		if (errno == EINTR)
			return 0;
		return -1;
	}
	if (pi_tout >= 0 && (fds[pi_tout].revents & (POLLIN | POLLHUP))) {
		unsigned char tmp[8192];
		ssize_t n = read(from_target[0], tmp, sizeof tmp);
		if (n > 0) {
			tr_buf(1, tmp, (size_t)n);
			if (rxbuf.len + (size_t)n <= sizeof rxbuf.data) {
				memcpy(rxbuf.data + rxbuf.len, tmp, (size_t)n);
				rxbuf.len += (size_t)n;
			}
		} else if (n == 0) {
			close(from_target[0]);
			from_target[0] = -1;
		}
	}
	if (pi_terr >= 0 && (fds[pi_terr].revents & (POLLIN | POLLHUP))) {
		ssize_t n = read(target_stderr[0],
				 stderr_capture + stderr_len,
				 sizeof stderr_capture - stderr_len);
		if (n > 0) {
			tr_buf(1, stderr_capture + stderr_len, (size_t)n);
			stderr_len += (size_t)n;
		} else if (n == 0) {
			close(target_stderr[0]);
			target_stderr[0] = -1;
		}
	}
	return 0;
}

/* push to_target_buf out to the target; returns -1 on EPIPE (dead target) */
static int
flush_to_target(void)
{
	if (target_dead)
		return -1;
	while (to_target_buf.len) {
		ssize_t n = write(to_target[1], to_target_buf.data,
				  to_target_buf.len);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN) {
				if (poll_one_out() < 0)
					return -1;
				continue;
			}
			target_dead = 1;
			return -1; /* EPIPE */
		}
		memmove(to_target_buf.data, to_target_buf.data + n,
			to_target_buf.len - (size_t)n);
		to_target_buf.len -= (size_t)n;
	}
	return 0;
}

static int
poll_one_out(void)
{
	struct pollfd p;
	p.fd = to_target[1];
	p.events = POLLOUT;
	p.revents = 0;
	if (poll(&p, 1, 200) < 0 && errno != EINTR)
		return -1;
	return 0;
}

static void
send_bytes(const unsigned char *b, size_t n)
{
	size_t off = 0;
	if (target_dead) {
		tr_mark("# dropped %zu bytes: target already gone", n);
		return;
	}
	tr_buf(0, b, n);
	if (getenv("ZFUZZ_DEBUG")) {
		fprintf(stderr, "send %zu bytes:", n);
		{ size_t qi; for (qi = 0; qi < n && qi < 24; qi++)
			fprintf(stderr, " %02x", b[qi]); }
		fputc('\n', stderr);
	}
	while (off < n) {
		size_t room = sizeof to_target_buf.data - to_target_buf.len;
		size_t chunk = n - off < room ? n - off : room;
		memcpy(to_target_buf.data + to_target_buf.len, b + off, chunk);
		to_target_buf.len += chunk;
		off += chunk;
		if (to_target_buf.len == sizeof to_target_buf.data)
			if (flush_to_target() < 0)
				return;
		drain_target();
	}
	if (flush_to_target() < 0)
		return;
}

static int
wait_target(int *exitcode)
{
	int status = 0;
	pid_t r;
	sigset_t alrm, oldmask;

	alarm_fired = 0;
	/* The alarm must fire *here*, not while the target-side plumbing
	 * or a mutation step blocks it: a deferred SIGALRM would otherwise
	 * be delivered at an arbitrary later sigprocmask restore (observed
	 * killing the harness itself against targets that ignore reads,
	 * e.g. zmtx-zmrx-1.02). Arming happens BEFORE unblocking so no
	 * alarm can slip through. */
	sigemptyset(&alrm);
	sigaddset(&alrm, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &alrm, &oldmask);
	alarm((unsigned)(alarm_seconds + 5));
	for (;;) {
		r = waitpid(target_pid, &status, WNOHANG);
		if (r == target_pid)
			break;
		if (r < 0) {
			if (errno == EINTR) {
				/* interrupted: alarm may have fired, fall
				 * through to the alarm_fired check below */
				if (alarm_fired)
					break;
				continue;
			}
			/* early out must not leak an armed alarm or an
			 * unblocked mask into the rest of the iteration */
			alarm(0);
			sigprocmask(SIG_BLOCK, &alrm, NULL);
			return -1;
		}
		if (alarm_fired)
			break;
		drain_target();
	}
	alarm(0);
	/* re-block: the rest of the iteration (mutation sends, plumbing)
	 * must not lose the harness to a stray SIGALRM */
	sigprocmask(SIG_BLOCK, &alrm, NULL);
	if (alarm_fired) {
		kill(target_pid, SIGKILL);
		waitpid(target_pid, &status, 0);
		return -1; /* hang */
	}
	if (WIFEXITED(status))
		*exitcode = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
		*exitcode = -WTERMSIG(status);
	else
		*exitcode = -99;
	return 0;
}

/* --- invariant helpers --- */

static int
file_changed(const char *a, const char *b)
{
	struct stat sa, sb;
	if (stat(a, &sa) != 0 || stat(b, &sb) != 0)
		return 1;
	if (sa.st_size != sb.st_size)
		return 1;
	/* content check: cheap full compare (canaries are <= 64 KiB) */
	{
		static unsigned char ca[70000], cb[70000];
		int fa = open(a, O_RDONLY);
		int fb = open(b, O_RDONLY);
		ssize_t na = 0, nb = 0;
		if (fa < 0 || fb < 0) {
			if (fa >= 0) close(fa);
			if (fb >= 0) close(fb);
			return 1;
		}
		na = read(fa, ca, sizeof ca);
		nb = read(fb, cb, sizeof cb);
		close(fa);
		close(fb);
		if (na != nb)
			return 1;
		return memcmp(ca, cb, (size_t)na) != 0;
	}
}

/* --- failure report --- */

static char transcript_name[768];

static void
fail(const char *what, long it, int exitcode)
{
	char path[768];
	snprintf(path, sizeof path, "/tmp/zmodemfuzz-fail-seed%ld-it%ld.log",
		 seed, it);
	if (transcript) {
		fclose(transcript);
		transcript = NULL;
	}
	if (rename(transcript_name, path) != 0 && errno != ENOENT)
		fprintf(stderr, "zmodemfuzz: transcript rename failed: %s\n",
			strerror(errno));
	fprintf(stderr, "FAIL iter %ld: %s (exit %d, transcript: %s)\n",
		it, what, exitcode, path);
	if (stderr_len) {
		/* full stderr next to the fail transcript (the on-stderr
		 * excerpt stays capped at 800 bytes) */
		char spath[768];
		FILE *f;
		fprintf(stderr, "  target stderr: %.*s\n",
			(int)(stderr_len < 800 ? stderr_len : 800),
			stderr_capture);
		snprintf(spath, sizeof spath,
			 "/tmp/zmodemfuzz-fail-seed%ld-it%ld.stderr",
			 seed, it);
		f = fopen(spath, "wb");
		if (f) {
			fwrite(stderr_capture, 1, stderr_len, f);
			fclose(f);
			fprintf(stderr, "  full target stderr: %s\n", spath);
		}
	}
}

/* --- frame parsing (what the target sends to us) --- */

#define FUZZ_TIMEOUT (-1000)
#define FUZZ_EOF     (-1001)

struct framebuf {
	int type;
	unsigned char hdr[4];
	unsigned char data[2048];
	size_t data_len;
	int frame_ind;
	int term;	/* data subpacket: the ZCRCx terminator byte */
	int crc_ok;	/* data subpacket: CRC checked out */
};

enum rstate { R_IDLE, R_ZPADS, R_FRAME, R_HEX, R_BIN, R_BIN32 };

struct rparser {
	enum rstate state;
	unsigned char hdr[4];
	int type;
	int frame_ind;
	int bytecnt;
	char hexbuf[16];
	int hexcnt;
	unsigned short crc16;
	unsigned long crc32;
	unsigned char crcrecv[4];
	int crccnt;
	int crc_esc;
	int canc;
	int term_byte;	/* data subpacket: the ZCRCx terminator */
	int in_data;	/* binary data frame: subpackets follow, not CRC */
	int sub_term;	/* current data subpacket's ZCRCx terminator */
	int sub_crc_cnt; /* collecting subpacket CRC bytes */
	int data_len;	/* data bytes of the current subpacket */
	int hdr_crc_left; /* binary header CRC bytes still pending */
	int eof_frame;	/* ZEOF: frame ends after the header CRC */
};

static void
rp_init(struct rparser *p)
{
	memset(p, 0, sizeof *p);
	p->frame_ind = -1;
	p->type = -1;
}

static int
rp_emit(struct rparser *p, struct framebuf *fb)
{
	fb->type = p->type;
	memcpy(fb->hdr, p->hdr, 4);
	fb->data_len = 0;
	fb->frame_ind = p->frame_ind;
	fb->term = p->term_byte;
	if (p->state == R_BIN)
		fb->crc_ok = (p->crc16 == 0);
	else
		fb->crc_ok = (p->crc32 == 0xDEBB20E3UL);
	memset(p, 0, sizeof *p);
	p->type = -1;
	p->frame_ind = -1;
	return 1;
}

/* process one byte; returns 1 when a frame is complete */
static int
rp_byte(struct rparser *p, unsigned char c, struct framebuf *fb)
{
	switch (p->state) {
	case R_IDLE:
		if (c == CAN) {
			if (++p->canc >= 5) {
				fb->type = ZCAN;
				fb->data_len = 0;
				fb->frame_ind = -1;
				memset(p, 0, sizeof *p);
				p->type = -1;
				p->frame_ind = -1;
				return 1;
			}
			return 0;
		}
		p->canc = 0;
		if (c == ZPAD)
			p->state = R_ZPADS;
		return 0;
	case R_ZPADS:
		if (c == ZPAD)
			return 0;
		if (c == ZDLE) {
			p->state = R_FRAME;
			return 0;
		}
		p->state = R_IDLE;
		return 0;
	case R_FRAME:
		if (c == ZHEX) {
			p->frame_ind = ZHEX;
			p->hexcnt = 0;
			p->state = R_HEX;
		} else if (c == ZBIN) {
			p->frame_ind = ZBIN;
			p->crc16 = 0;
			p->bytecnt = 0;
			p->crccnt = 0;
			p->crc_esc = 0;
			p->state = R_BIN;
		} else if (c == ZBIN32) {
			p->frame_ind = ZBIN32;
			p->crc32 = 0xFFFFFFFFUL;
			p->bytecnt = 0;
			p->crccnt = 0;
			p->crc_esc = 0;
			p->state = R_BIN32;
		} else {
			p->state = R_IDLE;
		}
		return 0;
	case R_HEX:
		if (getenv("ZFUZZ_DEBUG") && c != '\r' && c != 0x11 && c != 0x8a)
			fprintf(stderr, "R_HEX got %02x (%c) cnt=%d\n",
				c, (c >= 0x20 && c < 0x7f) ? c : '?', p->hexcnt);
		if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
		    || (c >= 'A' && c <= 'F')) {
			if (p->hexcnt < 14)
				p->hexbuf[p->hexcnt] = (char)c;
			p->hexcnt++;
			if (p->hexcnt == 14) {
				unsigned int t, d0, d1, d2, d3, crc;
				sscanf(p->hexbuf, "%2x%2x%2x%2x%2x%4x",
				       &t, &d0, &d1, &d2, &d3, &crc);
				p->type = (int)(t & 0xff);
				p->hdr[0] = (unsigned char)d0;
				p->hdr[1] = (unsigned char)d1;
				p->hdr[2] = (unsigned char)d2;
				p->hdr[3] = (unsigned char)d3;
			}
			return 0;
		}
		/* line ending: CR, LF|0x80, XON, NUL */
		if (c == '\r' || c == 0x8a || c == 0x11 || c == 0) {
			if (p->hexcnt >= 14) {
				rp_emit(p, fb);
				return 1;
			}
			return 0;
		}
		p->state = R_IDLE;
		return 0;
	default:
		break;
	case R_BIN:
	case R_BIN32:
		if (p->in_data && p->bytecnt >= 5) {
			/* data subpackets: the ZDLE handling below is
			 * per-byte, done in the branches themselves */
		} else if (p->crc_esc) {
			p->crc_esc = 0;
			c = (unsigned char)(c ^ 0x40);
		} else if (c == ZDLE) {
			p->crc_esc = 1;
			return 0;
		}
		if (p->bytecnt == 0) {
			p->type = c;
			p->term_byte = 0;
			p->in_data = 0;
			p->sub_term = 0;
			p->sub_crc_cnt = 0;
			if (p->state == R_BIN)
				p->crc16 = updcrc(c, 0);
			else
				p->crc32 = updc32(c, p->crc32);
			p->bytecnt = 1;
			return 0;
		}
		if (p->bytecnt <= 4) {
			p->hdr[p->bytecnt - 1] = c;
			if (p->state == R_BIN)
				p->crc16 = updcrc(c, p->crc16);
			else
				p->crc32 = updc32(c, p->crc32);
			p->bytecnt++;
			if (p->bytecnt == 5) {
				/* header complete: binary headers end
				 * with their own CRC (2 bytes 16-bit /
				 * 4 bytes 32-bit) BEFORE any data
				 * subpackets; the CRC bytes must be
				 * consumed first */
				switch (p->type) {
				case ZRPOS:
				case ZACK:
				case ZRQINIT:
				case ZRINIT:
				case ZFIN:
				case ZNAK:
				case ZABORT:
				case ZFERR:
				case ZSKIP:
				case ZCRC:
				case ZCHALLENGE:
				case ZCOMPL:
				case ZCAN:
				case ZFREECNT:
				case ZCOMMAND:
				case ZSTDERR:
					p->in_data = 0; /* CRC follows */
					break;
				case ZSINIT:
					/* ZSINIT carries the attention
					 * string as a data subpacket,
					 * but lsz sends it only after
					 * ESCCTL/TESC8 negotiation - the
					 * header CRC comes first either
					 * way */
					p->in_data = 0;
					break;
				default:
					/* ZFILE/ZDATA/ZEOF: header CRC
					 * first (consumed in the
					 * !in_data branch). ZDATA/ZFILE
					 * then carry data subpackets;
					 * ZEOF ENDS after its header CRC
					 * (no subpackets) */
					p->in_data = 0;
					p->hdr_crc_left =
					    (p->state == R_BIN) ? 2 : 4;
					p->eof_frame = (p->type == ZEOF);
					break;
				}
			}
			return 0;
		}
		if (!p->in_data) {
			/* header CRC bytes: 2 (16-bit) / 4 (32-bit) close
			 * the header; for data frames they precede the
			 * subpackets instead of ending the frame */
			p->crcrecv[p->crccnt++] = c;
			if (p->crccnt == 1)
				p->term_byte = c;
			if (p->state == R_BIN)
				p->crc16 = updcrc(c, p->crc16);
			else
				p->crc32 = updc32(c, p->crc32);
			if ((p->state == R_BIN && p->crccnt == 2)
			    || (p->state == R_BIN32 && p->crccnt == 4)) {
				if (p->hdr_crc_left && !p->eof_frame) {
					/* data frame: header CRC done,
					 * subpackets follow; re-init the
					 * CRC chain: the subpacket CRC
					 * covers only data + terminator +
					 * CRC, not the header */
					p->hdr_crc_left = 0;
					p->crccnt = 0;
					p->in_data = 1;
					p->sub_crc_cnt = 0;
					if (p->state == R_BIN)
						p->crc16 = 0;
					else
						p->crc32 = 0xFFFFFFFFUL;
					return 0;
				}
				rp_emit(p, fb);
				return 1;
			}
			return 0;
		}
		/* data frame: parse the subpacket stream. A ZDLE pair
		 * 'h'/'i'/'j'/'k' is the subpacket terminator (the raw
		 * terminator is never ZDLE-escaped as data), then 2/4
		 * CRC bytes, also ZDLE-escaped. One event per subpacket. */
		if (p->sub_crc_cnt == 0) {
			/* collecting data bytes */
			if (p->crc_esc) {
				/* the byte after ZDLE: terminator or
				 * escaped data? Only 'h'/'i'/'j'/'k' are
				 * terminators; everything else is data */
				if (c == ZCRCE || c == ZCRCG || c == ZCRCQ
				    || c == ZCRCW) {
					p->sub_term = c;
					p->crc_esc = 0;
					if (p->state == R_BIN)
						p->crc16 = updcrc(c,
							p->crc16);
					else
						p->crc32 = updc32(c,
							p->crc32);
					p->sub_crc_cnt = 1;
					return 0;
				}
				/* escaped data byte */
				p->crc_esc = 0;
				if (p->data_len < (int)sizeof fb->data)
					fb->data[p->data_len++] =
					    (unsigned char)(c ^ 0x40);
				if (p->state == R_BIN)
					p->crc16 = updcrc(
						(unsigned char)(c ^ 0x40),
						p->crc16);
				else
					p->crc32 = updc32(
						(unsigned char)(c ^ 0x40),
						p->crc32);
				return 0;
			}
			if (c == ZDLE) {
				p->crc_esc = 1;
				return 0;
			}
			if (p->data_len < (int)sizeof fb->data)
				fb->data[p->data_len++] = c;
			if (p->state == R_BIN)
				p->crc16 = updcrc(c, p->crc16);
			else
				p->crc32 = updc32(c, p->crc32);
			return 0;
		}
		/* subpacket CRC bytes */
		{
			int want = (p->state == R_BIN) ? 2 : 4;
			p->crcrecv[p->crccnt++] = c;
			if (p->state == R_BIN)
				p->crc16 = updcrc(c, p->crc16);
			else
				p->crc32 = updc32(c, p->crc32);
			if (p->crccnt >= want) {
				fb->data_len = p->data_len;
				fb->type = p->type;
				memcpy(fb->hdr, p->hdr, 4);
				fb->frame_ind = p->frame_ind;
				fb->term = p->sub_term;
				if (p->state == R_BIN)
					fb->crc_ok = (p->crc16 == 0);
				else
					fb->crc_ok =
					    (p->crc32 == 0xDEBB20E3UL);
				p->data_len = 0;
				p->crccnt = 0;
				p->sub_crc_cnt = 0;
				{
					int t = p->sub_term;

					p->sub_term = 0;
					if (t == ZCRCE || t == ZCRCW) {
						/* frame ends: the next
						 * bytes are a new header
						 * (lsz flushes after
						 * ZCRCE/ZCRCW subpackets) */
						p->state = R_IDLE;
						p->bytecnt = 0;
					} else {
						/* stay in data mode: the
						 * next subpacket continues
						 * this frame */
						p->bytecnt = 5;
					}
				}
				return 1;
			}
		}
		return 0;
	}
	return 0;
}

/* read one frame from the target with a deadline (ms); fills fb.
 * returns 1 = frame complete, 0 = deadline */
static int
read_frame(struct rparser *rp, struct framebuf *fb, int ms)
{
	time_t start = time(NULL);
	time_t end = start + (ms + 999) / 1000;

	for (;;) {
		/* consume buffered bytes first */
		while (rxbuf.pos < rxbuf.len) {
			if (rp_byte(rp, rxbuf.data[rxbuf.pos++], fb))
				return 1;
		}
		rxbuf.len = 0;
		rxbuf.pos = 0;
		if (from_target[0] < 0)
			return 0;
		if (time(NULL) >= end)
			return 0;
		drain_target(); /* fills rxbuf (50ms poll) */
	}
}

/* --- phase 1: fuzz-receive (target = lrz; we are the sender) --- */

static const char *hostile_names[] = {
	"normal.bin",
	"with space.bin",
	"..",
	"../escape-target.bin",
	"/tmp/zmodemfuzz-escape-target.bin",
	"-dashleading",
	".hidden",
	"\xc3\xa4-\xc3\xb6-\xc3\xbc-8bit.bin",  /* utf-8 umlauts */
	"tab\tand\nnewline.bin",
	"-",
	"a.bin.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak"
	".bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak"
	".bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak"
	".bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak.bak",
	"subdir/deep/deeper/file.bin",
};

#define N_HOSTILE ((int)(sizeof hostile_names / sizeof hostile_names[0]))

/* mutation classes (for logging/coverage checks) */
enum mclass {
	M_NONE = 0,      /* clean baseline */
	M_BYTEFLIP,      /* random bit flips in the stream */
	M_CRC16,         /* corrupt 16-bit CRCs */
	M_CRC32,         /* corrupt 32-bit CRCs */
	M_CRCMODE,       /* claim CRC32 flag but send 16-bit CRC */
	M_FRAMETYPE,     /* frame-type byte roulette */
	M_ZDLE_ILL,      /* illegal ZDLE escapes */
	M_POSITION,      /* position-field extremes */
	M_FLAGS,         /* capability/flag byte storms */
	M_GARBAGE,       /* garbage floods */
	M_CANCAN,        /* CAN bursts */
	M_TRUNC,         /* truncate the stream at a random point */
	M_EOF,           /* premature close */
	M_NAME,          /* hostile filename */
	M_LONGPACKET,    /* oversized/large data subpackets */
	N_MCLASS
};

/* --- per-class run statistics (for the end-of-run summary) --- */
struct class_stats {
	long count;
	long total_ms;
	long max_ms;
	long hangs; /* target waited until the alarm */
	long timeouts; /* hangs + excused child-alarm (-14) deaths */
	long fails;
	long crc32; /* iterations that negotiated 32-bit CRC (receive mode) */
};
static struct class_stats rx_stats[N_MCLASS];   /* receive mode: class */
static struct class_stats tx_stats[13];         /* send mode: choice */
static int send_choice = -1;
static int n_tx_choices = 13; /* includes the data-phase ACK-loop */
static enum mclass mc_current = M_NONE; /* class of the running iteration */
static int iter_crc32; /* iteration negotiated 32-bit CRC (receive mode) */

/* per-class fail-reason histogram: the raw `bad` string from
 * check_invariants()/fail() grouped by class/choice, so a summary row
 * says WHAT failed, not just how often */
#define N_FAIL_REASONS 64
struct fail_reason {
	const char *what;
	long count;
};
static struct fail_reason fail_reasons[N_MCLASS][N_FAIL_REASONS];
static struct fail_reason fail_reasons_tx[13][N_FAIL_REASONS];

static void
note_fail_reason(const char *what, int rx, long idx)
{
	struct fail_reason (*tab)[N_FAIL_REASONS] =
	    rx ? fail_reasons : fail_reasons_tx;
	struct fail_reason *t = tab[idx];
	int i;
	for (i = 0; i < N_FAIL_REASONS && t[i].what; i++) {
		if (t[i].what == what) { /* same string literal */
			t[i].count++;
			return;
		}
	}
	if (i < N_FAIL_REASONS) {
		t[i].what = what;
		t[i].count = 1;
	}
}

/* build one ZFILE metadata subpacket around `name` */
static size_t
build_zfile_data(unsigned char *dst, size_t dstcap, const char *name,
		 size_t len, int ctlesc)
{
	char meta[120];
	int mlen;

	mlen = snprintf(meta, sizeof meta, "%lu %lo 100644 0 1 %lu",
			(unsigned long)len,
			(unsigned long)time(NULL),
			(unsigned long)len);
	(void)mlen;
	(void)ctlesc;
	{
		size_t nl = strlen(name);
		size_t ml = strlen(meta);
		size_t pos = 0;
		size_t i;
		/* nl + NUL + ml + trailing NUL (+ terminator added by the
		 * append layer) must fit; the trailing NUL is required by
		 * receivers that validate the last block byte (zmrx-2.02
		 * ZNAKs the ZFILE without it - the bug behind its
		 * "odd exit code 4" run) */
		if (nl + ml + 3 > dstcap)
			return 0;
		for (i = 0; i < nl; i++)
			dst[pos++] = (unsigned char)name[i];
		dst[pos++] = 0;
		for (i = 0; i < ml; i++)
			dst[pos++] = (unsigned char)meta[i];
		dst[pos++] = 0;
		return pos;
	}
}

/* one clean transfer exchange against lrz; every parameter can be
 * corrupted per the mutation matrix. Returns 0 = ok. */

/* --- mutation parameters (bounded corruption, deterministic) --- */

/* per-iteration mutation parameters, drawn from the main xorshift
 * stream so -s/-S stay deterministic. draw_mut_params() is the SINGLE
 * source of the draw sequence: both fuzz_receive_iter() and
 * fast_forward_rng() call it, which keeps the mirror exact by
 * construction. */
struct mut_params {
	int n_flips;		/* M_BYTEFLIP: number of bit flips */
	int flip_off;		/* M_BYTEFLIP: first flip position */
	int flip_bit;		/* M_BYTEFLIP: bit index (0..7) */
	int crc_n;		/* M_CRC16/32: CRC bytes to corrupt */
	int crc_off;		/* M_CRC16/32: offset of the CRC byte */
	int frame_type;		/* M_FRAMETYPE: substitute frame type */
	int frame_off;		/* M_FRAMETYPE: position to substitute */
	int ill_n;		/* M_ZDLE_ILL: illegal escapes to inject */
	int ill_off;		/* M_ZDLE_ILL: injection offset */
	int pos_mode;		/* M_POSITION: 0=0, 1=max, 2=pos-1, 3=pos+1 */
	int flags_bits;		/* M_FLAGS: bits to XOR into flag bytes */
	int flags_off;		/* M_FLAGS: position to corrupt */
	int garb_n;		/* M_GARBAGE: noise bytes to inject */
	int garb_off;		/* M_GARBAGE: injection offset */
	unsigned char garb[64];	/* M_GARBAGE: the noise bytes themselves */
	int can_n;		/* M_CANCAN: CAN bytes to inject */
	int can_off;		/* M_CANCAN: injection offset */
	int trunc_stop;		/* M_TRUNC: stop after this many subpackets */
	int eof_after_zfile;	/* M_EOF: drop the line after ZRPOS */
	unsigned int sub_chunk;	/* M_LONGPACKET: subpacket size to send */
};

static struct mut_params mut;

/* Draw all bounded mutation parameters for one iteration. `mc` selects
 * which fields are actually consumed; the draws happen in a fixed order
 * so the sequence is reproducible regardless of how the consumer uses
 * the values. */
static void
draw_mut_params(enum mclass mc)
{
	mut.n_flips = (mc == M_BYTEFLIP) ? 1 + (int)rng_below(8) : 0;
	mut.flip_off = (int)rng_below(256);
	mut.flip_bit = (int)rng_below(8);
	mut.crc_n = (mc == M_CRC16 || mc == M_CRC32) ? 1 + (int)rng_below(2)
	    : 0;
	mut.crc_off = (int)rng_below(64);
	mut.frame_type = (mc == M_FRAMETYPE) ? (int)rng_below(32) : 0;
	mut.frame_off = (int)rng_below(32);
	mut.ill_n = (mc == M_ZDLE_ILL) ? 1 + (int)rng_below(4) : 0;
	mut.ill_off = (int)rng_below(128);
	mut.pos_mode = (mc == M_POSITION) ? (int)rng_below(4) : -1;
	mut.flags_bits = (mc == M_FLAGS) ? (int)(rng_next() & 0xff) : 0;
	mut.flags_off = (int)rng_below(48);
	mut.garb_n = (mc == M_GARBAGE) ? 1 + (int)rng_below(64) : 0;
	mut.garb_off = (int)rng_below(512);
	{
		int k;
		for (k = 0; k < mut.garb_n && k < 64; k++)
			mut.garb[k] = (unsigned char)rng_next();
	}
	mut.can_n = (mc == M_CANCAN) ? 2 + (int)rng_below(8) : 0;
	mut.can_off = (int)rng_below(128);
	mut.trunc_stop = (mc == M_TRUNC) ? (int)rng_below(9) : -1;
	mut.eof_after_zfile = (mc == M_EOF) ? 1 : 0;
	mut.sub_chunk = 0;
	if (mc == M_LONGPACKET) {
		static const unsigned int sizes[5] =
		    { SUBPKT_MAX, SUBPKT_MAX + 1, 16384, 32768, 65536 };
		mut.sub_chunk = sizes[rng_below(5)];
	}
}

/* inject n bytes at offset off into the built frame (clamped); used by
 * M_GARBAGE and M_CANCAN */
static void
inject_bytes(size_t off, const unsigned char *bytes, size_t n)
{
	size_t i;
	if (off > outpos)
		off = outpos;
	if (n > sizeof outbuf - outpos)
		n = sizeof outbuf - outpos > n ? n : 0;
	if (n == 0)
		return;
	memmove(outbuf + off + n, outbuf + off, outpos - off);
	for (i = 0; i < n; i++)
		outbuf[off + i] = bytes[i];
	outpos += n;
}

/* mutate the freshly built frame in outbuf[0..outpos) according to the
 * drawn parameters, then send it. Clean classes pass through untouched.
 * Mutations only apply to data-path frames (ZDATA header, subpackets,
 * ZEOF): corrupting the handshake frames wedges the session state machine
 * (the receiver ZNAKs forever) instead of exercising the data path, which
 * is what the bounded classes are for. Session-level mutations (M_TRUNC,
 * M_EOF, M_NAME) act on the exchange, not on these frames. */
static void
mut_send(enum mclass mc, int data_frame)
{
	int i;

	if (!data_frame || mc == M_NONE || mc == M_NAME || mc == M_CRCMODE
	    || mc == M_TRUNC || mc == M_EOF || mc == M_LONGPACKET) {
		send_bytes(outbuf, outpos);
		return;
	}

	if (mc == M_BYTEFLIP) {
		for (i = 0; i < mut.n_flips; i++) {
			size_t off = (size_t)mut.flip_off + (size_t)i * 7;
			if (outpos == 0)
				break;
			off %= outpos;
			outbuf[off] ^= (unsigned char)(1 << mut.flip_bit);
		}
	} else if (mc == M_CRC16 || mc == M_CRC32) {
		/* corrupt the trailing CRC bytes of the frame: they sit
		 * right before the ZDLE+terminator pair (2 bytes), i.e. at
		 * outpos-4 .. outpos-3 (16-bit) or outpos-6..-3 (32-bit) */
		for (i = 0; i < mut.crc_n; i++) {
			size_t off = outpos >= 6
			    ? outpos - 4 - (size_t)i : 0;
			outbuf[off] ^= 0xa5;
		}
	} else if (mc == M_FRAMETYPE) {
		/* the frame-type byte is the first byte after the ZDLE+ind
		 * pair (outbuf[3] for binary, hex digit pair start for hex);
		 * simplest reliable target: XOR a few bytes near the front */
		size_t off = (size_t)mut.frame_off % (outpos ? outpos : 1);
		outbuf[off] ^= (unsigned char)(mut.frame_type & 0x1f);
	} else if (mc == M_ZDLE_ILL) {
		unsigned char ill[4]={0,0,0,0};
		for (i = 0; i < mut.ill_n && i < 4; i++)
			ill[i] = ZDLE;
		inject_bytes((size_t)mut.ill_off, ill, (size_t)(i ? i : 1));
	} else if (mc == M_FLAGS) {
		size_t off = (size_t)mut.flags_off % (outpos ? outpos : 1);
		outbuf[off] ^= (unsigned char)mut.flags_bits;
	} else if (mc == M_GARBAGE) {
		/* mut.garb[] was drawn in draw_mut_params(): rng draws must
		 * never happen at send time or -S fast-forward desyncs */
		inject_bytes((size_t)mut.garb_off, mut.garb, (size_t)mut.garb_n);
	} else if (mc == M_CANCAN) {
		unsigned char cans[9];
		for (i = 0; i < mut.can_n && i < 9; i++)
			cans[i] = CAN;
		inject_bytes((size_t)mut.can_off, cans, (size_t)i);
	} else if (mc == M_POSITION) {
		/* corrupt a position byte inside a data header: the four
		 * position bytes of binary headers live at outbuf[5..8] */
		if (outpos >= 9) {
			size_t off = 5 + (size_t)(mut.pos_mode & 3) % 4;
			static const unsigned char pm[4] =
			    { 0x00, 0xff, 0xfe, 0x7f };
			outbuf[off] = pm[mut.pos_mode & 3];
		}
	}
	send_bytes(outbuf, outpos);
}

/* fast-forward the PRNG over iterations 0..it-1 without running the
 * target: replay exactly the draws a real run made, so iteration `it`
 * starts with the identical rng_state and behaves byte-for-byte like
 * it did in the original run. Mirrors the main loop's per-iteration
 * draw order: build_payload(), mutation class, optional hostile-name
 * pick, then fuzz_receive_iter()'s payload length and payload bytes
 * plus draw_mut_params(). In send mode the mirror is fuzz_send_iter()'s
 * unconditional sequence: build_payload(), mc draw, draw_mut_params(),
 * choice draw (the clean-baseline overrides never touch the stream). */
static void
fast_forward_rng(long it)
{
	long i;

	for (i = 0; i < it; i++) {
		unsigned int mc;
		if (it > 100000 && (i % 1000000) == 999999) {
			/* no wall-clock dedup here: the loop can pass
			 * several marks within one time() second, and a
			 * silently suppressed report is exactly what we
			 * are guarding against */
			fprintf(stderr,
				"zmodemfuzz: fast-forwarding rng: "
				"%ld/%ld iterations done (%d%%)\n",
				i + 1, it, (int)((i + 1) * 100 / it));
		}
		build_payload(); /* main loop's per-iteration draw */
		mc = rng_below(N_MCLASS - 1) + 1;
		if (i % 17 == 0)
			mc = M_NONE;
		if (!mode_receive) {
			draw_mut_params((enum mclass)mc);
			(void)rng_below(n_tx_choices); /* fuzz_send_iter's choice */
			continue;
		}
		if ((enum mclass)mc == M_NAME)
			(void)rng_below(N_HOSTILE);
		build_payload(); /* fuzz_receive_iter()'s own call */
		payload_len = 1 + rng_below(20000);
		draw_mut_params((enum mclass)mc);
		if ((enum mclass)mc == M_LONGPACKET && mut.sub_chunk)
			/* keep the mirror identical to fuzz_receive_iter():
			 * one conditional draw for the extra payload len */
			payload_len = mut.sub_chunk + 1024
			    + rng_below(8192);
		{
			size_t k;
			for (k = 0; k < payload_len; k++)
				(void)rng_next();
		}
	}
}

static int
fuzz_receive_iter(int it, enum mclass mc, int *exitcode)
{
	struct rparser rp;
	int nameidx = (mc == M_NAME) ? (rng_below(N_HOSTILE)) : 0;
	const char *name = hostile_names[nameidx];
	int crc32 = 0;
	int ctlesc = 0;
	unsigned long sent_pos = 0;
	mc_current = mc;
	build_payload();
	snprintf(payload_name, sizeof payload_name, "%s", name);
	payload_len = 1 + rng_below(20000);
	/* the clean baseline uses 16-bit CRC everywhere; M_CRCMODE
	 * negotiates 32-bit CRC and sends CRC32-protected frames (lrz
	 * advertises CANFC32 in every ZRINIT). The old "escape
	 * subtleties" concern is obsolete: append_data32/build_binhdr32
	 * escape control bytes and CRC bytes like lrz expects. */
	crc32 = (mc == M_CRCMODE);
	iter_crc32 = crc32;
	draw_mut_params(mc);
	if (mc == M_LONGPACKET && mut.sub_chunk)
		payload_len = mut.sub_chunk + 1024 + rng_below(8192);
	{
		size_t i;
		for (i = 0; i < payload_len; i++)
			payload[i] = (unsigned char)rng_next();
	}

	/* frame builder state */
	{
		/* 1. send ZRQINIT up to 4 times */
		int tries;
		int got_zrinit = 0;
		struct framebuf fb;

		for (tries = 0; tries < 4 && !got_zrinit; tries++) {
			unsigned char hdr[4] = { 0, 0, 0, 0 };
			out_reset();
			build_hexhdr(ZRQINIT, hdr);
			tr_mark("> ZRQINIT try %d", tries);
			mut_send(mc, 0);
			rp_init(&rp);
			if (read_frame(&rp, &fb, 2000) && fb.type == ZRINIT) {
				got_zrinit = 1;
				/* honor ESCCTL, but stay at CRC16 for the
				 * clean baseline (see crc32 = 0 below) */
				ctlesc = (fb.hdr[3] & ESCCTL) ? 1 : 0;
			}
		}
		if (!got_zrinit) {
			tr_mark("# no ZRINIT: aborting exchange");
			/* counted as ok: a receiver may legitimately ignore
			 * a mutated ZRQINIT. But if it printed anything it
			 * is usually a misconfigured invocation - say so
			 * loudly instead of looking like a healthy run. */
			if (stderr_len) {
				fprintf(stderr,
					"zmodemfuzz: iter %d: target sent no "
					"ZRINIT and printed:\n  %.*s\n",
					it,
					(int)(stderr_len < 400 ? stderr_len
							       : 400),
					stderr_capture);
				*exitcode = -2;
				return 1; /* counted as ok (target rejected us) */
			}
			/* total silence: on iteration 0 the target cannot
			 * be "rejecting" anything - it never started
			 * (exec failure in spawn_target, e.g. binary
			 * missing). Anything else is a mutated ZRQINIT
			 * the receiver legitimately ignored. */
			if (it == 0) {
				die("target never spoke (exec failure? "
				    "target built?); fuzzing aborted");
			}
			*exitcode = -2;
			return 1; /* counted as ok (target rejected us) */
		}

		/* 3. ZFILE with (possibly hostile) name */
			tr_mark("# entering ZFILE phase (class=%d)", (int)mc);
		{
			unsigned char zdata[1200];
			size_t zlen;
			unsigned char hdr[4];
			zlen = build_zfile_data(zdata, sizeof zdata,
						name, payload_len, ctlesc);
			hdr[0] = 0;
			hdr[1] = 0;
			hdr[2] = 0;
			hdr[3] = ZCBIN;
			out_reset();
			if (crc32)
				build_binhdr32(ZFILE, hdr);
			else
				build_binhdr16(ZFILE, hdr, ctlesc);
			{
				size_t npos = outpos;
				if (crc32)
					outpos = append_data32(outbuf,
							       sizeof outbuf,
							       npos, zdata,
							       zlen, ZCRCW,
							       ctlesc);
				else
					outpos = append_data16(outbuf,
							       sizeof outbuf,
							       npos, zdata,
							       zlen, ZCRCW,
							       ctlesc);
			}
			tr_mark("> ZFILE name=%s len=%lu crc32=%d",
				name, (unsigned long)payload_len, crc32);
			mut_send(mc, 0);

			/* expect ZRPOS (or ZCRC first) */
			rp_init(&rp);
			{
				/* junk frames (ZNAK etc.) tolerated up to 3
				 * times per exchange: the counter MUST be
				 * per-iteration - a function-static leaked
				 * across iterations and permanently broke
				 * the exchange wait after 3 junk frames
				 * had accumulated in earlier iterations */
				int znaks = 0;
				while (read_frame(&rp, &fb, 2500)) {
					if (fb.type == ZRPOS)
						break;
					if (fb.type == ZSKIP) {
						tr_mark("# receiver skipped the file");
						goto finish;
					}
					if (fb.type == ZCRC) {
						/* answer the CRC query */
						unsigned long crc = 0xFFFFFFFFUL;
						size_t i;
						unsigned char chdr[4] = { 0, 0, 0, 0 };
						for (i = 0; i < payload_len; i++)
							crc = updc32(payload[i], crc);
						crc = ~crc;
						chdr[0] = crc & 0xff;
						chdr[1] = (crc >> 8) & 0xff;
						chdr[2] = (crc >> 16) & 0xff;
						chdr[3] = (crc >> 24) & 0xff;
						out_reset();
						build_hexhdr(ZCRC, chdr);
						mut_send(mc, 0);
						continue;
					}
					if (fb.type == ZCAN || fb.type == ZSKIP
					    || fb.type == ZABORT
					    || fb.type == ZFERR) {
						goto finish;
					}
					if (++znaks > 3)
						break;
				}
			}
			/* M_EOF: premature line drop right after the file
			 * offer was acknowledged - the receiver sees the
			 * carrier go away mid-session */
			if (mut.eof_after_zfile) {
				tr_mark("# dropping the line (M_EOF)");
				close(to_target[1]);
				to_target[1] = -1;
				target_dead = 1;
			}
		}

		/* 4. ZDATA + subpackets: ONE ZDATA header per frame - after a
		 * ZCRCG-terminated subpacket the receiver expects the next
		 * subpacket to continue within the same frame (no new
		 * header), so all chunks stream here under a single header
		 * until the final ZCRCE. M_LONGPACKET sends one oversized
		 * subpacket (up to 64k, sizes other implementations accept)
		 * and then resyncs like a real sender: on the receiver's
		 * ZRPOS it resumes from the acknowledged offset with legal
		 * 1024-byte subpackets. Receivers with a smaller subpacket
		 * limit (e.g. crzsz: 1024) reject even "legal" chunks; the
		 * post-stream drain below honors their (repeated) ZRPOS
		 * requests and re-streams the remainder from the requested
		 * offset, as many times as they ask - bounded. */
		{
			size_t off = 0;
			int done = 0;
			int resyncs = 0;
			unsigned char hdr[4];
			size_t cap = (mc == M_LONGPACKET && mut.sub_chunk)
			    ? mut.sub_chunk : 1024;
			longpkt_done = 0;
			hdr[0] = 0;
			hdr[1] = 0;
			hdr[2] = 0;
			hdr[3] = 0;
			out_reset();
			if (crc32)
				build_binhdr32(ZDATA, hdr);
			else
				build_binhdr16(ZDATA, hdr, ctlesc);
			mut_send(mc, 1);
			tr_mark("> ZDATA pos=0 (single header)");
			if (mc == M_LONGPACKET && mut.sub_chunk)
				tr_mark("# long-packet size=%u",
					mut.sub_chunk);
			while (off < payload_len && !done) {
				size_t chunk = payload_len - off;
				int term;
				if (chunk > cap)
					chunk = cap;
				if (off + chunk >= payload_len) {
					term = ZCRCE;
					done = 1;
				} else
					term = ZCRCG;
				out_reset();
				{
					size_t npos = outpos;
					if (crc32)
						outpos = append_data32(outbuf,
							sizeof outbuf, npos,
							payload + off, chunk,
							term, ctlesc);
					else
						outpos = append_data16(outbuf,
							sizeof outbuf, npos,
							payload + off, chunk,
							term, ctlesc);
				}
				tr_mark("> subpacket pos=%lu chunk=%lu term=%c",
					(unsigned long)off,
					(unsigned long)chunk, term);
				mut_send(mc, 1);
				off += chunk;
				/* M_LONGPACKET: the receiver rejects a
				 * subpacket larger than its buffer
				 * ("Data subpacket too long") and drops
				 * into its error-resync loop; it answers
				 * with ZRPOS(pos) - wait for it and
				 * resume from the acknowledged offset */
				if (cap > SUBPKT_MAX) {
					size_t resume_pos;
					int got;
					struct rparser rp3;
					tr_mark("# long-packet: sent %lu-byte"
						" subpacket (> %d)",
						(unsigned long)chunk,
						SUBPKT_MAX);
					cap = 1024;
					rp_init(&rp3);
					got = read_frame(&rp3, &fb, 3000);
					if (!got)
						got = read_frame(&rp3, &fb,
								 3000);
					if (got && fb.type == ZRPOS) {
						resume_pos =
						    (size_t)fb.hdr[0]
						    | ((size_t)fb.hdr[1] << 8)
						    | ((size_t)fb.hdr[2] << 16)
						    | ((size_t)fb.hdr[3] << 24);
						if (resume_pos > off)
							resume_pos = off;
						tr_mark("# long-packet: "
							"resync at pos=%lu",
							(unsigned long)
							resume_pos);
						hdr[0] = resume_pos & 0xff;
						hdr[1] = (resume_pos >> 8)
						    & 0xff;
						hdr[2] = (resume_pos >> 16)
						    & 0xff;
						hdr[3] = (resume_pos >> 24)
						    & 0xff;
						out_reset();
						if (crc32)
							build_binhdr32(ZDATA,
								       hdr);
						else
							build_binhdr16(ZDATA,
								       hdr,
								       ctlesc);
						mut_send(mc, 1);
						tr_mark("> ZDATA pos=%lu "
							"(resync)",
							(unsigned long)
							resume_pos);
						off = resume_pos;
						continue;
					}
					tr_mark("# long-packet: no ZRPOS "
						"(receiver kept it), "
						"continuing");
				}
				/* M_TRUNC: cut the stream after a bounded
				 * number of subpackets - the receiver is
				 * left waiting for data that never comes */
				if (mut.trunc_stop >= 0
				    && (int)(off / 1024) >= mut.trunc_stop) {
					tr_mark("# truncating stream after "
						"%d subpackets",
						mut.trunc_stop);
					sent_pos = off;
					goto after_data;
				}
			}
			/* the stream is out; a receiver that rejected chunks
			 * (small subpacket limit) is error-retrying and has
			 * sent ZRPOS requests while we were streaming. Honor
			 * them now, newest wins, and re-stream the remainder
			 * from the requested offset - a real sender does
			 * exactly this, as many times as it takes. Bounded:
			 * a wedged receiver must not extend the iteration
			 * forever. IMPORTANT: after a rejection the receiver
			 * keeps scanning the leftover of the rejected data
			 * as garbage; re-streaming immediately would feed
			 * THAT scan too (the receiver then errors again).
			 * So after each ZRPOS: quiesce first (collect more
			 * ZRPOS for a grace period), then stream once.
			 * Gated on M_LONGPACKET only: for the other classes
			 * a receiver stuck in an error loop over corrupted
			 * data IS the mutation - answering its retries would
			 * turn every BYTEFLIP/FLAGS iteration into a
			 * 14s resync marathon instead of a bounded abort.
			 * SKIPPED when the stream ran to completion
			 * (off >= payload_len): the ZEOF at after_data:
			 * is due the instant the last subpacket is out.
			 * Waiting here for the receiver's next ZRPOS lets
			 * its error-retry budget expire on timeouts before
			 * the ZEOF arrives, and lrz then unlinks the
			 * COMPLETE file (observed: seed 453009526 it 535 -
			 * 20 queued ZRPOS(73598) answered post-mortem). */
			if (off < payload_len && mc == M_LONGPACKET
			    && mut.sub_chunk) {
				size_t last_pos = 0;
				int same_pos = 0;
                                size_t wait_ms = 200;
				for (;;) {
					size_t p2;
					struct rparser rp4;
					size_t newest;
					int got_any;
					/* quiesce: collect ZRPOS until the
					 * receiver stops asking */
					newest = payload_len;
					got_any = 0;
					for (;;) {
						rp_init(&rp4);
						if (!read_frame(&rp4, &fb,
								(int)wait_ms))
							break;
						if (fb.type == ZRPOS) {
							newest =
							    (size_t)fb.hdr[0]
							    | ((size_t)fb.hdr
							       [1] << 8)
							    | ((size_t)fb.hdr
							       [2] << 16)
							    | ((size_t)fb.hdr
							       [3] << 24);
							if (newest >
							    payload_len)
								newest =
								payload_len;
							got_any = 1;
							if (newest >=
							    payload_len) {
								/* the receiver
								 * has everything:
								 * it wants the
								 * ZEOF, not more
								 * collecting. Break
								 * out NOW - the
								 * receiver is
								 * burning its
								 * error-retry
								 * budget (one ZRPOS
								 * per timeout
								 * round, 20 rounds)
								 * while we collect;
								 * if the budget
								 * expires before our
								 * ZEOF lands it
								 * unlinks the
								 * COMPLETE file
								 * (observed as the
								 * two LONGPKT fails
								 * of seed
								 * 1812719613). */
								break;
							}
						} else if (fb.type == ZSKIP
							   || fb.type == ZCAN
							   || fb.type ==
							   ZABORT
							   || fb.type ==
							   ZFERR) {
							tr_mark("# receiver "
								"aborted "
								"during "
								"resync");
							goto finish;
						}
					}
					if (!got_any)
						break;
					p2 = newest;
					if (resyncs >= 20) {
						tr_mark("# resync budget "
							"exhausted, "
							"abandoning");
						goto finish;
					}
					resyncs++;
					tr_mark("# resync at pos=%lu",
						(unsigned long)p2);
					if (p2 >= payload_len) {
						/* the receiver already has
						 * everything: it wants the
						 * ZEOF, not another header */
						break;
					}
					hdr[0] = p2 & 0xff;
					hdr[1] = (p2 >> 8) & 0xff;
					hdr[2] = (p2 >> 16) & 0xff;
					hdr[3] = (p2 >> 24) & 0xff;
					out_reset();
					if (crc32)
						build_binhdr32(ZDATA, hdr);
					else
						build_binhdr16(ZDATA, hdr,
							       ctlesc);
					mut_send(mc, 1);
					tr_mark("> ZDATA pos=%lu (resync)",
						(unsigned long)p2);
					/* re-stream payload[p2..] with
					 * legal 1024-byte subpackets */
					{
						size_t o2 = p2;
						while (o2 < payload_len) {
							size_t c2 =
							    payload_len
							    - o2;
							size_t npos;
							if (c2 > 1024)
								c2 = 1024;
							out_reset();
							npos = outpos;
							if (crc32)
								outpos =
								append_data32(
								outbuf,
								sizeof outbuf,
								npos,
								payload + o2,
								c2,
								(o2 + c2 >=
								 payload_len)
								 ? ZCRCE
								 : ZCRCG,
								ctlesc);
							else
								outpos =
								append_data16(
								outbuf,
								sizeof outbuf,
								npos,
								payload + o2,
								c2,
								(o2 + c2 >=
								 payload_len)
								 ? ZCRCE
								 : ZCRCG,
								ctlesc);
							mut_send(mc, 1);
							o2 += c2;
						}
						tr_mark("# re-streamed "
							"pos=%lu..%lu",
							(unsigned long)p2,
							(unsigned long)
							payload_len);
					}
					/* if the receiver asks for the SAME
					 * position again, it discarded our
					 * re-stream as garbage: back off so
					 * its scanner can drain the pipe */
					if (p2 == last_pos) {
						same_pos++;
						if (same_pos >= 2)
							wait_ms = 1200;
					} else {
						same_pos = 0;
						last_pos = p2;
                                                wait_ms = 200;
					}
					/* the receiver has everything: stop
					 * resyncing entirely - the ZEOF is
					 * sent at after_data:, reached the
					 * moment this loop breaks. Waiting
					 * here (or for another ZRPOS) while
					 * the receiver sits in ST_HDR lets
					 * its 20-round error budget expire
					 * before the ZEOF lands, and it
					 * unlinks the COMPLETE file. */
					if (p2 >= payload_len)
						goto zeof_now;
				}
			}
zeof_now:
			sent_pos = payload_len;
			if (mc == M_LONGPACKET && mut.sub_chunk)
				longpkt_done = 1;
		}
after_data:
		/* 5. ZEOF (skipped after M_TRUNC: the receiver is left
		 * hanging mid-stream - that is the mutation). The cut is
		 * followed by a CAN burst so the receiver cancels the
		 * file promptly instead of waiting out its full 10s
		 * Rxtimeout retries: the mid-stream interruption (and the
		 * resulting partial write) is the mutation, and the
		 * bounded cancel keeps the iteration fast. */
		if (mut.trunc_stop >= 0) {
			/* six CANs: lrz's cancount needs five consecutive
			 * CANs to trigger zcancel */
			unsigned char cans[6] =
			    { CAN, CAN, CAN, CAN, CAN, CAN };
			tr_mark("# truncated mid-stream: cancelling");
			send_bytes(cans, sizeof cans);
			goto finish;
		}
		{
			unsigned char hdr[4];
			hdr[0] = sent_pos & 0xff;
			hdr[1] = (sent_pos >> 8) & 0xff;
			hdr[2] = (sent_pos >> 16) & 0xff;
			hdr[3] = (sent_pos >> 24) & 0xff;
			out_reset();
			build_hexhdr(ZEOF, hdr);
			tr_mark("> ZEOF pos=%lu", (unsigned long)sent_pos);
			mut_send(mc, 1);
		}

		/* 6. wait for ZRINIT (next file) or ZFIN/skip, then ZFIN */
			tr_mark("# entering final wait");
		{
			int final = 0;
			int rounds;
			for (rounds = 0; rounds < 6 && !final; rounds++) {
				rp_init(&rp);
				if (!read_frame(&rp, &fb, 1200))
					break;
				switch (fb.type) {
				case ZRINIT:
					final = 1;
					break;
				case ZRPOS:
					/* resume request after an M_LONGPACKET
					 * resync was already honored above;
					 * here it means an error loop we do
					 * not replay - just abort */
					final = 1;
					break;
				case ZFIN:
					final = 2;
					break;
				case ZSKIP:
					final = 1;
					break;
				case ZCAN:
					final = 3;
					break;
				default:
					break;
				}
			}
			if (final != 2) {
				unsigned char hdr[4] = { 0, 0, 0, 0 };
				struct rparser rp2;
				out_reset();
				build_hexhdr(ZFIN, hdr);
				tr_mark("> ZFIN (final)");
				mut_send(mc, 0);
				/* complete the close handshake: the receiver
				 * answers ZFIN, then expects the "OO" -
				 * without it it lingers in bounded retries */
				rp_init(&rp2);
				if (read_frame(&rp2, &fb, 1500) && fb.type == ZFIN) {
					unsigned char oo[2];
					oo[0] = 'O';
					oo[1] = 'O';
					tr_mark("> OO");
					send_bytes(oo, sizeof oo);
				}
			}
		}
	}
finish:
	/* every early exit skips wait_target() - a pending alarm would
	 * otherwise fire mid-later-iteration where nothing checks it
	 * (observed: harness killed by its own SIGALRM against
	 * zmtx-zmrx-1.02, which ignores reads) */
	alarm(0);
	(void)it;
	(void)exitcode;
	return 0;
}

/* main-loop hook: an M_LONGPACKET exchange that ran to completion sends
 * only clean frames after the oversized one, so the received file must
 * match byte for byte - the resync path is where resume-position bugs
 * would live. */
static int
longpkt_completed(void)
{
	return mc_current == M_LONGPACKET && mut.sub_chunk != 0
	    && longpkt_done;
}

/* --- phase 2: fuzz-send (target = lsz; we are the receiver) --- */

static void
fuzz_send_iter(void)
{
	struct rparser rp;
	struct framebuf fb;
	int crc32 = 0;
	/* byte-level mutation class for the receiver's own frames: the
	 * mirror of receive mode. The choice below still decides the
	 * session-level behavior; mc corrupts the answer frames. Every
	 * 17th iteration is forced to M_NONE (like receive mode's clean
	 * baseline) so the data-phase ACK loop can complete cleanly -
	 * the mutation classes corrupt every ACK by design, which would
	 * otherwise keep lsz in its bounded retry paths forever. The
	 * override happens AFTER the draw: the PRNG stream is unaffected
	 * (which is what makes send-mode -S replay possible). */
	enum mclass mc = (enum mclass)(rng_below(N_MCLASS - 1) + 1);
	if (current_iteration % 17 == 0)
		mc = M_NONE;
	draw_mut_params(mc);

	/* wait for the first ZRQINIT */
	rp_init(&rp);
	if (!read_frame(&rp, &fb, 5000)) {
		/* the sender never even spoke. A sender that printed
		 * to stderr is a misconfigured invocation - fail
		 * loudly instead of masquerading as a healthy
		 * "silence" iteration. */
		if (stderr_len) {
			fprintf(stderr,
				"zmodemfuzz: target sent nothing and printed:\n"
				"  %.*s\n",
				(int)(stderr_len < 400 ? stderr_len : 400),
				stderr_capture);
			die("target never spoke (bad file argument?); "
			    "fuzzing aborted");
		}
		/* total silence means the target never started: the
		 * classic case is an exec failure (_exit(127) in
		 * spawn_target, e.g. binary missing or not built).
		 * This used to return and the main loop still booked
		 * the iteration into tx_stats[0] ("silence") - a run
		 * against a nonexistent target then looked perfectly
		 * green. Die loudly instead. */
		die("target never spoke and said nothing "
		    "(exec failure? target built?); fuzzing aborted");
	}

	{
		int got_file = 0;
		for (;;) {
			unsigned char hdr[4] = { 0, 0, 0,
						 CANFDX | CANOVIO | CANBRK
						     | CANFC32 };
			if (target_dead)
				break; /* the target is gone */
			out_reset();
			build_hexhdr(ZRINIT, hdr);
			tr_mark("< ZRINIT (hostile receiver ready)");
			send_bytes(outbuf, outpos); /* session init: never mutated */
			rp_init(&rp);
			if (read_frame(&rp, &fb, 5000)) {
				if (fb.type == ZFILE || fb.type == ZSINIT) {
					got_file = 1;
					break;
				}
			}
		}
		if (!got_file && stderr_len) {
			/* lsz answered ZRQINIT but then died without
			 * ever offering a file, and said why on stderr:
			 * that is a misconfigured invocation (e.g. a
			 * file argument that does not exist relative to
			 * the harness's cwd) - the classic symptom is
			 * "cannot open <file>: No such file or
			 * directory". This used to masquerade as a
			 * healthy 0-failure run of "silence"
			 * iterations. Fail loudly instead. */
			fprintf(stderr,
				"zmodemfuzz: target never offered a file and "
				"printed:\n  %.*s\n",
				(int)(stderr_len < 400 ? stderr_len : 400),
				stderr_capture);
			die("target never offered a file (bad file "
			    "argument?); fuzzing aborted");
		}
		if (!got_file) {
			/* same as above: a live lsz always offers a file
			 * or says something on stderr; total silence is
			 * an exec/startup failure and must not book a
			 * healthy "silence" iteration */
			die("target never offered a file and said nothing "
			    "(exec failure? target built?); fuzzing aborted");
		}
	}
	/* now ZFILE arrived: decide hostile answer */
	{
		unsigned int choice = rng_below(n_tx_choices);
		/* clean-baseline iterations always take the ACK loop:
		 * the mutated choices would keep lsz in its bounded
		 * handshake retry paths and never exercise the data
		 * phase; the PRNG stream keeps its draw (which is what
		 * makes send-mode -S replay possible) */
		if (current_iteration % 17 == 0)
			choice = 11;
		send_choice = (int)choice;
		switch (choice) {
		case 0: /* never respond: lsz must hit its retry bounds */
			drain_target();
			break;
		case 1: { /* ZNAK loop */
			int i;
			for (i = 0; i < 5; i++) {
				unsigned char h[4] = { 0, 0, 0, 0 };
				out_reset();
				build_hexhdr(ZNAK, h);
				mut_send(mc, 1);
				read_frame(&rp, &fb, 2000);
			}
			break;
		}
		case 2: { /* skip the file */
			unsigned char h[4] = { 0, 0, 0, 0 };
			out_reset();
			build_hexhdr(ZSKIP, h);
			tr_mark("< ZSKIP");
			mut_send(mc, 1);
			read_frame(&rp, &fb, 3000);
			break;
		}
		case 3: { /* abort with ZABORT: lsz must reply ZFIN+OO */
			unsigned char h[4] = { 0, 0, 0, 0 };
			out_reset();
			build_hexhdr(ZABORT, h);
			tr_mark("< ZABORT");
			mut_send(mc, 1);
			read_frame(&rp, &fb, 5000);
			{
				unsigned char h2[4] = { 0, 0, 0, 0 };
				out_reset();
				build_hexhdr(ZFIN, h2);
				mut_send(mc, 1);
			}
			break;
		}
		case 4: { /* fatal file error ZFERR: same as ZABORT */
			unsigned char h[4] = { 0, 0, 0, 0 };
			out_reset();
			build_hexhdr(ZFERR, h);
			tr_mark("< ZFERR");
			mut_send(mc, 1);
			read_frame(&rp, &fb, 5000);
			{
				unsigned char h2[4] = { 0, 0, 0, 0 };
				out_reset();
				build_hexhdr(ZFIN, h2);
				mut_send(mc, 1);
			}
			break;
		}
		case 5: { /* ZCRC request (crc check) then skip */
			unsigned char h[4] = { 0, 0, 0, 0 };
			out_reset();
			build_hexhdr(ZCRC, h);
			mut_send(mc, 1);
			read_frame(&rp, &fb, 3000);
			{
				unsigned char h2[4] = { 0, 0, 0, 0 };
				out_reset();
				build_hexhdr(ZSKIP, h2);
				mut_send(mc, 1);
			}
			break;
		}
		case 6: { /* challenge (receiver MUST NOT send; lsz MAY ack) */
			unsigned char h[4] = { 0x12, 0x34, 0x56, 0x78 };
			out_reset();
			build_hexhdr(ZCHALLENGE, h);
			tr_mark("< ZCHALLENGE (protocol-hostile)");
			mut_send(mc, 1);
			read_frame(&rp, &fb, 2000);
			break;
		}
		case 7: { /* RPOS to absurd offset */
			unsigned char h[4] = { 0xff, 0xff, 0xff, 0x7f };
			out_reset();
			build_hexhdr(ZRPOS, h);
			tr_mark("< ZRPOS 0x7fffffff");
			mut_send(mc, 1);
			read_frame(&rp, &fb, 3000);
			break;
		}
		case 8: { /* cancel burst */
			int i;
			for (i = 0; i < 9; i++) {
				unsigned char c = CAN;
				send_bytes(&c, 1);
			}
			tr_mark("< CAN*9");
			break;
		}
		case 9: { /* ZRINIT storm: ack nothing, keep ZRINITing */
			int i;
			for (i = 0; i < 6; i++) {
				unsigned char h[4] = { 0, 0, 0,
						       CANFDX | CANOVIO };
				out_reset();
				build_hexhdr(ZRINIT, h);
				mut_send(mc, 1);
				read_frame(&rp, &fb, 1000);
			}
			break;
		}
		case 10: { /* finish the session politely */
			unsigned char h[4] = { 0, 0, 0, 0 };
			out_reset();
			build_hexhdr(ZFIN, h);
			tr_mark("< ZFIN (receiver quits)");
			mut_send(mc, 1);
			read_frame(&rp, &fb, 3000);
			break;
		}
		case 11: {
			/* the data-phase ACK loop: act as a real receiver -
			 * answer the ZFILE's ZCRCW with ZRPOS(0), then ack
			 * every data subpacket: ZCRCQ → ZACK(pos), ZCRCW →
			 * ZACK(pos), ZCRCE → ZACK(pos) + ZEOF handling and
			 * the final ZRINIT (next-file) close. All answer
			 * frames go through mut_send(mc, 1), so the 14
			 * byte-mutation classes corrupt ACKs, positions and
			 * CRCs mid-stream - this is the only send-mode
			 * choice that exercises the ACK path at all. */
			unsigned char rh[4];
			unsigned long ack_pos = 0;
			long n_acked = 0;
			int rounds;
			int data_phase_open = 0;

			/* ZRINIT for the data phase: advertise CANFDX with
			 * a 4096-byte buffer so lsz opens its streaming
			 * window (Txwindow = (Rxbuflen/2/64)*64, lsz.c) and
			 * pings ZCRCQ every Txwspac bytes - without a
			 * buffer size it streams ZCRCG-only and the ACK
			 * loop stays idle until ZCRCE. CANFC32 stays on:
			 * when lsz negotiates CRC32 the data subpackets
			 * arrive with 4-byte CRCs (R_BIN32) and our ZACKs
			 * must be built with the 32-bit builder. */
			rh[0] = 0x00;	/* Rxbuflen low */
			rh[1] = 0x10;	/* Rxbuflen high: 0x1000 = 4096 */
			rh[2] = 0;
			rh[3] = CANFDX | CANOVIO | CANBRK | CANFC32;
			out_reset();
			build_hexhdr(ZRINIT, rh);
			tr_mark("< ZRINIT (ACK-loop, buffer 4096)");
			send_bytes(outbuf, outpos); /* clean: session frame */

			for (rounds = 0; rounds < 100; rounds++) {
				if (target_dead)
					break;
				rp_init(&rp);
				/* 8s, not 4s: after a ZRINIT bad-ack lsz
				 * eats input for up to 5s (READLINE_PF(50)
				 * in its ZPAD hunter) before it resends
				 * the ZFILE; 4s missed the resend every
				 * time ("0 subpackets acked"). Once the
				 * data phase is open, lsz's stdio
				 * buffering can hold ZCRCG subpackets
				 * for a while (pipes: auto-flush at
				 * ~4KB), so keep reading up to 15s
				 * before declaring the sender dead. */
				if (!read_frame(&rp, &fb,
						data_phase_open ? 15000
								: 8000))
					break; /* lsz stopped talking */
				ack_pos = (unsigned long)fb.hdr[0]
				    | ((unsigned long)fb.hdr[1] << 8)
				    | ((unsigned long)fb.hdr[2] << 16)
				    | ((unsigned long)fb.hdr[3] << 24);
				switch (fb.type) {
				case ZFILE:
				case ZSINIT:
					/* re-offer / attention string:
					 * answer ZRPOS(0) - the sender
					 * (re)starts the data phase.
					 * Handshake frame: pass through
					 * UNMUTATED (like receive mode's
					 * mut_send(mc, 0)) - a mutated
					 * ZRPOS fails lsz's CRC check and
					 * lsz resends the ZFILE forever
					 * (bounded by its bad_acks>3), so
					 * the data phase would never open
					 * for the mutation classes. The
					 * ZACK/ZNAK data-phase answers
					 * below stay mutated. */
					rh[0] = 0;
					rh[1] = 0;
					rh[2] = 0;
					rh[3] = 0;
					out_reset();
					build_hexhdr(ZRPOS, rh);
					tr_mark("< ZRPOS pos=0");
					mut_send(mc, 0);
					break;
				case ZDATA:
					/* the parser emits the ZDATA
					 * frame's FIRST subpacket as a
					 * ZDATA event (fb.term carries the
					 * ZCRCx); later subpackets of the
					 * same frame arrive as more ZDATA
					 * events. Both are acked below via
					 * the shared term-based path. */
					if (fb.term != ZCRCE
					    && fb.term != ZCRCG
					    && fb.term != ZCRCQ
					    && fb.term != ZCRCW)
						break; /* pure header */
					goto ack_subpacket;
				case ZEOF:
					/* the sender is done: confirm with
					 * ZRINIT (file accepted) and end
					 * the loop on a clean note */
					rh[0] = 0;
					rh[1] = 0;
					rh[2] = 0;
					rh[3] = CANFDX | CANOVIO | CANBRK
					    | CANFC32;
					out_reset();
					build_hexhdr(ZRINIT, rh);
					tr_mark("< ZRINIT (after ZEOF, "
						"file accepted)");
					send_bytes(outbuf, outpos);
					rounds = 1000; /* leave the loop */
					break;
				case ZCAN:
				case ZABORT:
				case ZFERR:
					tr_mark("# sender aborted the "
						"data phase");
					rounds = 1000;
					break;
				case ZFIN:
					rh[0] = 0;
					rh[1] = 0;
					rh[2] = 0;
					rh[3] = 0;
					out_reset();
					build_hexhdr(ZFIN, rh);
					tr_mark("< ZFIN (close)");
					mut_send(mc, 1);
					rounds = 1000;
					break;
				default:
					if (fb.term != ZCRCE
					    && fb.term != ZCRCG
					    && fb.term != ZCRCQ
					    && fb.term != ZCRCW)
						break;
ack_subpacket:
					/* a data subpacket: the ack
					 * carries the file offset lsz has
					 * sent so far (header position +
					 * acked bytes); lsz's getinsync
					 * compares it with its bytes_sent
					 * and eats mismatched acks */
					if (fb.crc_ok) {
						ack_pos += fb.data_len;
						rh[0] = (unsigned char)
						    ack_pos;
						rh[1] = (unsigned char)
						    (ack_pos >> 8);
						rh[2] = (unsigned char)
						    (ack_pos >> 16);
						rh[3] = (unsigned char)
						    (ack_pos >> 24);
						out_reset();
						if (crc32)
							build_binhdr32(ZACK,
								       rh);
						else
							build_hexhdr(ZACK,
								     rh);
						tr_mark("< ZACK pos=%lu "
							"(term=%c)",
							ack_pos, fb.term);
					} else {
						/* corrupted subpacket:
						 * protocol answer is
						 * ZNAK; position unchanged */
						rh[0] = 0;
						rh[1] = 0;
						rh[2] = 0;
						rh[3] = 0;
						out_reset();
						build_hexhdr(ZNAK, rh);
						tr_mark("< ZNAK (bad "
							"subpacket CRC, "
							"term=%c)",
							fb.term);
					}
					mut_send(mc, 1);
					n_acked++;
					if (fb.type == ZDATA)
						data_phase_open = 1;
					break;
				}
			}
			tr_mark("# ACK-loop: %ld subpackets acked",
				n_acked);
			/* six CANs so lsz exits promptly instead of
			 * waiting out saybibi()'s 60s read timeout (the
			 * session was torn down mid-stream; the same
			 * trick keeps receive-mode M_TRUNC bounded) */
			{
				unsigned char cans[6] =
				    { CAN, CAN, CAN, CAN, CAN, CAN };
				tr_mark("# ACK-loop: cancelling");
				send_bytes(cans, sizeof cans);
			}
			break;
		}
		default: { /* ZRPOS offset 0: force restart, then skip */
			unsigned char h[4] = { 0, 0, 0, 0 };
			out_reset();
			build_hexhdr(ZRPOS, h);
			mut_send(mc, 1);
			read_frame(&rp, &fb, 3000);
			{
				unsigned char h2[4] = { 0, 0, 0, 0 };
				out_reset();
				build_hexhdr(ZSKIP, h2);
				mut_send(mc, 1);
				read_frame(&rp, &fb, 3000);
			}
			break;
		}
		}
	}
	(void)crc32;
}

/* --- per-iteration setup/teardown --- */

static void
setup_iteration(void)
{
	char cmd[1024];
	FILE *f;

	snprintf(fuzzdir, sizeof fuzzdir, "/tmp/zmodemfuzz-%ld", (long)getpid());
	snprintf(recvdir, sizeof recvdir, "%s/recv", fuzzdir);
	snprintf(canary, sizeof canary, "%s/escape-target.bin", fuzzdir);
	snprintf(canary_pristine, sizeof canary_pristine, "%s/cp", fuzzdir);
	snprintf(canary2, sizeof canary2, "%s/outside/escape-target.bin", fuzzdir);
	snprintf(canary2_pristine, sizeof canary2_pristine, "%s/cp2", fuzzdir);
	/* idempotent */
	mkdir(fuzzdir, 0777);
	{
		char cmd2[1100];
		snprintf(cmd2, sizeof cmd2, "rm -rf %s", recvdir);
		run_cmd(cmd2);
		mkdir(recvdir, 0777);
	}
	snprintf(transcript_name, sizeof transcript_name,
		 "%s/fuzz-trans-%ld-%ld.log", fuzzdir, seed, (long)getpid());
	transcript = fopen(transcript_name, "w");
	if (!transcript)
		fprintf(stderr, "zmodemfuzz: cannot open transcript %s: %s\n",
			transcript_name, strerror(errno));
	/* outside/: a real directory beside the receiving directory;
	 * recvdir/sub is a symlink to it, so a receiver that escapes the
	 * receiving directory via a symlinked directory lands in outside/
	 * and touches the canary there */
	snprintf(cmd, sizeof cmd, "mkdir -p %s/outside", fuzzdir);
	run_cmd(cmd);
	snprintf(cmd, sizeof cmd, "ln -sfn ../outside %s/sub 2>/dev/null", recvdir);
	run_cmd(cmd);

	/* payload-sized canary */
	f = fopen(canary, "wb");
	if (f) {
		fwrite(payload, 1, 1024, f);
		fclose(f);
	}
	f = fopen(canary_pristine, "wb");
	if (f) {
		fwrite(payload, 1, 1024, f);
		fclose(f);
	}
	f = fopen(canary2, "wb");
	if (f) {
		fwrite(payload, 1024, 1, f);
		fclose(f);
	}
	f = fopen(canary2_pristine, "wb");
	if (f) {
		fwrite(payload, 1024, 1, f);
		fclose(f);
	}
}

static void
teardown_iteration(void)
{
	char cmd[1100];
	if (transcript) {
		fclose(transcript);
		transcript = NULL;
	}
	/* persist the target's stderr next to the transcript; with -k it
	 * stays in the working dir, otherwise it is deleted again with
	 * the dir below (failures copy it to /tmp first, see fail()) */
	if (stderr_len) {
		char sp[1100];
		FILE *f;
		snprintf(sp, sizeof sp,
			 "%s/target-stderr-%ld-it%ld.log",
			 fuzzdir, seed, current_iteration);
		f = fopen(sp, "wb");
		if (f) {
			fwrite(stderr_capture, 1, stderr_len, f);
			fclose(f);
		}
	}
	if (keep_dirs)
		return; /* debugging: keep the working tree */
	snprintf(cmd, sizeof cmd, "rm -rf %s", fuzzdir);
	run_cmd(cmd);
}

/* check invariants after one iteration; returns string failure or NULL */
static const char *
check_invariants(int exitcode, int hang)
{
	/* distinct string literals per exit code so the fail-reason
	 * histogram can group by pointer identity */
	static char reason[256];

	if (hang)
		return "target hang (alarm)";
	if (exitcode < 0) {
		switch (-exitcode) {
		case 2: return "death by signal 2";
		case 4: return "death by signal 4";
		case 6: return "death by signal 6";
		case 7: return "death by signal 7";
		case 8: return "death by signal 8";
		case 11: return "death by signal 11";
		default: break;
		}
		snprintf(reason, sizeof reason, "death by signal %d", -exitcode);
		return reason;
	}
	/* exitcode 0/1/4/24/127/128+ are fine; anything else is suspicious.
	 * 127 is lrz's Security-Violation outcome: checkpath() rejects a
	 * hostile name (e.g. the M_NAME class sends "..") with
	 * bibi(-1) → fatal_error(128 + -1, ...) → exit(127). That is a
	 * correct, intended rejection, not a fuzzer finding.
	 * 4 is zmtx-zmrx-2.02's EXIT_TRANSFER_FAILED: its designed
	 * abort when a transfer cannot complete (the mutation classes
	 * corrupt exchanges by design - a receiver giving up then is
	 * like lrz's exit 1). A clean iteration (it % 17 == 0) must
	 * NEVER end in 4; that was the harness's missing trailing-NUL
	 * bug in build_zfile_data, fixed.
	 * 24 is zmtx-zmrx-1.02's "remote aborted": it counts >=5 CAN
	 * bytes as a receiver cancel and tears down (verified directly:
	 * 5 raw CANs on stdin → exit 24, no message; the harness's
	 * post-transfer 6-CAN close burst lands as a cancel even on a
	 * clean exchange). Same class as 4 - designed cancel, not a bug. */
	if (exitcode != 0 && exitcode != 1 && exitcode != 4
	    && exitcode != 24 && exitcode != 127 && exitcode < 0x80) {
		switch (exitcode) {
		case 2: return "odd exit code 2";
		case 3: return "odd exit code 3";
		case 5: return "odd exit code 5";
		case 101: return "odd exit code 101";
		case 126: return "odd exit code 126";
		case 128: return "odd exit code 128";
		default: break;
		}
		snprintf(reason, sizeof reason, "odd exit code %d", exitcode);
		return reason;
	}
	if (file_changed(canary, canary_pristine))
		return "parent-dir canary modified";
	if (file_changed(canary2, canary2_pristine))
		return "subdir canary modified";
	return NULL;
}

/* after a clean receive-mode exchange: SOME file in the receiving
 * directory (lrz may rename hostile names) must match the payload byte
 * for byte */
static const char *
	check_received(void)
{
	static unsigned char buf[131072];
	DIR *d;
	struct dirent *e;

	d = opendir(recvdir);
	if (!d)
		return "clean exchange: receiving directory vanished";
	while ((e = readdir(d)) != NULL) {
		char path[1024];
		int fd;
		ssize_t n;
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		snprintf(path, sizeof path, "%s/%s", recvdir, e->d_name);
		fd = open(path, O_RDONLY);
		if (fd < 0)
			continue;
		n = read(fd, buf, sizeof buf);
		close(fd);
		if ((size_t)n == payload_len
		    && memcmp(buf, payload, payload_len) == 0) {
			closedir(d);
			return NULL;
		}
	}
	closedir(d);
	return "clean exchange: no file matches the payload";
}

/* --- selftest: the clean exchange must always pass --- */

static int
selftest_receive(long it, int *exitcode)
{
	/* like fuzz_receive_iter but with mc = M_NONE and a fixed name */
	return fuzz_receive_iter(it, M_NONE, exitcode);
}

/* --- main loop --- */

static void
usage(void)
{
	fprintf(stderr,
"Usage: zmodemfuzz -m receive|send [-s SEED] [-n N] [-S N] [-t SEC] [--] TARGET [args...]\n"
"  -m receive  target is a receiver (lrz); fuzzer plays a mutating sender\n"
"  -m send     target is a sender (lsz); fuzzer plays a hostile receiver.\n"
"              Its file arguments are anchored to THIS process's cwd\n"
"              (relative paths like test/4k.bin work from anywhere)\n"
"  -s SEED     deterministic PRNG seed (default: time-based)\n"
"  -n N        iterations (default 200; FUZZITER overrides when -n is absent)\n"
"  -S N        fast-forward to iteration N: replay the PRNG draws of\n"
"              iterations 0..N-1 without running the target, then run\n"
"              iterations N..N-1+(-n N). NOTE: the draw sequence is\n"
"              only stable within one binary - changes to the mutation\n"
"              code invalidate older seeds' offsets\n"
"  -t SEC      per-iteration timeout in seconds (default 20). Keep this\n"
"              at 8s or more for exploratory runs: the receiver has a\n"
"              fixed 10s internal retry wait, and smaller values cut\n"
"              recovery paths short and make clean baselines flaky.\n"
"              Small values (1-5s) are fine for targeted -S replays.\n"
"  -k          keep the per-iteration working directory (debugging)\n"
"  -P, --progress  print a progress line (with elapsed ms) per iteration\n"
"  --selftest  run the clean-baseline exchange only\n\n"
"  A target that exits (or prints to stderr) before speaking ZMODEM is\n"
"  reported loudly, not silently skipped.\n");
	exit(2);
}

int
main(int argc, char *argv[])
{
	int i;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	init_crc_tables();

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-m") && i + 1 < argc) {
			if (!strcmp(argv[i + 1], "receive"))
				mode_receive = 1;
			else if (!strcmp(argv[i + 1], "send"))
				mode_receive = 0;
			else
				usage();
			i++;
		} else if (!strcmp(argv[i], "-s") && i + 1 < argc) {
			seed = strtol(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "-n") && i + 1 < argc) {
			iterations = strtol(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "-S") && i + 1 < argc) {
			start_iteration = strtol(argv[++i], NULL, 0);
			/* replays are for debugging a run of tens to
			 * thousands of iterations. Each fast-forwarded
			 * iteration costs ~200 PRNG draws, so a large
			 * -S spins for hours at 100% CPU with no alarm
			 * protection (observed with a seed-sized value).
			 * Die before the freeze instead. */
			if (start_iteration < 0 || start_iteration > 10000000)
				die("unreasonable -S start_iteration %ld "
				    "(replay range is small; did you mean "
				    "-s for the seed?)", start_iteration);
		} else if (!strcmp(argv[i], "-t") && i + 1 < argc) {
			alarm_seconds = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--selftest")) {
			do_selftest = 1;
		} else if (!strcmp(argv[i], "-k")) {
			keep_dirs = 1;
		} else if (!strcmp(argv[i], "-P") || !strcmp(argv[i], "--progress")) {
			show_progress = 1;
		} else if (!strcmp(argv[i], "--")) {
			i++;
			break;
		} else if (argv[i][0] == '-' && argv[i][1]) {
			usage();
		} else {
			break;
		}
	}
	if (i >= argc)
		usage();
	target_argc = 0;
	for (; i < argc && target_argc < 64; i++)
		target_argv[target_argc++] = argv[i];
	if (!target_argc)
		usage();
	/* the child chdir()s to the receiving directory in receive mode:
	 * make every target argument that names an executable OR a
	 * readable regular file a path relative to OUR cwd, so relative
	 * invocations keep working (this covers nested execs like
	 * zmodemsnif lrz and send-mode data files like test/4k.bin -
	 * in send mode the target's file arguments are relative to the
	 * harness's cwd, not the fuzz working directory). Absolute paths
	 * (leading '/') must NOT be touched: rewriting "/bin/true"
	 * produced "<cwd>//bin/true" and a confusing exit 127. */
	{
		static char abspath[68][4096];
		char cwd[2048];
		int ai;
		if (getcwd(cwd, sizeof cwd) != NULL) {
			for (ai = 0; ai < target_argc; ai++) {
				struct stat st;
				if (target_argv[ai][0] == '-'
				    || target_argv[ai][0] == '/')
					continue;
				/* readable regular files (lsz's data
				 * arguments like test/4k.bin) and
				 * executables (nested execs like
				 * zmodemsnif lrz) are both anchored
				 * to OUR cwd */
				if (stat(target_argv[ai], &st) == 0
				    && S_ISREG(st.st_mode)
				    && access(target_argv[ai], R_OK) == 0)
					; /* fall through to rewrite */
				else if (access(target_argv[ai], X_OK) != 0)
					continue;
				snprintf(abspath[ai], sizeof abspath[ai],
					 "%s/%s", cwd, target_argv[ai]);
				target_argv[ai] = abspath[ai];
			}
		}
	}
	if (mode_receive < 0)
		usage();
	if (iterations < 0) {
		const char *e = getenv("FUZZITER");
		iterations = e ? strtol(e, NULL, 0) : 200;
	}
	if (!seed)
		seed = (long)time(NULL) ^ ((long)getpid() << 16);
	rng_state = (unsigned long long)seed;
	if (!rng_state)
		rng_state = 88172645463325252ULL;
	fuzz_start = time(NULL);

	/* install on_alarm via sigaction WITHOUT SA_RESETHAND: signal()
	 * here came up one-shot (SA_RESETHAND visible in strace), so the
	 * SECOND alarm fired with SIG_DFL and killed the harness against
	 * targets that outlive one deadline (zmtx-zmrx-1.02). The handler
	 * must stay installed for the whole run. */
	{
		struct sigaction sa;
		memset(&sa, 0, sizeof sa);
		sa.sa_handler = on_alarm;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGALRM, &sa, NULL);
	}
	signal(SIGPIPE, SIG_IGN);

	tr_mark("# zmodemfuzz seed=%ld mode=%s iters=%ld target=%s",
		seed, mode_receive ? "receive" : "send", iterations,
		target_argv[0]);
	snprintf(fuzzdir, sizeof fuzzdir, "/tmp/zmodemfuzz-%ld", (long)getpid());

	{
		long it;
		int failures = 0;
	if (start_iteration > 0) {
		if (start_iteration >= iterations)
			iterations = start_iteration + 1;
		fast_forward_rng(start_iteration);
		tr_mark("# fast-forwarded rng to iteration %ld",
			start_iteration);
	}
		for (it = start_iteration; it < iterations; it++) {
			int exitcode = 0;
			const char *bad = NULL;
			struct timespec ts_start;
			struct timespec ts_end;
			struct class_stats *st = NULL;
			int hang;
			long ms;
			current_iteration = it;
			setup_iteration();
			build_payload();
			clock_gettime(CLOCK_MONOTONIC, &ts_start);
			if (pipe(to_target) < 0 || pipe(from_target) < 0
			    || pipe(target_stderr) < 0)
				die("pipe: %s", strerror(errno));
			spawn_target();
			stderr_len = 0;
			rxbuf_reset();
			/* a previous iteration's EPIPE may have poisoned the
			 * write path: clear the flag and any bytes that were
			 * buffered when the old target went away, so the new
			 * target starts with a clean output stream */
			target_dead = 0;
			to_target_buf.len = 0;
			iter_crc32 = 0; /* set by fuzz_receive_iter if CRC32 negotiated */
			if (mode_receive) {
				if (do_selftest) {
					send_choice = -1;
					selftest_receive(it, &exitcode);
				} else {
					enum mclass mc =
					    (enum mclass)(rng_below(N_MCLASS - 1) + 1);
					if (it % 17 == 0)
						mc = M_NONE; /* keep a clean baseline */
					fuzz_receive_iter(it, mc, &exitcode);
					st = &rx_stats[mc];
					/* the harness has finished its part
					 * of the exchange; the target may
					 * still linger (e.g. waiting for
					 * the next ZRQINIT after a ZSKIP
					 * or for a sender that never
					 * resumes). Close the write end so
					 * the target sees EOF and exits
					 * instead of burning the alarm. */
					if (to_target[1] >= 0) {
						close(to_target[1]);
						to_target[1] = -1;
					}
				}
		} else {
			fuzz_send_iter();
			exitcode = 0;
			st = &tx_stats[send_choice >= 0
				       && send_choice < 13
				       ? send_choice : 0];
			/* same as receive mode: the harness has finished
			 * its part; close the write end so the target
			 * (lsz) sees EOF and exits instead of burning
			 * the alarm (e.g. inside saybibi()'s 60s read) */
			if (to_target[1] >= 0) {
				close(to_target[1]);
				to_target[1] = -1;
			}
		}
			{
				int w = wait_target(&exitcode);
				int clean = (mode_receive
					     && (it % 17 == 0 || do_selftest));
				hang = (w < 0);
				if (hang) {
					/* hang: for mutation iterations an
					 * exchange that deliberately breaks
					 * mid-stream can leave the receiver
					 * waiting for data that never comes -
					 * that is the mutation working, not a
					 * target bug. Only signal deaths,
					 * canary violations and clean-exchange
					 * failures are bugs. */
					if (!clean) {
						tr_mark("# target waited: "
							"expected after a "
							"mid-stream mutation");
						failures += 0;
						if (st)
							st->timeouts++;
					} else {
						fail("hang", it, exitcode);
						failures++;
						if (st)
							st->timeouts++;
					}
				} else {
					bad = check_invariants(exitcode, 0);
	if (bad && exitcode == -14) {
		/* the child's alarm fired: a
		 * mutation (or, in send mode,
		 * every iteration is one)
		 * broke the exchange mid-stream
		 * by design and the target
		 * waited - expected */
		bad = NULL;
		/* still bookkeep the budget burn */
		if (st)
			st->timeouts++;
	}
	if (!bad && clean)
		bad = check_received();
	if (!bad && !clean && longpkt_completed())
		bad = check_received();
	if (bad) {
		fail(bad, it, exitcode);
		failures++;
		if (st)
			note_fail_reason(bad, mode_receive,
					 mode_receive
					     ? (long)(st - rx_stats)
					     : send_choice < 13
						   ? send_choice : 0);
	}
				}
			}
			close_pipes();
			teardown_iteration();
			clock_gettime(CLOCK_MONOTONIC, &ts_end);
			ms = (ts_end.tv_sec - ts_start.tv_sec)
			    * 1000L
			    + (ts_end.tv_nsec - ts_start.tv_nsec)
			    / 1000000L;
			/* per-class statistics (selftest runs are skipped:
			 * they have no class) */
			if (st) {
				st->count++;
				st->total_ms += ms;
				if (ms > st->max_ms)
					st->max_ms = ms;
				if (hang)
					st->hangs++;
				if (bad)
					st->fails++;
				if (iter_crc32)
					st->crc32++;
			}
			if (show_progress) {
				fprintf(stderr,
					"zmodemfuzz: %ld/%ld iters, "
					"%d failures, %ld ms\n",
					it + 1, iterations, failures,
					ms);
			} else if ((it % 20) == 19) {
				fprintf(stderr,
					"zmodemfuzz: %ld/%ld iters, "
					"%d failures\n",
					it + 1, iterations, failures);
			}
		}
		fprintf(stderr, "zmodemfuzz: done, %ld iterations, %d failures\n",
			iterations - start_iteration, failures);
		/* per-class summary: one row per mutation class (receive
		 * mode) or hostile choice (send mode) */
		{
			const char *names[N_MCLASS] = {
				"NONE", "BYTEFLIP", "CRC16", "CRC32",
				"CRCMODE", "FRAMETYPE", "ZDLE_ILL",
				"POSITION", "FLAGS", "GARBAGE", "CANCAN",
				"TRUNC", "EOF", "NAME", "LONGPKT"
			};
			const char *choice_names[13] = {
				"silence", "ZNAK", "ZSKIP", "ZABORT",
				"ZFERR", "ZCRC+skip", "ZCHALLENGE",
				"ZRPOS-max", "CAN*9", "ZRINIT-storm",
				"ZFIN", "ACK-loop", "ZRPOS-0+skip"
			};
			long ci;
			fprintf(stderr,
				"zmodemfuzz: class summary "
				"(seed=%ld mode=%s t=%ds target=%s)\n",
				seed, mode_receive ? "receive" : "send",
				alarm_seconds, target_argv[0]);
			fprintf(stderr, "zmodemfuzz: "
				"(count, avg ms, max ms, hangs, timeouts"
				", fails%s)\n",
				mode_receive ? ", crc32" : "");
			if (mode_receive) {
				for (ci = 0; ci < N_MCLASS; ci++) {
					if (!rx_stats[ci].count)
						continue;
					fprintf(stderr,
						"  %-9s %6ld %7ld %7ld"
						" %5ld %5ld %5ld %5ld\n",
						names[ci],
						rx_stats[ci].count,
						rx_stats[ci].total_ms
						    / rx_stats[ci].count,
						rx_stats[ci].max_ms,
						rx_stats[ci].hangs,
						rx_stats[ci].timeouts,
						rx_stats[ci].fails,
						rx_stats[ci].crc32);
					{
						int r;
						for (r = 0; r < N_FAIL_REASONS
							     && fail_reasons[ci][r].what;
						     r++)
							fprintf(stderr,
								"    ^ %ldx %s\n",
								fail_reasons[ci][r]
								    .count,
								fail_reasons[ci][r]
								    .what);
					}
				}
			} else {
				for (ci = 0; ci < n_tx_choices; ci++) {
					if (!tx_stats[ci].count)
						continue;
					fprintf(stderr,
						"  choice %2ld %-15s"
						" %6ld %7ld %7ld"
						" %5ld %5ld %5ld\n",
						ci, choice_names[ci],
						tx_stats[ci].count,
						tx_stats[ci].total_ms
						/ tx_stats[ci].count,
						tx_stats[ci].max_ms,
						tx_stats[ci].hangs,
						tx_stats[ci].timeouts,
						tx_stats[ci].fails);
					{
						int r;
						for (r = 0; r < N_FAIL_REASONS
							     && fail_reasons_tx[ci][r].what;
						     r++)
							fprintf(stderr,
								"    ^ %ldx %s\n",
								fail_reasons_tx[ci][r]
								    .count,
								fail_reasons_tx[ci][r]
								    .what);
					}
				}
			}
		}
		return failures ? 1 : 0;
	}
}

bin_PROGRAMS=lrz lsz
COMMON_SOURCES=timing.c zperr.c zreadline.c crctab.c rbsb.c zm.c protname.c lsyslog.c canit.c stats.c util.c
lrz_SOURCES=lrz.c $(COMMON_SOURCES)
lsz_SOURCES=lsz.c $(COMMON_SOURCES)
noinst_HEADERS = timing.h zglobal.h zmodem.h stats.h util.h crctab.h
noinst_PROGRAMS = zmodemsnif linesimulator timestamp zmodemfuzz

LDADD = @LIBINTL@
AM_CPPFLAGS = -I.. -I$(srcdir) -I$(top_srcdir)/src -DLOCALEDIR=\"$(localedir)\" -D_POSIX_C_SOURCE=@MIN_POSIX_C_STD@
AM_CFLAGS = @WARN_CFLAGS@ @LRZSZ_CFLAGS@
# clang development aid: to enable nearly every clang warning, move the
# following line into AM_CFLAGS above (removing the leading '#'); the
# -Wno-* list mutes the warnings that are pure noise for this code base.
#-Weverything -Wno-disabled-macro-expansion -Wno-reserved-macro-identifier -Wno-used-but-marked-unused -Wno-padded -Wno-unsafe-buffer-usage -Wno-implicit-int-conversion -Wno-sign-conversion -Wno-implicit-fallthrough -Wno-unreachable-code-break -Wno-implicit-int-float-conversion -Wno-float-equal -Wno-covered-switch-default -Wno-unused-macros
AM_LDFLAGS = @LRZSZ_LDFLAGS@

# The hard links below point at the installed binaries, so those must exist
# first. Order it explicitly, otherwise FTBFS with make --shuffle=reverse.
install-exec-local: install-binPROGRAMS
	rm -f $(DESTDIR)/$(bindir)/`echo lsb | sed -e '$(transform)'`
	ln $(DESTDIR)/$(bindir)/`echo lsz |sed -e '$(transform)'` \
		$(DESTDIR)/$(bindir)/`echo lsb |sed -e '$(transform)'`
	rm -f $(DESTDIR)/$(bindir)/`echo lsx | sed -e '$(transform)'`
	ln $(DESTDIR)/$(bindir)/`echo lsz |sed -e '$(transform)'` \
		$(DESTDIR)/$(bindir)/`echo lsx |sed -e '$(transform)'`
	rm -f $(DESTDIR)/$(bindir)/`echo lrb | sed -e '$(transform)'`
	ln $(DESTDIR)/$(bindir)/`echo lrz |sed -e '$(transform)'` \
		$(DESTDIR)/$(bindir)/`echo lrb |sed -e '$(transform)'`
	rm -f $(DESTDIR)/$(bindir)/`echo lrx | sed -e '$(transform)'`
	ln $(DESTDIR)/$(bindir)/`echo lrz |sed -e '$(transform)'` \
		$(DESTDIR)/$(bindir)/`echo lrx |sed -e '$(transform)'`

uninstall-local:
	rm -f $(DESTDIR)/$(bindir)/`echo lsb |sed -e '$(transform)'`
	rm -f $(DESTDIR)/$(bindir)/`echo lsx |sed -e '$(transform)'`
	rm -f $(DESTDIR)/$(bindir)/`echo lrb |sed -e '$(transform)'`
	rm -f $(DESTDIR)/$(bindir)/`echo lrx |sed -e '$(transform)'`

TIDY_FILES = $(COMMON_SOURCES) lrz.c lsz.c \
	zmodemsnif.c linesimulator.c timestamp.c zmodemfuzz.c
tidy: $(TIDY_FILES) $(noinst_HEADERS)
	clang-tidy --checks='-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling' $(TIDY_FILES) -- $(AM_CPPFLAGS) $(AM_CFLAGS)

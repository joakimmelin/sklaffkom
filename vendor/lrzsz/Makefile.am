SUBDIRS = src po man
EXTRA_DIST = m4/ChangeLog check.lrzsz COMPATABILITY README.cvs README.isdn4linux \
	README.tests remote-build-check.sh \
	doc/README doc/zmodem-forsberg.txt doc/zmodem-zmtx-zmrx.txt doc/zmodem-wip.txt doc/zmodemsnif.txt

PR=@DIST_TARBALL@
DISTCLEANFILES=fastcheck.done
CLEANFILES=fastcheck.done
ACLOCAL_AMFLAGS = -I m4
QUICKCHECKS=xmodem-lsz-lrz-16k-binary xmodem-lsz-lrz-large-binary-1k-crc zmodem-lsz-lrz-MGMT-default-nonexisting zmodem-lsz-lrz-MGMT-ZMCRC-r \
	zmodem-lsz-lrz-multiple zmodem-lsz-lrz-4k zmodem-lsz-lrz-8k-turbo zmodem-lsz-lrz-MGMT-mode-0666 \
	zmodem-lsz-lrz-MGMT-traversal-dotdot zmodem-lsz-lrz-RESUME-shrink

# Note: the inofficial target is to compile on systems of up to 20 years of age.
# We support one C standard older than that.
#
SUPPORTED_C_STANDARDS=@SUPPORTED_C_STDS@
SUPPORTED_COMPILERS=gcc clang
buildcheck:
	for cc in $(SUPPORTED_COMPILERS) ; do if command -v "$$cc" >/dev/null ; then \
		for std in $(SUPPORTED_C_STANDARDS) ; do \
			echo "running clean/configure/make with CC=$$cc --enable-std=$$std"; \
			make -s V=0 clean >/dev/null ; \
			./configure CC="$$cc" -q --enable-std=$$std || exit 1 ; \
			make -s V=0 -j 4 || exit 1 ; \
		done \
	fi ; done ; \
        make -s V=0 clean >/dev/null
buildcheck+:
	for cc in $(SUPPORTED_COMPILERS) ; do if command -v "$$cc" >/dev/null ; then \
		for std in $(SUPPORTED_C_STANDARDS) ; do \
			echo "running clean/configure/make/vcheck with CC=$$cc --enable-std=$$std"; \
			make -s V=0 clean >/dev/null ; \
			./configure CC="$$cc" -q --enable-std=$$std || exit 1 ; \
			make -s V=0 -j 4 || exit 1 ; \
			QUIET=1 make -s V=0 vcheck || exit 1 ; \
		done \
	fi ; done ; \
        make -s V=0 clean >/dev/null ;

# Remote build checks (gnulib cfg.mk convention): the host list lives in a
# local, non-distributed cfg.mk, e.g.:
#	REMOTE_HOSTS = uwe@openbsd uwe@dev
-include cfg.mk

buildcheck-remote: dist
	@for h in $(REMOTE_HOSTS) ; do \
		echo "remote buildcheck on $$h"; \
		scp ./remote-build-check.sh $(PR).tar.gz "$$h": || exit 1; \
		ssh "$$h" sh ./remote-build-check.sh $(PR) || exit 1; \
	done
.PHONY: buildcheck-remote

oldcheck-local:
	QUIET=-q $(srcdir)/check.lrzsz $(srcdir) `pwd`

quickcheck:
	$(srcdir)/check.lrzsz $(srcdir) `pwd` select $(QUICKCHECKS)

vcheck:
	$(srcdir)/check.lrzsz $(srcdir) `pwd`
vcheck-%:
	$(srcdir)/check.lrzsz $(srcdir) `pwd`	$(subst vcheck-,,$@)
vcheck-x:
	$(srcdir)/check.lrzsz $(srcdir) `pwd`	x
vcheck-y:
	$(srcdir)/check.lrzsz $(srcdir) `pwd`	y
vcheck-z:
	$(srcdir)/check.lrzsz $(srcdir) `pwd` z

# need clang compiler options for clang-tidy.
clangconfigure:
	CC=clang ./configure -q
tidy:
	@$(MAKE) -C src tidy
.PHONY: tiny

# releasecheck runs about 2h.
releasecheck: distclean clangconfigure all tidy maintainertools dist distcheck vcheck qa buildcheck+ buildcheck-remote fuzz1000

# need clang compiler options for clang-tidy.
releasecheck1: distclean
	CC=clang ./configure -q
	@$(MAKE) 
maintainertools: 
	@$(MAKE) -C src zmodemfuzz zmodemsnif timestamp linesimulator

# zmodem protocol fuzzing: 25 iterations per mode (FUZZITER overrides), each
# iteration is alarm limited (-t 20).
# fails if the target crashes, is killed by a signal (!= timeout ALRM), 
# escapes the receiving directory, or transfers some wrong content.
# -P prints one progress line per iteration.
fuzz:
	@$(MAKE) -C src zmodemfuzz lrz lsz; \
	$(srcdir)/src/zmodemfuzz -P -m receive -n $${FUZZITER:-25} -t 20 -- ./src/lrz -q -Z; \
	$(srcdir)/src/zmodemfuzz -P -m send -n $${FUZZITER:-25} -t 20 -- ./src/lsz -q -Z ./test/4k.bin

fuzz1000:
	@$(MAKE) -C src zmodemfuzz lrz lsz; \
	$(srcdir)/src/zmodemfuzz -P -m receive -n 1000 -t 20 -- ./src/lrz -q -Z; \
	$(srcdir)/src/zmodemfuzz -P -m send -n 1000 -t 20 -- ./src/lsz -q -Z ./test/4k.bin

.PHONY: fuzz fuzz1000

qa: asan msan ubsan coverage

.PHONY: $(SANITIZERS) qa coverage

SANITIZERS = asan msan ubsan
$(SANITIZERS): CC_LIST = clang gcc
msan: CC_LIST = clang
$(SANITIZERS):
	for i in $(CC_LIST) ; do if command -v "$$i" >/dev/null ; then \
		echo "running clean/configure/vcheck with $$i --enable-$@"; \
		make -s V=0 clean >/dev/null ; \
		./configure CC="$$i" -q --enable-$@ || exit 1 ; \
		make -s V=0 -j 4 || exit 1 ; \
		QUIET=1 make -s V=0 vcheck || exit 1 ; \
	fi ; done

coverage: coverage/coverage.html
coverage/coverage.html: $(wildcard src/*.c src/*.h)
	mkdir coverage 2>/dev/null || exit 0
	make -s V=0 clean
	./configure --enable-coverage -q
	make -s V=0 -j 4
	make -s V=0 vcheck
	if command -v "gcovr" >/dev/null; then gcovr --html-details --output coverage/coverage.html src/; else echo "gcovr not found, no coverage report"; fi
	./configure -q
	make -s V=0 clean
	make -s V=0 -j 4


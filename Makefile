#
# Makefile for SklaffKOM
#

SKLAFFUSER=sklaff
SKLAFFBIN=/usr/local/bin
SKLAFFDIR=/usr/local/sklaff

# Define target system, one of: FREEBSD, SOLARIS, LINUX
SKLAFFSYS = FREEBSD	# FreeBSD
#SKLAFFSYS = SOLARIS	# Solaris
#SKLAFFSYS = LINUX	# Linux

CC=gcc
SKLAFFFLAGS = -D$(SKLAFFSYS) -DSKLAFFDIR=\"$(SKLAFFDIR)\" -DSKLAFFBIN=\"$(SKLAFFBIN)\"
CFLAGS = $(SKLAFFFLAGS) -O2 -g -pipe -Wall -Werror

# uncomment for SYSV
#LIBS=-lc_s -lsklaff -ltermcap -lcposix -linet -lm

# uncomment for DG_UX
#LIBS=-lsklaff -ltermcap -lelf -lm

# uncomment for UNIXWARE
#LIBS=-lsklaff -ltermcap -lelf -lm

#uncomment for BSD
LIBS=-lsklaff -lm

# uncomment for LINUX/SUNOS/ULTRIX
# debian users : sudo apt install libbsd-dev
LIBS=-lsklaff -lbsd -lm

# uncomment for SOLARIS
#CFLAGS=-g -I/usr/ucbinclude
#LIBS=-L/usr/ucblib -lm -lsklaff -ltermcap -lucb -lsocket -lnsl -lelf -laio

#
# Change nothing below this line
#

OBJS = \
	admin.o \
	buf.o \
	commands.o \
	command_list.o \
	conf.o \
	edit.o \
	file.o \
	flag.o \
	msg.o \
	parse.o\
	survey.o \
	text.o \
	user.o \
	version.o
KOMOBJ=sklaffkom.o
ADMOBJ=sklaffadm.o
WHOOBJ=sklaffwho.o
ACCTOBJ=sklaffacct.o
MTOSSOBJ=mailtoss.o
NTOSSOBJ=newstoss.o
SURVREPOBJ=survreport.o
FTCOBJ=forwardtoconf.o
FTYOBJ=forwardtoyell.o

SKLAFFLIB=lib/libsklaff.a

#newstoss was missing below, added 2025-07-14
all: $(SKLAFFLIB) sklaffkom sklaffadm sklaffacct newstoss mailtoss survreport sklaffwho

$(OBJS): sklaff.h ext_globals.h struct.h lang.h
$(KOMOBJ): sklaff.h globals.h struct.h lang.h
$(ADMOBJ): sklaff.h globals.h struct.h lang.h
$(WHOOBJ): sklaff.h globals.h struct.h lang.h
$(ACCTOBJ): sklaff.h globals.h struct.h lang.h
$(MTOSSOBJ): sklaff.h globals.h struct.h lang.h
$(NTOSSOBJ): sklaff.h globals.h struct.h lang.h
$(SURVREPOBJ): sklaff.h globals.h struct.h lang.h
$(FTCOBJ): sklaff.h globals.h struct.h lang.h
$(FTYOBJ): sklaff.h globals.h struct.h lang.h

sklaffkom: $(SKLAFFLIB) $(KOMOBJ) $(OBJS)
	$(CC) -g -o sklaffkom $(KOMOBJ) $(OBJS) -Llib $(LIBS)
	#chmod u+s sklaffkom
	#chown $(SKLAFFUSER) sklaffkom

sklaffadm: $(SKLAFFLIB) $(ADMOBJ) $(OBJS)
	$(CC) -o sklaffadm $(ADMOBJ) $(OBJS) -Llib $(LIBS)
	strip sklaffadm
	chmod og-rwx sklaffadm

sklaffwho: $(SKLAFFLIB) $(WHOOBJ) $(OBJS)
	$(CC) -o sklaffwho $(WHOOBJ) $(OBJS) -Llib $(LIBS)
	strip sklaffwho
	#chmod u+s sklaffwho

sklaffacct: $(SKLAFFLIB) $(ACCTOBJ) $(OBJS)
	$(CC) -o sklaffacct $(ACCTOBJ) $(OBJS) -Llib $(LIBS)
	strip sklaffacct
	#chmod u+s sklaffacct

mailtoss: $(SKLAFFLIB) $(MTOSSOBJ) $(OBJS)
	$(CC) -o mailtoss $(MTOSSOBJ) $(OBJS) -Llib $(LIBS)
#	strip mailtoss

newstoss: $(SKLAFFLIB) $(NTOSSOBJ) $(OBJS)
	$(CC) -o newstoss $(NTOSSOBJ) $(OBJS) -Llib $(LIBS)
#	strip newstoss

survreport: $(SKLAFFLIB) $(SURVREPOBJ) $(OBJS)
	$(CC) -o survreport $(SURVREPOBJ) $(OBJS) -Llib $(LIBS)
	strip survreport

forwardtoconf: $(SKLAFFLIB) $(FTCOBJ) $(OBJS)
	$(CC) -o forwardtoconf $(FTCOBJ) $(OBJS) -Llib $(LIBS)
	strip forwardtoconf

forwardtoyell: $(SKLAFFLIB) $(FTYOBJ) $(OBJS)
	$(CC) -o forwardtoyell $(FTYOBJ) $(OBJS) -Llib $(LIBS)
	strip forwardtoyell

version.c:
	@./version.sh

$(SKLAFFLIB):
	(cd lib; $(MAKE) CC=$(CC) CFLAGS='$(CFLAGS) -I..')

install: sklaffkom sklaffadm sklaffacct survreport sklaffwho newstoss
	@echo Making libraries
	-mkdir $(SKLAFFDIR)
	-mkdir $(SKLAFFDIR)/etc
	-mkdir $(SKLAFFDIR)/log
	-mkdir $(SKLAFFBIN)
	@echo Installing SklaffKOM
	-mv $(SKLAFFBIN)/sklaffkom $(SKLAFFBIN)/sklaffkom.old
	cp sklaffkom sklaffadm sklaffwho sklaffacct $(SKLAFFBIN)/
	cp survreport $(SKLAFFBIN)/srep
	-chown $(SKLAFFUSER) $(SKLAFFBIN)/sklaffkom $(SKLAFFBIN)/sklaffadm $(SKLAFFBIN)/sklaffwho \
		$(SKLAFFBIN)/sklaffacct $(SKLAFFDIR)/log
	-chown root   $(SKLAFFBIN)/srep
	chmod u+s $(SKLAFFBIN)/sklaffkom $(SKLAFFBIN)/sklaffacct $(SKLAFFBIN)/sklaffwho
	chmod og-rxw $(SKLAFFBIN)/sklaffadm
	-chmod 4755   $(SKLAFFBIN)/srep
	cp newstoss mailtoss $(SKLAFFDIR)/etc
	cp newstoss $(SKLAFFDIR)/etc/ntoss
	cp mailtoss $(SKLAFFDIR)/etc/mtoss
	# Better copy mailtoss and newstoss to sklaffbin also
	cp mailtoss $(SKLAFFBIN)
	cp newstoss $(SKLAFFBIN)
	-chown root $(SKLAFFDIR)/etc/newstoss $(SKLAFFDIR)/etc/ntoss $(SKLAFFDIR)/etc/mailtoss $(SKLAFFDIR)/etc/mtoss
	-chmod og-rxw $(SKLAFFDIR)/etc/newstoss $(SKLAFFDIR)/etc/ntoss $(SKLAFFDIR)/etc/mailtoss $(SKLAFFDIR)/etc/mtoss
installdb:
	@echo Installing datafiles
	-mkdir $(SKLAFFDIR)
	chown $(SKLAFFUSER) $(SKLAFFDIR)
	chmod og-rwx $(SKLAFFDIR)
	-rm -rf $(SKLAFFDIR)/db
	mkdir $(SKLAFFDIR)/db
	chown $(SKLAFFUSER) $(SKLAFFDIR)/db
	chmod og-rwx $(SKLAFFDIR)/db
	-rm -rf $(SKLAFFDIR)/user
	mkdir $(SKLAFFDIR)/user
	chown $(SKLAFFUSER) $(SKLAFFDIR)/user
	chmod og-rwx $(SKLAFFDIR)/user
	-rm -rf $(SKLAFFDIR)/mbox
	mkdir $(SKLAFFDIR)/mbox
	chown $(SKLAFFUSER) $(SKLAFFDIR)/mbox
	chmod og-rwx $(SKLAFFDIR)/mbox
	-rm -rf $(SKLAFFDIR)/files
	mkdir $(SKLAFFDIR)/files
	chown $(SKLAFFUSER) $(SKLAFFDIR)/files
	chmod og-rwx $(SKLAFFDIR)/files
	-rm -rf $(SKLAFFDIR)/etc
	mkdir $(SKLAFFDIR)/etc
	chown $(SKLAFFUSER) $(SKLAFFDIR)/etc
	chmod og-rwx $(SKLAFFDIR)/etc
	mkdir $(SKLAFFDIR)/etc/help.swe
	chown $(SKLAFFUSER) $(SKLAFFDIR)/etc/help.swe
	chmod og-rxw $(SKLAFFDIR)/etc/help.swe
	mkdir $(SKLAFFDIR)/etc/help.eng
	chown $(SKLAFFUSER) $(SKLAFFDIR)/etc/help.eng
	chmod og-rxw $(SKLAFFDIR)/etc/help.eng
	-cp etc/* $(SKLAFFDIR)/etc/
	cp etc/help.swe/* $(SKLAFFDIR)/etc/help.swe/
	cp etc/help.eng/* $(SKLAFFDIR)/etc/help.eng/
	chown $(SKLAFFUSER) $(SKLAFFDIR)/etc/*
	chown $(SKLAFFUSER) $(SKLAFFDIR)/etc/help.swe/*
	chown $(SKLAFFUSER) $(SKLAFFDIR)/etc/help.eng/*
	chmod og-rxw $(SKLAFFDIR)/etc/*
	chmod og-rxw $(SKLAFFDIR)/etc/help.swe/*
	chmod og-rxw $(SKLAFFDIR)/etc/help.eng/*

clean:
	(cd lib; make clean)
	-rm -f *.o *.a \#* *~ core sklaffkom sklaffadm sklaffacct mailtoss newstoss survreport sklaffwho version.c

# distrib is rewritten 2025-08-16 by PL to work in our modern times

distrib:
	echo 0 > .compile
	@BASENAME=$$(basename "$$PWD"); \
	DISTDIR="./dist"; \
	LIST="/tmp/sklaff.$$"; \
	mkdir -p "$$DISTDIR"; \
	( \
	  echo "Updating version.c from version.sh..."; \
	  sh "version.sh" >/dev/null 2>&1 || { echo "version.sh failed!"; exit 1; }; \
	  find . -type f \
	    ! -name '*.o' ! -name '*.a' ! -name '*~' \
	    \( ! -perm /111 -o -name 'version.sh' \) \
	    -print | sort > "$$LIST"; \
	  RAWV=$$(sh "version.sh" -s 2>/dev/null | awk '{print $$NF}' || echo unknown); \
	  PRETTYV=$$(printf '%s' "$$RAWV" | sed -E 's/#.*//' | sed -E 's/\([^)]+\)//g' | sed -E 's/([0-9]+)\.([0-9]+)b([0-9]+)/\1.\2-beta\3/'); \
	  SAFEV=$$(printf '%s' "$$PRETTYV" | tr -cd 'A-Za-z0-9._+-'); \
	  PKG="sklaff-$${SAFEV}"; \
	  echo "Version string from version.sh: $$RAWV"; \
	  echo "Filename-safe version: $$SAFEV"; \
	  if command -v compress >/dev/null 2>&1; then \
	    tar -cf - --files-from "$$LIST" | compress > "$$DISTDIR/$$PKG.tz"; \
	  else \
	    echo "Note: 'compress' not found; skipping .tz"; \
	  fi; \
	  tar -cf - --files-from "$$LIST" | gzip -9c > "$$DISTDIR/$$PKG.tgz"; \
	  zip -q -@ "$$DISTDIR/$$PKG.zip" < "$$LIST"; \
	  rm -f "$$LIST"; \
	  echo "Done. Created files in $$DISTDIR:"; \
	  ls -l "$$DISTDIR"/$$PKG.* 2>/dev/null || true \
	)


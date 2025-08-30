#
# Makefile for libsklaff.a
#

OBJS=tty_raw.o tty_reset.o sig_setup.o sig_reset.o critical.o non_critical.o\
lock.o unlock.o input.o input.o output.o down_string.o up_string.o\
time_string.o sys_error.o file_size.o rawio.o user_dir.o cmp_strings.o\
copy_file.o wc.o rtrim.o ltrim.o parse_string.o file_exists.o hostname.o\
sort_util.o prog_name.o mbox_dir.o compat.o helpers.o

#
# Include for debugging
#
# memory.o

all: libsklaff.a

$(OBJS): ../sklaff.h ../ext_globals.h ../struct.h ../lang.h

libsklaff.a: $(OBJS)  ../sklaff.h ../ext_globals.h ../struct.h ../lang.h
	ar cr libsklaff.a $(OBJS)
	-ranlib libsklaff.a

clean:
	-rm -f *.o \#* *~ core libsklaff.a

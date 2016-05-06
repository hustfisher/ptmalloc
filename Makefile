# Makefile for multi-thread malloc test
# by Wolfram Gloger 1996-1999

DIST_FILES = COPYRIGHT README ChangeLog Makefile \
 thread-m.h ptmalloc.h ptmalloc.c \
 lran2.h t-test.h t-test1.c t-test2.c
DIST_FILES2 = $(DIST_FILES) RCS/*,v

CC = cc

OPT_FLAGS  = -O #-g
WARN_FLAGS = # -Wall
SH_FLAGS   = -shared

# Flags for the test programs
T_FLAGS   = -DUSE_MALLOC=1 -DMALLOC_HOOKS -DTEST=1

# Flags for the compilation of ptmalloc.c
M_FLAGS   = -DTHREAD_STATS=1 -DMALLOC_HOOKS #-DMALLOC_DEBUG=1

# Flags for the threads package configuration.  Adjust this for your
# architecture, or see the platform-specific targets below.
# Valid discerning symbols are:
# USE_PTHREADS=1 for Posix threads
# USE_THR=1      for Solaris threads
# USE_SPROC=1    for SGI sprocs
THR_FLAGS = -DUSE_PTHREADS=1 -DUSE_TSD_DATA_HACK -D_REENTRANT
THR_LIBS  = -lpthread

RM        = rm -f

# Don't need it for the Linux C library 6, see `glibc' target below.
MALLOC = ptmalloc.o

T_SUF =
TESTS = t-test1$(T_SUF) t-test2$(T_SUF)

CFLAGS = $(OPT_FLAGS) $(WARN_FLAGS) $(THR_FLAGS)

.c.o:
	$(CC) -c $(CFLAGS) $<

all: $(TESTS)

shared: ptmalloc.so

ptmalloc.o: ptmalloc.c ptmalloc.h thread-m.h
	$(CC) -c $(CFLAGS) $(M_FLAGS) $<

ptmalloc.so: ptmalloc.c ptmalloc.h thread-m.h
	$(CC) $(SH_FLAGS) $(CFLAGS) $(M_FLAGS) $< -o $@

again:
	$(RM) $(TESTS)
	$(MAKE) $(TESTS)

clean:
	$(RM) ptmalloc.o ptmalloc.so $(TESTS) core

t-test1$(T_SUF): t-test1.c t-test.h thread-m.h $(MALLOC)
	$(CC) $(CFLAGS) $(T_FLAGS) t-test1.c $(MALLOC) $(THR_LIBS) -o $@

t-test2$(T_SUF): t-test2.c t-test.h thread-m.h $(MALLOC)
	$(CC) $(CFLAGS) $(T_FLAGS) t-test2.c $(MALLOC) $(THR_LIBS) -o $@

# Platform-specific targets. The ones ending in `-libc' are provided
# to enable comparison with the standard malloc implementation from
# the system's native C library.  The option USE_TSD_DATA_HACK is now
# the default for pthreads systems, as most (Irix 6, Solaris 2) seem
# to need it.  Try with USE_TSD_DATA_HACK undefined only if you're
# confident that your systems's thread specific data functions do not
# use malloc themselves.

posix:
	$(MAKE) THR_FLAGS='-DUSE_PTHREADS=1 -DUSE_TSD_DATA_HACK -D_REENTRANT' \
 THR_LIBS=-lpthread

posix-with-tsd:
	$(MAKE) THR_FLAGS='-DUSE_PTHREADS=1 -D_REENTRANT' THR_LIBS=-lpthread

posix-libc:
	$(MAKE) THR_FLAGS='-DUSE_PTHREADS=1 -D_REENTRANT' THR_LIBS=-lpthread \
	 MALLOC= T_FLAGS= T_SUF=-libc

glibc:
	$(MAKE) THR_FLAGS=-DUSE_PTHREADS=1 MALLOC=

linux-pthread:
	$(MAKE) CC='gcc -D_GNU_SOURCE=1' WARN_FLAGS='-Wall' OPT_FLAGS='-O2' \
	 THR_FLAGS='-DUSE_PTHREADS=1 -DUSE_TSD_DATA_HACK'

sgi:
	$(MAKE) THR_FLAGS='-DUSE_SPROC=1' THR_LIBS='' CC='$(CC)' all

sgi-shared:
	$(MAKE) THR_FLAGS='-DUSE_SPROC=1' THR_LIBS= \
	 SH_FLAGS='-shared -check_registry /usr/lib/so_locations' \
	  MALLOC=ptmalloc.so shared all

sgi-libc:
	$(MAKE) THR_FLAGS='-DUSE_SPROC=1' THR_LIBS= MALLOC= T_FLAGS= \
	 T_SUF=-libc

solaris:
	$(MAKE) THR_FLAGS='-DUSE_THR=1 -D_REENTRANT' THR_LIBS=-lthread

solaris-libc:
	$(MAKE) THR_FLAGS='-DUSE_THR=1 -D_REENTRANT' THR_LIBS=-lthread \
	 MALLOC= T_FLAGS= T_SUF=-libc

nothreads:
	$(MAKE) THR_FLAGS='' THR_LIBS=''

gcc-nothreads:
	$(MAKE) CC='gcc -D_GNU_SOURCE=1' WARN_FLAGS='-Wall' OPT_FLAGS='-O2' \
	 THR_FLAGS='' THR_LIBS='' M_FLAGS='$(M_FLAGS)'

traditional:
	$(MAKE) THR_FLAGS='' THR_LIBS='' CC='gcc -traditional'

check: $(TESTS)
	./t-test1
	./t-test2

snap:
	cd ..; tar cf - $(DIST_FILES:%=ptmalloc/%) | \
	 gzip -9 >ptmalloc-current.tar.gz

dist:
	cd ..; tar cf - $(DIST_FILES2:%=ptmalloc/%) | \
	 gzip -9 >ptmalloc.tar.gz

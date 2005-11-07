
# osp module makefile

# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=osp.so
DEFS+=-D_POSIX_THREADS

LIBS=$(shell if [ -f /usr/local/lib/libosptk.a ]; then echo "-losptk" ; else echo "-losp" ; fi)
LIBS+=-lssl -lcrypto -lpthread -lm



include ../../Makefile.modules


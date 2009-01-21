
# osp module makefile

# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=osp.so
DEFS+=-D_POSIX_THREADS -I$(LOCALBASE)/include
LIBS=-L$(LOCALBASE)/lib -losptk -lssl -lcrypto -lpthread -lm

include ../../Makefile.modules

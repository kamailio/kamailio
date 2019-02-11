#
# dnssec module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=dnssec.so

DEFS+=-I$(LOCALBASE)/include

LIBS+=-L$(LOCALBASE)/lib
LIBS+= -lval-threads -lcrypto -lsres -lpthread -lssl

include ../../Makefile.modules

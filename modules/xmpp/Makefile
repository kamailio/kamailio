#
# xmpp module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=xmpp.so
LIBS=

# expat.h location
DEFS += -I$(LOCALBASE)/include
LIBS  = -L$(LOCALBASE)/lib -lexpat

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules

#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=sctp.so
LIBS=

ifeq ($(OS), linux)
LIBS+=-lsctp
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules

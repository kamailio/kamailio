# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=rabbitmq.so
LIBS=

DEFS+=-DKAMAILIO_MOD_INTERFACE
LIBS+=-lrabbitmq


SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
include ../../Makefile.modules

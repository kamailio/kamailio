#
# tsilo module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=tsilo.so
LIBS=

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

ifeq ($(INSTALL_FLAVOUR),kamailio)
DEFS+= -DWITH_EVENT_LOCAL_REQUEST
endif # INSTALL_FLAVOUR

include ../../Makefile.modules

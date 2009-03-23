# $Id:$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=dialplan.so
DEFS+=-I$(LOCALBASE)/include
LIBS=-L$(LOCALBASE)/lib -lpcre

DEFS+=-DOPENSER_MOD_INTERFACE

include ../../Makefile.modules

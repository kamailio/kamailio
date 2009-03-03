#
# utils module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=utils.so
LIBS=

DEFS+=-I$(LOCALBASE)/include

LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -lcurl

DEFS+=-DOPENSER_MOD_INTERFACE

include ../../Makefile.modules

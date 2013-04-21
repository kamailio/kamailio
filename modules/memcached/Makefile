# $Id: Makefile 3642 2008-02-06 15:46:09Z henningw $
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=memcached.so

DEFS+=-DKAMAILIO_MOD_INTERFACE

DEFS +=-I$(LOCALBASE)/include -I$(SYSBASE)/include
LIBS+=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lmemcached

include ../../Makefile.modules

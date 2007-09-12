# $Id: Makefile 1856 2007-03-15 21:06:00Z  $
#
# Resource List Server
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=rls.so
LIBS=

DEFS+=-I$(SYSBASE)/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
      -I$(LOCALBASE)/include
LIBS+=-L$(SYSBASE)/include/lib  -L$(LOCALBASE)/lib -lxml2

include ../../Makefile.modules

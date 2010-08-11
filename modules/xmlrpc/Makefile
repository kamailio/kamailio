# $Id$
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=xmlrpc.so
INCLUDES += -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
				-I$(LOCALBASE)/include
LIBS += -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2

DEFS+=-DSER_MOD_INTERFACE

include ../../Makefile.modules

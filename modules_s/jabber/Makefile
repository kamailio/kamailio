# $Id$
#
# example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=jabber.so

# extra debug messages
DEFS+=-DXJ_EXTRA_DEBUG
# -DHAVE_IHTTP

# expat.h location
DEFS+=-I$(LOCALBASE)/include -I/usr/pkg/include
LIBS=-L$(LOCALBASE)/lib -L/usr/pkg/lib -lexpat

include ../../Makefile.modules

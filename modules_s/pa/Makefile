# $Id$
#
# Presence Agent
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=pa.so
LIBS=

# DEFS+=-I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2
# LIBS+=-L/usr/include/lib  -L$(LOCALBASE)/lib -lxml2

# experimental (vku)
DEFS += -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 -I$(LOCALBASE)/include -Wall -I../../lib -DSER -I../..
# LIBS += -L$(LOCALBASE)/lib -L/usr/pkg/lib -L../../lib/xcap -L../../lib/presence -L../../lib/cds -lxcap -lpresence -lcds -lxml2 -lcurl 
LIBS += -L$(LOCALBASE)/lib -L/usr/pkg/lib -L../../lib/xcap -L../../lib/presence -L../../lib/cds -l_ser_xcap -l_ser_presence -l_ser_cds

include ../../Makefile.modules

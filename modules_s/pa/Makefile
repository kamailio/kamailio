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
DEFS+=-DSER
INCLUDES += -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
			-I$(LOCALBASE)/include -I../../lib -I../..
LIBS = -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2 
SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/cds/ser_cds $(SERLIBPATH)/presence/ser_presence \
			$(SERLIBPATH)/xcap/ser_xcap


DEFS+=-DSER_MOD_INTERFACE

SER_LIBS+=$(SERLIBPATH)/srdb2/srdb2
include ../../Makefile.modules

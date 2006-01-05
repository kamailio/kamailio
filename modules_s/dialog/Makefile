# $Id$
#
# Registrar Presence User Agent
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
NAME = dialog.so

# if using libcds, the directive SER must be defined !
# and root ser directory must be in include directories

DEFS += -Wall -I../../lib -DSER -I../..
LIBS += -L../../lib/cds -l_ser_cds

#DEFS = -I../.. -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 -Wall -I../../lib -DSER $(DEFS)
#LIBS = -L../../lib/cds -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2 -lcurl -lcds $(LIBS)

include ../../Makefile.modules

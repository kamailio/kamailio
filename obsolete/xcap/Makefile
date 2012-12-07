# $Id$
#
# Registrar Presence User Agent
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME = xcap.so


# if using libcds, the directive SER must be defined !
# and root ser directory must be in include directories

DEFS +=-DSER 
INCLUDES += -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
			-I$(LOCALBASE)/include -I../../lib -I../..
LIBS += -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2 -lcurl 
SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/cds/ser_cds
# not used: $(SERLIBPATH)/presence/ser_presence $(SERLIBPATH)/xcap/ser_xcap

DEFS+=-DSER_MOD_INTERFACE

include ../../Makefile.modules

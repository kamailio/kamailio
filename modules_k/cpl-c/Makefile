# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=cpl-c.so

DEFS +=-I$(SYSBASE)/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
		-I$(LOCALBASE)/include  # iconv.h

LIBS= -L$(LOCALBASE)/lib -lxml2

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules

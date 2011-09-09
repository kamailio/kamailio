# $Id: Makefile $
#
# XCAP server for Kamailio
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=xcap_server.so
LIBS=

XML2CFG=$(shell which xml2-config)

ifneq ($(XML2CFG),)

	DEFS += $(shell $(XML2CFG) --cflags)
	LIBS += $(shell $(XML2CFG) --libs)

else

	DEFS+=-I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
		-I$(LOCALBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -lxml2

endif

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1

include ../../Makefile.modules

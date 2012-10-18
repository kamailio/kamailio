# $Id: Makefile,v 1.6 2003/05/23 15:43:43 andrei Exp $
#
# carrierroute Makefile
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=carrierroute.so

BUILDER = $(shell which confuse-config)

ifeq ($(BUILDER),)
	BUILDER = $(shell which pkg-config)
	ifeq ($(BUILDER),)
		CONFUSEDEFS=-I$(LOCALBASE)/include -I/usr/local/include -I/opt/include \
			-I/usr/sfw/include
		CONFUSELIBS=-L$(LOCALBASE)/lib -L/usr/local/lib -L/usr/sfw/lib \
			-L/opt/lib -lconfuse
	else
		CONFUSEDEFS = $(shell pkg-config --cflags libconfuse)
		CONFUSELIBS = $(shell pkg-config --libs libconfuse)
	endif
else
		CONFUSEDEFS = $(shell confuse-config --cflags)
		CONFUSELIBS = $(shell confuse-config --libs)
endif

DEFS+=$(CONFUSEDEFS)
LIBS=$(CONFUSELIBS)

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/trie/trie $(SERLIBPATH)/srdb1/srdb1 $(SERLIBPATH)/kmi/kmi $(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules

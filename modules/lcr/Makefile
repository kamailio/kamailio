#
# Least Cost Routing Module
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=lcr.so

BUILDER = $(shell which pcre-config)

ifeq ($(BUILDER),)
	PCREDEFS=-I$(LOCALBASE)/include -I/usr/local/include -I/opt/include \
	-I/usr/sfw/include
	PCRELIBS=-L$(LOCALBASE)/lib -L/usr/local/lib -L/usr/sfw/lib \
	-L/opt/lib -lpcre
else
	PCREDEFS = $(shell pcre-config --cflags)
	PCRELIBS = $(shell pcre-config --libs)
endif

DEFS+=$(PCREDEFS)
LIBS=$(PCRELIBS)

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
include ../../Makefile.modules


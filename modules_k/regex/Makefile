# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=regex.so

ifeq ($(CROSS_COMPILE),)
BUILDER = $(shell which pcre-config)
endif

ifeq ($(BUILDER),)
	PCREDEFS=-I$(LOCALBASE)/include
	PCRELIBS=-L$(LOCALBASE)/lib -lpcre
else
	PCREDEFS = $(shell pcre-config --cflags)
	PCRELIBS = $(shell pcre-config --libs)
endif

DEFS+=$(PCREDEFS)
LIBS+=$(PCRELIBS)

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules

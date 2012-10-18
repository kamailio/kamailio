#
#

include ../../Makefile.defs
auto_gen=
NAME=app_mono.so

BUILDER = $(shell which pkg-config)
ifeq ($(BUILDER),)
	DEFS+=-D_REENTRANT -I/usr/include/mono-2.0
	LIBS= -lmono-2.0 -lm -lrt -ldl -lpthread
else
	DEFS+= $(shell pkg-config --cflags mono-2)
	LIBS = $(shell pkg-config --libs mono-2)
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules

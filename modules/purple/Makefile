include ../../Makefile.defs
auto_gen=
NAME=purple.so
BUILDER = $(shell which pkg-config)

ifeq ($(BUILDER),)
	DEFS+= -I/usr/lib/glib-2.0/include -I/usr/include/glib-2.0 \
		-I/usr/include/libxml2
	LIBS= -lglib-2.0 -lpurple -lxml2
else
	DEFS+= $(shell pkg-config --cflags glib-2.0)
	LIBS=  $(shell pkg-config --libs glib-2.0)
	DEFS+= $(shell pkg-config --cflags purple)
	LIBS+= $(shell pkg-config --libs purple)
	DEFS+= $(shell pkg-config --cflags libxml-2.0)
	LIBS+= $(shell pkg-config --libs libxml-2.0)
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

#
# app_sqlang module makefile
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_sqlang.so

BUILDER = $(shell which pkg-config)
ifeq ($(BUILDER),)
	DEFS+=-I/usr/squirrel/include
	LIBS+=-lsquirrel -lsqstdlib -lstdc++
else
	DEFS+= $(shell pkg-config --cflags squirrel3)
	LIBS = $(shell pkg-config --libs squirrel3)
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules

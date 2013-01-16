# $Id$
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_java.so


# for now is hard coded, will resolve this later
JAVA_HOME ?= /usr/lib/jvm/java-gcj-4.7
DEFS += $(shell pkg-config libgcj-4.7 --cflags) -I$(JAVA_HOME)/include
LIBS += $(shell pkg-config libgcj-4.7 --libs) -L$(JAVA_HOME)/lib  -ljvm

ifeq ($(OS), freebsd)
LIBS+=-pthread
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules


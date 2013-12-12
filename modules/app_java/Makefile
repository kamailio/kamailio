# $Id$
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_java.so

#DEFS += -DEXTRA_DEBUG

DIST = $(shell if [ -f "/etc/redhat-release" ]; then cat /etc/redhat-release | sed "s/.*\([0-9]\)\.[0-9].*/\1/g"; fi)
ifeq ($(DIST),6)
JVM_PATH = $(shell dirname `find /usr/lib/jvm/java/ -name "libjvm.so"`)
DEFS += $(shell pkg-config libgcj-4.4 --cflags)
LIBS += $(shell pkg-config libgcj-4.4 --cflags) -L$(JVM_PATH) -ljvm
else
# try to detect JAVA_HOME
JAVA_HOME ?= $(shell readlink -f /usr/bin/javac | sed "s:bin/javac::")
DEFS += $(shell pkg-config libgcj12 --cflags) -I$(JAVA_HOME)/include
LIBS += $(shell pkg-config libgcj12 --libs) -L$(JAVA_HOME)/lib  -ljvm

ifeq ($(OS), freebsd)
LIBS+=-pthread
endif
ifeq ($(OS), linux)
DEFS += -I$(JAVA_HOME)/include/linux
endif
endif

# disable optimisation for segfaults debugging
INCLUDE += -O0 -g
INCLUDES += -O0 -g

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules


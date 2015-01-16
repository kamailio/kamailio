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
DEFS += $(shell pkg-config libgcj --cflags) -I$(JAVA_HOME)/include
LIBS += $(shell pkg-config libgcj --libs) -L$(JAVA_HOME)/lib  -ljvm

# On Debian 7.5 there is a bug with JAVA_HOME detection.
# $(shell readlink -f /usr/bin/javac | sed "s:bin/javac::") points to perl wrapper script (/usr/bin/gcj-wrapper-4.7)
# whereas the real compiler is at /usr/bin/gcj-4.7. As the result, JAVA_HOME will not be a directory, that is incorrect.
# At this point I don't see any universal method as explicit setting this variable at the compile phase.
# -- ez
ifeq ($(shell [ -d "${JAVA_HOME}" -a -f "$(JAVA_HOME)/include/jni.h" -a -f "$(JAVA_HOME)/lib/libjvm.so" ] && echo 1 || echo 0),0)
    $(error Can't locate Java Development Kit. You have to specify environment JAVA_HOME to build app_java)
endif

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


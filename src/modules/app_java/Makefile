#
# WARNING: do not run this directly, it should be run by the master Makefile

# to generate readme file on a system without java, use DOCBUILD=yes in make command

include ../../Makefile.defs
auto_gen=
NAME=app_java.so

USE_GCJ ?= yes

#DEFS += -DEXTRA_DEBUG

DIST = $(shell if [ -f "/etc/redhat-release" ]; then cat /etc/redhat-release | sed "s/.*\([0-9]\)\.[0-9].*/\1/g"; fi)
ifeq ($(DIST),6)
JVM_PATH = $(shell dirname `find /usr/lib/jvm/java/ -name "libjvm.so"`)
DEFS += $(shell pkg-config libgcj-4.4 --cflags)
LIBS += $(shell pkg-config libgcj-4.4 --cflags) -L$(JVM_PATH) -ljvm
else
# try to detect JAVA_HOME
ifeq ($(USE_GCJ),yes)
JAVA_HOME ?= $(shell readlink -f /usr/bin/javac | sed "s:bin/javac::")
DEFS += $(shell pkg-config libgcj --cflags) -I$(JAVA_HOME)/include
LIBS += $(shell pkg-config libgcj --libs) -L$(JAVA_HOME)/lib  -ljvm
else
ifeq ($(ARCH),i386)
ATYPE=i386
else ifeq ($(ARCH),x86_64)
ATYPE=amd64
endif
JAVA_HOME ?= /usr/lib/jvm/java
DEFS += -I$(JAVA_HOME)/include
LIBS += -L$(JAVA_HOME)/jre/lib/$(ATYPE)/server -ljvm
endif

# On Debian 7.5 there is a bug with JAVA_HOME detection.
# $(shell readlink -f /usr/bin/javac | sed "s:bin/javac::") points to perl wrapper script (/usr/bin/gcj-wrapper-4.7)
# whereas the real compiler is at /usr/bin/gcj-4.7. As the result, JAVA_HOME will not be a directory, that is incorrect.
# At this point I don't see any universal method as explicit setting this variable at the compile phase.
# -- ez
ifeq ($(USE_GCJ),yes)
ifeq ($(shell [ -d "${JAVA_HOME}" -a -f "$(JAVA_HOME)/include/jni.h" -a -f "$(JAVA_HOME)/lib/libjvm.so" ] && echo 1 || echo 0),0)
ifneq ($(DOCBUILD),yes)
$(error Cannot locate Java Development Kit. You have to specify environment JAVA_HOME to build app_java)
endif
endif
else
ifeq ($(shell [ -d "${JAVA_HOME}" -a -f "$(JAVA_HOME)/include/jni.h" -a -f "$(JAVA_HOME)/jre/lib/$(ATYPE)/server/libjvm.so" ] && echo 1 || echo 0),0)
ifneq ($(DOCBUILD),yes)
$(error Cannot locate Java Development Kit. You have to specify environment JAVA_HOME to build app_java)
endif
endif
endif

ifeq ($(OS), freebsd)
LIBS+=-pthread
endif
ifeq ($(OS), linux)
DEFS += -I$(JAVA_HOME)/include/linux
endif
endif

# disable optimisation for segfaults debugging
ifeq ($(USE_GCJ),yes)
INCLUDE += -O0 -g
INCLUDES += -O0 -g
endif

include ../../Makefile.modules


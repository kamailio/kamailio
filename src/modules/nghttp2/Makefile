#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=nghttp2.so

ifeq ($(CROSS_COMPILE),)
   LNGHTTP2_BUILDER=$(shell \
       if pkg-config --exists libnghttp2; then \
           echo 'pkg-config libnghttp2'; \
       else \
           which curl-config; \
       fi)
endif

ifneq ($(LNGHTTP2_BUILDER),)
   LNGHTTP2DEFS = $(shell $(LNGHTTP2_BUILDER) --cflags)
   LNGHTTP2LIBS = $(shell $(LNGHTTP2_BUILDER) --libs)
else
   LNGHTTP2DEFS = -I$(LOCALBASE)/include -I$(SYSBASE)/include
   LNGHTTP2LIBS = -L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lnghttp2 -levent
endif


ifeq ($(CROSS_COMPILE),)
LEVENT_BUILDER = $(shell \
	if pkg-config --exists libevent; then \
		echo 'pkg-config libevent'; \
	fi)
endif

ifeq ($(LEVENT_BUILDER),)
	LEVENTDEFS=-I$(LOCALBASE)/include -I$(SYSBASE)/include
	LEVENTLIBS=-L$(LOCALBASE)/lib -levent
else
	LEVENTDEFS = $(shell $(LEVENT_BUILDER) --cflags)
	LEVENTLIBS = $(shell $(LEVENT_BUILDER) --libs)
endif

DEFS+=$(LNGHTTP2DEFS) $(LEVENTDEFS)
LIBS=$(LNGHTTP2LIBS) $(LEVENTLIBS)

include ../../Makefile.modules

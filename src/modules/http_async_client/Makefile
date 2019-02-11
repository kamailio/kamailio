#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=http_async_client.so

ifeq ($(CROSS_COMPILE),)
CURL_BUILDER=$(shell \
	if pkg-config --exists libcurl; then \
		echo 'pkg-config libcurl'; \
	else \
		which curl-config; \
	fi)
endif


ifneq ($(CURL_BUILDER),)
	CURLDEFS += $(shell $(CURL_BUILDER) --cflags)
	CURLLIBS += $(shell $(CURL_BUILDER) --libs)
else
	CURLDEFS+=-I$(LOCALBASE)/include -I$(SYSBASE)/include
	CURLLIBS+=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lcurl -levent
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

DEFS+=$(CURLDEFS) $(LEVENTDEFS)
LIBS=$(CURLLIBS) $(LEVENTLIBS)

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srutils/srutils

include ../../Makefile.modules

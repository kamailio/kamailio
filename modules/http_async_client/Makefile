#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=http_async_client.so

#ifeq ($(CROSS_COMPILE),)
#CURL_BUILDER=$(shell \
#	if pkg-config --exists libcurl; then \
#		echo 'pkg-config libcurl'; \
#	else \
#		which curl-config; \
#	fi)
#endif


ifneq ($(CURL_BUILDER),)
	DEFS += $(shell $(CURL_BUILDER) --cflags)
	LIBS += $(shell $(CURL_BUILDER) --libs)
else	
	DEFS+=-I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lcurl -levent
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srutils/srutils
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules

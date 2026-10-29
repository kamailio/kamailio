#
# ims_qos_n5 module makefile
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=ims_qos_n5.so

ifeq ($(CROSS_COMPILE),)
CURL_BUILDER=$(shell \
	if pkg-config --exists libcurl; then \
		echo 'pkg-config libcurl'; \
	else \
		which curl-config; \
	fi)
JNS_BUILDER=$(shell \
	if pkg-config --exists jansson; then \
		echo 'pkg-config jansson'; \
	fi)
endif

ifneq ($(CURL_BUILDER),)
	DEFS += $(shell $(CURL_BUILDER) --cflags)
	LIBS += $(shell $(CURL_BUILDER) --libs)
else
	DEFS += -I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -lcurl
endif

ifneq ($(JNS_BUILDER),)
	DEFS += $(shell $(JNS_BUILDER) --cflags)
	LIBS += $(shell $(JNS_BUILDER) --libs)
else
	DEFS += -I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -ljansson
endif

include ../../Makefile.modules

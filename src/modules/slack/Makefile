#
# Slack client for Kamailio
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=slack.so

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
        CURLLIBS+=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lcurl
endif

DEFS+=$(CURLDEFS)
LIBS=$(CURLLIBS)

include ../../Makefile.modules

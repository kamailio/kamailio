#
# Arnacon Authentication Module Makefile
#

include ../../Makefile.defs
auto_gen=
NAME=auth_arnacon.so
LIBS=

DEFS+=-DKAMAILIO_MOD -DDISABLE_NAGLE

# Link with curl and secp256k1
ifeq ($(CROSS_COMPILE),)
CURL_BUILDER=$(shell \
	if pkg-config --exists libcurl; then \
		echo 'pkg-config libcurl'; \
	else \
		which curl-config; \
	fi)
endif

ifneq ($(CURL_BUILDER),)
	DEFS+= $(shell $(CURL_BUILDER) --cflags )
	LIBS+= $(shell $(CURL_BUILDER) --libs)
else
	DEFS+= -I$(LOCALBASE)/include
	LIBS+= -L$(LOCALBASE)/lib -lcurl
endif

LIBS+= -lsecp256k1

include ../../Makefile.modules

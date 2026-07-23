#
# 
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=jwt3.so

ifeq ($(CROSS_COMPILE),)
JWT_BUILDER=$(shell \
	if pkg-config --exists libjwt; then \
		echo 'pkg-config libjwt'; \
	fi)
CURL_BUILDER=$(shell \
	if pkg-config --exists libcurl; then \
		echo 'pkg-config libcurl'; \
	fi)
endif

ifneq ($(JWT_BUILDER),)
	DEFS += $(shell $(JWT_BUILDER) --cflags)
	LIBS += $(shell $(JWT_BUILDER) --libs)
else
	DEFS += -I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -ljwt
endif

ifneq ($(CURL_BUILDER),)
	DEFS += $(shell $(CURL_BUILDER) --cflags)
	LIBS += $(shell $(CURL_BUILDER) --libs)
else
	LIBS += -lcurl
endif

include ../../Makefile.modules


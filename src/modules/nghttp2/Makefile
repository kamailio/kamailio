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


ifeq ($(CROSS_COMPILE),)
LEVENTSSL_BUILDER = $(shell \
	if pkg-config --exists libevent_openssl; then \
		echo 'pkg-config libevent_openssl'; \
	fi)
endif

ifeq ($(LEVENTSSL_BUILDER),)
	LEVENTSSLDEFS=-I$(LOCALBASE)/include -I$(SYSBASE)/include
	LEVENTSSLLIBS=-L$(LOCALBASE)/lib -levent_openssl
else
	LEVENTSSLDEFS = $(shell $(LEVENTSSL_BUILDER) --cflags)
	LEVENTSSLLIBS = $(shell $(LEVENTSSL_BUILDER) --libs)
endif


ifeq ($(CROSS_COMPILE),)
LSSL_BUILDER=$(shell \
	if pkg-config --exists libssl; then \
		echo 'pkg-config libssl'; \
	fi)
LCRYPTO_BUILDER=$(shell \
	if pkg-config --exists libcrypto; then \
		echo 'pkg-config libcrypto'; \
	fi)
endif

ifneq ($(LSSL_BUILDER),)
	LSSLDEFS = $(shell $(LSSL_BUILDER) --cflags)
	LSSLLIBS = $(shell $(LSSL_BUILDER) --libs)

ifneq ($(CRYPTO_BUILDER),)
	LCRYPTODEFS = $(shell $(LCRYPTO_BUILDER) --cflags)
	LCRYPTOLIBS = $(shell $(LCRYPTO_BUILDER) --libs)
endif

else
	LSSLDEFS = -I$(LOCALBASE)/ssl/include
	LSSLLIBS = -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib \
			-L$(LOCALBASE)/lib64 -L$(LOCALBASE)/ssl/lib64 \
			-lssl -lcrypto
	LCRYPTODEFS =
	LCRYPTOLIBS =
endif



DEFS+=$(LNGHTTP2DEFS) $(LEVENTDEFS) $(LEVENTSSLDEFS) $(LSSLDEFS) $(LCRYPTODEFS)
LIBS=$(LNGHTTP2LIBS) $(LEVENTLIBS) $(LEVENTSSLLIBS) $(LSSLLIBS) $(LCRYPTOLIBS)

include ../../Makefile.modules

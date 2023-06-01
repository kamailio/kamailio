#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=outbound.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
endif

ifneq ($(BUILDER),)
SSL_BUILDER:=$(shell \
	if pkg-config --exists libssl; then \
		echo 'pkg-config libssl'; \
	fi)

CRYPTO_BUILDER:=$(shell \
	if pkg-config --exists libcrypto; then \
		echo 'pkg-config libcrypto'; \
	fi)

ifneq ($(SSL_BUILDER),)
	DEFS += $(shell $(SSL_BUILDER) --cflags)
	LIBS += $(shell $(SSL_BUILDER) --libs)
endif
ifneq ($(CRYPTO_BUILDER),)
	DEFS += $(shell $(CRYPTO_BUILDER) --cflags)
	LIBS += $(shell $(CRYPTO_BUILDER) --libs)
endif

else
	DEFS += -I$(LOCALBASE)/ssl/include
	LIBS += -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib \
			-L$(LOCALBASE)/lib64 -L$(LOCALBASE)/ssl/lib64 \
			-lssl -lcrypto
	# NOTE: depending on the way in which libssl was compiled you might
	#       have to add -lz -lkrb5   (zlib and kerberos5).
	#       E.g.: make TLS_HOOKS=1 TLS_EXTRA_LIBS="-lz -lkrb5"
endif

LIBS+= $(TLS_EXTRA_LIBS)

# Static linking, if you'd like to use TLS and OUTBOUND at the same time
#
#LIBS+= /usr/lib/libcurl.a /usr/lib/libssl.a /usr/lib/libcrypto.a -lkrb5 -lidn -lz -lgssapi_krb5 -lrt

include ../../Makefile.modules


# osp module makefile

# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=osp.so

ifeq ($(CROSS_COMPILE),)
SSL_BUILDER=$(shell \
	if pkg-config --exists libssl; then \
		echo 'pkg-config libssl'; \
	fi)
endif

ifneq ($(SSL_BUILDER),)
	DEFS += $(shell $(SSL_BUILDER) --cflags)
	LIBS += $(shell $(SSL_BUILDER) --libs)
else
	DEFS += -I$(LOCALBASE)/ssl/include
	LIBS += -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib \
			-L$(LOCALBASE)/lib64 -L$(LOCALBASE)/ssl/lib64 \
			-lssl -lcrypto
	# NOTE: depending on the way in which libssl was compiled you might
	#     #       have to add -lz -lkrb5   (zlib and kerberos5).
	#       E.g.: make TLS_HOOKS=1 TLS_EXTRA_LIBS="-lz -lkrb5"
endif
LIBS+= $(TLS_EXTRA_LIBS)

# Static linking, if you'd like to use TLS and OSP at the same time
#
#LIBS+= /usr/lib/libcurl.a /usr/lib/libssl.a /usr/lib/libcrypto.a -lkrb5 -lidn -lz -lgssapi_krb5 -lrt

DEFS+=-D_POSIX_THREADS -I$(LOCALBASE)/include
LIBS+=-L$(LOCALBASE)/lib -losptk -lpthread -lm

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules


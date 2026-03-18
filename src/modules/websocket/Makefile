#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=websocket.so

EMBEDDED_UTF8_DECODE ?= 0

ifeq ($(EMBEDDED_UTF8_DECODE),0)
	DEFS += -I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -lunistring
else
	DEFS += -DEMBEDDED_UTF8_DECODE
endif

# Static linking, if you'd like to use TLS and WEBSOCKET at the same time
#
#LIBS+= /usr/lib/libcurl.a /usr/lib/libssl.a /usr/lib/libcrypto.a -lkrb5 -lidn -lz -lgssapi_krb5 -lrt

include ../../Makefile.modules

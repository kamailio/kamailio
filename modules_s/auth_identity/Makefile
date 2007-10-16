
include ../../Makefile.defs

auto_gen=
NAME=auth_identity.so

DEFS+= -Wall -I$(LOCALBASE)/ssl/include
#
# Dinamic linking
#
LIBS+= -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib -lssl -lcrypto -lcurl

#
# Static linking, if you'd like to use TLS and AUTH_IDENTITY at the same time
#
#LIBS+= /usr/lib/libcurl.a /usr/lib/libssl.a /usr/lib/libcrypto.a -lkrb5 -lidn -lz -lgssapi_krb5

include ../../Makefile.modules

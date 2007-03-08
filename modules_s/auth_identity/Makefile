
include ../../Makefile.defs

auto_gen=
NAME=auth_identity.so

DEFS+= -Wall -I$(LOCALBASE)/ssl/include
LIBS+= -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib -lssl -lcrypto -lcurl

include ../../Makefile.modules

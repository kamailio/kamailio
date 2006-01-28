# Makefile v 1.0 2002/12/27
#
# TLS module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=tls.so

DEFS+= -I$(LOCALBASE)/ssl/include
LIBS+= -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib -lssl  -lcrypto

include ../../Makefile.modules

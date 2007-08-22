# $Id: Makefile $
#
# XCAP client for OpenSer
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=xcap_client.so
LIBS=

DEFS+=-I$(SYSBASE)/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
      -I$(LOCALBASE)/include

LIBS+=-L$(SYSBASE)/include/lib  -L$(LOCALBASE)/lib   -lcurl -lxml2 

include ../../Makefile.modules

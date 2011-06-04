# $Id: Makefile $
#
# XCAP client for Kamailio
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=xcap_client.so
LIBS=

DEFS+=-I$(LOCALBASE)/include

LIBS+=-L$(SYSBASE)/lib -L$(LOCALBASE)/lib -lcurl

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi

include ../../Makefile.modules

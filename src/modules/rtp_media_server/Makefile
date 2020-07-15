include ../../Makefile.defs
auto_gen=
NAME=rtp_media_server.so

DEFS+=-I$(LOCALBASE)/lib

ORTPLIBS=-lortp
BCUNITLIBS=-lbcunit
MS2LIBS=-lmediastreamer

LIBS=$(ORTPLIBS) $(BCUNITLIBS) $(MS2LIBS)
DEFS+=-DKAMAILIO_MOD_INTERFACE
include ../../Makefile.modules

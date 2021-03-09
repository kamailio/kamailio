include ../../Makefile.defs
auto_gen=
NAME=rtp_media_server.so

DEFS+=-I$(LOCALBASE)/lib

ORTPLIBS=-lortp
BCUNITLIBS=-lbcunit
MS2LIBS=-lmediastreamer_voip -lmediastreamer_base

LIBS=$(ORTPLIBS) $(BCUNITLIBS) $(MS2LIBS)
include ../../Makefile.modules

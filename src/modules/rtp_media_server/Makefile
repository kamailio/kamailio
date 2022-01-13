include ../../Makefile.defs
auto_gen=
NAME=rtp_media_server.so

DEFS+=-I$(LOCALBASE)/include

ORTPLIBS=-lortp
BCUNITLIBS=-lbcunit
MS2LIBS=-lmediastreamer

LIBS+=-L$(LOCALBASE)/lib $(ORTPLIBS) $(BCUNITLIBS) $(MS2LIBS)
include ../../Makefile.modules

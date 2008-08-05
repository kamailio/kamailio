# $Id$
#
#  db_orasel Makefile
# 

include ../../Makefile.defs

auto_gen=
NAME=kamailio_orasel

include ../../Makefile.sources

ORAPATH=

# use for multiple client sdk version install
ifneq ($(ORAVERSION),)
    ORAVERDIR=/$(ORAVERSION)
endif

# use include/library path's for full client installation
ifneq ($(ORAHOME),)
    DEFS +=-I$(ORAHOME)/include
    LIBS +=-L$(ORAHOME)/lib
ifeq ($(ORAPATH),)
    ORAPATH=$(ORAHOME)/lib
endif
else
# use standard know paths oci.h locations (linux)
    DEFS +=-I$(LOCALBASE)/include/oracle$(ORAVERDIR) \
	-I$(SYSBASE)/include/oracle$(ORAVERDIR)
endif

# search 'so' path if it non standard (possible liboclntsh locations on linux)
ifeq ($(ORAPATH),)
    ORAPATH=$(shell [ -f $(LOCALBASE)/lib64/oracle$(ORAVERDIR)/libocci.so ] && \
	    echo $(LOCALBASE)/lib64/oracle$(ORAVERDIR) )
endif
ifeq ($(ORAPATH),)
    ORAPATH=$(shell [ -f $(SYSBASE)/lib64/oracle$(ORAVERDIR)/libocci.so ] && \
	    echo $(SYSBASE)/lib64/oracle$(ORAVERDIR) )
endif
ifeq ($(ORAPATH),)
    ORAPATH=$(shell [ -f $(SYSBASE)/lib/oracle$(ORAVERDIR)/libocci.so ] && \
	    echo $(SYSBASE)/lib/oracle$(ORAVERDIR) )
endif
ifeq ($(ORAPATH),)
    ORAPATH=$(shell [ -f $(SYSBASE)/lib/oracle$(ORAVERDIR)/libocci.so ] && \
	    echo $(SYSBASE)/lib/oracle$(ORAVERDIR) )
endif

ifneq ($(ORAPATH),)
    LIBS +=-L$(ORAPATH)
endif

LIBS +=-locci -lclntsh

#DEFS +=-DLINUX -D_GNU_SOURCE -D_REENTRANT
#LIBS +=-lpthread

ifneq ($(ORAPATH),)
    LIBS +=-Wl,-rpath $(ORAPATH)
endif

include ../../Makefile.rules

modules:

#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=db_oracle.so

# can be defined for non standard placement of oracle so's
ORAPATH=/usr/lib/oracle/xe/app/oracle/product/10.2.0/client
DEFS += -I$(ORAPATH)/rdbms/public
LIBS += -L$(ORAPATH)/lib


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

#DEFS+=-DLINUX -D_GNU_SOURCE -D_REENTRANT
#LIBS+=-lpthread

ifneq ($(ORAPATH),)
    LIBS +=-Wl,-rpath $(ORAPATH)
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

ifeq ($(INSTALL_FLAVOUR),kamailio)
# extra install for kamailio

install-oracle-scripts: $(bin_prefix)/$(bin_dir)
		ORACLEON=yes make -C ../../utils/kamctl/ install-modules

install-scripts: install-oracle-scripts

endif # INSTALL_FLAVOUR

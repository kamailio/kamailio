#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=db_oracle.so

# default path for oracle 18.3 client
DEFS +=-I/usr/include/oracle/18.3/client64
LIBS +=-locci -lclntsh -L/usr/lib/oracle/18.3/client64/lib

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

ifeq ($(INSTALL_FLAVOUR),kamailio)
# extra install for kamailio

install-oracle-scripts: $(bin_prefix)/$(bin_dir)
	ORACLEON=yes $(MAKE) -C ../../../utils/kamctl/ install-modules

install-scripts: install-oracle-scripts

endif # INSTALL_FLAVOUR

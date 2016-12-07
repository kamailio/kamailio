#
# db_berkeley module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile
include ../../Makefile.defs 
auto_gen=
NAME=db_berkeley.so

# extra debug messages
# -DBDB_EXTRA_DEBUG is optional
DEFS +=-DSER_MOD_INTERFACE -I$(LOCALBASE)/include -I$(LOCALBASE)/BerkeleyDB.4.6/include \
	-I$(SYSBASE)/include


LIBS=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -L$(LOCALBASE)/BerkeleyDB.4.6/lib -ldb

SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1 $(SERLIBPATH)/kmi/kmi

include ../../Makefile.modules

ifeq ($(INSTALL_FLAVOUR),kamailio)
# extra install for kamailio

install-berkeley-scripts: $(bin_prefix)/$(bin_dir)
		BERKELEYDBON=yes make -C ../../utils/kamctl/ install-modules

install-scripts: install-berkeley-scripts

endif # INSTALL_FLAVOUR

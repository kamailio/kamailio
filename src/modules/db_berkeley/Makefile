#
# db_berkeley module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile
include ../../Makefile.defs
auto_gen=
NAME=db_berkeley.so

# extra debug messages
# -DBDB_EXTRA_DEBUG is optional
DEFS +=-I$(LOCALBASE)/include -I$(LOCALBASE)/BerkeleyDB.4.6/include \
	-I$(SYSBASE)/include


LIBS=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -L$(LOCALBASE)/BerkeleyDB.4.6/lib -ldb

include ../../Makefile.modules

ifeq ($(INSTALL_FLAVOUR),kamailio)
# extra install for kamailio

install-berkeley-scripts: $(bin_prefix)/$(bin_dir)
	BERKELEYDBON=yes $(MAKE) -C ../../../utils/kamctl/ install-modules

install-scripts: install-berkeley-scripts

endif # INSTALL_FLAVOUR

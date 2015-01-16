#
# example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

# extra debug messages
DEFS+=-DDBT_EXTRA_DEBUG
 
include ../../Makefile.defs
auto_gen=
NAME=db_text.so
LIBS=

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
include ../../Makefile.modules

ifeq ($(INSTALL_FLAVOUR),kamailio)
# extra install for kamailio

install-dbtext-scripts: $(bin_prefix)/$(bin_dir)
		DBTEXTON=yes $(MAKE) -C ../../utils/kamctl/ install-modules

install-scripts: install-dbtext-scripts

endif # INSTALL_FLAVOUR

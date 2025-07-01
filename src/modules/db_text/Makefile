#
# example module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

# extra debug messages
DEFS+=-DDBT_EXTRA_DEBUG

include ../../Makefile.defs
auto_gen=
NAME=db_text.so
LIBS=

include ../../Makefile.modules

ifeq ($(INSTALL_FLAVOUR),kamailio)
# extra install for kamailio

install-dbtext-scripts: $(bin_prefix)/$(bin_dir)
		DBTEXTON=yes $(MAKE) -C ../../../utils/kamctl/ install-modules

install-scripts: install-dbtext-scripts

endif # INSTALL_FLAVOUR

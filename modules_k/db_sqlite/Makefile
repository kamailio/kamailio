# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=db_sqlite.so

LIBS +=-lsqlite3

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

ifeq ($(INSTALL_FLAVOUR),kamailio)
# extra install for kamailio

install-sqlite-scripts: $(bin_prefix)/$(bin_dir)
		SQLITEON=yes make -C ../../utils/kamctl/ install-modules

install-scripts: install-sqlite-scripts

endif # INSTALL_FLAVOUR

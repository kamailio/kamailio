#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=db_mysql.so

DEFS +=-DSER_MOD_INTERFACE

# set CROSS_COMPILE to true if you want to skip
# the autodetection
# CROSS_COMPILE=true

ifeq ($(CROSS_COMPILE),)
MYSQLCFG=$(shell which mysql_config)
ifeq ($(MYSQLCFG),)
MYSQLCFG=$(shell which mysql_config5)
endif
endif

ifneq ($(MYSQLCFG),)
	# use autodetection
	DEFS += $(shell $(MYSQLCFG) --include | sed 's/\(-I[^ ]*\)\/mysql/\1/g' )
	LIBS = $(shell $(MYSQLCFG) --libs)
else
	# mysql.h locations (freebsd,openbsd  solaris)
	DEFS +=-I$(LOCALBASE)/include \
		-I$(LOCALBASE)/include/mysql \
		-I$(LOCALBASE)/mysql/include
	# libmysqlclient locations on RH/Suse, Solaris /OpenBSD, FreeBSD
	# (Debian does the right thing and puts it in /usr/lib)
	LIBS=-L$(LOCALBASE)/lib -L$(LOCALBASE)/lib/mysql \
		-L$(LOCALBASE)/mysql/lib/mysql/ \
		-L$(LOCALBASE)/mysql/lib \
		-lmysqlclient -lz
endif

SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1
ifneq (,$(filter sip-router ser, $(INSTALL_FLAVOUR)))
MOD_INSTALL_SHARE= ../../scripts/mysql/my_create.sql \
				   ../../scripts/mysql/my_data.sql   \
				   ../../scripts/mysql/my_drop.sql
else ifeq ($(INSTALL_FLAVOUR),kamailio)
#TODO
endif

include ../../Makefile.modules

ifneq (,$(filter sip-router ser, $(INSTALL_FLAVOUR)))
# extra install for ser and sip-router

install-mysql-script: $(bin_prefix)/$(bin_dir)
	sed -e "s#^DEFAULT_SCRIPT_DIR.*#DEFAULT_SCRIPT_DIR=\"$(share_prefix)/$(share_dir)\"#g" \
	< ../../scripts/mysql/$(SCR_NAME)_mysql.sh > \
	$(bin_prefix)/$(bin_dir)/$(MAIN_NAME)_mysql.sh
	chmod 755 $(bin_prefix)/$(bin_dir)/$(MAIN_NAME)_mysql.sh

install-scripts: install-mysql-script

else ifeq ($(INSTALL_FLAVOUR),kamailio)
# extra install for kamailio

install-mysql-scripts: $(bin_prefix)/$(bin_dir)
		MYSQLON=yes $(MAKE) -C ../../utils/kamctl/ install-modules

install-scripts: install-mysql-scripts

endif # INSTALL_FLAVOUR

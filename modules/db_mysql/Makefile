# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=db_mysql.so

# mysql.h locations (freebsd,openbsd  solaris)
DEFS +=-DSER_MOD_INTERFACE -I$(LOCALBASE)/include -I$(LOCALBASE)/include/mysql \
		-I$(LOCALBASE)/mysql/include \
		-I/usr/include/mysql

# libmysqlclient locations on RH/Suse, Solaris /OpenBSD, FreeBSD
# (Debian does the right thing and puts it in /usr/lib)
LIBS=-L/usr/lib/mysql -L$(LOCALBASE)/lib -L$(LOCALBASE)/lib/mysql \
		-L$(LOCALBASE)/mysql/lib/mysql/ \
		-L$(LOCALBASE)/mysql/lib \
		-L/usr/lib64/mysql \
		-lmysqlclient -lz

SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1

install-mysql-script: $(bin_prefix)/$(bin_dir)
	sed -e "s#^DEFAULT_SCRIPT_DIR.*#DEFAULT_SCRIPT_DIR=\"$(share_prefix)/$(share_dir)\"#g" \
	< ../../scripts/mysql/$(SCR_NAME)_mysql.sh > \
	$(bin_prefix)/$(bin_dir)/$(MAIN_NAME)_mysql.sh
	chmod 755 $(bin_prefix)/$(bin_dir)/$(MAIN_NAME)_mysql.sh

install-scripts: install-mysql-script

include ../../Makefile.modules

# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=mysql.so

# uncomment the following line if you what to skip
# the autodetection
MYSQLCFG=$(shell which mysql_config)

ifneq ($(MYSQLCFG),)

	# use autodetection
	DEFS += $(shell $(MYSQLCFG) --include)
	LIBS = $(shell $(MYSQLCFG) --libs)

else

	# use standard know paths
	# mysql.h locations (freebsd,openbsd  solaris)
	DEFS +=-I$(LOCALBASE)/include -I$(LOCALBASE)/include/mysql \
		-I$(LOCALBASE)/mysql/include/mysql -I$(LOCALBASE)/mysql/include \
		-I$(SYSBASE)/include/mysql

	# libmysqlclient locations on RH/Suse, Solaris /OpenBSD, FreeBSD
	# (Debian does the right thing and puts it in /usr/lib)
	LIBS=-L$(SYSBASE)/lib/mysql -L$(LOCALBASE)/lib -L$(LOCALBASE)/lib/mysql \
		-L$(LOCALBASE)/mysql/lib/mysql/ \
		-L$(LOCALBASE)/mysql/lib \
		-L$(SYSBASE)/lib64/mysql \
		-lm -lmysqlclient -lz

endif

include ../../Makefile.modules

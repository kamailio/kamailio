# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=mysql.so


# mysql.h locations (freebsd,openbsd  solaris)
DEFS +=-I$(LOCALBASE)/include -I$(LOCALBASE)/include/mysql \
		-I$(LOCALBASE)/mysql/include -I/usr/pkg/include \
		-I/usr/include/mysql

# Database API, standard includes
DEFS +=-I../../db -I../.. -I../../mem

# libmysqlclient locations on RH/Suse, Solaris /OpenBSD, FreeBSD
# (Debian does the right thing and puts it in /usr/lib)
LIBS=-L/usr/lib/mysql -L$(LOCALBASE)/lib -L$(LOCALBASE)/lib/mysql \
		-L$(LOCALBASE)/mysql/lib/mysql/ -L/usr/pkg/lib/mysql \
		-L$(LOCALBASE)/mysql/lib \
		-lmysqlclient -lz

include ../../Makefile.modules

# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=mysql.so

# mysql.h locations (freebsd,openbsd  solaris)
DEFS +=-I$(LOCALBASE)/include -I$(LOCALBASE)/include/mysql \
		-I$(LOCALBASE)/mysql/include \
		-I/usr/include/mysql

# libmysqlclient locations on RH/Suse, Solaris /OpenBSD, FreeBSD
# (Debian does the right thing and puts it in /usr/lib)
LIBS=-L/usr/lib/mysql -L$(LOCALBASE)/lib -L$(LOCALBASE)/lib/mysql \
		-L$(LOCALBASE)/mysql/lib/mysql/ \
		-L$(LOCALBASE)/mysql/lib \
		-L/usr/lib64/mysql \
		-lmysqlclient -lz

DEFS+=-DSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb2/srdb2
include ../../Makefile.modules

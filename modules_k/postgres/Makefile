# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=postgres.so

# libpq-fe.h locations
DEFS +=-I$(LOCALBASE)/include -I$(LOCALBASE)/pgsql/include \
	 -I/usr/include/postgresql -I/usr/include/postgresql/8.0
LIBS=-L$(LOCALBASE)/lib -L$(LOCALBASE)/pgsql/lib -L$(LOCALBASE)/lib/pgsql \
	 -lpq

include ../../Makefile.modules

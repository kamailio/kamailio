# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

auto_gen=
NAME=postgres.so

# libpq-fe.h locations
DEFS +=-I/usr/local/pgsql/include
LIBS=-L$(LOCALBASE)/pgsql/lib -L$(LOCALBASE)/lib/pgsql -L/usr/pkg/lib \
		-L/usr/pkg/lib/pgsql -lpq

include ../../Makefile.modules

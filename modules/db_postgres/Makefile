# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=postgres.so

# libpq-fe.h locations
#DEFS += -DPG_TEST
DEFS +=-DSER_MOD_INTERFACE -I$(LOCALBASE)/include -I$(LOCALBASE)/pgsql/include  \
	-I$(LOCALBASE)/include/postgresql \
	-I/usr/include/postgresql -I/usr/include/postgresql/8.0 \
	-I/usr/include/pgsql/
LIBS=-L$(LOCALBASE)/lib -L$(LOCALBASE)/pgsql/lib -L$(LOCALBASE)/lib/pgsql -L /usr/lib -lpq

SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1

include ../../Makefile.modules

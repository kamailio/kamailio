# $Id$
#
# example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

# extra debug messages
#DEFS+=-DBDB_EXTRA_DEBUG
 
include ../../Makefile.defs
auto_gen=
NAME=bdb.so

# db.h locations
#DEFS += -I$(LOCALBASE)/include/db41
#LIBS  = -L$(LOCALBASE)/lib -ldb41
DEFS += -I$(LOCALBASE)/include/db44
LIBS  = -L$(LOCALBASE)/lib -ldb-4.4


DEFS+=-DSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb2/srdb2
include ../../Makefile.modules

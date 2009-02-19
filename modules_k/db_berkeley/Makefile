# $Id:  $
#
# example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

# extra debug messages
# -DBDB_EXTRA_DEBUG is optional

include ../../Makefile.defs 
auto_gen=
NAME=db_berkeley.so

DEFS +=-I$(LOCALBASE)/include -I$(LOCALBASE)/BerkeleyDB.4.6/include \
	-I$(LOCALBASE)/include/db4 -I$(SYSBASE)/include

LIBS=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -L$(LOCALBASE)/BerkeleyDB.4.6/lib -ldb

include ../../Makefile.modules

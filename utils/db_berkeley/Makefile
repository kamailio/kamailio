#
#  db_berkeley Makefile
#

include ../../src/Makefile.defs

auto_gen=
NAME=kambdb_recover

include ../../src/Makefile.sources

# if you want to tune or reset flags
#DEFS:=-DEXTRA_DEBUG  -I$(LOCALBASE)/BerkeleyDB.4.6/include
DEFS+=-I$(LOCALBASE)/include -I$(LOCALBASE)/BerkeleyDB.4.6/include \
        -I$(LOCALBASE)/include/db4 -I$(SYSBASE)/include

LIBS=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -L$(LOCALBASE)/BerkeleyDB.4.6/lib -ldb

include ../../src/Makefile.rules

modules:


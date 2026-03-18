#
#  db_orasel Makefile
#

include ../../src/Makefile.defs

auto_gen=
NAME=kamailio_orasel

include ../../src/Makefile.sources

# default path for oracle 18.3 client
DEFS +=-I/usr/include/oracle/18.3/client64
LIBS +=-locci -lclntsh -L/usr/lib/oracle/18.3/client64/lib

include ../../src/Makefile.rules

modules:

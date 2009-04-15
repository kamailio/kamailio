include ../../Makefile.defs
auto_gen=
NAME=purple.so
LIBS= 

DEFS+=-I/usr/lib/glib-2.0/include -I/usr/include/glib-2.0 \
		-I/usr/include/libxml2
LIBS+=-lglib-2.0 -lpurple -lxml2


DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

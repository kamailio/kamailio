# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=userblacklist.so
LIBS=

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
SER_LIBS+=$(SERLIBPATH)/trie/trie
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules


# example library makefile
#

COREPATH= ../../..

include ../../../Makefile.defs
auto_gen=
NAME:=kazoo-deps
MAJOR_VER=1
MINOR_VER=0
BUGFIX_VER=0
DEFS+=-DSER -DHAVE_CONFIG_H
LIBS+=-lrt -lssl -lcrypto
INCLUDES=  -I$(CURDIR) -I$(CURDIR)/../../lib -I$(CURDIR)/../../.. 

include ../../../Makefile.libs


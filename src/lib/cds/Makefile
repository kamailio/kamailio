#
# example library makefile
#

include ../../Makefile.defs
auto_gen=
NAME:=ser_cds
MAJOR_VER=0
MINOR_VER=1
BUGFIX_VER=0
DEFS+=-DSER
INCLUDES= -I$(CURDIR)/.. -I$(CURDIR)/../..
LIBS=

ifeq ($(OS), darwin)
	LIB_LDFLAGS+= -flat_namespace -undefined suppress
endif

include ../../Makefile.libs



# example library makefile
#

include ../../Makefile.defs
auto_gen=
NAME:=ser_presence
MAJOR_VER=0
MINOR_VER=1
BUGFIX_VER=0
DEFS+=-DSER
libxml2_includes=-I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
					-I$(LOCALBASE)/include
libxml2_libs=-L$(LOCALBASE)/lib -lxml2
INCLUDES= -I$(CURDIR)/.. -I$(CURDIR)/../.. $(libxml2_includes) 
LIBS=$(libxml2_libs)
SERLIBPATH=..
SER_LIBS=$(SERLIBPATH)/cds/ser_cds

include ../../Makefile.libs


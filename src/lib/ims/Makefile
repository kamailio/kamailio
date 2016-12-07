
# example library makefile
#

include ../../Makefile.defs
auto_gen=
NAME:=kamailio_ims
MAJOR_VER=0
MINOR_VER=1
BUGFIX_VER=0
DEFS+=-DSER

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
endif

ifneq ($(XML2CFG),)
	libxml2_includes = $(shell $(XML2CFG) --cflags)
	libxml2_libs = $(shell $(XML2CFG) --libs)
else
	libxml2_includes = -I$(LOCALBASE)/include/libxml2 -I$(LOCALBASE)/include
	libxml2_libs = -L$(LOCALBASE)/lib -lxml2
endif

INCLUDES= -I$(CURDIR)/.. -I$(CURDIR)/../.. $(libxml2_includes) 
LIBS=$(libxml2_libs)
#SERLIBPATH=..
#SER_LIBS=$(SERLIBPATH)/cds/ser_cds

include ../../Makefile.libs


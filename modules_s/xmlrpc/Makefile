# $Id$
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

# Set to true if you want to use K sl module API
USE_K_SL_API=false

include ../../Makefile.defs
auto_gen=
NAME=xmlrpc.so
DEFS += -I/usr/include/libxml2 -I/usr/local/include/libxml2 -I/usr/local/include
LIBS +=-lxml2

DEFS+=-DSER_MOD_INTERFACE

ifeq ($(USE_K_SL_API),true)
DEFS+=-DUSE_K_SL_API
endif

include ../../Makefile.modules

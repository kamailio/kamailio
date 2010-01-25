# $Id: Makefile 2 2005-06-13 16:47:24Z bogdan_iancu $
#
# print example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_lua.so
LIBS= -llua5.1

DEFS+=-DOPENSER_MOD_INTERFACE -I/usr/include/lua5.1/

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules

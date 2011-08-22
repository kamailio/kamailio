# $Id: Makefile 2 2005-06-13 16:47:24Z bogdan_iancu $
#
# print example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_lua.so

BUILDER = $(shell which lua-config)
ifeq ($(BUILDER),)
	BUILDER = $(shell which pkg-config)
	ifeq ($(BUILDER),)
		LUA51 = $(shell ls /usr/lib/liblua* | grep liblua5.1)
		ifeq ($(LUA51),)
			DEFS+=-I/usr/include/lua
			LIBS= -llua
		else
			DEFS+=-I/usr/include/lua5.1
			LIBS= -llua5.1
		endif
	else
		LUALIBS = $(shell pkg-config --silence-errors --libs lua)
		ifeq ($(LUALIBS),)
			LUA51 = $(shell ls /usr/lib/liblua* | grep liblua5.1)
			ifeq ($(LUA51),)
				DEFS+=-I/usr/include/lua
				LIBS= -llua
			else
				DEFS+=-I/usr/include/lua5.1
				LIBS= -llua5.1
			endif
		else
			DEFS+= $(shell pkg-config --cflags lua)
			LIBS = $(shell pkg-config --libs lua)
		endif
	endif
else
	DEFS+= $(shell lua-config --include)
	LIBS = $(shell lua-config --libs)
endif

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules

# $Id$
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
			LIBS= -llua -lm
		else
			DEFS+=-I/usr/include/lua5.1
			LIBS= -llua5.1 -lm
		endif
	else
		LUAVER=XX
		LUALIBS = $(shell pkg-config --silence-errors --libs lua)
		ifeq ($(LUALIBS),)
			LUALIBS = $(shell pkg-config --silence-errors --libs lua5.1)
			ifneq ($(LUALIBS),)
				LUAVER=51
			endif
		else
			LUAVER=5X
		endif
		ifeq ($(LUAVER),XX)
			LUA51 = $(shell ls /usr/lib/liblua* | grep liblua5.1)
			ifeq ($(LUA51),)
				DEFS+=-I/usr/include/lua
				LIBS= -llua -lm
			else
				DEFS+=-I/usr/include/lua5.1
				LIBS= -llua5.1 -lm
			endif
		else
			ifeq ($(LUAVER),51)
				DEFS+= $(shell pkg-config --cflags lua5.1)
				LIBS = $(shell pkg-config --libs lua5.1)
			else
				DEFS+= $(shell pkg-config --cflags lua)
				LIBS = $(shell pkg-config --libs lua)
			endif
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

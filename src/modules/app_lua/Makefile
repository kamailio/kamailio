#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_lua.so

LUALIB=lua

ifeq ($(LUAJIT),)
	ifeq ($(CROSS_COMPILE),)
		BUILDER = $(shell which lua-config)
	endif

	ifeq ($(BUILDER),)
		ifeq ($(CROSS_COMPILE),)
			BUILDER = $(shell which pkg-config)
		endif
		ifeq ($(BUILDER),)
			ifneq ($(shell ls $(LOCALBASE)/lib/liblua* | grep liblua5.1),)
				LUALIB=lua5.1
			else ifneq ($(shell ls $(LOCALBASE)/lib/liblua* | grep liblua5.2),)
				LUALIB=lua5.2
			else ifneq ($(shell ls $(LOCALBASE)/lib/liblua* | grep liblua5.3),)
				LUALIB=lua5.3
			else ifneq ($(shell ls $(LOCALBASE)/lib/liblua* | grep liblua5.4),)
				LUALIB=lua5.4
			endif
			DEFS+= -I$(LOCALBASE)/include -DLUA_COMPAT_MODULE -I$(LOCALBASE)/include/$(LUALIB)
			LIBS+= -L$(LOCALBASE)/lib -l$(LUALIB) -lm
		else
			ifneq ($(shell pkg-config --silence-errors --libs lua-5.1),)
				LUALIB=lua-5.1
			else ifneq ($(shell pkg-config --silence-errors --libs lua5.1),)
				LUALIB=lua5.1
			else ifneq ($(shell pkg-config --silence-errors --libs lua5.2),)
				LUALIB=lua5.2
			else ifneq ($(shell pkg-config --silence-errors --libs lua5.3),)
				LUALIB=lua5.3
			else ifneq ($(shell pkg-config --silence-errors --libs lua5.4),)
				LUALIB=lua5.4
			else ifneq ($(shell pkg-config --silence-errors --libs lua),)
				LUALIB=lua
			else
				LUALIB=
			endif
			ifeq ($(LUALIB),lua5.1)
				DEFS+= $(shell pkg-config --cflags lua5.1 | sed -e "s/\\\\/'/" -e "s/\\\\\"/\"'/")
				LIBS = $(shell pkg-config --libs lua5.1)
			else ifneq ($(LUALIB),)
				DEFS+= -DLUA_COMPAT_MODULE
				DEFS+= $(shell pkg-config --cflags $(LUALIB))
				LIBS = $(shell pkg-config --libs $(LUALIB))
			else
				LUALIB=lua
				ifneq ($(shell ls $(LOCALBASE)/lib/liblua* | grep liblua5.1),)
					LUALIB=lua5.1
				else ifneq ($(shell ls $(LOCALBASE)/lib/liblua* | grep liblua5.2),)
					LUALIB=lua5.2
				else ifneq ($(shell ls $(LOCALBASE)/lib/liblua* | grep liblua5.3),)
					LUALIB=lua5.3
				else ifneq ($(shell ls $(LOCALBASE)/lib/liblua* | grep liblua5.4),)
					LUALIB=lua5.4
				endif
				DEFS+= -I$(LOCALBASE)/include -DLUA_COMPAT_MODULE -I$(LOCALBASE)/include/$(LUALIB)
				LIBS+= -L$(LOCALBASE)/lib -l$(LUALIB) -lm
			endif
		endif
	else
		DEFS+= $(shell lua-config --include)
		LIBS = $(shell lua-config --libs)
	endif
else
	# Build using LuaJIT
	BUILDER = $(shell which pkg-config)
	ifeq ($(BUILDER),)
		DEFS+= -I/usr/include/luajit-2.1
		LIBS = -lluajit-5.1
	else
		DEFS+= $(shell pkg-config --silence-errors --cflags luajit)
		LIBS = $(shell pkg-config --silence-errors --libs luajit)
	endif
endif

include ../../Makefile.modules

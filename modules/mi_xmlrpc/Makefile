# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=mi_xmlrpc.so
LIBS=

NEW_VERSION_THRESHOLD_UP=10630         # 1.06.30
NEW_VERSION_THRESHOLD_DOWN=10310       # 1.03.10

OLD_VERSION_THRESHOLD_UP=910           # 0.09.10
OLD_VERSION_THRESHOLD_DOWN=901         # 0.09.10


# set CROSS_COMPILE to true if you want to skip
# the autodetection
# CROSS_COMPILE=true

ifeq ($(CROSS_COMPILE),)
XMLRPC-C-CONFIG=$(shell which xmlrpc-c-config)
endif

ifneq ($(XMLRPC-C-CONFIG),)
# get the version
XMLRPC_VER=$(shell $(XMLRPC-C-CONFIG) --version)
else
# try new version
XMLRPC_VER=1.03.10
endif

XMLRPC_VERSION=$(shell echo $(XMLRPC_VER) | sed  "s/\.\([0-9]\)\./.0\1./g" | sed  "s/\.\([0-9]\)\$$/.0\1/g" | tr -d "." | sed -e "s/^0*//" )


# check the versions
OLD=$(shell echo $(XMLRPC_VERSION) $(OLD_VERSION_THRESHOLD_UP) $(OLD_VERSION_THRESHOLD_DOWN) | awk '{ if (($$1 <= $$2) && ($$1 >= $$3)) print "yes"}' )
NEW=$(shell echo $(XMLRPC_VERSION) $(NEW_VERSION_THRESHOLD_UP) $(NEW_VERSION_THRESHOLD_DOWN) | awk '{ if (($$1 <= $$2) && ($$1 >= $$3)) print "yes"}' )


# supported version ?
ifeq ($(OLD),yes)
	DEFS+=-DXMLRPC_OLD_VERSION
else
ifeq ($(NEW),yes)
	# nothing to do
#else
#$(warning			You are using an unsupported libxmlrpc-c3 \
					 version  ($(XMLRPC_VER)), compile at your own risk!)
endif
endif

ifneq ($(XMLRPC-C-CONFIG),)
MY_DEFS=$(shell $(XMLRPC-C-CONFIG) abyss-server --cflags)
MY_LIBS=$(shell $(XMLRPC-C-CONFIG) abyss-server --libs)
else
MY_DEFS=-I$(LOCALBASE)/include -I$(LOCALBASE)/include/xmlrpc-c \
		-I$(SYSBASE)/include/xmlrpc-c -D_UNIX
MY_LIBS=-L$(SYSBASE)/lib  -L$(LOCALBASE)/lib -lxmlrpc -lxmlrpc_xmlparse \
		-lxmlrpc_xmltok -lxmlrpc_abyss -lxmlrpc_abyss_server \
		-lxmlrpc_client -lwwwinit -lwwwstream -lwwwxml \
		-lwwwapp -lwwwcache -lwwwcore -lwwwdir -lwwwfile \
		-lwwwftp -lwwwgopher -lwwwhtml -lwwwhttp \
		-lwwwmime -lwwwmux -lwwwnews -lwwwtelnet \
		-lwwwtrans -lwwwzip
endif

ifeq ($(NEW),yes)
	# remove the abyss library - we already have
	# the code in the module
	MY_DEFS+=-D_UNIX
	MY_LIBS:=$(filter-out %xmlrpc_abyss %xmlrpc_server_abyss %pthread, $(MY_LIBS))
else
	# remove from compiling the code we have in the
	# module for abyss server
	exclude_files=$(wildcard abyss_*.c)
endif

# Solaris has filio.h
ifeq ($(OS),solaris)
	MY_DEFS+=-DHAVE_SYS_FILIO_H
endif

DEFS+=$(MY_DEFS)
LIBS+=$(MY_LIBS)


DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules


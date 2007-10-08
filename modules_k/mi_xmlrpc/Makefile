# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=mi_xmlrpc.so
LIBS=

NEW_VERSION_THRESHOLD_UP=10618         # 1.06.18
NEW_VERSION_THRESHOLD_DOWN=10310       # 1.03.10

OLD_VERSION_THRESHOLD_UP=910           # 0.09.10
OLD_VERSION_THRESHOLD_DOWN=901         # 0.09.10


# get the version
XMLRPC-C-CONFIG=$(shell which xmlrpc-c-config)

XMLRPC_VER=$(shell $(XMLRPC-C-CONFIG) --version)

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
else
$(warning			You are using an unsupported libxmlrpc-c3 \
					 version  ($(XMLRPC_VER)), compile at your own risk!)
endif
endif

DEFS+=$(shell $(XMLRPC-C-CONFIG) abyss-server --cflags)
LIBS+=$(shell $(XMLRPC-C-CONFIG) abyss-server --libs)


include ../../Makefile.modules


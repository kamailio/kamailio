# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=mi_xmlrpc.so
LIBS=

DEFS+=-I$(LOCALBASE)/include -I$(LOCALBASE)/include/xmlrpc-c \
		-I$(SYSBASE)/include/xmlrpc-c
LIBS+=-L$(SYSBASE)/lib  -L$(LOCALBASE)/lib -lxmlrpc -lxmlrpc_xmlparse \
		-lxmlrpc_xmltok -lxmlrpc_abyss -lxmlrpc_abyss_server \
		-lxmlrpc_client -lwwwinit -lwwwstream -lwwwxml

include ../../Makefile.modules

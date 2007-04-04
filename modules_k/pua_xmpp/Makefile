# $Id: Makefile,v 1.2 2005/10/25 16:23:28 bogdan_iancu Exp $
#
# PUBLISH
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME= pua_xmpp.so
LIBS=

DEFS+=-I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 -I$(LOCALBASE)/include
LIBS+=-L/usr/include/lib  -L$(LOCALBASE)/lib -lxml2

include ../../Makefile.modules

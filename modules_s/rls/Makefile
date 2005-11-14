# $Id$
#
# Registrar Presence User Agent
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME = rls.so

# .PHONY:	libraries
#
# $(NAME):	libraries
# 
# install:	libraries
# 
# clean:	libraries
# 
# libraries:	
# 			make $(MAKECMDGOALS) -f Makefile.ser -C ../../lib

# if using libcds, the directive SER must be defined !
# and root ser directory must be in include directories

DEFS += -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 -Wall -I../../lib -DSER -I../..
LIBS += -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2 -lcurl -L../../lib/presence -L../../lib/cds -L../../lib/xcap -lxcap -lpresence -lcds

#DEFS = -I../.. -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 -Wall -I../../lib -DSER $(DEFS)
#LIBS = -L../../lib/cds -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2 -lcurl -lcds $(LIBS)

include ../../Makefile.modules

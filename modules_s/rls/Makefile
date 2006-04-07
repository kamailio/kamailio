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

DEFS += -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 -I$(LOCALBASE)/include -Wall -I../../lib -DSER -I../..
LIBS += -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2 -L../../lib/presence -L../../lib/cds -L../../lib/xcap -l_ser_xcap -l_ser_presence -l_ser_cds

#DEFS = -I../.. -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 -Wall -I../../lib -DSER $(DEFS)
#LIBS = -L../../lib/cds -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2 -lcurl -lcds $(LIBS)

include ../../Makefile.modules

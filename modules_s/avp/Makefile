# $Id$
#
# print example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=avp.so

# Comment the next two line and uncomment the following two if you want
# to enable Radius support
DEFS+= -DWITH_DB_SUPPORT
LIBS=
#DEFS+= -DWITH_RADIUS_SUPPORT
#LIBS=-L$(LOCALBASE)/lib -lradiusclient

ifeq ($(MAKELEVEL), 0)
# make called directly in the module dir!

all::
	cd ../..; make all

else

include ../../Makefile.modules

endif

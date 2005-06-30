# $Id$
#
# avp_radius module Makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
include ../../Makefile.radius

auto_gen=
NAME=avp_radius.so

LIBS=-l$(RADIUSCLIENT_LIB)

ifeq ($(MAKELEVEL), 0)
# make called directly in the module dir!

all::
	cd ../..; make all

else

include ../../Makefile.modules

endif

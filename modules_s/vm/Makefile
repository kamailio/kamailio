# $Id$
#
# print example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=vm.so
LIBS=
DEFS+= -DWITH_LDAP_SUPPORT

ifeq ($(MAKELEVEL), 0)
# make called directly in the module dir!

all::
	cd ../..; make all

else

include ../../Makefile.modules

endif

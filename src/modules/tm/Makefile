#
# tm module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=tm.so
LIBS=
DEFS+= -DSER_MOD_INTERFACE

ifeq ($(INSTALL_FLAVOUR),kamailio)
DEFS+= -DWITH_EVENT_LOCAL_REQUEST
endif # INSTALL_FLAVOUR

include ../../Makefile.modules

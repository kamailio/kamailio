# $Id: Makefile,v 1.1.1.1 2005/06/13 16:47:47 bogdan_iancu Exp $
#
# sp-ul_db module Makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile
include ../../Makefile.defs

auto_gen=
NAME=p_usrloc.so
LIBS=

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
include ../../Makefile.modules

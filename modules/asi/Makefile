# $Id$
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

NAME=asi.so

DEFS+=-DSER_MOD_INTERFACE

DEFS+=-DXLL_NULL_FIX
DEFS+=-DASI_WITH_LOCDGRAM
DEFS+=-DASI_WITH_RESYNC
DEFS+=-DASI_FAKE_408
DEFS+=-DORIG_TID_4CANCEL

DEFS+=-I$(SERLIBPATH)/binrpc2/
DEFS+=-I../../


SER_LIBS+=$(SERLIBPATH)/binrpc2/binrpc2

include ../../Makefile.modules

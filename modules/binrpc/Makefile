# $Id$
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

NAME=binrpc.so

DEFS+=-DSER_MOD_INTERFACE

DEFS+=-I$(SERLIBPATH)/binrpc2/
DEFS+=-I../../


SER_LIBS+=$(SERLIBPATH)/binrpc2/binrpc2

include ../../Makefile.modules


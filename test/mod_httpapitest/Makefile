#
# curl module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=httpapitest.so

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
#SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules

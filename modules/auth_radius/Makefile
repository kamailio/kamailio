#
# Digest Authentication - Radius support
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
include ../../Makefile.radius

auto_gen=
NAME=auth_radius.so

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules

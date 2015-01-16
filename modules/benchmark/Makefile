#
# User aliases
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=benchmark.so
LIBS=
DEFS+=

# nanosec precision - requires realtime library
#DEFS+= -DBM_CLOCK_REALTIME
#LIBS+= -lrt

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules

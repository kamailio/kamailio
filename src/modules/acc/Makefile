# acc module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=acc.so
LIBS=

# set ENABLE_DIAMETER_ACC to true if you wish to enable DIAMETER accounting
#   (uncomment next line or 'ENABLE_DIAMETER_ACC=true make all')
#ENABLE_DIAMETER_ACC=true
ifeq ($(ENABLE_DIAMETER_ACC),true)
DEFS+=-DDIAM_ACC
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1

include ../../Makefile.modules

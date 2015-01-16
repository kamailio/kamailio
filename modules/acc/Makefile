# acc module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=acc.so
LIBS=

# comment the next line if you wish to disable SQL accounting
DEFS+=-DSQL_ACC

# set ENABLE_RADIUS_ACC to true if you wish to enable RADIUS accounting
#   (uncomment next line or 'ENABLE_RADIUS_ACC=true make all')
#ENABLE_RADIUS_ACC=true
ifeq ($(ENABLE_RADIUS_ACC),true)
include ../../Makefile.radius
DEFS+=-DRAD_ACC
endif

# set ENABLE_DIAMETER_ACC to true if you wish to enable DIAMETER accounting
#   (uncomment next line or 'ENABLE_DIAMETER_ACC=true make all')
#ENABLE_DIAMETER_ACC=true
ifeq ($(ENABLE_DIAMETER_ACC),true)
DEFS+=-DDIAM_ACC
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1 $(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules

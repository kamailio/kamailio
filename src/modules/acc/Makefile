# acc module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=acc.so
LIBS=

ifeq ($(CROSS_COMPILE),)
JSON_SUPPORTED=$(shell \
	if pkg-config --exists jansson; then \
		echo 'libjansson found'; \
	fi)
endif
ifneq ($(JSON_SUPPORTED),)
	DEFS+= $(shell pkg-config --cflags jansson)
	DEFS+= -DWITH_JSON
	LIBS+= $(shell pkg-config --libs jansson)
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

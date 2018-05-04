#
# cnxcc module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=cnxcc.so

ifeq ($(CROSS_COMPILE),)
HIREDIS_BUILDER = $(shell \
	if pkg-config --exists hiredis; then \
		echo 'pkg-config hiredis'; \
	fi)
endif

ifeq ($(HIREDIS_BUILDER),)
	HIREDISDEFS=-I$(LOCALBASE)/include
	HIREDISLIBS=-L$(LOCALBASE)/lib -lhiredis
else
	HIREDISDEFS = $(shell $(HIREDIS_BUILDER) --cflags)
	HIREDISLIBS = $(shell $(HIREDIS_BUILDER) --libs)

ifeq ($(HIREDISLIBS),-L -lhiredis)
		HIREDISDEFS = $(shell $(HIREDIS_BUILDER) --cflags) /opt/local/include
		HIREDISLIBS = -L/opt/local/lib -lhiredis
endif

endif

ifeq ($(CROSS_COMPILE),)
LEVENT_BUILDER = $(shell \
	if pkg-config --exists libevent; then \
		echo 'pkg-config libevent'; \
	fi)
endif

ifeq ($(LEVENT_BUILDER),)
	LEVENTDEFS=
	LEVENTLIBS=-L$(LOCALBASE)/lib -levent
else
	LEVENTDEFS = $(shell $(LEVENT_BUILDER) --cflags)
	LEVENTLIBS = $(shell $(LEVENT_BUILDER) --libs)
endif

DEFS+=$(HIREDISDEFS) $(LEVENTDEFS)
LIBS=$(HIREDISLIBS) $(LEVENTLIBS)

DEFS+=-DKAMAILIO_MOD_INTERFACE
SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=topos_redis.so

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

DEFS+=$(HIREDISDEFS)
LIBS=$(HIREDISLIBS)

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules

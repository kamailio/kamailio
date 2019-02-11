#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=ndb_redis.so

ifeq ($(CROSS_COMPILE),)
HIREDIS_BUILDER = $(shell \
	if pkg-config --exists hiredis; then \
		echo 'pkg-config hiredis'; \
	fi)
endif

ifeq ($(HIREDIS_BUILDER),)
	HIREDISDEFS=-I$(LOCALBASE)/include -I$(LOCALBASE)/include/hiredis  -I/usr/include/hiredis
	HIREDISLIBS=-L$(LOCALBASE)/lib -lhiredis
else
	HIREDISDEFS = $(shell $(HIREDIS_BUILDER) --cflags)
	HIREDISLIBS = $(shell $(HIREDIS_BUILDER) --libs)

ifeq (,$(findstring hiredis,$(HIREDISDEFS)))
	DEFS+=-DWITH_HIREDIS_PATH
endif

ifeq ($(HIREDISLIBS),-L -lhiredis)
		HIREDISDEFS = $(shell $(HIREDIS_BUILDER) --cflags) /opt/local/include
		HIREDISLIBS = -L/opt/local/lib -lhiredis
endif

endif

DEFS+=$(HIREDISDEFS)
LIBS=$(HIREDISLIBS)

include ../../Makefile.modules

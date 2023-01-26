#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=db_redis.so

ifeq ($(CROSS_COMPILE),)
HIREDIS_BUILDER = $(shell \
	if pkg-config --exists hiredis; then \
		echo 'pkg-config hiredis'; \
	fi)

HIREDIS_CLUSTER_BUILDER = $(shell \
	if pkg-config --exists hiredis_cluster; then \
		echo 'pkg-config hiredis_cluster'; \
	fi)

endif

ifeq ($(HIREDIS_BUILDER),)
	HIREDISDEFS=-I$(LOCALBASE)/include -I$(LOCALBASE)/include/hiredis -I/usr/include/hiredis
	HIREDISLIBS=-L$(LOCALBASE)/lib -lhiredis_ssl -lhiredis
else
	HIREDISDEFS = $(shell $(HIREDIS_BUILDER) --cflags)
	HIREDISLIBS = $(shell $(HIREDIS_BUILDER) --libs) -lhiredis_ssl

ifeq (,$(findstring hiredis,$(HIREDISDEFS)))
	DEFS+=-DWITH_HIREDIS_PATH
endif

ifeq ($(HIREDISLIBS),-L -lhiredis)
		HIREDISDEFS = $(shell $(HIREDIS_BUILDER) --cflags) /opt/local/include
		HIREDISLIBS = -L/opt/local/lib -lhiredis -lhiredis_ssl
endif

endif

DEFS+=$(HIREDISDEFS)
LIBS=$(HIREDISLIBS)

ifneq ($(HIREDIS_CLUSTER_BUILDER),)
	HIREDISCLUSTERDEFS = $(shell $(HIREDIS_CLUSTER_BUILDER) --cflags)
	HIREDISCLUSTERLIBS = $(shell $(HIREDIS_CLUSTER_BUILDER) --libs) -lhiredis_ssl
	DEFS+=-DWITH_HIREDIS_CLUSTER
	DEFS+=$(HIREDISCLUSTERDEFS)
	LIBS+=$(HIREDISCLUSTERLIBS)
endif

SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1

include ../../Makefile.modules

install-redis-scripts: $(bin_prefix)/$(bin_dir)
	REDISON=yes $(MAKE) -C ../../../utils/kamctl/ install-modules

install-scripts: install-redis-scripts

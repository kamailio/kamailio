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
	HIREDISLIBS=-L$(LOCALBASE)/lib -lhiredis
	ifneq ($(shell ls $(LOCALBASE) | grep libhiredis_ssl.so),)
		HIREDISDEFS += -DWITH_SSL
		HIREDISLIBS += -lhiredis_ssl
	endif
else
	HIREDISDEFS = $(shell $(HIREDIS_BUILDER) --cflags)
	HIREDISLIBS = $(shell $(HIREDIS_BUILDER) --libs)
	HIREDISLIBSPATH = $(shell $(HIREDIS_BUILDER) --libs-only-L | cut -c 3-)
    ifeq ($(HIREDISLIBSPATH),)
        GCCSEARCHDIRS = $(shell $(CC) -print-search-dirs | grep -Eo '^.*libraries: =.*' | cut -d "=" -f2- | tr : ' ')
        ifneq ($(shell find $(GCCSEARCHDIRS) -name libhiredis_ssl.so 2>/dev/null),)
		    HIREDISDEFS += -DWITH_SSL
		    HIREDISLIBS += -lhiredis_ssl
        endif
    else
        ifneq ($(shell ls $(HIREDISLIBSPATH) | grep libhiredis_ssl.so),)
		HIREDISDEFS += -DWITH_SSL
		HIREDISLIBS += -lhiredis_ssl
        endif
    endif

ifeq (,$(findstring hiredis,$(HIREDISDEFS)))
	DEFS+=-DWITH_HIREDIS_PATH
endif

ifeq ($(HIREDISLIBS),-L -lhiredis)
		HIREDISDEFS = $(shell $(HIREDIS_BUILDER) --cflags) /opt/local/include
		HIREDISLIBS = -L/opt/local/lib -lhiredis
		ifneq ($(shell ls /opt/local/lib | grep libhiredis_ssl.so),)
			HIREDISDEFS += -DWITH_SSL
			HIREDISLIBS += -lhiredis_ssl
		endif
endif

endif

DEFS+=$(HIREDISDEFS)
LIBS=$(HIREDISLIBS)

ifneq ($(HIREDIS_CLUSTER_BUILDER),)
	HIREDISCLUSTERDEFS = $(shell $(HIREDIS_CLUSTER_BUILDER) --cflags)
	HIREDISCLUSTERLIBS = $(shell $(HIREDIS_CLUSTER_BUILDER) --libs)
	HIREDISCLUSTERLIBSPATH = $(shell $(HIREDIS_CLUSTER_BUILDER) --libs-only-L | cut -c 3-)
	ifneq ($(shell ls $(HIREDISCLUSTERLIBSPATH) | grep libhiredis_cluster_ssl.so),)
		HIREDISCLUSTERDEFS += -DWITH_SSL
		HIREDISCLUSTERLIBS += -lhiredis_cluster_ssl
	endif
	DEFS+=-DWITH_HIREDIS_CLUSTER
	DEFS+=$(HIREDISCLUSTERDEFS)
	LIBS+=$(HIREDISCLUSTERLIBS)
endif

include ../../Makefile.modules

install-redis-scripts: $(bin_prefix)/$(bin_dir)
	REDISON=yes $(MAKE) -C ../../../utils/kamctl/ install-modules

install-scripts: install-redis-scripts

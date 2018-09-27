# acc module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=acc_json.so
LIBS=

ifeq ($(CROSS_COMPILE),)
JSON_SUPPORTED=$(shell \
	if pkg-config --exists jansson; then \
		echo 'libjansson found'; \
	fi)
endif
ifneq ($(JSON_SUPPORTED),)
	DEFS+= $(shell pkg-config --cflags jansson)
	LIBS+= $(shell pkg-config --libs jansson)
else
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -ljansson
endif

include ../../Makefile.modules

#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=kafka.so

ifeq ($(CROSS_COMPILE),)
RDKAFKA_SUPPORTED=$(shell  \
	if pkg-config --exists rdkafka; then \
		echo 'librdkafka found'; \
	fi)
endif

ifneq ($(RDKAFKA_SUPPORTED),)
	DEFS+= $(shell pkg-config --cflags rdkafka)
	LIBS+= $(shell pkg-config --libs rdkafka)
else
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -lrdkafka
endif

include ../../Makefile.modules

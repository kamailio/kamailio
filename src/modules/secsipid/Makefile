#
# secsipid module makefile
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=secsipid.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
ifneq ($(BUILDER),)
	PKGLIBSECSIPID = $(shell $(BUILDER) --exists secsipid-1 > /dev/null 2>&1 ; echo $$? )
ifneq ($(PKGLIBSECSIPID),0)
	BUILDER =
endif
endif
endif

ifeq ($(BUILDER),)
	DEFS+= -I.
	LIBS = -L. -lsecsipid
else
	DEFS+= $(shell pkg-config --cflags secsipid-1)
	LIBS = $(shell pkg-config --libs secsipid-1)
endif

ifeq ($(OS), darwin)
LIBS+= -framework CoreFoundation -framework Security -lpthread
endif

include ../../Makefile.modules

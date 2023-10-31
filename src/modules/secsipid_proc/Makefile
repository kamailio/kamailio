#
# secsipid module makefile
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=secsipid_proc.so

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
	DEFS +=-I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS +=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lsecsipid -lpthread
else
	DEFS+= $(shell pkg-config --cflags secsipid-1)
	LIBS = $(shell pkg-config --libs secsipid-1) -lpthread
endif

ifeq ($(OS), darwin)
LIBS+= -framework CoreFoundation -framework Security
endif

include ../../Makefile.modules

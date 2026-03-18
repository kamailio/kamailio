#
# secsipid module makefile
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=ruxc.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
ifneq ($(BUILDER),)
	PKGLIBRUXC = $(shell $(BUILDER) --exists libruxc > /dev/null 2>&1 ; echo $$? )
ifneq ($(PKGLIBRUXC),0)
	BUILDER =
endif
endif
endif

ifeq ($(BUILDER),)
	DEFS+= -I. -I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS = -L. -L$(LOCALBASE)/lib -lruxc
else
	DEFS+= $(shell pkg-config --cflags libruxc)
	LIBS = $(shell pkg-config --libs libruxc)
endif

ifeq ($(OS), darwin)
LIBS+= -framework CoreFoundation -framework Security
endif

include ../../Makefile.modules

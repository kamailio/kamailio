#
# stirshaken module makefile
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=stirshaken.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
ifneq ($(BUILDER),)
	PKGLIBSECSIPID = $(shell $(BUILDER) --exists stirshaken > /dev/null 2>&1 ; echo $$? )
ifneq ($(PKGLIBSECSIPID),0)
	BUILDER =
endif
endif
endif

ifeq ($(BUILDER),)
	DEFS+= -I.
	LIBS = -L. -lstirshaken
else
	DEFS+= $(shell pkg-config --cflags stirshaken)
	LIBS = $(shell pkg-config --libs stirshaken)
endif

include ../../Makefile.modules

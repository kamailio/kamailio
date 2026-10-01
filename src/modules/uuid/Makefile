#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=uuid.so

ifeq ($(CROSS_COMPILE),)
BUILDER = $(shell \
	if pkg-config --exists uuid; then \
		echo 'pkg-config uuid'; \
	fi)
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags)
	LIBS += $(shell $(BUILDER) --libs)
	# uuid_generate_time_v7() is available since util-linux 2.41
	ifeq ($(shell $(BUILDER) --atleast-version=2.41; echo $$?),0)
		DEFS += -DHAVE_UUID_GENERATE_TIME_V7
	endif
else
	DEFS +=-I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -luuid
endif

include ../../Makefile.modules

#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=gcrypt.so

ifeq ($(CROSS_COMPILE),)
GCRYPT_BUILDER=$(shell \
	if pkg-config --exists libgcrypt; then \
		echo 'pkg-config libgcrypt'; \
	fi)
endif

ifneq ($(GCRYPT_BUILDER),)
	DEFS += $(shell $(GCRYPT_BUILDER) --cflags)
	LIBS += $(shell $(GCRYPT_BUILDER) --libs)
else
	DEFS += -I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib \
			-L$(LOCALBASE)/lib64 \
			-lgcrypt
endif

include ../../Makefile.modules

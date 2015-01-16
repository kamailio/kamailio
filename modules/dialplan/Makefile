#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=dialplan.so

ifeq ($(CROSS_COMPILE),)
PCRE_BUILDER = $(shell \
	if pkg-config --exists libcre; then \
		echo 'pkg-config libpcre'; \
	else \
		which pcre-config; \
	fi)
endif

ifeq ($(PCRE_BUILDER),)
	PCREDEFS=-I$(LOCALBASE)/include
	PCRELIBS=-L$(LOCALBASE)/lib -lpcre
else
	PCREDEFS = $(shell $(PCRE_BUILDER) --cflags)
	PCRELIBS = $(shell $(PCRE_BUILDER) --libs)
endif
DEFS+=$(PCREDEFS)
LIBS=$(PCRELIBS)

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

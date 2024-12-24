#
# XMLOPS: XML operations
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=xmlops.so
LIBS=

ifeq ($(CROSS_COMPILE),)
PKGCFGTOOL=$(shell which xml2-config)
ifeq ($(PKGCFGTOOL),)
PKGCFGTOOL=$(shell \
	if pkg-config --exists libxml-2.0; then \
		echo 'pkg-config libxml-2.0'; \
	fi)
endif
endif

ifneq ($(PKGCFGTOOL),)
	DEFS += $(shell $(PKGCFGTOOL) --cflags )
	LIBS += $(shell $(PKGCFGTOOL) --libs)
else
	DEFS+=-I$(LOCALBASE)/include/libxml2 \
		-I$(LOCALBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -lxml2
endif
${eval LIBS=$(LIBS)}

include ../../Makefile.modules

# cdp Makefile

include ../../Makefile.defs
auto_gen=
NAME=cdp.so

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
ifeq ($(XML2CFG),)
XML2CFG=$(shell \
	if pkg-config --exists libxml-2.0; then \
		echo 'pkg-config libxml-2.0'; \
	fi)
endif
endif

ifneq ($(XML2CFG),)
	DEFS += $(shell $(XML2CFG) --cflags )
	LIBS += $(shell $(XML2CFG) --libs)
else
	INCLUDES +=-I$(LOCALBASE)/include/libxml2 \
		-I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -lxml2
endif

ifneq ($(OS),darwin)
	LIBS += -lrt
	LIBS += -lpthread
	LIBS += -lssl
	LIBS += -lcrypto
endif
${eval LIBS=$(LIBS)}

include ../../Makefile.modules


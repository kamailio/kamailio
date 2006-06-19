# $Id$

include ../../Makefile.defs

auto_gen=
NAME=sercmd
RELEASE=0.1

readline_locations= /usr/include/readline/readline.h \
					$(LOCALBASE)/usr/include/readline/readline.h
use_readline ?=

DEFS:= -DNAME='"$(NAME)"' -DVERSION='"$(RELEASE)"' \
		$(filter -D%HAVE -DARCH% -DOS% -D__CPU% -D__OS%, $(DEFS))
LIBS:=$(filter-out -lfl  -ldl -lpthread -lssl -lcrypto, $(LIBS))

ifeq ($(use_readline),)
readline_path := $(shell  \
						for r in $(readline_locations) ""; do \
							if [ -r "$$r" ] ; then echo $$r; exit; fi;  \
						done;\
					)
ifneq ($(readline_path),)
use_readline := 1
endif
endif

ifeq ($(use_readline),1)
	DEFS+=-DUSE_READLINE
	LIBS+=-lreadline -lncurses
endif



include ../../Makefile.sources
include ../../Makefile.rules

$(NAME).o: msg

.PHONY: msg
msg:
	@if [ "$(use_readline)" = "1" ]; then \
		echo; echo "readline detected ($(readline_path)):"; \
		echo "command completion enabled"; echo ; \
	else \
		echo ; \
		echo "no readline include files detected, disabling readline support";\
		echo "command completion disabled"; \
		echo "(to force readline support try 'make use_readline=1')";\
		echo ; \
	fi

modules:

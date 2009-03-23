# $Id$

include ../../Makefile.defs
include ../../Makefile.targets

auto_gen=
NAME=sercmd
RELEASE=0.1

readline_locations= /usr/include/readline/readline.h \
					$(LOCALBASE)/include/readline/readline.h

use_readline ?=
ifneq (,$(MAKECMDGOALS))
ifeq (,$(filter-out $(nodep_targets),$(MAKECMDGOALS)))
#set it to empty, we don't need to detect/use it for clean, doc a.s.o
override use_readline:=
quiet=1
endif
endif #ifneq (,$(MAKECMDGOALS))


# erase common DEFS (not needed)
C_DEFS:=
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

ifeq (,$(quiet))
ifeq ($(use_readline),1)
$(info readline detected ($(readline_path)) )
$(info command completion enabled)
else
$(info "no readline include files detected, disabling readline support")
$(info "command completion disabled" )
$(info "to force readline support try 'make use_readline=1'")
endif
endif # ifeq (,$(quiet))

$(NAME).o: 

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

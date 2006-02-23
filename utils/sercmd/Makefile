# $Id$

include ../../Makefile.defs

auto_gen=
NAME=sercmd
RELEASE=0.1
use_readline=1
DEFS:= -DNAME='"$(NAME)"' -DVERSION='"$(RELEASE)"' \
		$(filter -D%HAVE -DARCH% -DOS% -D__CPU% -D__OS%, $(DEFS))
LIBS:=$(filter-out -lfl  -ldl -lpthread -lssl -lcrypto, $(LIBS))

ifeq ($(use_readline),1)
	DEFS+=-DUSE_READLINE
	LIBS+=-lreadline
endif

include ../../Makefile.sources
include ../../Makefile.rules


modules:

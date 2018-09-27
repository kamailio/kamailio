#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=erlang.so

ERLANG=$(shell which erl)

ifneq ($(ERLANG),)
ERLANG_LIBDIR=$(shell $(ERLANG) -noshell -eval 'io:format("~n~s/lib~n", [[code:lib_dir("erl_interface")]]).' -s erlang halt | tail -n 1)
ERLANG_INCDIR=$(shell $(ERLANG) -noshell -eval 'io:format("~n~s/include~n", [[code:lib_dir("erl_interface")]]).' -s erlang halt | tail -n 1)
endif

ifeq ($(ERLANG_LIBDIR)$(ERLANG_INCDIR),)
$(error Not found Erlang)
endif

LIBS=-L$(ERLANG_LIBDIR) -lei -lpthread

DEFS+=-I$(ERLANG_INCDIR)
DEFS+=-D_REENTRANT


include ../../Makefile.modules


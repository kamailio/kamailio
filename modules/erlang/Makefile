# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=erlang.so

ERLANG=$(shell which erl)

ifneq ($(ERLANG),)
ERLANG_LIBDIR=$(shell $(ERLANG) -noshell -eval 'io:format("~n~s/lib~n", [[code:lib_dir("erl_interface")]]).' -s erlang halt | tail -n 1)
ERLANG_INCDIR=$(shell $(ERLANG) -noshell -eval 'io:format("~n~s/include~n", [[code:lib_dir("erl_interface")]]).' -s erlang halt | tail -n 1)
else
$(error Not found Erlang)
endif
LIBS=-L$(ERLANG_LIBDIR) -lei -lpthread

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

DEFS+=-I$(ERLANG_INCDIR)
DEFS+=-DKAMAILIO_MOD_INTERFACE -D_REENTRANT


include ../../Makefile.modules


#

COREPATH=../..
include $(COREPATH)/Makefile.defs
include $(COREPATH)/Makefile.targets

auto_gen=
NAME=protoshoot
RELEASE=0.2

ifneq (,$(findstring -DUSE_SCTP,$(C_DEFS)))
SCTP=1
endif

ifeq ($(SCTP),1)
ifeq ($(OS), linux)
	LIBS+= -lsctp
endif
endif

include $(COREPATH)/Makefile.utils

$(NAME).o:

modules:


#

COREPATH=../../../src
include $(COREPATH)/Makefile.defs
include $(COREPATH)/Makefile.targets

auto_gen=
NAME=protoshoot
RELEASE=0.2

ifeq ($(SCTP),0)
C_DEFS:=$(filter-out -DUSE_SCTP, $(C_DEFS))
else
ifneq (,$(findstring -DUSE_SCTP,$(C_DEFS)))
SCTP?=1
endif
endif

ifeq ($(SCTP),1)
ifeq ($(OS), linux)
	LIBS+= -lsctp
endif
endif

include $(COREPATH)/Makefile.utils

$(NAME).o:

modules:


#
# ims_charging make file
#
#

include ../../Makefile.defs
auto_gen=
NAME=ims_charging.so

LIBS += -lm

ifneq ($(OS),darwin)
	LIBS += -lrt
endif

include ../../Makefile.modules

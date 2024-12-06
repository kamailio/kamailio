#
# ims_qos_npn make file
#
#

include ../../Makefile.defs
auto_gen=
NAME=ims_qos_npn.so
LIBS=

ifneq ($(OS),darwin)
	LIBS += -lrt
	LIBS += -lpthread
endif

include ../../Makefile.modules

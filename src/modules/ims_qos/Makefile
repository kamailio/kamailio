#
# ims_qos make file
#
#

include ../../Makefile.defs
auto_gen=
NAME=ims_qos.so
LIBS=

ifneq ($(OS),darwin)
	LIBS += -lrt
	LIBS += -lpthread
endif

include ../../Makefile.modules

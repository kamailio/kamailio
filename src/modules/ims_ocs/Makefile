#
# ims_ocs make file
#
#

include ../../Makefile.defs
auto_gen=
NAME=ims_ocs.so
LIBS=

ifneq ($(OS),darwin)
	LIBS += -lrt
	LIBS += -lpthread
endif

include ../../Makefile.modules

#
# ims_diameter_server make file
#
#

include ../../Makefile.defs
auto_gen=
NAME=ims_diameter_server.so
LIBS=-lm

ifneq ($(OS),darwin)
	LIBS += -lrt
	LIBS += -lpthread
endif

include ../../Makefile.modules

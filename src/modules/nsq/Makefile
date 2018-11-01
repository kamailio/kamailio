#
# NSQ module
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=nsq.so

LIBS=-lnsq -lev -levbuffsock -lcurl -ljson-c
DEFS+=-I$(LOCALBASE)/include -I/usr/local/include $(shell pkg-config --cflags json-c)

include ../../Makefile.modules

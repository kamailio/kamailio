
include ../../../Makefile.defs

CFLAGS += -g -I../../../lib/binrpc/binrpc/include/ -L../../../lib/binrpc/binrpc/src/
CFLAGS += -DNODEBUG

LDLIBS += -lbinrpc -lpthread

all: srv

re: clean all

clean:
	rm -rf srv


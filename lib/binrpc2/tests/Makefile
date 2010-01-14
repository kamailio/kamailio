
CFLAGS+=-I../include -L../src
LDLIBS+=-lbinrpc

SRC=$(wildcard test_*.c)
DEP=$(SRC:.c=.d)
BIN=$(SRC:.c=)

%.d: %.c Makefile*
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $(subst .c,,$<)

.PHONY: all
all: libbinrpc $(DEP)

.PHONY: libbinrpc
ifeq ($(MAKELEVEL),0)
libbinrpc:
	$(MAKE) -C ..
else
libbinrpc:
endif # MAKELEVEL

clean:
	rm -rf $(BIN) $(DEP)


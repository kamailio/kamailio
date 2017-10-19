#
# app_sqlang module makefile
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_sqlang.so

DEFS+=-DKAMAILIO_MOD_INTERFACE -I./squirrel/include
LIBS += -L. -lsquirrel -lsqstdlib -lstdc++

MKSQLANG=

SQLANGARCHBSZ= $(shell echo $(ARCH) | sed -e 's/.*64.*/64b/')
ifeq ($(SQLANGARCHBSZ),64b)
	MKSQLANG=sq64
endif

include ../../Makefile.modules

libsquirrel.a:
	$(MAKE) -C ./squirrel/squirrel CC_EXTRA_FLAGS="-fPIC" \
		OUT="../../libsquirrel.a" $(MKSQLANG)

libsqstdlib.a:
	$(MAKE) -C ./squirrel/sqstdlib CC_EXTRA_FLAGS="-fPIC" \
		OUT="../../libsqstdlib.a" $(MKSQLANG)

app_sqlang.so: libsquirrel.a libsqstdlib.a

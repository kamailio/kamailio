#
# app_sqlang module makefile
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_sqlang.so

DEFS+=-I./squirrel/include
LIBS += -L. -lsquirrel -lsqstdlib -lstdc++

MKSQLANG=
SQMARCH64=

SQLANGARCHBSZ= $(shell echo $(ARCH) | sed -e 's/.*64.*/64b/')
ifeq ($(SQLANGARCHBSZ),64b)
	MKSQLANG=sq64
endif

ifeq ($(ARCH), x86_64)
	SQMARCH64=-m64
endif

ifeq ($(ARCH), sparc64)
	SQMARCH64=-m64
endif

ifeq ($(ARCH), arm6)
	SQMARCH64="-march=armv6"
endif

ifeq ($(ARCH), aarch64)
	SQMARCH64=
endif

ifeq ($(ARCH), mips64)
	SQMARCH64=-mips64
endif

include ../../Makefile.modules

libsquirrel.a:
	$(MAKE) -C ./squirrel/squirrel SQMARCH64="$(SQMARCH64)" CC_EXTRA_FLAGS="-fPIC" \
		OUT="../../libsquirrel.a" $(MKSQLANG)

libsqstdlib.a:
	$(MAKE) -C ./squirrel/sqstdlib SQMARCH64="$(SQMARCH64)" CC_EXTRA_FLAGS="-fPIC" \
		OUT="../../libsqstdlib.a" $(MKSQLANG)

app_sqlang.so: libsquirrel.a libsqstdlib.a

#
# app_sqlang module makefile
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_sqlang.so

DEFS+=-DKAMAILIO_MOD_INTERFACE -I./squirrel/include
LIBS += -L. -lsquirrel -lsqstdlib

include ../../Makefile.modules

libsquirrel.a:
	make -C ./squirrel/squirrel

libsqstdlib.a:
	make -C ./squirrel/sqstdlib

app_sqlang.so: libsquirrel.a libsqstdlib.a

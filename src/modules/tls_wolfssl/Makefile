#
# TLS module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=tls_wolfssl.so

LIBS += lib/lib/libwolfssl.a
INCLUDES += -I./lib/include/


include ../../Makefile.modules
$(objs): lib/include/wolfssl/options.h

lib/include/wolfssl/options.h lib/lib/libwolfssl.a:
	@cd ./lib/wolfssl; \
	if [ ! -f "configure" ]; then \
		./autogen.sh; \
	fi; \
	if [ ! -f "Makefile" ]; then \
		env -u DEFS -u CFLAGS -u LDFLAGS -u LIBS EXTRA_CFLAGS="-g -fPIC -Wno-error=array-bounds -Wno-error=stringop-overflow" ./configure \
		--enable-all --enable-pkcs11 --enable-static --enable-aligndata=no \
		--disable-shared --disable-examples \
		--prefix=$(CURDIR)/lib \
		--exec-prefix=$(CURDIR)/lib; \
	fi;
	@$(MAKE) -C ./lib/wolfssl install

$(NAME): lib/lib/libwolfssl.a

clean-wolfssl:
	@rm -rf ./lib/{bin,include,share,lib}; \
	(cd ./lib/wolfssl; make distclean) || /bin/true

clean-module:
	@rm -f *.o *.so

clean-all: clean-wolfssl clean-module

.PHONY: clean-wolfssl clean-module clean-all

# debugging for dependency on lib/wolfssl
$(warning sources = $(sources), auto_gen = $(auto_gen), objs = $(objs))

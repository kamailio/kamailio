#
# TLS module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=tls_wolfssl.so

LIBS += lib/lib/libwolfssl.a
DEFS += -I./lib/include/


include ../../Makefile.modules
SOURCES := $(shell ls *.c)
OBJECTS = $(SOURCES:.c=.o)
MOD_DIR := $(shell pwd)

$(OBJECTS): lib/lib/libwolfssl.a

lib/lib/libwolfssl.a:
	@cd ./lib/wolfssl; \
	if [ ! -f "configure" ]; then \
		./autogen.sh; \
	fi; \
	if [ ! -f "Makefile" ]; then \
		env -u DEFS -u CFLAGS -u LDFLAGS -u LIBS EXTRA_CFLAGS="-g -fPIC" ./configure \
		--enable-all --enable-pkcs11 --enable-static --enable-aligndata=no \
		--disable-shared --disable-silent-rules --disable-examples \
		--prefix=$(MOD_DIR)/lib \
		--exec-prefix=$(MOD_DIR)/lib; \
		sed -i s/-Werror// Makefile; \
	fi;
	@$(MAKE) -C ./lib/wolfssl install

$(NAME): lib/lib/libwolfssl.a

clean-wolfssl:
	@rm -rf ./lib/{bin,include,share,lib}; \
	cd ./lib/wolfssl; make clean || /bin/true

clean-module:
	@rm -f *.o *.so

clean-all: clean-wolfssl clean-module
  
.PHONY: clean-wolfssl clean-module clean-all

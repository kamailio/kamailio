#
# TLS module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=tls_wolfssl.so

WOLFSSL_PREFIX = ../../../misc/external/wolfssl/build
WOLFSSL_SRC = ../../../misc/external/wolfssl/wolfssl

LIBS += $(WOLFSSL_PREFIX)/lib/libwolfssl.a
INCLUDES += -I$(WOLFSSL_PREFIX)/include/

include ../../Makefile.modules
$(objs): $(WOLFSSL_PREFIX)/include/wolfssl/options.h

$(WOLFSSL_PREFIX)/include/wolfssl/options.h $(WOLFSSL_PREFIX)/lib/libwolfssl.a:
	@cd $(WOLFSSL_SRC); \
	if [ ! -f "configure" ]; then \
		./autogen.sh; \
	fi; \
	if [ ! -f "Makefile" ]; then \
		env -u DEFS -u CFLAGS -u LDFLAGS -u LIBS EXTRA_CFLAGS="-g -fPIC" ./configure \
		--enable-all --enable-pkcs11 --enable-static --enable-aligndata=no \
		--disable-shared --disable-examples \
		--prefix=$(CURDIR)/$(WOLFSSL_PREFIX) \
		--exec-prefix=$(CURDIR)/$(WOLFSSL_PREFIX); \
	fi;
	@$(MAKE) -C $(WOLFSSL_SRC) install

$(NAME): $(WOLFSSL_PREFIX)/lib/libwolfssl.a

clean-wolfssl:
	@rm -rf $(WOLFSSL_PREFIX)/{bin,include,share,lib}; \
	(cd $(WOLFSSL_SRC); make distclean) || /bin/true

clean-module:
	@rm -f *.o *.so

clean-all: clean-wolfssl clean-module

.PHONY: clean-wolfssl clean-module clean-all

# debugging for dependency on lib/wolfssl
$(warning sources = $(sources), auto_gen = $(auto_gen), objs = $(objs))

#
# TLSA module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=tlsa.so

# set to yes when wanting to link with static libraries compiled from source
LIBSSL_STATIC_SRCLIB ?= no
# set to the path of the folder with static libraries compiled from source
LIBSSL_STATIC_SRCPATH ?= /usr/local/src/openssl
# or set to the path where the static libraries are installed, for example:
# LIBSSL_STATIC_SRCPATH ?= /usr/lib/x86_64-linux-gnu/

ifeq ($(CROSS_COMPILE),)
SSL_BUILDER=$(shell \
	if pkg-config --exists libssl; then \
		echo 'pkg-config libssl'; \
	fi)
CRYPTO_BUILDER=$(shell \
	if pkg-config --exists libcrypto; then \
		echo 'pkg-config libcrypto'; \
	fi)
endif

DEFS += -DKSR_LIBSSL_STATIC

ifneq ($(SSL_BUILDER),)

ifneq ($(LIBSSL_STATIC_SRCLIB),yes)
	## when static libs (*.a) from packages are compiled with -fPIC
	DEFS += $(shell $(SSL_BUILDER) --cflags)
	LIBS += $(shell $(SSL_BUILDER) --libs-only-L)
ifneq ($(CRYPTO_BUILDER),)
	DEFS += $(shell $(CRYPTO_BUILDER) --cflags)
	LIBS += $(shell $(CRYPTO_BUILDER) --libs-only-L)
endif # ifneq ($(CRYPTO_BUILDER),)

	LIBS += -l:libssl.a -l:libcrypto.a -l:libz.a -ldl
	# TODO: explore use of LIBS += -Wl,-Bstatic $(shell $(SSL_BUILDER) --libs-only-l)

else
	## when linking against static libs compiled from sources
	## default OpenSSL installation has lib/,lib64/ as siblings of include/
	ifneq "$(wildcard $(LIBSSL_STATIC_SRCPATH)/../include )" ""
		DEFS += -I$(LIBSSL_STATIC_SRCPATH)/../include
	else
		DEFS += -I$(LIBSSL_STATIC_SRCPATH)/include
	endif
	LIBS += $(LIBSSL_STATIC_SRCPATH)/libssl.a $(LIBSSL_STATIC_SRCPATH)/libcrypto.a
endif # ifneq ($(LIBSSL_STATIC_SRCLIB),yes)

else # ifneq ($(SSL_BUILDER),)

	DEFS += -I$(LOCALBASE)/ssl/include
	LIBS += -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib \
			-L$(LOCALBASE)/lib64 -L$(LOCALBASE)/ssl/lib64 \
			-l:libssl.a -l:libcrypto.a -l:libz.a -ldl
endif # ifneq ($(SSL_BUILDER),)

include ../../Makefile.modules

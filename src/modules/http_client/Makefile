#
# http_client module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=http_client.so

ifeq ($(CROSS_COMPILE),)
CURL_BUILDER=$(shell \
	if pkg-config --exists libcurl; then \
		echo 'pkg-config libcurl'; \
	else \
		which curl-config; \
	fi)
endif

# set to yes when wanting to link with static libraries
LIBSSL_STATIC ?= no
# set to yes when wanting to link with static libraries compiled from source
LIBSSL_STATIC_SRCLIB ?= no
# set to the path of the folder with static libraries compiled from source
LIBSSL_STATIC_SRCPATH ?= /usr/local/src/openssl

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

ifneq ($(SSL_BUILDER),)

ifneq ($(LIBSSL_STATIC),yes)
        DEFS += $(shell $(SSL_BUILDER) --cflags)
        LIBS += $(shell $(SSL_BUILDER) --libs)

ifneq ($(CRYPTO_BUILDER),)
        DEFS += $(shell $(CRYPTO_BUILDER) --cflags)
        LIBS += $(shell $(CRYPTO_BUILDER) --libs)
endif # ifneq ($(CRYPTO_BUILDER),)

else # $(LIBSSL_STATIC),yes)

        DEFS += -DKSR_LIBSSL_STATIC

ifneq ($(LIBSSL_STATIC_SRCLIB),yes)
        ## when static libs (*.a) from packages are compiled with -fPIC
        DEFS += $(shell $(SSL_BUILDER) --cflags)
        LIBS += $(shell $(SSL_BUILDER) --libs-only-L)
        # TODO: explore use of LIBS += -Wl,-Bstatic $(shell $(SSL_BUILDER) --libs-only-l)
        LIBS += -l:libssl.a -l:libcrypto.a -l:libz.a -l:libdl.a
else
        ## when linking against static libs compiled from sources
        DEFS += -I$(LIBSSL_STATIC_SRCPATH)/include
        LIBS += $(LIBSSL_STATIC_SRCPATH)/libssl.a $(LIBSSL_STATIC_SRCPATH)/libcrypto.a
endif # ifneq ($(LIBSSL_STATIC_SRCLIB),yes)

endif # ifneq ($(LIBSSL_STATIC),yes)

else # ifneq ($(SSL_BUILDER),)

        DEFS += -I$(LOCALBASE)/ssl/include
        LIBS += -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib \
                        -L$(LOCALBASE)/lib64 -L$(LOCALBASE)/ssl/lib64 \
                        -lssl -lcrypto

endif # ifneq ($(SSL_BUILDER),)

ifneq ($(CURL_BUILDER),)
	DEFS += $(shell $(CURL_BUILDER) --cflags )
	LIBS += $(shell $(CURL_BUILDER) --libs)
else
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -lcurl
endif

include ../../Makefile.modules

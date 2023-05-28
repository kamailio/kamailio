#
# TLS module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=tls.so

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
	# NOTE: depending on the way in which libssl was compiled you might
	#       have to add -lz -lkrb5   (zlib and kerberos5).
	#       E.g.: make TLS_HOOKS=1 TLS_EXTRA_LIBS="-lz -lkrb5"

endif # ifneq ($(SSL_BUILDER),)

LIBS+= $(TLS_EXTRA_LIBS) -lpthread

# dcm: tls.cfg installed via local 'install-cfg' to update paths
#MOD_INSTALL_CFGS=tls.cfg

MOD_INSTALL_UTILS=utils/openssl_mutex_shared

include ../../Makefile.modules

install-tls-cert: $(cfg_prefix)/$(cfg_dir)
	MAIN_NAME=$(MAIN_NAME) ./tls_cert.sh -d $(cfg_prefix)/$(cfg_dir)

install-cfg:
	@if ! [ -d $(cfg_prefix)/$(cfg_dir) ]; then \
		mkdir -p "$(cfg_prefix)/$(cfg_dir)" ; \
	fi
	@$(call try_err, $(INSTALL_TOUCH) \
			"$(cfg_prefix)/$(cfg_dir)tls.cfg.sample" )
	@sed -e "s#\/usr/local/etc/kamailio/#$(cfg_target)#g" \
			-e "s#kamailio-selfsigned#$(MAIN_NAME)-selfsigned#g" \
				< ./tls.cfg > "$(cfg_prefix)/$(cfg_dir)tls.cfg.sample"
	@if [ -z "${skip_cfg_install}" -a \
			! -f "$(cfg_prefix)/$(cfg_dir)tls.cfg" ]; then \
		mv -f "$(cfg_prefix)/$(cfg_dir)tls.cfg.sample" \
						"$(cfg_prefix)/$(cfg_dir)tls.cfg" ; \
	fi


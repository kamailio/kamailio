#
# TLS module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=tls_wolfssl.so

# set to yes when wanting to link with static libraries
LIBSSL_STATIC ?= no
# set to yes when wanting to link with static libraries compiled from source
LIBSSL_STATIC_SRCLIB ?= no
# set to the path of the folder with static libraries compiled from source
LIBSSL_STATIC_SRCPATH ?= /usr/local/src/openssl

ifeq ($(CROSS_COMPILE),)
WOLFSSL_BUILDER=$(shell \
	if pkg-config --exists wolfssl; then \
		echo 'pkg-config wolfssl'; \
	fi)
endif

ifneq ($(WOLFSSL_BUILDER),)

ifneq ($(WOLFSSL_STATIC),yes)
	DEFS += $(shell $(WOLFSSL_BUILDER) --cflags)
	LIBS += $(shell $(WOLFSSL_BUILDER) --libs)

else # $(WOLFSSL_STATIC),yes)

	DEFS += -DKSR_WOLFSSL_STATIC

ifneq ($(WOLFSSL_STATIC_SRCLIB),yes)
	## when static libs (*.a) from packages are compiled with -fPIC
	DEFS += $(shell $(WOLFSSL_BUILDER) --cflags)
	LIBS += $(shell $(WOLFSSL_BUILDER) --libs-only-L)
	# TODO: explore use of LIBS += -Wl,-Bstatic $(shell $(SSL_BUILDER) --libs-only-l)
	LIBS += -l:libwolfssl.a -l:libz.a -l:libdl.a
else
	## when linking against static libs compiled from sources
	DEFS += -I$(WOLFSSL_STATIC_SRCPATH)/include
	LIBS += $(WOLFSSL_STATIC_SRCPATH)/libwolfssl.a
endif # ifneq ($(WOLFSSL_STATIC_SRCLIB),yes)

endif # ifneq ($(WOLFSSL_STATIC),yes)

else # ifneq ($(SSL_BUILDER),)

	DEFS += -I$(LOCALBASE)/wolfssl/include
	LIBS += -L$(LOCALBASE)/lib \
			-L$(LOCALBASE)/lib64  \
			-lwolfssl
endif # ifneq ($(SSL_BUILDER),)

LIBS+= $(TLS_EXTRA_LIBS)

# dcm: tls.cfg installed via local 'install-cfg' to update paths
#MOD_INSTALL_CFGS=tls.cfg

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


#
# TLS module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=tls.so

ifeq ($(CROSS_COMPILE),)
SSL_BUILDER=$(shell \
	if pkg-config --exists libssl; then \
		echo 'pkg-config libssl'; \
	fi)
endif

ifneq ($(SSL_BUILDER),)
	DEFS += $(shell $(SSL_BUILDER) --cflags)
	LIBS += $(shell $(SSL_BUILDER) --libs)
else
	DEFS += -I$(LOCALBASE)/ssl/include
	LIBS += -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib \
			-L$(LOCALBASE)/lib64 -L$(LOCALBASE)/ssl/lib64 \
			-lssl -lcrypto
	# NOTE: depending on the way in which libssl was compiled you might
	#       have to add -lz -lkrb5   (zlib and kerberos5).
	#       E.g.: make TLS_HOOKS=1 TLS_EXTRA_LIBS="-lz -lkrb5"
endif

LIBS+= $(TLS_EXTRA_LIBS)

# dcm: tls.cfg installed via local 'install-cfg' to update paths
#MOD_INSTALL_CFGS=tls.cfg

DEFS+=-DKAMAILIO_MOD_INTERFACE
include ../../Makefile.modules


install-tls-cert: $(cfg_prefix)/$(cfg_dir)
	MAIN_NAME=$(MAIN_NAME) ./$(SCR_NAME)_cert.sh -d $(cfg_prefix)/$(cfg_dir)

install-cfg:  install-tls-cert
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


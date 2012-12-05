# Makefile v 1.0 2002/12/27
#
# TLS module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=tls.so

CFLASFOUND = 0

ifeq ($(CROSS_COMPILE),)
BUILDER = $(shell which pkg-config)
endif

ifneq ($(BUILDER),)
	ifeq ($(CROSS_COMPILE),)
	SSLLIBS = $(shell pkg-config --silence-errors --libs libssl)
	endif
	ifneq ($(SSLLIBS),)
		DEFS+= $(shell pkg-config --silence-errors --cflags libssl)
		LIBS+= $(shell pkg-config --silence-errors --libs libssl)
		LIBS+= $(TLS_EXTRA_LIBS)
		CFLASFOUND = 1
	endif
endif

ifeq ($(CFLASFOUND),0)
	DEFS+= -I$(LOCALBASE)/ssl/include
	LIBS+=	-L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib \
			-L$(LOCALBASE)/lib64 -L$(LOCALBASE)/ssl/lib64 \
			-lssl  -lcrypto \
			$(TLS_EXTRA_LIBS)
	# NOTE: depending on the way in which libssl was compiled you might
	#       have to add -lz -lkrb5   (zlib and kerberos5).
	#       E.g.: make TLS_HOOKS=1 TLS_EXTRA_LIBS="-lz -lkrb5"
endif

# dcm: tls.cfg installed via local 'install-cfg' to update paths
#MOD_INSTALL_CFGS=tls.cfg

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules


install-tls-cert: $(cfg_prefix)/$(cfg_dir)
	MAIN_NAME=$(MAIN_NAME) ./$(SCR_NAME)_cert.sh -d $(cfg_prefix)/$(cfg_dir)

install-cfg:  install-tls-cert
	@$(call try_err, $(INSTALL_TOUCH) \
			"$(cfg_prefix)/$(cfg_dir)tls.cfg.sample" )
	@sed -e "s#\./modules/tls/#$(cfg_target)#g" \
			-e "s#ser-selfsigned#$(MAIN_NAME)-selfsigned#g" \
				< ./tls.cfg > "$(cfg_prefix)/$(cfg_dir)tls.cfg.sample"
	@if [ -z "${skip_cfg_install}" -a \
			! -f "$(cfg_prefix)/$(cfg_dir)tls.cfg" ]; then \
		mv -f "$(cfg_prefix)/$(cfg_dir)tls.cfg.sample" \
						"$(cfg_prefix)/$(cfg_dir)tls.cfg" ; \
	fi

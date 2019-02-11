#
# Digest Authentication - Radius support
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
include ../../Makefile.radius

auto_gen=
NAME=auth_radius.so

include ../../Makefile.modules

install-cfg:
	@if ! [ -d $(cfg_prefix)/$(cfg_dir) ]; then \
		mkdir -p "$(cfg_prefix)/$(cfg_dir)" ; \
	fi
	@$(call try_err, $(INSTALL_TOUCH) \
			"$(cfg_prefix)/$(cfg_dir)dictionary.$(CFG_NAME).sample" )
	@$(call try_err, $(INSTALL_CFG) \
			"cfg/dictionary.$(CFG_NAME)" \
			"$(cfg_prefix)/$(cfg_dir)dictionary.$(CFG_NAME).sample" )
	@if [ -z "${skip_cfg_install}" -a \
			! -f "$(cfg_prefix)/$(cfg_dir)dictionary.$(CFG_NAME)" ]; then \
		mv -f "$(cfg_prefix)/$(cfg_dir)dictionary.$(CFG_NAME).sample" \
						"$(cfg_prefix)/$(cfg_dir)dictionary.$(CFG_NAME)" ; \
	fi

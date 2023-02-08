#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=xhttp_pi.so
DEFS +=
LIBS +=

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
ifeq ($(XML2CFG),)
XML2CFG=$(shell \
	if pkg-config --exists libxml-2.0; then \
		echo 'pkg-config libxml-2.0'; \
	fi)
endif
endif

ifneq ($(XML2CFG),)
	DEFS += $(shell $(XML2CFG) --cflags )
	LIBS += $(shell $(XML2CFG) --libs)
else
	DEFS += -I$(LOCALBASE)/include/libxml2 \
		-I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -lxml2
endif

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

install-cfg:
	# provisioning xml framework samples
	@mkdir -p $(data_prefix)/$(data_dir)/xhttp_pi
	@for FILE in $(wildcard ../../../utils/kamctl/xhttp_pi/*) ; do \
		if [ -f $$FILE ] ; then \
			$(INSTALL_TOUCH) $$FILE \
				$(data_prefix)/$(data_dir)/xhttp_pi/`basename "$$FILE"` ; \
			$(INSTALL_CFG) $$FILE \
				$(data_prefix)/$(data_dir)/xhttp_pi/`basename "$$FILE"` ; \
		fi ; \
	done
	# provisioning xml framework
	@$(call try_err, mkdir -p $(cfg_prefix)/$(cfg_dir))
	@$(call try_err, $(INSTALL_TOUCH) \
		$(cfg_prefix)/$(cfg_dir)/pi_framework.xml.sample)
	@$(call try_err, $(INSTALL_CFG) \
		../../../misc/examples/pi_framework/pi_framework.xml \
		$(cfg_prefix)/$(cfg_dir)/pi_framework.xml.sample)
	@if [ -z "${skip_cfg_install}" -a \
		! -f $(cfg_prefix)/$(cfg_dir)/pi_framework.xml ]; then \
		mv -f $(cfg_prefix)/$(cfg_dir)/pi_framework.xml.sample \
			$(cfg_prefix)/$(cfg_dir)/pi_framework.xml; \
	fi


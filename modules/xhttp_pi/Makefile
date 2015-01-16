#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=xhttp_pi.so
DEFS +=
LIBS +=

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
endif

ifneq ($(XML2CFG),)
	DEFS += $(shell $(XML2CFG) --cflags )
	LIBS += $(shell $(XML2CFG) --libs)
else
	DEFS += -I$(LOCALBASE)/include/libxml2 \
		-I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -lxml2
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

install-cfg:
	# provisionning xml framework samples
	@mkdir -p $(data_prefix)/$(data_dir)/xhttp_pi
	@for FILE in $(wildcard ../../utils/kamctl/xhttp_pi/*) ; do \
		if [ -f $$FILE ] ; then \
			$(INSTALL_TOUCH) $$FILE \
				$(data_prefix)/$(data_dir)/xhttp_pi/`basename "$$FILE"` ; \
			$(INSTALL_CFG) $$FILE \
				$(data_prefix)/$(data_dir)/xhttp_pi/`basename "$$FILE"` ; \
		fi ; \
	done
	# provisionning xml framework
	@$(call try_err, mkdir -p $(cfg_prefix)/$(cfg_dir))
	@$(call try_err, $(INSTALL_TOUCH) \
		$(cfg_prefix)/$(cfg_dir)/pi_framework.xml.sample)
	@$(call try_err, $(INSTALL_CFG) ../../examples/pi_framework.xml \
		$(cfg_prefix)/$(cfg_dir)/pi_framework.xml.sample)
	@if [ -z "${skip_cfg_install}" -a \
		! -f $(cfg_prefix)/$(cfg_dir)/pi_framework.xml ]; then \
		mv -f $(cfg_prefix)/$(cfg_dir)/pi_framework.xml.sample \
			$(cfg_prefix)/$(cfg_dir)/pi_framework.xml; \
	fi


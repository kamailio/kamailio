#
# Root Makefile for Kamailio project
# - forward all commands to the Makefile in the src/ subfolder
#


# path to the source code folder
KSR_DIR ?= src/

# default target for makefile
.DEFAULT_GOAL := default


# strip the src/ from the path to modules
SMODPARAM=
ifneq ($(modules),)
ifneq (,$(findstring src/,$(modules)))
smodules=$(subst src/,,$(modules))
SMODPARAM=modules=$(smodules)
endif
endif

# forward all named targets
%:
	$(MAKE) -C $(KSR_DIR) $@ $(SMODPARAM)

# forward the default target
default:
	$(MAKE) -C $(KSR_DIR)

# forward the install target
.PHONY: install
install:
	$(MAKE) -C $(KSR_DIR) $@ $(SMODPARAM)

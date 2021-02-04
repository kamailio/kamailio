#
# Root Makefile for Kamailio project
# - forward all commands to the Makefile in the src/ subfolder
#


# path to the source code folder
KSR_DIR ?= src/

# default target for makefile
.DEFAULT_GOAL := default

ifneq ($(wildcard modules),)
$(warning "old Kamailio modules directory found, you should clean that")
endif

# strip the src/ from the path to modules
SMODPARAM=
ifneq ($(modules),)
ifneq (,$(findstring src/,$(modules)))
smodules=$(subst src/,,$(modules))
SMODPARAM=modules=$(smodules)
endif
endif

MKTAGS?=ctags
EMACS_COMPAT=
ifneq ($(INSIDE_EMACS),)
EMACS_COMPAT=-e
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

.PHONY: TAGS
.PHONY: tags
TAGS tags:
	$(MKTAGS) $(EMACS_COMPAT) --exclude="misc/*" --exclude="test/*" -R .

# clean everything generated - shortcut on maintainer-clean
.PHONY: pure
clean pure distclean:
	@rm -f .*.swp tags TAGS
	$(MAKE) -C $(KSR_DIR) $@

#

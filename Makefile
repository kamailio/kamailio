#
# Root Makefile for Kamailio project
# - forward all commands to the Makefile in the src/ subfolder
#

# path to the source code folder
KSR_DIR ?= src/

# default target for makefile
.DEFAULT_GOAL := default

# forward all named targets
%:
	$(MAKE) -C $(KSR_DIR) "$@"

# forward the default target
default:
	$(MAKE) -C $(KSR_DIR)

.PHONY: install
install:
	$(MAKE) -C $(KSR_DIR) "$@"

#Default configuration is local/default/local.m4, so you need to do: make default
use_config?=$@

#Install path
prefix?=/usr/local/etc/ser

# Prefix to local configs, can be relative to top-level directory
local_prefix?=local

# Include general defs
# hack to force makefile.defs re-inclusion (needed when make calls itself with
# other options -- e.g. make bin)
makefile_defs=0
DEFS:=

m4_param?=

# This is how local.m4 in a server-dir is preferred over the one in common
m4_include_dirs=-I $(local_prefix)/$(use_config) -I $(local_prefix)/common
# Change if m4 is located somewhere else
M4=/usr/bin/m4 $(m4_param) $(m4_include_dirs)

# If no target is specified, use the first (none)
none:
	make default

install:
	cp ./ser*.cfg $(prefix)/

xml:
	$(M4) -D XMLOUT -D GETTINGSTARTED=HELLOWORLD $(local_prefix)/$(use_config)/config.m4 templates/ser.cfg.m4 > ./helloworld.xml ; 

clean:
	rm ./ser_*.cfg ;	
	rm ./helloworld.xml ;

# If a target is specified (not found below), use this default
.DEFAULT: 
	@echo 'Using $(local_prefix)/$(use_config)/local.m4 for creating ser.cfg.'
	@echo ' '
	@echo 'Use configure to generate $(local_prefix)/$(use_config)/config.m4 the first time.'
	@echo ' '
	$(M4) $(local_prefix)/$(use_config)/config.m4 templates/ser.cfg.m4 > ./ser_$(use_config).cfg ; 



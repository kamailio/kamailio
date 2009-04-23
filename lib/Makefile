#  $Id$
#
#  libs makefile
#
# This makefile multiplexes the original goal to all the libraries listed in 
# SUBDIRS.
#


#SUBDIRS=binrpc cds xcap presence shm_regex

SUBDIRS=$(filter-out CVS doc $(lib_exclude), $(strip \
			$(patsubst %/Makefile,%,$(wildcard */Makefile))))

.PHONY: subdirs $(SUBDIRS) 

# clean install tags proper


# we need sub_goals without 'all' target
sub_goals = $(patsubst all,,$(MAKECMDGOALS))

subdirs: $(SUBDIRS)

$(SUBDIRS):	
			-@echo "Making $(sub_goals) in $@" ; $(MAKE) $(sub_goals) -C $@

COREPATH=..
include ../Makefile.targets

all:	subdirs

$(clean_targets): subdirs

install:	subdirs


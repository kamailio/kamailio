#  $Id$
#
#  libs makefile
#
# This makefile multiplexes the original goal to all the libraries listed in 
# SUBDIRS.
#


SUBDIRS=binrpc cds xcap presence shm_regex

.PHONY: subdirs $(SUBDIRS) 

# clean install tags proper


# we need sub_goals without 'all' target
sub_goals = $(patsubst all,,$(MAKECMDGOALS))

subdirs: $(SUBDIRS)

$(SUBDIRS):	
			-@echo "Making $(sub_goals) in $@" ; $(MAKE) $(sub_goals) -C $@

all:	subdirs

proper:	clean

clean:	subdirs

install:	subdirs


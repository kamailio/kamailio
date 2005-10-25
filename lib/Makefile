# variables to change

INCLUDES += -I$(CURDIR)
LIBS     += -L$(CURDIR)/cds -L$(CURDIR)/qsa

####################################
# make rules

export LIBS
export INCLUDES

SUBDIRS=cds xcap presence

.PHONY: subdirs $(SUBDIRS) 

# clean install tags proper

subdirs:	$(SUBDIRS)

$(SUBDIRS):	
			@echo "Making $(MAKECMDGOALS) in $@" ; $(MAKE) $(MAKECMDGOALS) -C $@

all:	subdirs

proper:	clean

clean:	subdirs

install:	subdirs


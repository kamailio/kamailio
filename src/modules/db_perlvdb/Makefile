#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=db_perlvdb.so
ifeq ($(PERLLDOPTS),)
	LIBS:=$(shell perl -MExtUtils::Embed -e ldopts)
else
	LIBS+=$(PERLLDOPTS)
endif
PERL_DEFS:=$(shell perl -MExtUtils::Embed -e ccopts)
DEFS+=$(PERL_DEFS)

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules

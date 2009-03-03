# $Id$
#
# perl module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=perl.so
LIBS=

ifeq ($(PERLLDOPTS),)
	LIBS+=`perl -MExtUtils::Embed -e ldopts`
else
	LIBS+=$(PERLLDOPTS)
endif

ifeq ($(PERLCCOPTS),)
	DEFS+=`perl -MExtUtils::Embed -e ccopts`
else
	DEFS+=$(PERLCCOPTS)
endif


DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules

PODFILES=openserxs.xs `find lib/perl -name *.pm`

ifeq ($(TYPEMAP),)
	ifeq ($(PERLLIBPATH),)
		PERLLIBPATH=`perl -MConfig -e 'print $$Config{installprivlib}'`
	endif
	TYPEMAP=$(PERLLIBPATH)/ExtUtils/typemap
endif

perl.d: openserxs.o
perl.o: perl.c openserxs.o

openserxs.o: openserxs.c openserxs.d
	$(CC) $(MOD_CFLAGS) -Wno-unused $(DEFS) -c -o openserxs.o openserxs.c

openserxs.c: openserxs.xs typemap
	xsubpp -typemap $(TYPEMAP) -typemap typemap openserxs.xs > openserxs.c

clean:	clean-special

.PHONY: clean-special
clean-special:
	rm -f openserxs.c openserxs.o openserxs.d

.PHONY: perlpod
perlpod: doc/perl_pod.sgml

doc/perl_pod.sgml: openserxs.xs
	cat $(PODFILES) | pod2docbook --doctype=chapter --title='OpenSER Perl API' --no-header - doc/perl_pod.sgml

install_module_custom:
	echo "installing Perl OpenSER packages ..."
	mkdir -p $(modules-prefix)/$(lib-dir)/perl
	$(INSTALL_CFG) lib/perl/*.pm $(modules-prefix)/$(lib-dir)/perl
	mkdir -p $(modules-prefix)/$(lib-dir)/perl/OpenSER
	$(INSTALL_CFG) lib/perl/OpenSER/*.pm \
		$(modules-prefix)/$(lib-dir)/perl/OpenSER
	mkdir -p $(modules-prefix)/$(lib-dir)/perl/OpenSER/LDAPUtils
	$(INSTALL_CFG) lib/perl/OpenSER/LDAPUtils/*.pm \
		$(modules-prefix)/$(lib-dir)/perl/OpenSER/LDAPUtils
	mkdir -p $(modules-prefix)/$(lib-dir)/perl/OpenSER/Utils
	$(INSTALL_CFG) lib/perl/OpenSER/Utils/*.pm \
		$(modules-prefix)/$(lib-dir)/perl/OpenSER/Utils
	mkdir -p $(modules-prefix)/$(lib-dir)/perl/OpenSER/VDB
	$(INSTALL_CFG) lib/perl/OpenSER/VDB/*.pm \
		$(modules-prefix)/$(lib-dir)/perl/OpenSER/VDB
	mkdir -p $(modules-prefix)/$(lib-dir)/perl/OpenSER/VDB/Adapter
	$(INSTALL_CFG) lib/perl/OpenSER/VDB/Adapter/*.pm \
		$(modules-prefix)/$(lib-dir)/perl/OpenSER/VDB/Adapter


# $Id$
#
# perl module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=openserxs.c
NAME=perl.so
LIBS=

ifeq ($(PERLLDOPTS),)
	LIBS+=`perl -MExtUtils::Embed -e ldopts`
else
	LIBS+=$(PERLLDOPTS)
endif

ifeq ($(PERLCCOPTS),)
	PERLCCOPTS=$(shell perl -MExtUtils::Embed -e ccopts)
endif
DEFS+=$(PERLCCOPTS)


DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi

PODFILES=openserxs.xs `find lib/perl -name *.pm`

ifeq ($(TYPEMAP),)
	ifeq ($(PERLLIBPATH),)
		PERLLIBPATH=`perl -MConfig -e 'print $$Config{installprivlib}'`
	endif
	TYPEMAP=$(PERLLIBPATH)/ExtUtils/typemap
endif

include ../../Makefile.modules

perl.d: openserxs.o
perl.o: perl.c openserxs.o

openserxs.o: CFLAGS+=-Wno-unused

openserxs.c: openserxs.xs typemap
	xsubpp -typemap $(TYPEMAP) -typemap typemap openserxs.xs > openserxs.c

.PHONY: perlpod
perlpod: doc/perl_pod.sgml

doc/perl_pod.sgml: openserxs.xs
	cat $(PODFILES) | pod2docbook --doctype=chapter --title='OpenSER Perl API' --no-header - doc/perl_pod.sgml

install-perl-scripts:
	echo "installing Perl Kamailio (OpenSER) packages ..."
	mkdir -p $(modules_prefix)/$(lib_dir)/perl
	$(INSTALL_CFG) lib/perl/*.pm $(modules_prefix)/$(lib_dir)/perl
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/OpenSER
	$(INSTALL_CFG) lib/perl/OpenSER/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/OpenSER
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/OpenSER/LDAPUtils
	$(INSTALL_CFG) lib/perl/OpenSER/LDAPUtils/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/OpenSER/LDAPUtils
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/OpenSER/Utils
	$(INSTALL_CFG) lib/perl/OpenSER/Utils/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/OpenSER/Utils
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/OpenSER/VDB
	$(INSTALL_CFG) lib/perl/OpenSER/VDB/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/OpenSER/VDB
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/OpenSER/VDB/Adapter
	$(INSTALL_CFG) lib/perl/OpenSER/VDB/Adapter/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/OpenSER/VDB/Adapter

install-scripts: install-perl-scripts


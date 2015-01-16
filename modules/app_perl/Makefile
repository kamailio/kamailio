# perl module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=kamailioxs.c
NAME=app_perl.so
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

PODFILES=kamailioxs.xs `find lib/perl -name *.pm`

ifeq ($(TYPEMAP),)
	ifeq ($(PERLLIBPATH),)
		PERLLIBPATH=`perl -MConfig -e 'print $$Config{installprivlib}'`
	endif
	TYPEMAP=$(PERLLIBPATH)/ExtUtils/typemap
endif

include ../../Makefile.modules

perl.d: kamailioxs.o
perl.o: perl.c kamailioxs.o

kamailioxs.o: CFLAGS+=-Wno-unused

kamailioxs.c: kamailioxs.xs typemap
	xsubpp -typemap $(TYPEMAP) -typemap typemap kamailioxs.xs > kamailioxs.c

.PHONY: perlpod
perlpod: doc/app_perl_pod.xml

doc/app_perl_pod.sgml: kamailioxs.xs
	cat $(PODFILES) | pod2docbook --doctype=chapter --title='Kamailio Perl API' --no-header - doc/app_perl_pod.sgml

install-perl-scripts:
	echo "installing Perl Kamailio packages ..."
	mkdir -p $(modules_prefix)/$(lib_dir)/perl
	$(INSTALL_CFG) lib/perl/*.pm $(modules_prefix)/$(lib_dir)/perl
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/Kamailio
	$(INSTALL_CFG) lib/perl/Kamailio/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/Kamailio
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/Kamailio/LDAPUtils
	$(INSTALL_CFG) lib/perl/Kamailio/LDAPUtils/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/Kamailio/LDAPUtils
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/Kamailio/Utils
	$(INSTALL_CFG) lib/perl/Kamailio/Utils/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/Kamailio/Utils
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/Kamailio/VDB
	$(INSTALL_CFG) lib/perl/Kamailio/VDB/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/Kamailio/VDB
	mkdir -p $(modules_prefix)/$(lib_dir)/perl/Kamailio/VDB/Adapter
	$(INSTALL_CFG) lib/perl/Kamailio/VDB/Adapter/*.pm \
		$(modules_prefix)/$(lib_dir)/perl/Kamailio/VDB/Adapter

install-scripts: install-perl-scripts


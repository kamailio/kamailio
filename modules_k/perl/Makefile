# $Id$
#
# perl module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=perl.so

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

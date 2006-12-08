# $Id$
#
# perl module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=perl.so
LIBS+=`perl -MExtUtils::Embed -e ldopts`
DEFS+=`perl -MExtUtils::Embed -e ccopts`

include ../../Makefile.modules

PODFILES=openserxs.xs `find lib/perl -name *.pm`

PERLLIBPATH=`perl -MConfig -e 'print $$Config{installprivlib}'`
TYPEMAP=$(PERLLIBPATH)/ExtUtils/typemap

perl.d: openserxs.o
perl.o: perl.c openserxs.o

openserxs.o: openserxs.c openserxs.d
	$(CC) $(DEFS) -c -o openserxs.o openserxs.c

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

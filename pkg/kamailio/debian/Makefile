# $Id$
#
# makefile to generate the needed files for debian packaging
# 

ifneq ($(NOTLS),)
	PKGVER=-notls
	CONVER=
	WORDNO=no
	TLSON=
else
	PKGVER=
	CONVER=-notls
	WORDNO=
	TLSON=1
endif


.PHONY: all
all: clean
	-@echo "Building changelog file"
	sed -e 's/##PKGVER##/$(PKGVER)/g' changelog.in >changelog
	-@echo "Done"
	-@echo "Building control file"
	sed -e 's/##PKGVER##/$(PKGVER)/g' control.in \
		| sed -e 's/##CONVER##/$(CONVER)/g' \
		| sed -e 's/##WORDNO##/$(WORDNO)/g' >control
	-@echo "Done"
	-@echo "Building rules file"
	sed -e 's/##PKGVER##/$(PKGVER)/g' rules.in \
		| sed -e 's/##TLSON##/$(TLSON)/g' >rules
	chmod +x rules
	-@echo "Done"
	-@echo "Building extra files"
	sed -e 's/##PKGVER##/$(PKGVER)/g' openser.config.in \
		>openser$(PKGVER).config
	sed -e 's/##PKGVER##/$(PKGVER)/g' openser.init.in \
		>openser$(PKGVER).init
	sed -e 's/##PKGVER##/$(PKGVER)/g' openser.postrm.in \
		>openser$(PKGVER).postrm
	sed -e 's/##PKGVER##/$(PKGVER)/g' openser.examples.in \
		>openser$(PKGVER).examples
	sed -e 's/##PKGVER##/$(PKGVER)/g' openser.postinst.in \
		>openser$(PKGVER).postinst
	sed -e 's/##PKGVER##/$(PKGVER)/g' openser.templates.in \
		>openser$(PKGVER).templates
	-@echo "Done"

.PHONY: notls
notls:
	NOTLS=1 $(MAKE)

.PHONY: clean
clean:
	rm -f changelog
	rm -f control
	rm -f rules
	rm -f openser*.config
	rm -f openser*.init
	rm -f openser*.postrm
	rm -f openser*.examples
	rm -f openser*.postinst
	rm -f openser*.templates


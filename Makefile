# $Id$
#
# sip_router makefile
#
# WARNING: requires gmake (GNU Make)
#  Arch supported: Linux, FreeBSD, SunOS (tested on Solaris 8), OpenBSD (3.2),
#  limited WinNT (cygwin) support

auto_gen=lex.yy.c cfg.tab.c   #lexx, yacc etc

#include  source related defs
include Makefile.sources

override exclude_modules:=CVS cpl cpl-c ext radius_acc radius_auth snmp \
	$(exclude_modules)
static_modules=
static_modules_path=$(addprefix modules/, $(static_modules))
extra_sources=$(wildcard $(addsuffix /*.c, $(static_modules_path)))
extra_objs=$(extra_sources:.c=.o)

static_defs= $(foreach  mod, $(static_modules), \
		-DSTATIC_$(shell echo $(mod) | tr [:lower:] [:upper:]) )
DEFS=$(static_defs)
modules=$(filter-out $(addprefix modules/, \
			$(exclude_modules) $(static_modules)), \
			$(wildcard modules/*))
modules_names=$(shell echo $(modules)| \
				sed -e 's/modules\/\([^/ ]*\)\/*/\1.so/g' )
#modules_names=$(patsubst modules/%, %.so, $(modules))
modules_full_path=$(join  $(modules), $(addprefix /, $(modules_names)))

NAME=ser

ALLDEP=Makefile Makefile.sources Makefile.defs Makefile.rules

#include general defs (like CC, CFLAGS  a.s.o)
include Makefile.defs

#export relevant variables to the sub-makes
export DEFS PROFILE CC  LD MKDEP MKTAGS CFLAGS LDFLAGS MOD_CFLAGS MOD_LDFLAGS
export LEX YACC YACC_FLAGS


# include the common rules
include Makefile.rules

#extra targets 

$(NAME): static_modules

lex.yy.c: cfg.lex $(ALLDEP)
	$(LEX) $<

cfg.tab.c: cfg.y  $(ALLDEP)
	$(YACC) $(YACC_FLAGS) $<

.PHONY: all
all: $(NAME) modules



.PHONY: modules
modules:
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			$(MAKE) -C $$r ; \
		fi ; \
	done 

.PHONY: static_modules
static_modules:
	-@echo "Extra objs: $(extra_objs)" 
	-@for r in $(static_modules_path) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "Making static module $r" ; \
			$(MAKE) -C $$r static ; \
		fi ; \
	done 


	
dbg: ser
	gdb -command debug.gdb

.PHONY: tar
tar: mantainer-clean 
	$(TAR) -C .. \
		--exclude=$(notdir $(CURDIR))/test \
		--exclude=$(notdir $(CURDIR))/tmp \
		--exclude=$(notdir $(CURDIR))/debian/ser \
		--exclude=$(notdir $(CURDIR))/debian/ser-mysql-module \
		 -zcf ../$(NAME)-$(RELEASE)_src.tar.gz  $(notdir $(CURDIR)) 

# binary dist. tar.gz
.PHONY: bin
bin:
	mkdir -p tmp/ser/usr/local
	$(MAKE) install basedir=tmp/ser prefix=/usr/local 
	$(TAR) -C tmp/ser/ -zcf ../$(NAME)-$(RELEASE)_$(OS)_$(ARCH).tar.gz .
	rm -rf tmp/ser

.PHONY: deb
deb:
	dpkg-buildpackage -rfakeroot -tc

.PHONY: sunpkg
sunpkg:
	mkdir -p tmp/ser
	mkdir -p tmp/ser_sun_pkg
	$(MAKE) install basedir=tmp/ser prefix=/usr/local
	(cd solaris; \
	pkgmk -r ../tmp/ser/usr/local -o -d ../tmp/ser_sun_pkg/ -v "$(RELEASE)" ;\
	cd ..)
	cat /dev/null > ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local
	pkgtrans -s tmp/ser_sun_pkg/ ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local \
		IPTELser
	gzip -9 ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local
	rm -rf tmp/ser
	rm -rf tmp/ser_sun_pkg


install: all mk-install-dirs install-cfg install-bin install-modules \
	install-doc install-man

.PHONY: dbinstall
dbinstall:
	-@echo "Initializing ser database"
	scripts/ser_mysql.sh create
	-@echo "Done"

mk-install-dirs: $(cfg-prefix)/$(cfg-dir) $(bin-prefix)/$(bin-dir) \
			$(modules-prefix)/$(modules-dir) $(doc-prefix)/$(doc-dir) \
			$(man-prefix)/$(man-dir)/man8 $(man-prefix)/$(man-dir)/man5

$(cfg-prefix)/$(cfg-dir): 
		mkdir -p $(cfg-prefix)/$(cfg-dir)

$(bin-prefix)/$(bin-dir):
		mkdir -p $(bin-prefix)/$(bin-dir)

$(modules-prefix)/$(modules-dir):
		mkdir -p $(modules-prefix)/$(modules-dir)


$(doc-prefix)/$(doc-dir):
		mkdir -p $(doc-prefix)/$(doc-dir)

$(man-prefix)/$(man-dir)/man8:
		mkdir -p $(man-prefix)/$(man-dir)/man8

$(man-prefix)/$(man-dir)/man5:
		mkdir -p $(man-prefix)/$(man-dir)/man5

install-cfg: $(cfg-prefix)/$(cfg-dir)
		sed -e "s#/usr/lib/ser/modules/#$(modules-target)#g" \
			< etc/ser.cfg > $(cfg-prefix)/$(cfg-dir)ser.cfg
		chmod 644 $(cfg-prefix)/$(cfg-dir)ser.cfg
#		$(INSTALL-CFG) etc/ser.cfg $(cfg-prefix)/$(cfg-dir)

install-bin: $(bin-prefix)/$(bin-dir) utils/gen_ha1/gen_ha1
		$(INSTALL-BIN) ser $(bin-prefix)/$(bin-dir)
		$(INSTALL-BIN) scripts/sc $(bin-prefix)/$(bin-dir)/serctl
		$(INSTALL-BIN) scripts/ser_mysql.sh  $(bin-prefix)/$(bin-dir)
		$(INSTALL-BIN) utils/gen_ha1/gen_ha1 $(bin-prefix)/$(bin-dir)

utils/gen_ha1/gen_ha1:
		cd utils/gen_ha1; $(MAKE) all

install-modules: modules $(modules-prefix)/$(modules-dir)
	-@for r in $(modules_full_path) "" ; do \
		if [ -n "$$r" ]; then \
			$(INSTALL-MODULES)  $$r  $(modules-prefix)/$(modules-dir) ; \
		fi ; \
	done 


install-doc: $(doc-prefix)/$(doc-dir)
	$(INSTALL-DOC) README $(doc-prefix)/$(doc-dir)
	$(INSTALL-DOC) README.cfg $(doc-prefix)/$(doc-dir)
	$(INSTALL-DOC) INSTALL $(doc-prefix)/$(doc-dir)
	$(INSTALL-DOC) README-MODULES $(doc-prefix)/$(doc-dir)
	$(INSTALL-DOC) AUTHORS $(doc-prefix)/$(doc-dir)

install-man: $(man-prefix)/$(man-dir)/man8 $(man-prefix)/$(man-dir)/man5
	$(INSTALL-MAN)  ser.8 $(man-prefix)/$(man-dir)/man8
	$(INSTALL-MAN)  ser.cfg.5 $(man-prefix)/$(man-dir)/man5


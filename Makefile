# $Id$
#
# sip_router makefile
#
# WARNING: requires gmake (GNU Make)
#  Arch supported: Linux, FreeBSD, SunOS (tested on Solaris 8), OpenBSD (3.2),
#  NetBSD (1.6).
#
#  History:
#  --------
#              created by andrei
#  2003-02-24  make install no longer overwrites ser.cfg  - patch provided
#               by Maxim Sobolev   <sobomax@FreeBSD.org> and 
#                  Tomas Björklund <tomas@webservices.se>
#  2003-03-11  PREFIX & LOCALBASE must also be exported (andrei)
#  2003-04-07  hacked to work with solaris install (andrei)
#  2003-04-17  exclude modules overwritable from env. or cmd. line,
#               added include_modules and skip_modules (andrei)
#  2003-05-30  added extra_defs & EXTRA_DEFS
#               Makefile.defs force-included to allow recursive make
#               calls -- see comment (andrei)
#  2003-06-02   make tar changes -- unpacks in $NAME-$RELEASE  (andrei)
#

auto_gen=lex.yy.c cfg.tab.c   #lexx, yacc etc

#include  source related defs
include Makefile.sources

#extra modules to exclude
skip_modules?=

# if not set on the cmd. line or the env, exclude this modules:
exclude_modules?= 			cpl cpl-c ext extcmd mangler pdt \
							postgres snmp  \
							im radius_acc radius_auth \
							jabber mysql \
							auth_radius group_radius uri_radius 
# always exclude the CVS dir
override exclude_modules+= CVS $(skip_modules)

#always include this modules
include_modules?=

# first 2 lines are excluded because of the experimental or incomplete
# status of the modules
# the rest is excluded because it depends on external libraries
#
static_modules=
static_modules_path=$(addprefix modules/, $(static_modules))
extra_sources=$(wildcard $(addsuffix /*.c, $(static_modules_path)))
extra_objs=$(extra_sources:.c=.o)

static_defs= $(foreach  mod, $(static_modules), \
		-DSTATIC_$(shell echo $(mod) | tr [:lower:] [:upper:]) )

override extra_defs+=$(static_defs) $(EXTRA_DEFS)
export extra_defs

modules=$(filter-out $(addprefix modules/, \
			$(exclude_modules) $(static_modules)), \
			$(wildcard modules/*))
modules:=$(filter-out $(modules), $(addprefix modules/, $(include_modules) )) \
			$(modules)
modules_names=$(shell echo $(modules)| \
				sed -e 's/modules\/\([^/ ]*\)\/*/\1.so/g' )
modules_basenames=$(shell echo $(modules)| \
				sed -e 's/modules\/\([^/ ]*\)\/*/\1/g' )
#modules_names=$(patsubst modules/%, %.so, $(modules))
modules_full_path=$(join  $(modules), $(addprefix /, $(modules_names)))

NAME=ser

ALLDEP=Makefile Makefile.sources Makefile.defs Makefile.rules

#include general defs (like CC, CFLAGS  a.s.o)
# hack to force makefile.defs re-inclusion (needed when make calls itself with
# other options -- e.g. make bin)
makefile_defs=0
DEFS:=
include Makefile.defs

#export relevant variables to the sub-makes
export DEFS PROFILE CC  LD MKDEP MKTAGS CFLAGS LDFLAGS MOD_CFLAGS MOD_LDFLAGS
export LEX YACC YACC_FLAGS
export PREFIX LOCALBASE
# export relevant variables for recursive calls of this makefile 
# (e.g. make deb)
#export LIBS
#export TAR 
#export NAME RELEASE OS ARCH 
#export cfg-prefix cfg-dir bin-prefix bin-dir modules-prefix modules-dir
#export doc-prefix doc-dir man-prefix man-dir ut-prefix ut-dir
#export cfg-target modules-target
#export INSTALL INSTALL-CFG INSTALL-BIN INSTALL-MODULES INSTALL-DOC INSTALL-MAN 
#export INSTALL-TOUCH


# include the common rules
include Makefile.rules

#extra targets 

$(NAME): static_modules

lex.yy.c: cfg.lex cfg.tab.h $(ALLDEP)
	$(LEX) $<

cfg.tab.c cfg.tab.h: cfg.y  $(ALLDEP)
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
.PHONY: dist

dist: tar

tar: 
	$(TAR) -C .. \
		--exclude=$(notdir $(CURDIR))/test* \
		--exclude=$(notdir $(CURDIR))/tmp* \
		--exclude=$(notdir $(CURDIR))/debian/ser* \
		--exclude=CVS* \
		--exclude=*.[do] \
		--exclude=*.so \
		--exclude=*.il \
		--exclude=$(notdir $(CURDIR))/ser \
		--exclude=*.gz \
		--exclude=*.bz2 \
		--exclude=*.tar \
		-cf - $(notdir $(CURDIR)) | \
			(mkdir -p tmp/_tar1; mkdir -p tmp/_tar2 ; \
			    cd tmp/_tar1; $(TAR) -xf - ) && \
			    mv tmp/_tar1/$(notdir $(CURDIR)) \
			       tmp/_tar2/"$(NAME)-$(RELEASE)" && \
			    (cd tmp/_tar2 && $(TAR) \
			                    -zcf ../../"$(NAME)-$(RELEASE)_src".tar.gz \
			                               "$(NAME)-$(RELEASE)" ) ; \
			    rm -rf tmp/_tar1; rm -rf tmp/_tar2

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
			< etc/ser.cfg > $(cfg-prefix)/$(cfg-dir)ser.cfg.default
		chmod 644 $(cfg-prefix)/$(cfg-dir)ser.cfg.default
		if [ ! -f $(cfg-prefix)/$(cfg-dir)ser.cfg ]; then \
			cp -p $(cfg-prefix)/$(cfg-dir)ser.cfg.default \
				$(cfg-prefix)/$(cfg-dir)ser.cfg; \
		fi
#		$(INSTALL-CFG) etc/ser.cfg $(cfg-prefix)/$(cfg-dir)

install-bin: $(bin-prefix)/$(bin-dir) utils/gen_ha1/gen_ha1
		$(INSTALL-TOUCH) $(bin-prefix)/$(bin-dir)/ser 
		$(INSTALL-BIN) ser $(bin-prefix)/$(bin-dir)
		$(INSTALL-TOUCH)   $(bin-prefix)/$(bin-dir)/sc
		$(INSTALL-BIN) scripts/sc $(bin-prefix)/$(bin-dir)
		mv -f $(bin-prefix)/$(bin-dir)/sc $(bin-prefix)/$(bin-dir)/serctl
		$(INSTALL-TOUCH)   $(bin-prefix)/$(bin-dir)/ser_mysql.sh  
		$(INSTALL-BIN) scripts/ser_mysql.sh  $(bin-prefix)/$(bin-dir)
		$(INSTALL-TOUCH)   $(bin-prefix)/$(bin-dir)/gen_ha1
		$(INSTALL-BIN) utils/gen_ha1/gen_ha1 $(bin-prefix)/$(bin-dir)

utils/gen_ha1/gen_ha1:
		cd utils/gen_ha1; $(MAKE) all

install-modules: modules $(modules-prefix)/$(modules-dir)
	-@for r in $(modules_full_path) "" ; do \
		if [ -n "$$r" ]; then \
			if [ -f "$$r" ]; then \
				$(INSTALL-TOUCH) \
					$(modules-prefix)/$(modules-dir)/`basename "$$r"` ; \
				$(INSTALL-MODULES)  "$$r"  $(modules-prefix)/$(modules-dir) ; \
			else \
				echo "ERROR: module $$r not compiled" ; \
			fi ;\
		fi ; \
	done 


install-doc: $(doc-prefix)/$(doc-dir)
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/README.cfg 
	$(INSTALL-DOC) README.cfg $(doc-prefix)/$(doc-dir)
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/INSTALL 
	$(INSTALL-DOC) INSTALL $(doc-prefix)/$(doc-dir)
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/README-MODULES 
	$(INSTALL-DOC) README-MODULES $(doc-prefix)/$(doc-dir)
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/AUTHORS 
	$(INSTALL-DOC) AUTHORS $(doc-prefix)/$(doc-dir)
	-@for r in $(modules_basenames) "" ; do \
		if [ -n "$$r" ]; then \
			if [ -f modules/"$$r"/README ]; then \
				$(INSTALL-TOUCH)  $(doc-prefix)/$(doc-dir)/README ; \
				$(INSTALL-DOC)  modules/"$$r"/README  \
									$(doc-prefix)/$(doc-dir)/README ; \
				mv -f $(doc-prefix)/$(doc-dir)/README \
						$(doc-prefix)/$(doc-dir)/README."$$r" ; \
			fi ; \
		fi ; \
	done 
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/README 
	$(INSTALL-DOC) README $(doc-prefix)/$(doc-dir)

install-man: $(man-prefix)/$(man-dir)/man8 $(man-prefix)/$(man-dir)/man5
	$(INSTALL-TOUCH)  $(man-prefix)/$(man-dir)/man8/ser.8 
	$(INSTALL-MAN)  ser.8 $(man-prefix)/$(man-dir)/man8
	$(INSTALL-TOUCH)  $(man-prefix)/$(man-dir)/man5/ser.cfg.5 
	$(INSTALL-MAN)  ser.cfg.5 $(man-prefix)/$(man-dir)/man5


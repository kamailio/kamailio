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
#  2003-06-02  make tar changes -- unpacks in $NAME-$RELEASE  (andrei)
#  2003-06-03  make install-cfg will properly replace the module path
#               in the cfg (re: /usr/.*lib/ser/modules)
#              ser.cfg.default is installed only if there is a previous
#               cfg. -- fixes packages containing ser.cfg.default (andrei)
#  2003-08-29  install-modules-doc split from install-doc, added 
#               install-modules-all, removed README.cfg (andrei)
#              added skip_cfg_install (andrei)
#  2004-09-02  install-man will automatically "fix" the path of the files
#               referred in the man pages
#  2006-02-14  added utils & install-utils (andrei)
#  2006-03-15  added nodeb parameter for make tar (andrei)
#  2006-09-29  added modules-doc as target and doc_format= as make option (greger)
#  2006-12-09  added new group_include as make option and defined groups defining
#		which modules to include
#		Also added new target print-modules that you can use to check which
#		modules will be compiled (greger)
#  2007-01-10   added new group_include targets mysql, radius, and presence 
#		improved print-modules output
#		fixed problem in include/exclude logic when using group_include (greger)

auto_gen=lex.yy.c cfg.tab.c #lexx, yacc etc
auto_gen_others=cfg.tab.h  # auto generated, non-c

#include  source related defs
include Makefile.sources

# whether or not to install ser.cfg or just ser.cfg.default
# (ser.cfg will never be overwritten by make install, this is usefull
#  when creating packages)
skip_cfg_install?=

#extra modules to exclude
skip_modules?=

# Set document format
# Alternatives are txt, html, xhtml, and pdf (see Makefile.doc)
doc_format?=html

# Module group definitions, default only include the standard group
# Make backwards compatible, don't set group_include default...
#group_include?="standard"

# Modules in this group are considered a standard part of SER (due to widespread usage)
# but they have no dependencies (note that some of these interplay with external systems.
# However, they don't have compile or link dependencies)
module_group_standard=acc_syslog auth avp avpops ctl dispatcher diversion enum \
				exec fifo flatstore gflags maxfwd mediaproxy \
				nathelper options pdt permissions pike print ratelimit \
				registrar rr sanity sl textops timer tm uac unixsock uri \
				usrloc xlog

# Modules in this group are considered a standard part of SER (due to widespread usage)
# but they have dependencies that most be satisfied for compilation
# acc_radius, auth_radius, avp_radius, uri_radius => radiusclient-ng
# acc_db, auth_db, avp_db, db_ops, domain, lcr, msilo, mysql, dialog, postgres, speeddial
# uri_db
#      => mysql server, postgres server or other database back-end (ex. mysql-devel)
# pa, xmlrpc, rls, presence_b2b, xcap => libxml2
# xcap => libcurl
# pa, rls, presence_b2b => dialog
#
# NOTE! All presence modules (dialog, pa, presence_b2b, rls, xcap) have been included in this
# group due to interdependencies
module_group_standard_dep=acc_db acc_radius auth_db auth_radius avp_db avp_radius \
				db_ops domain eval lcr msilo mysql dialog pa postgres \
				presence_b2b rls speeddial uri_db xcap xmlrpc

# For mysql
module_group_mysql=acc_db auth_db avp_db db_ops uri_db domain lcr msilo mysql speeddial

# For radius
module_group_radius=acc_radius auth_radius avp_radius

# For presence
module_group_presence=dialog pa presence_b2b rls xcap

# Modules in this group satisfy specific or niche applications, but are considered
# stable for production use. They may or may not have dependencies
# cpl-c => libxml2
# jabber => expat (library)
# osp => OSP Toolkit (sipfoundry)
# sms => none (external modem)
module_group_stable=cpl-c dbtext jabber osp sms

# Modules in this group are either not complete, untested, or without enough reports
# of usage to allow the module into the stable group. They may or may not have dependencies
module_group_experimental=tls

# if not set on the cmd. line or the env, exclude the below modules.
ifneq ($(group_include),)
	# For group_include, default all modules are excluded except those in include_modules
	exclude_modules?=
else
	# Old defaults for backwards compatibility
	exclude_modules?= 			acc cpl ext extcmd radius_acc radius_auth vm\
							group mangler auth_diameter \
							postgres snmp \
							im \
							jabber  \
							cpl-c \
							auth_radius group_radius uri_radius avp_radius \
							acc_radius dialog pa rls presence_b2b xcap xmlrpc \
							osp tls \
							unixsock eval
endif

# always exclude the CVS dir
override exclude_modules+= CVS $(skip_modules)

# Test for the groups and add to include_modules
ifneq (,$(findstring standard,$(group_include)))
	override include_modules+= $(module_group_standard)
endif

ifneq (,$(findstring standard-dep,$(group_include)))
	override include_modules+= $(module_group_standard_dep)
endif

ifneq (,$(findstring mysql,$(group_include)))
	override include_modules+= $(module_group_mysql)
endif

ifneq (,$(findstring radius,$(group_include)))
	override include_modules+= $(module_group_radius)
endif

ifneq (,$(findstring presence,$(group_include)))
	override include_modules+= $(module_group_presence)
endif

ifneq (,$(findstring stable,$(group_include)))
	override include_modules+= $(module_group_stable)
endif

ifneq (,$(findstring experimental,$(group_include)))
	override include_modules+= $(module_group_experimental)
endif

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

# Historically, the resultant set of modules is: modules/* - exclude_modules + include_modules
# When group_include is used, we want: include_modules (based on group_include) - exclude_modules
ifneq ($(group_include),)
	modules=$(filter-out $(addprefix modules/, \
			$(exclude_modules) $(static_modules)), \
			$(addprefix modules/, $(include_modules) ))
else	
	# Standard, old resultant set
	modules=$(filter-out $(addprefix modules/, \
			$(exclude_modules) $(static_modules)), \
			$(wildcard modules/*))
	modules:=$(filter-out $(modules), $(addprefix modules/, $(include_modules) )) \
			$(modules)
endif
modules_names=$(shell echo $(modules)| \
				sed -e 's/modules\/\([^/ ]*\)\/*/\1.so/g' )
modules_basenames=$(shell echo $(modules)| \
				sed -e 's/modules\/\([^/ ]*\)\/*/\1/g' )
#modules_names=$(patsubst modules/%, %.so, $(modules))
modules_full_path=$(join  $(modules), $(addprefix /, $(modules_names)))


# which utils need compilation (directory path) and which to install
# (full path including file name)
utils_compile=	utils/gen_ha1 utils/sercmd
utils_install=	utils/gen_ha1/gen_ha1 \
				utils/sercmd/sercmd
share_install= scripts/mysql/my_create.sql \
			   scripts/mysql/my_data.sql \
			   scripts/mysql/my_drop.sql


ALLDEP=Makefile Makefile.sources Makefile.defs Makefile.rules

# by default compile with tls hooks support (so that no ser recompile is
#  needed before the tls module can be used)
TLS_HOOKS=1

#include general defs (like CC, CFLAGS  a.s.o)
# hack to force makefile.defs re-inclusion (needed when make calls itself with
# other options -- e.g. make bin)
makefile_defs=0
DEFS:=
include Makefile.defs

NAME=$(MAIN_NAME)

#export relevant variables to the sub-makes
export DEFS PROFILE CC LD MKDEP MKTAGS CFLAGS LDFLAGS INCLUDES MOD_CFLAGS MOD_LDFLAGS 
export LIBS
export LEX YACC YACC_FLAGS
export PREFIX LOCALBASE
# export relevant variables for recursive calls of this makefile 
# (e.g. make deb)
#export LIBS
#export TAR 
#export NAME RELEASE OS ARCH 
#export cfg-prefix cfg-dir bin-prefix bin-dir modules-prefix modules-dir
#export doc-prefix doc-dir man-prefix man-dir ut-prefix ut-dir
#export share-prefix share-dir
#export cfg-target modules-target
#export INSTALL INSTALL-CFG INSTALL-BIN INSTALL-MODULES INSTALL-DOC INSTALL-MAN 
#export INSTALL-TOUCH

tar_name=$(NAME)-$(RELEASE)_src

tar_extra_args+=$(addprefix --exclude=$(notdir $(CURDIR))/, \
					$(auto_gen) $(auto_gen_others))
ifeq ($(CORE_TLS), 1)
	tar_extra_args+=
else
	tar_extra_args+=--exclude=$(notdir $(CURDIR))/tls/* 
endif

ifneq ($(nodeb),)
	tar_extra_args+=--exclude=$(notdir $(CURDIR))/debian 
	tar_name:=$(tar_name)_nodeb
endif

# sanity checks
ifneq ($(TLS),)
        $(warning "make TLS option is obsoleted, try TLS_HOOKS or CORE_TLS")
endif

# include the common rules
include Makefile.rules

#extra targets 

$(NAME): $(extra_objs) # static_modules

lex.yy.c: cfg.lex cfg.tab.h $(ALLDEP)
	$(LEX) $<

cfg.tab.c cfg.tab.h: cfg.y  $(ALLDEP)
	$(YACC) $(YACC_FLAGS) $<

.PHONY: all
all: $(NAME) modules

.PHONY: print-modules
print-modules:
	@echo The following modules was chosen to be included: $(include_modules) ; \
	echo ---------------------------------------------------------- ; \
	echo The following modules will be excluded: $(exclude_modules) ; \
	echo ---------------------------------------------------------- ; \
	echo The following modules will be made: $(modules_basenames) ; \

.PHONY: modules
modules:
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			$(MAKE) -C $$r ; \
		fi ; \
	done 

$(extra_objs):
	-@echo "Extra objs: $(extra_objs)" 
	-@for r in $(static_modules_path) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "Making static module $r" ; \
			$(MAKE) -C $$r static ; \
		fi ; \
	done 

.PHONY: utils
utils:
	-@for r in $(utils_compile) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			$(MAKE) -C $$r ; \
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
		--exclude=$(notdir $(CURDIR))/debian/ser \
		--exclude=$(notdir $(CURDIR))/debian/ser-* \
		--exclude=$(notdir $(CURDIR))/ser_tls* \
		--exclude=CVS* \
		--exclude=.svn* \
		--exclude=.cvsignore \
		--exclude=*.[do] \
		--exclude=*.so \
		--exclude=*.il \
		--exclude=$(notdir $(CURDIR))/ser \
		--exclude=*.gz \
		--exclude=*.bz2 \
		--exclude=*.tar \
		--exclude=*.patch \
		--exclude=.\#* \
		--exclude=*.swp \
		${tar_extra_args} \
		-cf - $(notdir $(CURDIR)) | \
			(mkdir -p tmp/_tar1; mkdir -p tmp/_tar2 ; \
			    cd tmp/_tar1; $(TAR) -xf - ) && \
			    mv tmp/_tar1/$(notdir $(CURDIR)) \
			       tmp/_tar2/"$(NAME)-$(RELEASE)" && \
			    (cd tmp/_tar2 && $(TAR) \
			                    -zcf ../../"$(tar_name)".tar.gz \
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
	-@if [ -d debian ]; then \
		dpkg-buildpackage -rfakeroot -tc; \
	else \
		ln -s pkg/debian debian; \
		dpkg-buildpackage -rfakeroot -tc; \
		rm debian; \
	fi

.PHONY: sunpkg
sunpkg:
	mkdir -p tmp/ser
	mkdir -p tmp/ser_sun_pkg
	$(MAKE) install basedir=tmp/ser prefix=/usr/local
	(cd pkg/solaris; \
	pkgmk -r ../../tmp/ser/usr/local -o -d ../../tmp/ser_sun_pkg/ -v "$(RELEASE)" ;\
	cd ../..)
	cat /dev/null > ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local
	pkgtrans -s tmp/ser_sun_pkg/ ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local \
		IPTELser
	gzip -9 ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local
	rm -rf tmp/ser
	rm -rf tmp/ser_sun_pkg

.PHONY: modules-doc
modules-doc:
	-@for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			$(MAKE) -C $$r/doc $(doc_format) ; \
		fi ; \
	done 

.PHONY: install
install: all mk-install-dirs install-cfg install-bin install-modules \
	install-doc install-man install-utils install-share


.PHONY: dbinstall
dbinstall:
	-@echo "Initializing ser database"
	scripts/mysql/ser_mysql.sh create
	-@echo "Done"

mk-install-dirs: $(cfg-prefix)/$(cfg-dir) $(bin-prefix)/$(bin-dir) \
			$(modules-prefix)/$(modules-dir) $(doc-prefix)/$(doc-dir) \
			$(man-prefix)/$(man-dir)/man8 $(man-prefix)/$(man-dir)/man5 \
			$(share-prefix)/$(share-dir)


$(cfg-prefix)/$(cfg-dir): 
		mkdir -p $(cfg-prefix)/$(cfg-dir)

$(bin-prefix)/$(bin-dir):
		mkdir -p $(bin-prefix)/$(bin-dir)

$(share-prefix)/$(share-dir):
		mkdir -p $(share-prefix)/$(share-dir)

$(modules-prefix)/$(modules-dir):
		mkdir -p $(modules-prefix)/$(modules-dir)


$(doc-prefix)/$(doc-dir):
		mkdir -p $(doc-prefix)/$(doc-dir)

$(man-prefix)/$(man-dir)/man8:
		mkdir -p $(man-prefix)/$(man-dir)/man8

$(man-prefix)/$(man-dir)/man5:
		mkdir -p $(man-prefix)/$(man-dir)/man5
		
# note: on solaris 8 sed: ? or \(...\)* (a.s.o) do not work
install-cfg: $(cfg-prefix)/$(cfg-dir)
		sed -e "s#/usr/.*lib/ser/modules/#$(modules-target)#g" \
			< etc/ser-basic.cfg > $(cfg-prefix)/$(cfg-dir)ser.cfg.sample
		chmod 644 $(cfg-prefix)/$(cfg-dir)ser.cfg.sample
		if [ -z "${skip_cfg_install}" -a \
				! -f $(cfg-prefix)/$(cfg-dir)ser.cfg ]; then \
			mv -f $(cfg-prefix)/$(cfg-dir)ser.cfg.sample \
				$(cfg-prefix)/$(cfg-dir)ser.cfg; \
		fi
		sed -e "s#/usr/.*lib/ser/modules/#$(modules-target)#g" \
			< etc/ser-oob.cfg > $(cfg-prefix)/$(cfg-dir)ser-advanced.cfg.sample
		chmod 644 $(cfg-prefix)/$(cfg-dir)ser-advanced.cfg.sample
		if [ -z "${skip_cfg_install}" -a \
				! -f $(cfg-prefix)/$(cfg-dir)ser-advanced.cfg ]; then \
			mv -f $(cfg-prefix)/$(cfg-dir)ser-advanced.cfg.sample \
				$(cfg-prefix)/$(cfg-dir)ser-advanced.cfg; \
		fi
		# radius dictionary
		$(INSTALL-TOUCH) $(cfg-prefix)/$(cfg-dir)/dictionary.ser 
		$(INSTALL-CFG) etc/dictionary.ser $(cfg-prefix)/$(cfg-dir)

		# TLS configuration
		$(INSTALL-TOUCH) $(cfg-prefix)/$(cfg-dir)/tls.cfg
		$(INSTALL-CFG) modules/tls/tls.cfg $(cfg-prefix)/$(cfg-dir)
		modules/tls/ser_cert.sh -d $(cfg-prefix)/$(cfg-dir)

install-bin: $(bin-prefix)/$(bin-dir) 
		$(INSTALL-TOUCH) $(bin-prefix)/$(bin-dir)/ser 
		$(INSTALL-BIN) ser $(bin-prefix)/$(bin-dir)

install-share: $(share-prefix)/$(share-dir)
	-@for r in $(share_install) "" ; do \
		if [ -n "$$r" ]; then \
			if [ -f "$$r" ]; then \
				$(INSTALL-TOUCH) \
					$(share-prefix)/$(share-dir)/`basename "$$r"` ; \
				$(INSTALL-SHARE)  "$$r"  $(share-prefix)/$(share-dir) ; \
			else \
				echo "ERROR: $$r not found" ; \
			fi ;\
		fi ; \
	done

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

install-utils: utils $(bin-prefix)/$(bin-dir)
	-@for r in $(utils_install) "" ; do \
		if [ -n "$$r" ]; then \
			if [ -f "$$r" ]; then \
				$(INSTALL-TOUCH) \
					$(bin-prefix)/$(bin-dir)/`basename "$$r"` ; \
				$(INSTALL-BIN)  "$$r"  $(bin-prefix)/$(bin-dir) ; \
			else \
				echo "ERROR: $$r not compiled" ; \
			fi ;\
		fi ; \
	done
	sed -e "s#^DEFAULT_SCRIPT_DIR.*#DEFAULT_SCRIPT_DIR=\"$(share-prefix)/$(share-dir)\"#g" \
		< scripts/mysql/ser_mysql.sh > $(bin-prefix)/$(bin-dir)/ser_mysql.sh
	chmod 755 $(bin-prefix)/$(bin-dir)/ser_mysql.sh

install-modules-all: install-modules install-modules-doc


install-doc: $(doc-prefix)/$(doc-dir) install-modules-doc
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/INSTALL 
	$(INSTALL-DOC) INSTALL $(doc-prefix)/$(doc-dir)
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/README-MODULES 
	$(INSTALL-DOC) README-MODULES $(doc-prefix)/$(doc-dir)
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/AUTHORS 
	$(INSTALL-DOC) AUTHORS $(doc-prefix)/$(doc-dir)
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/NEWS
	$(INSTALL-DOC) NEWS $(doc-prefix)/$(doc-dir)
	$(INSTALL-TOUCH) $(doc-prefix)/$(doc-dir)/README 
	$(INSTALL-DOC) README $(doc-prefix)/$(doc-dir)


install-modules-doc: $(doc-prefix)/$(doc-dir)
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


install-man: $(man-prefix)/$(man-dir)/man8 $(man-prefix)/$(man-dir)/man5
		sed -e "s#/etc/ser/ser\.cfg#$(cfg-target)ser.cfg#g" \
			-e "s#/usr/sbin/#$(bin-target)#g" \
			-e "s#/usr/lib/ser/modules/#$(modules-target)#g" \
			-e "s#/usr/share/doc/ser/#$(doc-target)#g" \
			< ser.8 >  $(man-prefix)/$(man-dir)/man8/ser.8
		chmod 644  $(man-prefix)/$(man-dir)/man8/ser.8
		sed -e "s#/etc/ser/ser\.cfg#$(cfg-target)ser.cfg#g" \
			-e "s#/usr/sbin/#$(bin-target)#g" \
			-e "s#/usr/lib/ser/modules/#$(modules-target)#g" \
			-e "s#/usr/share/doc/ser/#$(doc-target)#g" \
			< ser.cfg.5 >  $(man-prefix)/$(man-dir)/man5/ser.cfg.5
		chmod 644  $(man-prefix)/$(man-dir)/man5/ser.cfg.5


##################
# making libraries
# 
# you can use:
#    make libs all include_modules=... install prefix=...
#    make libs proper
#
# but libs should be compiled/installed automaticaly when there are any modules which need them

lib_dependent_modules = dialog pa rls presence_b2b xcap

# exports for libs
export cfg-prefix cfg-dir bin-prefix bin-dir modules-prefix modules-dir
export doc-prefix doc-dir man-prefix man-dir ut-prefix ut-dir \
				  share-prefix share-dir
export INSTALL INSTALL-CFG INSTALL-BIN INSTALL-MODULES INSTALL-DOC INSTALL-MAN \
			   INSTALL-SHARE
export INSTALL-TOUCH

dep_mods = $(filter $(addprefix modules/, $(lib_dependent_modules)), $(modules))
dep_mods += $(filter $(lib_dependent_modules), $(static_modules))

# make 'modules' dependent on libraries if there are modules which need them (experimental)
ifneq ($(strip $(dep_mods)),)
modules:	libs

endif

.PHONY: clean_libs libs

clean_libs:
			$(MAKE) -f Makefile.ser -C lib proper

# cleaning in libs always when cleaning ser
clean:	clean_libs

# remove 'libs' target from targets
lib_goals = $(patsubst libs,,$(MAKECMDGOALS))

libs:	
		$(MAKE) -C lib -f Makefile.ser $(lib_goals)


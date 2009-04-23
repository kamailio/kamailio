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
#  2006-12-09  added new group_include as make option and defined groups 
#               defining which modules to include. Also added new target 
#               print-modules that you can use to check which modules will be 
#               compiled (greger)
#  2007-01-10  added new group_include targets mysql, radius, and presence 
#               improved print-modules output fixed problem in include/exclude
#               logic when using group_include (greger)
#  2007-03-01  fail if a module or a required utility make fail unless 
#              err_fail=0; don't try to make modules with no Makefiles (andrei)
#  2007-03-16  moved the exports to Makefile.defs (andrei)
#  2007-03-29  install-modules changed to use make -C modpath install (andrei)
#  2007-05-04  "if ! foo" not supported in standard sh, switched to 
#                "if foo; then :; else ... ; fi" (andrei)
#  2008-06-23  added 2 new targets: README and man (re-generate the README
#              or manpages for all the modules) (andrei)
#  2008-06-25  make cfg support (use a pre-built cfg.: config.mak) (andrei)
#  2008-06-28  added clean-all, proper-all, install-modules-man and error 
#               checks for install-utils & doc (andrei)
#  2008-07-01  split module list from config.mak into modules.lst so that
#               the modules list can be changed without rebuilding the whole
#               ser (andrei)
#              added cfg-defs, new target that only rebuilds config.mak
#  2009-03-10  replaced DEFS with C_DEFS (DEFS are now used only for
#              "temporary" defines inside modules or libs) (andrei)
#  2009-03-27  multiple modules directory support, see modules_dirs (andrei)
#  2009-04-02  workaround for export not supported in gnu make 3.80
#               target specific variables: use mk_params for each
#               $(MAKE) invocation (andrei)
#  2009-04-22  don't rebuild config.mak or modules.lst if not needed
#              (e.g. on clean) (andrei)
#

# check make version
# required 3.80, recommended 3.81
req_ver=3.80
# the check below works for version number of the type x.yy or x.yy.z*
# (from the GNU Make Cookbook)
ifeq (,$(filter $(req_ver),$(firstword $(sort $(MAKE_VERSION) $(req_ver)))))
$(error make version $(MAKE_VERSION) not supported, use at least $(req_ver))
endif


auto_gen=lex.yy.c cfg.tab.c #lexx, yacc etc
auto_gen_others=cfg.tab.h  # auto generated, non-c

COREPATH=.
#include  source related defs
include Makefile.sources
#include special targets lists
include Makefile.targets

# whether or not the entire build process should fail if building a module or
#  an utility fails
err_fail?=1

# whether or not to install ser.cfg or just ser.cfg.default
# (ser.cfg will never be overwritten by make install, this is usefull
#  when creating packages)
skip_cfg_install?=

#extra modules to exclude
skip_modules?=

# see Makefile.dirs for the directories used for the modules
include Makefile.dirs

# Set document format
# Alternatives are txt, html, xhtml, and pdf (see Makefile.doc)
doc_format?=html

# don't force modules.lst generation if the makefile goals do not
# require it (but if present use it)
ifeq (,$(strip $(filter-out $(clean_targets) $(aux_targets),$(MAKECMDGOALS))))
ifneq (,$(strip $(wildcard modules.lst)))
-include modules.lst
endif
else
include modules.lst
endif # ifneq (,$(strip $(filter-out ...,$(MAKECMDGOALS))))

#if called with group_include, ignore the modules from modules.lst
ifneq ($(group_include),)
	include_modules=
	exclude_modules=
	modules_configured:=0
endif

# Module group definitions, default only include the standard group
# Make backwards compatible, don't set group_include default...
#group_include?="standard"

# Modules in this group are considered a standard part of SER (due to 
# widespread usage) and have no external compile or link dependencies (note 
# that some of these interplay with external systems).
module_group_standard=acc_syslog auth avp ctl dispatcher diversion enum\
				eval exec fifo flatstore gflags maxfwd mediaproxy \
				nathelper options pdt permissions pike print ratelimit \
				registrar rr sanity sl textops timer tm uac unixsock uri \
				usrloc xlog cfg_rpc

# Modules in this group are considered a standard part of SER (due to 
# widespread usage) but they have dependencies that must be satisfied for 
# compilation.
# acc_radius, auth_radius, avp_radius, uri_radius => radiusclient-ng
# acc_db, auth_db, avp_db, db_ops, domain, lcr, msilo, dialog, speeddial,
# uri_db => database module (mysql, postgres, dbtext)
# mysql, postgres => mysql server and client libraries or postgres server and
#  client libraries or other database back-end (ex. mysql-devel)
# pa, xmlrpc => libxml2
# rls => pa
#
# NOTE! All presence modules (dialog, pa, presence_b2b, rls, xcap) have been
# included in this group due to interdependencies
module_group_standard_dep=acc_db acc_radius auth_db auth_radius avp_db \
				avp_radius auth_identity \
				db_ops domain lcr msilo mysql dialog pa postgres \
				presence_b2b rls speeddial uri_db xcap xmlrpc

# For mysql
module_group_mysql=acc_db auth_db avp_db db_ops uri_db domain lcr msilo mysql\
				speeddial

# For radius
module_group_radius=acc_radius auth_radius avp_radius

# For presence
module_group_presence=dialog pa presence_b2b rls xcap

# Modules in this group satisfy specific or niche applications, but are 
# considered stable for production use. They may or may not have dependencies
# cpl-c => libxml2
# jabber => expat (library)
# osp => OSP Toolkit (sipfoundry)
# sms => none (external modem)
module_group_stable=cpl-c dbtext jabber osp sms

# Modules in this group are either not complete, untested, or without enough
# reports of usage to allow the module into the stable group. They may or may
# not have dependencies
module_group_experimental=tls oracle iptrtpproxy

# if not set on the cmd. line, env or in the modules.lst (cfg_group_include)
# exclude the below modules.
ifneq ($(group_include)$(cfg_group_include),)
	# For group_include, default all modules are excluded except those in 
	# include_modules
	exclude_modules?=
else
	# Old defaults for backwards compatibility
	exclude_modules?= 		cpl mangler postgres jabber mysql cpl-c \
							auth_radius uri_radius avp_radius \
							acc_radius dialog pa rls presence_b2b xcap xmlrpc\
							osp tls oracle \
							unixsock dbg print_lib auth_identity ldap \
							db_berkeley db_mysql db_postgres db_oracle \
							db_unixodbc memcached mi_xmlrpc \
							nat_traversal perlvdb purple seas siptrace \
							snmpstats uac_redirect xmpp \
							carrierroute
	# excluded because they do not compile (remove them only after they are
	#  fixed) -- andrei
	exclude_modules+= avpops  bdb dbtext iptrtpproxy pa rls
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
static_modules:=

ALLDEP=config.mak Makefile Makefile.dirs Makefile.sources Makefile.rules

#include general defs (like CC, CFLAGS  a.s.o)
# hack to force makefile.defs re-inclusion (needed when make calls itself with
# other options -- e.g. make bin)
#makefile_defs=0
#C_DEFS:=


# try saved cfg, unless we are in the process of building it or if we're doing
# a clean
ifeq (,$(strip \
	$(filter config.mak config cfg cfg-defs $(clean_targets),$(MAKECMDGOALS))))
include config.mak
ifeq ($(makefile_defs),1)
$(info config.mak loaded)
# config_make valid & used
config_mak=1
endif
else # config.mak doesn't need to be used
ifneq (,$(filter cfg config cfg-defs,$(word 1,$(MAKECMDGOALS))))
# needed here to avoid starting a config submake 
# (e.g. rm -f config.mak; make config.mak), which would either require 
# double Makefile.defs defines execution (suboptimal), would loose
# $(value ...) expansion or would cause some warning (if Makefile.defs exec. 
# is skipped in the "main" makefile invocation).
$(shell rm -rf config.mak)
endif
endif


main_makefile=1
include Makefile.defs

static_modules_path=$(addprefix modules/, $(static_modules))
extra_sources=$(wildcard $(addsuffix /*.c, $(static_modules_path)))
extra_objs=$(extra_sources:.c=.o)

static_defs:= $(foreach  mod, $(static_modules), \
		-DSTATIC_$(shell echo $(mod) | tr [:lower:] [:upper:]) )

override extra_defs+=$(static_defs) $(EXTRA_DEFS)
export extra_defs

# Historically, the resultant set of modules is: modules/* - exclude_modules +
# include_modules
# When group_include is used, we want: include_modules (based on group_include)
# - exclude_modules

ifneq ($(modules_configured),1) 
#modules_all=$(filter-out modules/CVS,$(wildcard modules/*))

# create modules*_all vars
$(foreach mods,$(modules_dirs),$(eval \
	$(mods)_all=$$(filter-out $(mods)/CVS,$$(wildcard $(mods)/*))))
#debugging
#$(foreach mods,$(modules_dirs),$(info "$(mods)_all=$($(mods)_all)"))

ifneq ($(group_include),)
$(foreach mods,$(modules_dirs),$(eval \
	$(mods)=$$(filter-out $$(addprefix $(mods)/, \
			$$(exclude_modules) $$(static_modules)), \
			$$(addprefix $(mods)/, $$(include_modules) )) ))
else	
	# Standard, old resultant set
$(foreach mods,$(modules_dirs),$(eval \
	$(mods)_noinc=$$(filter-out $$(addprefix $(mods)/, \
			$$(exclude_modules) $$(static_modules)), $$($(mods)_all)) \
))
$(foreach mods,$(modules_dirs),$(eval \
	$(mods)=$$(filter-out $$(modules_noinc), \
			$$(addprefix $(mods)/, $$(include_modules) )) $$($(mods)_noinc) \
))
endif # ifneq($(group_include),)
endif # ifneq($(modules_configured),1)

$(foreach mods,$(modules_dirs),$(eval \
	$(mods)_names=$$(shell echo $$($(mods))| \
				sed -e "s/$(mods)"'\/\([^/ ]*\)\/*/\1.so/g' ) \
))
$(foreach mods,$(modules_dirs),$(eval \
	$(mods)_basenames:=$$(shell echo $$($(mods))| \
				sed -e "s/$(mods)"'\/\([^/ ]*\)\/*/\1/g' ) \
))

# all modules from all the $(modules_dirs)
all_modules_lst=$(foreach mods,$(modules_dirs), $($(mods)_all))

# compile modules list (all the compiled mods from  $(modules_dirs))
cmodules=$(foreach mods,$(modules_dirs), $($(mods)))

#modules_names=$(patsubst modules/%, %.so, $(modules))
#modules_full_path=$(join  $(modules), $(addprefix /, $(modules_names)))


# which utils need compilation (directory path) and which to install
# (full path including file name)
utils_compile=	utils/gen_ha1 utils/sercmd
utils_bin_install=	utils/gen_ha1/gen_ha1 utils/sercmd/sercmd
utils_script_install=

# This is the list of files to be installed into the arch-independent
# shared directory (by default /usr/local/share/ser)
share_install= scripts/mysql/my_create.sql \
			   scripts/mysql/my_data.sql   \
			   scripts/mysql/my_drop.sql



NAME=$(MAIN_NAME)

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

nullstring=
space=$(nullstring) $(nullstring)

modules_search_path=$(subst $(space),:,$(strip\
						$(addprefix $(modules_target),$(modules_dirs))))

main.o: DEFS+=-DMODS_DIR='"$(modules_search_path)"'

include Makefile.shared

ifeq ($(config_mak),1)

include Makefile.cfg

else

config.mak: Makefile.defs
	@echo making config...
	@echo "# this file is autogenerated by make cfg" >$@
	@$(call mapf2,cfg_save_var,saved_fixed_vars,$(@))
	@$(call mapf2,cfg_save_var2,saved_chg_vars,$(@))
	@echo "override makefile_defs:=1" >>$@
	@echo "C_DEFS:=\$$(filter-out \$$(DEFS_RM) \$$(extra_defs),\$$(C_DEFS))" \
					"\$$(extra_defs)"  >>$@
	@echo "CFLAGS:=\$$(filter-out \$$(CFLAGS_RM) \$$(CC_EXTRA_OPTS)," \
						"\$$(CFLAGS)) \$$(CC_EXTRA_OPTS)" >>$@

endif # ifeq ($(config_mak),1)

modules.lst:
	@echo  saving modules list...
	@echo "# this file is autogenerated by make modules-cfg" >$@
	@echo "modules_dirs:=$(modules_dirs)" >>$@
	@echo "cfg_group_include=$(group_include)" >>$@
	@$(call cfg_save_var2,include_modules,$@)
	@$(call cfg_save_var2,static_modules,$@)
	@$(call cfg_save_var2,skip_modules,$@)
	@$(call cfg_save_var2,exclude_modules,$@)
	@$(foreach mods,$(modules_dirs), \
		$(call cfg_save_var2,$(mods)_all,$@))
	@$(foreach mods,$(modules_dirs), \
		$(call cfg_save_var2,$(mods)_noinc,$@))
	@$(foreach mods,$(modules_dirs), \
		$(call cfg_save_var2,$(mods),$@))
	@echo "modules_configured:=1" >>$@


.PHONY: cfg config cfg-defs
cfg-defs: config.mak

cfg config: cfg-defs modules-cfg

.PHONY: modules-cfg modules-list modules-lst
modules-cfg modules-list modules-lst:
	rm -f modules.lst
	$(MAKE) modules.lst

.PHONY: all
all: $(NAME) every-module

.PHONY: print-modules
print-modules:
	@echo The following modules were chosen to be included: \
		$(include_modules) ; \
	echo ---------------------------------------------------------- ; \
	echo The following modules will be excluded: $(exclude_modules) ; \
	echo ---------------------------------------------------------- ; \
	echo The following modules will be made; \
	$(foreach mods,$(modules_dirs), \
		echo $(mods)/: $($(mods)_basenames) ; ) \
	#echo DBG: The following modules will be made: $(modules_basenames) ; \


# modules templates (instantiated based on modules_dirs contents)
define MODULES_RULES_template

$(1)_dst=$(modules_prefix)/$(modules_dir)$(1)

.PHONY: $(1)
$(1): modules.lst
	@for r in $($(1)) "" ; do \
		if [ -n "$$$$r" -a -r "$$$$r/Makefile" ]; then \
			echo  "" ; \
			echo  "" ; \
			if  $(MAKE) -C $$$$r $$(mk_params) || [ ${err_fail} != 1 ] ; then \
				:; \
			else \
				exit 1; \
			fi ; \
		fi ; \
	done; true

.PHONY: $(1)-doc
$(1)-doc: modules.lst
	@for r in $($(1)) "" ; do \
		if [ -n "$$$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			$(MAKE) -C $$$$r/doc $(doc_format) $$(mk_params); \
		fi ; \
	done

.PHONY: $(1)-readme

$(1)-readme: modules.lst
	-@for r in $($(1)) "" ; do \
		if [ -n "$$$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			if  $(MAKE) -C $$$$r $$(mk_params) README || [ ${err_fail} != 1 ];\
			then \
				:; \
			else \
				exit 1; \
			fi ; \
		fi ; \
	done; true

.PHONY: $(1)-man
$(1)-man: modules.lst
	-@for r in $($(1)) "" ; do \
		if [ -n "$$$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			if  $(MAKE) -C $$$$r $$(mk_params) man || [ ${err_fail} != 1 ] ;\
			then \
				:; \
			else \
				exit 1; \
			fi ; \
		fi ; \
	done; true

.PHONY: install-$(1)

install-$(1): modules.lst $$($(1)_dst)
	@for r in $($(1)) "" ; do \
		if [ -n "$$$$r" -a -r "$$$$r/Makefile" ]; then \
			echo  "" ; \
			echo  "" ; \
			if  $(MAKE) -C $$$$r install mods_dst=$$($(1)_dst) $$(mk_params) \
				|| [ ${err_fail} != 1 ] ; then \
				:; \
			else \
				exit 1; \
			fi ; \
		fi ; \
	done; true


.PHONY: install-$(1)-doc

install-$(1)-doc: modules.lst $(doc_prefix)/$(doc_dir)$(1)
	@for r in $($(1)_basenames) "" ; do \
		if [ -n "$$$$r" ]; then \
			if [ -f $(1)/"$$$$r"/README ]; then \
				$$(call try_err,\
					$(INSTALL_TOUCH) $(doc_prefix)/$(doc_dir)$(1)/README ); \
				$$(call try_err,\
					$(INSTALL_DOC)  $(1)/"$$$$r"/README  \
									$(doc_prefix)/$(doc_dir)$(1)/README ); \
				$$(call try_err,\
					mv -f $(doc_prefix)/$(doc_dir)$(1)/README \
							$(doc_prefix)/$(doc_dir)$(1)/README."$$$$r" ); \
			fi ; \
		fi ; \
	done; true

.PHONY: install-$(1)-man

install-$(1)-man: $(1)-man $(man_prefix)/$(man_dir)/man7
	@for r in $($(1)_basenames) "" ; do \
		if [ -n "$$$$r" ]; then \
			if [ -f $(1)/"$$$$r"/"$$$$r".7 ]; then \
				$$(call try_err,\
				  $(INSTALL_TOUCH) $(man_prefix)/$(man_dir)/man7/"$$$$r".7 );\
				$$(call try_err,\
					$(INSTALL_MAN)  $(1)/"$$$$r"/"$$$$r".7  \
									$(man_prefix)/$(man_dir)/man7 ); \
			fi ; \
		fi ; \
	done; true


$(modules_prefix)/$(modules_dir)$(1):
		mkdir -p $$(@)

$(doc_prefix)/$(doc_dir)$(1):
		mkdir -p $$(@)


endef

# instantiate the template
$(foreach mods,$(modules_dirs),$(eval $(call MODULES_RULES_template,$(mods))))

#$(foreach mods,$(modules_dirs),$(eval  $(info DUMP: $(call MODULES_RULES_template,$(mods)))))

# build all the modules
every-module: $(modules_dirs)

$(extra_objs):
	@echo "Extra objs: $(extra_objs)" 
	@for r in $(static_modules_path) "" ; do \
		if [ -n "$$r" -a -r "$$r/Makefile"  ]; then \
			echo  "" ; \
			echo  "Making static module $r" ; \
			if $(MAKE) -C $$r static $(mk_params) ; then  \
				:; \
			else \
				exit 1; \
			fi ;  \
		fi ; \
	done

.PHONY: utils
utils:
	@for r in $(utils_compile) "" ; do \
		if [ -n "$$r" ]; then \
			echo  "" ; \
			echo  "" ; \
			if  $(MAKE) -C $$r $(mk_params) || [ ${err_fail} != 1 ] ; \
			then \
				:; \
			else \
				exit 1; \
			fi ; \
		fi ; \
	done; true


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
		--exclude=librpath.lst \
		--exclude=libiname.lst \
		--exclude=makecfg.lst \
		--exclude=config.mak \
		--exclude=modules.lst \
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
	$(MAKE) install basedir=tmp/ser $(mk_params)
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
	$(MAKE) install basedir=tmp/ser prefix=/usr/local $(mk_params)
	(cd pkg/solaris; \
	pkgmk -r ../../tmp/ser/usr/local -o -d ../../tmp/ser_sun_pkg/ -v "$(RELEASE)" ;\
	cd ../..)
	cat /dev/null > ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local
	pkgtrans -s tmp/ser_sun_pkg/ ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local \
		IPTELser
	gzip -9 ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local
	rm -rf tmp/ser
	rm -rf tmp/ser_sun_pkg


.PHONY: install
install: mk_params="compile_for_install=yes"
install: install-bin install-every-module install-cfg \
	install-doc install-man install-utils install-share

.PHONY: dbinstall
dbinstall:
	-@echo "Initializing ser database"
	scripts/mysql/ser_mysql.sh create
	-@echo "Done"

.PHONY: README
README: $(foreach mods,$(modules_dirs),$(mods)-readme)

.PHONY: man
man: $(foreach mods,$(modules_dirs),$(mods)-man)

mk-install_dirs: $(cfg_prefix)/$(cfg_dir) $(bin_prefix)/$(bin_dir) \
			$(modules_prefix)/$(modules_dir) $(doc_prefix)/$(doc_dir) \
			$(man_prefix)/$(man_dir)/man8 $(man_prefix)/$(man_dir)/man5 \
			$(share_prefix)/$(share_dir) \
			$(foreach mods,$(modules_dirs),\
				$(modules_prefix)/$(modules_dir)$(mods) \
				$(doc_prefix)/$(doc_dir)$(mods) )

$(cfg_prefix)/$(cfg_dir): 
		mkdir -p $(cfg_prefix)/$(cfg_dir)

$(bin_prefix)/$(bin_dir):
		mkdir -p $(bin_prefix)/$(bin_dir)

$(share_prefix)/$(share_dir):
		mkdir -p $(share_prefix)/$(share_dir)

$(modules_prefix)/$(modules_dir):
		mkdir -p $(modules_prefix)/$(modules_dir)

$(doc_prefix)/$(doc_dir):
		mkdir -p $(doc_prefix)/$(doc_dir)

$(man_prefix)/$(man_dir)/man8:
		mkdir -p $(man_prefix)/$(man_dir)/man8

$(man_prefix)/$(man_dir)/man7:
		mkdir -p $(man_prefix)/$(man_dir)/man7

$(man_prefix)/$(man_dir)/man5:
		mkdir -p $(man_prefix)/$(man_dir)/man5

# note: sed with POSIX.1 regex doesn't support |, + or ? (darwin, solaris ...) 
install-cfg: $(cfg_prefix)/$(cfg_dir)
		sed -e "s#/usr/.*lib/ser/modules/#$(modules-target)#g" \
			< etc/ser-basic.cfg > $(cfg_prefix)/$(cfg_dir)ser.cfg.sample
		chmod 644 $(cfg_prefix)/$(cfg_dir)ser.cfg.sample
		if [ -z "${skip_cfg_install}" -a \
				! -f $(cfg_prefix)/$(cfg_dir)ser.cfg ]; then \
			mv -f $(cfg_prefix)/$(cfg_dir)ser.cfg.sample \
				$(cfg_prefix)/$(cfg_dir)ser.cfg; \
		fi
		sed -e "s#/usr/.*lib/ser/modules/#$(modules-target)#g" \
			< etc/ser-oob.cfg \
			> $(cfg_prefix)/$(cfg_dir)ser-advanced.cfg.sample
		chmod 644 $(cfg_prefix)/$(cfg_dir)ser-advanced.cfg.sample
		if [ -z "${skip_cfg_install}" -a \
				! -f $(cfg_prefix)/$(cfg_dir)ser-advanced.cfg ]; then \
			mv -f $(cfg_prefix)/$(cfg_dir)ser-advanced.cfg.sample \
				$(cfg_prefix)/$(cfg_dir)ser-advanced.cfg; \
		fi
		# radius dictionary
		$(INSTALL_TOUCH) $(cfg_prefix)/$(cfg_dir)/dictionary.ser
		$(INSTALL_CFG) etc/dictionary.ser $(cfg_prefix)/$(cfg_dir)

		# TLS configuration
		$(INSTALL_TOUCH) $(cfg_prefix)/$(cfg_dir)/tls.cfg
		$(INSTALL_CFG) modules/tls/tls.cfg $(cfg_prefix)/$(cfg_dir)
		modules/tls/ser_cert.sh -d $(cfg_prefix)/$(cfg_dir)

install-bin: $(bin_prefix)/$(bin_dir) $(NAME)
		$(INSTALL_TOUCH) $(bin_prefix)/$(bin_dir)/$(NAME)
		$(INSTALL_BIN) $(NAME) $(bin_prefix)/$(bin_dir)


install-share: $(share_prefix)/$(share_dir)
	@for r in $(share_install) "" ; do \
		if [ -n "$$r" ]; then \
			if [ -f "$$r" ]; then \
				$(call try_err, $(INSTALL_TOUCH) \
						$(share_prefix)/$(share_dir)/`basename "$$r"` ); \
				$(call try_err, \
					$(INSTALL_SHARE)  "$$r"  $(share_prefix)/$(share_dir) );\
			else \
				echo "ERROR: $$r not found" ; \
				if [ ${err_fail} = 1 ] ; then \
					exit 1; \
				fi ; \
			fi ;\
		fi ; \
	done; true


install-every-module: $(foreach mods,$(modules_dirs),install-$(mods))

install-every-module-doc: $(foreach mods,$(modules_dirs),install-$(mods)-doc)

install-every-module-man: $(foreach mods,$(modules_dirs),install-$(mods)-man)

install-utils: utils $(bin_prefix)/$(bin_dir)
	@for r in $(utils_bin_install) "" ; do \
		if [ -n "$$r" ]; then \
			if [ -f "$$r" ]; then \
				$(call try_err, $(INSTALL_TOUCH) \
						$(bin_prefix)/$(bin_dir)/`basename "$$r"` ); \
				$(call try_err,\
					$(INSTALL_BIN)  "$$r"  $(bin_prefix)/$(bin_dir) ); \
			else \
				echo "ERROR: $$r not compiled" ; \
				if [ ${err_fail} = 1 ] ; then \
					exit 1; \
				fi ; \
			fi ;\
		fi ; \
	done; true
	@for r in $(utils_script_install) "" ; do \
		if [ -n "$$r" ]; then \
			if [ -f "$$r" ]; then \
				$(call try_err, $(INSTALL_TOUCH) \
						$(bin_prefix)/$(bin_dir)/`basename "$$r"` ); \
				$(call try_err,\
					$(INSTALL_SCRIPT)  "$$r"  $(bin_prefix)/$(bin_dir) ); \
			else \
				echo "ERROR: $$r not compiled" ; \
				if [ ${err_fail} = 1 ] ; then \
					exit 1; \
				fi ; \
			fi ;\
		fi ; \
	done; true
	# FIXME: This is a hack, this should be (and will be) done properly in
    # per-module Makefiles
	sed -e "s#^DEFAULT_SCRIPT_DIR.*#DEFAULT_SCRIPT_DIR=\"$(share_prefix)/$(share_dir)\"#g" \
		< scripts/mysql/ser_mysql.sh > $(bin_prefix)/$(bin_dir)/ser_mysql.sh
	chmod 755 $(bin_prefix)/$(bin_dir)/ser_mysql.sh


install-modules-all: install-every-module install-every-module-doc


install-doc: $(doc_prefix)/$(doc_dir) install-every-module-doc
	$(INSTALL_TOUCH) $(doc_prefix)/$(doc_dir)/INSTALL 
	$(INSTALL_DOC) INSTALL $(doc_prefix)/$(doc_dir)
	$(INSTALL_TOUCH) $(doc_prefix)/$(doc_dir)/README-MODULES 
	$(INSTALL_DOC) README-MODULES $(doc_prefix)/$(doc_dir)
	$(INSTALL_TOUCH) $(doc_prefix)/$(doc_dir)/AUTHORS 
	$(INSTALL_DOC) AUTHORS $(doc_prefix)/$(doc_dir)
	$(INSTALL_TOUCH) $(doc_prefix)/$(doc_dir)/NEWS
	$(INSTALL_DOC) NEWS $(doc_prefix)/$(doc_dir)
	$(INSTALL_TOUCH) $(doc_prefix)/$(doc_dir)/README 
	$(INSTALL_DOC) README $(doc_prefix)/$(doc_dir)


install-ser-man: $(man_prefix)/$(man_dir)/man8 $(man_prefix)/$(man_dir)/man5
		sed -e "s#/etc/ser/ser\.cfg#$(cfg_target)ser.cfg#g" \
			-e "s#/usr/sbin/#$(bin_target)#g" \
			-e "s#/usr/lib/ser/modules/#$(modules_target)#g" \
			-e "s#/usr/share/doc/ser/#$(doc_target)#g" \
			< ser.8 >  $(man_prefix)/$(man_dir)/man8/ser.8
		chmod 644  $(man_prefix)/$(man_dir)/man8/ser.8
		sed -e "s#/etc/ser/ser\.cfg#$(cfg_target)ser.cfg#g" \
			-e "s#/usr/sbin/#$(bin_target)#g" \
			-e "s#/usr/lib/ser/modules/#$(modules_target)#g" \
			-e "s#/usr/share/doc/ser/#$(doc_target)#g" \
			< ser.cfg.5 >  $(man_prefix)/$(man_dir)/man5/ser.cfg.5
		chmod 644  $(man_prefix)/$(man_dir)/man5/ser.cfg.5

install-man:  install-ser-man install-every-module-man



# libs cleaning targets
.PHONY: clean-libs
clean-libs:
			$(MAKE) -C lib clean

.PHONY: proper-libs realclean-libs distclean-libs maintainer-clean-libs
proper-libs realclean-libs distclean-libs maintainer-clean-libs:
			$(MAKE) -C lib $(patsubst %-libs,%,$@)


# clean modules on make clean
clean: clean-modules
# clean utils on make clean
clean: clean-utils
# cleaning in libs always when cleaning ser
clean: clean-libs

# proper/distclean a.s.o modules, utils and libs too

proper: proper-modules proper-utils proper-libs
distclean: distclean-modules distclean-utils distclean-libs
realclean: realclean-modules realclean-utils realclean-libs
maintainer-clean: maintainer-clean-modules maintainer-clean-utils \
 maintainer-clean-libs

#try to clean everything (including all the modules, even ones that are not
# configured/compiled normally
.PHONY: clean-all
clean-all: cmodules=$(all_modules_lst)
clean-all: clean
maintainer-clean: modules=$(modules_all)

# on make proper clean also the build config (w/o module list)
proper realclean distclean maintainer-clean: clean_cfg

# on maintainer clean, remove also the configured module list
maintainer-clean: clean_modules_cfg

.PHONY: proper-all realclean-all distclean-all
proper-all realclean-all distclean-all: cmodules=$(all_modules_lst)
proper-all realclean-all distclean-all: proper


.PHONY: clean_cfg clean-cfg
clean_cfg clean-cfg:
	rm -f config.mak

.PHONY: clean_modules_cfg clean-modules-cfg
clean_modules_cfg clean-modules-cfg:
	rm -f modules.lst

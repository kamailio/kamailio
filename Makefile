# Kamailio makefile
#
# WARNING: requires gmake (GNU Make)
#  Arch supported: Linux, FreeBSD, SunOS (tested on Solaris 8), OpenBSD (3.2),
#  NetBSD (1.6), OS/X
#

# check make version
# everything works with 3.80, except evals inside ifeq/endif
# (see https://savannah.gnu.org/bugs/index.php?func=detailitem&item_id=1516).
# recommended 3.81
req_ver=3.81
# the check below works for version number of the type x.yy or x.yy.z*
# (from the GNU Make Cookbook)
ifeq (,$(filter $(req_ver),$(firstword $(sort $(MAKE_VERSION) $(req_ver)))))
$(error make version $(MAKE_VERSION) not supported, use at least $(req_ver))
endif


auto_gen=lex.yy.c cfg.tab.c #lexx, yacc etc
auto_gen_others=cfg.tab.h # auto generated, non-c
auto_gen_keep=autover.h # auto generated, should be included in archives

COREPATH=.
#include  source related defs
include Makefile.sources
#include special targets lists
include Makefile.targets

# whether or not the entire build process should fail if building a module or
#  an utility fails
err_fail?=1

# whether or not to install $(MAIN_NAME).cfg or just $(MAIN_NAME).cfg.default
# ($(MAIN_NAME).cfg will never be overwritten by make install, this is usefull
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

# get the groups of modules to compile
include Makefile.groups

# - automatically build the list of excluded modules
# if not set on the cmd. line, env or in the modules.lst (cfg_group_include)
# exclude the below modules.
ifneq ($(group_include)$(cfg_group_include),)
	# For group_include, default all modules are excluded except those in
	# include_modules
	exclude_modules?=
else
	# Old defaults for backwards compatibility
	# excluded because they depend on external libraries
ifeq ($(origin exclude_modules), undefined)
	exclude_modules:= $(sort \
				$(filter-out $(module_group_default), $(mod_list_all)))
endif
endif

# always add skip_modules list - it is done now in modules.lst (dcm)
# override exclude_modules+= $(skip_modules)

# Test for the groups and add to include_modules
ifneq (,$(group_include))
$(eval override include_modules+= $(foreach grp, $(group_include), \
										$(module_group_$(grp)) ))
endif

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
ifeq ($(quiet),verbose)
$(info config.mak loaded)
endif # verbose
export makefile_defs
# config_make valid & used
config_mak=1
ifeq ($(MAIN_NAME),)
$(error "bad config.mak, try re-running make cfg")
endif
endif
else # config.mak doesn't need to be used
ifneq (,$(filter cfg config cfg-defs,$(word 1,$(MAKECMDGOALS))))
# needed here to avoid starting a config submake 
# (e.g. rm -f config.mak; make config.mak), which would either require 
# double Makefile.defs defines execution (suboptimal), would loose
# $(value ...) expansion or would cause some warning (if Makefile.defs exec. 
# is skipped in the "main" makefile invocation).
$(shell rm -rf config.mak)
config_mak=0
makefile_defs=0
exported_vars=0
else
# config.mak not strictly needed, but try to load it if exists for $(Q)
config_mak=skip
-include config.mak
export makefile_defs
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



# list of utils directories that should be compiled by make utils
C_COMPILE_UTILS=	utils/kamcmd
# list of binaries that should be installed alongside
# (they should be created after make utils, see C_COMPILE_UTILS)
C_INSTALL_BIN=	# kamcmd is now installed by ctl

# which utils know to install themselves and should be installed
# along the core (list of utils directories)
ifeq ($(FLAVOUR),kamailio)
C_INSTALL_UTILS= utils/kamctl
else
C_INSTALL_UTILS=
endif
# list of scripts that should be installed along the core 
# (here a script is something that doesn't have a Makefile)
C_INSTALL_SCRIPTS=
# list of extra configs that should be installed along the core
# Note: all the paths of the form /usr/*lib/$(CFG_NAME)/<module_dir>
# will be updated to the directory where the modules will be installed.
C_INSTALL_CFGS=
# list of files that should be installed in the arch-independent 
# directory (by default /usr/local/share/$(MAIN_NAME)))
C_INSTALL_SHARE=




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
						$(foreach m,$(modules_dirs),$($(m)_target))))
		#				$(addprefix $(modules_target),$(modules_dirs))))

# special depends for main.o
main.o: DEFS+=-DMODS_DIR='"$(modules_search_path)"'


#special depends for ver.c
ver.d ver.o: autover.h

include Makefile.shared

ifeq ($(config_mak),1)

include Makefile.cfg

# fix basedir path (relative -> absolute)
ifneq (,$(basedir))
ifeq (,$(filter /%, $(basedir)))
override basedir:=$(CURDIR)/$(basedir)
# remove basedir from command line overrides
MAKEOVERRIDES:=$(filter-out basedir=%,$ $(MAKEOVERRIDES))
endif # (,$(filter /%, $(basedir)))
endif # (,$(basedir))

else ifneq ($(config_mak),skip)

config.mak: Makefile.defs Makefile.groups
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

modules.lst: Makefile.groups
	@echo  saving modules list...
	@echo "# this file is autogenerated by make modules-cfg" >$@
	@echo >>$@
	@echo "# the list of sub-directories with modules" >>$@
	@echo "modules_dirs:=$(modules_dirs)" >>$@
	@echo >>$@
	@echo "# the list of module groups to compile" >>$@
	@echo "cfg_group_include=$(group_include)" >>$@
	@echo >>$@
	@echo "# the list of extra modules to compile" >>$@
	@$(call cfg_save_var2,include_modules,$@)
	@echo >>$@
	@echo "# the list of static modules" >>$@
	@$(call cfg_save_var2,static_modules,$@)
	@echo >>$@
	@echo "# the list of modules to skip from compile list" >>$@
	@$(call cfg_save_var2,skip_modules,$@)
	@echo >>$@
	@echo "# the list of modules to exclude from compile list" >>$@
	@$(call cfg_save_var3,exclude_modules,skip_modules,$@)
	@echo >>$@
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

ifneq ($(wildcard .git),)
# if .git/ exists
repo_ver=$(shell  RV=`git rev-parse --verify --short=6 HEAD 2>/dev/null`;\
					[ -n "$$RV" ] && \
					test -n "`git update-index --refresh --unmerged >/dev/null\
							; git diff-index --name-only HEAD 2>/dev/null | \
								grep -v Makefile`" &&\
						RV="$$RV"-dirty; echo "$$RV")
repo_hash=$(subst -dirty,,$(repo_ver))
repo_state=$(subst %-dirty,dirty,$(findstring -dirty,$(repo_ver)))
autover_h_dep=.git $(filter-out $(auto_gen), $(sources)) cfg.y cfg.lex Makefile
else
# else if .git/ does not exist
repo_ver=
repo_hash="unknown"
repo_state=
autover_h_dep=
endif


autover.h: $(autover_h_dep)
	@echo  "generating autover.h ..."
	@echo "/* this file is autogenerated by make autover.h" >$@
	@echo " * DO NOT EDIT IT" >>$@
	@echo " */" >>$@
	@echo "" >>$@
	@echo "#define REPO_VER \"$(repo_ver)\"" >>$@
	@echo "#define REPO_HASH \"$(repo_hash)\"" >>$@
	@echo "#define REPO_STATE \"$(repo_state)\"" >>$@

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
$(1)_target=$(prefix)/$(modules_dir)$(1)

.PHONY: $(1)
$(1): modules.lst
	@$(foreach r,$($(1)),$(call module_make,$(r),$(mk_params)))

.PHONY: $(1)-doc
$(1)-doc: modules.lst
	+@for r in $($(1)) "" ; do \
		if [ -n "$$$$r" -a -r "$$$$r/Makefile" ]; then \
			$(call oecho, "" ;) \
			$(call oecho, "" ;) \
			$(MAKE) -C $$$$r/doc $(doc_format) $$(mk_params); \
		fi ; \
	done

.PHONY: $(1)-readme

$(1)-readme: modules.lst
	-+@for r in $($(1)) "" ; do \
		if [ -n "$$$$r" -a -r "$$$$r/Makefile" ]; then \
			$(call oecho, "" ;) \
			$(call oecho, "" ;) \
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
	-+@for r in $($(1)_basenames) "" ; do \
		if [ -n "$$$$r" -a -r $(1)/"$$$$r/Makefile" -a \
			 -r $(1)/"$$$$r/$$$$r.xml" ]; then \
			$(call oecho, "" ;) \
			$(call oecho, "" ;) \
			if  $(MAKE) -C $(1)/"$$$$r" $$(mk_params) man || \
				[ ${err_fail} != 1 ] ;\
			then \
				:; \
			else \
				exit 1; \
			fi ; \
		fi ; \
	done; true

.PHONY: install-$(1)

install-$(1): modules.lst $$($(1)_dst)
	+@for r in $($(1)) "" ; do \
		if [ -n "$$$$r" -a -r "$$$$r/Makefile" ]; then \
			$(call oecho, "" ;) \
			$(call oecho, "" ;) \
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
		if [ -n "$$$$r" -a -r $(1)/"$$$$r/Makefile" ]; then \
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
		if [ -n "$$$$r" -a -r $(1)/"$$$$r/Makefile" ]; then \
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
modules-all every-module: $(modules_dirs)

$(extra_objs):
	@echo "Extra objs: $(extra_objs)" 
	@for r in $(static_modules_path) "" ; do \
		if [ -n "$$r" -a -r "$$r/Makefile"  ]; then \
			$(call oecho, "" ;) \
			$(call oecho, "Making static module $r" ;) \
			if $(MAKE) -C $$r static $(mk_params) ; then  \
				:; \
			else \
				exit 1; \
			fi ;  \
		fi ; \
	done

.PHONY: utils
utils:
	@for r in $(C_COMPILE_UTILS) "" ; do \
		if [ -n "$$r" ]; then \
			$(call oecho, "" ;) \
			$(call oecho, "" ;) \
			if  $(MAKE) -C $$r $(mk_params) || [ ${err_fail} != 1 ] ; \
			then \
				:; \
			else \
				exit 1; \
			fi ; \
		fi ; \
	done; true


dbg: sip-router
	gdb -command debug.gdb

.PHONY: makefile_vars makefile-vars
makefile_vars makefile-vars:
	echo "FLAVOUR?=$(FLAVOUR)" > Makefile.vars

.PHONY: tar
.PHONY: dist

dist: tar

tar: makefile_vars $(auto_gen_keep)
	$(TAR) -C .. \
		--exclude=$(notdir $(CURDIR))/test* \
		--exclude=$(notdir $(CURDIR))/tmp* \
		--exclude=$(notdir $(CURDIR))/debian \
		--exclude=$(notdir $(CURDIR))/debian/$(MAIN_NAME) \
		--exclude=$(notdir $(CURDIR))/debian/$(MAIN_NAME)-* \
		--exclude=$(notdir $(CURDIR))/$(MAIN_NAME)_tls* \
		--exclude=.git* \
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
		--exclude=$(notdir $(CURDIR))/$(MAIN_NAME) \
		--exclude=*.gz \
		--exclude=*.bz2 \
		--exclude=*.tar \
		--exclude=*.patch \
		--exclude=.\#* \
		--exclude=*.swp \
		--exclude=*.swo \
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
	mkdir -p tmp/$(MAIN_NAME)/usr/local
	$(MAKE) install basedir=$(CURDIR)/tmp/$(MAIN_NAME) $(mk_params)
	$(TAR) -C tmp/$(MAIN_NAME)/ -zcf ../$(NAME)-$(RELEASE)_$(OS)_$(ARCH).tar.gz .
	rm -rf tmp/$(MAIN_NAME)

.PHONY: deb
deb:
	-@if [ -d debian ]; then \
		dpkg-buildpackage -rfakeroot -tc; \
		rm debian; \
	else \
		ln -s pkg/$(MAIN_NAME)/deb/debian debian; \
		dpkg-buildpackage -rfakeroot -tc; \
		rm debian; \
	fi

.PHONY: deb-stable
deb-stable:
	-@if [ -d debian ]; then \
		dpkg-buildpackage -rfakeroot -tc; \
		rm debian; \
	else \
		ln -s pkg/$(MAIN_NAME)/deb/wheezy debian; \
		dpkg-buildpackage -rfakeroot -tc; \
		rm debian; \
	fi

.PHONY: sunpkg
sunpkg:
	mkdir -p tmp/$(MAIN_NAME)
	mkdir -p tmp/$(MAIN_NAME)_sun_pkg
	$(MAKE) install basedir=$(CURDIR)/tmp/$(MAIN_NAME) \
			prefix=/usr/local $(mk_params)
	(cd pkg/$(MAIN_NAME)/solaris; \
	pkgmk -r ../../tmp/$(MAIN_NAME)/usr/local -o -d ../../tmp/$(MAIN_NAME)_sun_pkg/ -v "$(RELEASE)" ;\
	cd ../..)
	cat /dev/null > ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local
	pkgtrans -s tmp/$(MAIN_NAME)_sun_pkg/ ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local \
		IPTEL$(MAIN_NAME)
	gzip -9 ../$(NAME)-$(RELEASE)-$(OS)-$(ARCH)-local
	rm -rf tmp/$(MAIN_NAME)
	rm -rf tmp/$(MAIN_NAME)_sun_pkg


.PHONY: install
install: mk_params="compile_for_install=yes"
install: install-bin install-every-module install-cfg \
	install-doc install-man install-utils install-share

.PHONY: dbinstall
dbinstall:
	-@echo "Initializing $(MAIN_NAME) database"
	scripts/mysql/$(SCR_NAME)_mysql.sh create
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

$(run_prefix)/$(run_dir): 
		mkdir -p $(run_prefix)/$(run_dir)

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
	@if [ -f etc/$(CFG_NAME).cfg ]; then \
			sed $(foreach m,$(modules_dirs),\
					-e "s#/usr/[^:]*lib/$(CFG_NAME)/$(m)\([:/\"]\)#$($(m)_target)\1#g") \
					-e "s#/usr/local/etc/$(CFG_NAME)/#$(cfg_target)#g" \
				< etc/$(CFG_NAME).cfg \
				> $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME).cfg.sample; \
			chmod 644 $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME).cfg.sample; \
			if [ -z "${skip_cfg_install}" -a \
					! -f $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME).cfg ]; then \
				mv -f $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME).cfg.sample \
					$(cfg_prefix)/$(cfg_dir)$(MAIN_NAME).cfg; \
			fi; \
		fi
	@if [ -f etc/$(CFG_NAME)-basic.cfg ]; then \
			sed $(foreach m,$(modules_dirs),\
					-e "s#/usr/[^:]*lib/$(CFG_NAME)/$(m)\([:/\"]\)#$($(m)_target)\1#g") \
					-e "s#/usr/local/etc/$(CFG_NAME)/#$(cfg_target)#g" \
				< etc/$(CFG_NAME)-basic.cfg \
				> $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-basic.cfg.sample; \
			chmod 644 $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-basic.cfg.sample; \
			if [ -z "${skip_cfg_install}" -a \
					! -f $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-basic.cfg ]; then \
				mv -f $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-basic.cfg.sample \
					$(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-basic.cfg; \
			fi; \
		fi
	@if [ -f etc/$(CFG_NAME)-oob.cfg ]; then \
			sed $(foreach m,$(modules_dirs),\
					-e "s#/usr/[^:]*lib/$(CFG_NAME)/$(m)\([:/\"]\)#$($(m)_target)\1#g") \
					-e "s#/usr/local/etc/$(CFG_NAME)/#$(cfg_target)#g" \
				< etc/$(CFG_NAME)-oob.cfg \
				> $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-advanced.cfg.sample; \
			chmod 644 $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-advanced.cfg.sample; \
			if [ -z "${skip_cfg_install}" -a \
					! -f $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-advanced.cfg ]; \
			then \
				mv -f $(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-advanced.cfg.sample \
					$(cfg_prefix)/$(cfg_dir)$(MAIN_NAME)-advanced.cfg; \
			fi; \
		fi
	@# other configs
	@for r in $(C_INSTALL_CFGS) ""; do \
			if [ -n "$$r" ]; then \
				if [ -f "$$r" ]; then \
					n=`basename "$$r"` ; \
					sed $(foreach m,$(modules_dirs),\
							-e "s#/usr/[^:]*lib/$(CFG_NAME)/$(m)\([:/\"]\)#$($(m)_target)\1#g") \
						< "$$r" \
						> "$(cfg_prefix)/$(cfg_dir)$$n.sample" ; \
					chmod 644 "$(cfg_prefix)/$(cfg_dir)$$n.sample" ; \
					if [ -z "${skip_cfg_install}" -a \
							! -f "$(cfg_prefix)/$(cfg_dir)$$n" ]; \
					then \
						mv -f "$(cfg_prefix)/$(cfg_dir)$$n.sample" \
								"$(cfg_prefix)/$(cfg_dir)$$n"; \
					fi ; \
				else \
					echo "ERROR: $$r not found" ; \
					if [ ${err_fail} = 1 ] ; then \
						exit 1; \
					fi ; \
				fi ; \
			fi ; \
			: ; done; true
	@# radius dictionary
	@$(INSTALL_TOUCH) $(cfg_prefix)/$(cfg_dir)/dictionary.$(CFG_NAME)
	@$(INSTALL_CFG) etc/dictionary.$(CFG_NAME) $(cfg_prefix)/$(cfg_dir)
	@echo "config files installed"

install-bin: $(bin_prefix)/$(bin_dir) $(NAME)
	$(INSTALL_TOUCH) $(bin_prefix)/$(bin_dir)/$(NAME)
	$(INSTALL_BIN) $(NAME) $(bin_prefix)/$(bin_dir)


install-share: $(share_prefix)/$(share_dir)
	@for r in $(C_INSTALL_SHARE) "" ; do \
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
	@for r in $(C_INSTALL_BIN) "" ; do \
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
	@for r in $(C_INSTALL_SCRIPTS) "" ; do \
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
	@for ut in $(C_INSTALL_UTILS) "" ; do \
		if [ -n "$$ut" ]; then \
			if [ -d "$$ut" ]; then \
				$(call try_err, $(MAKE) -C "$${ut}" install-if-newer ) ;\
			fi ;\
		fi ; \
	done; true


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


install-sr-man: $(man_prefix)/$(man_dir)/man8 $(man_prefix)/$(man_dir)/man5
		@sed -e "s#/etc/$(CFG_NAME)/$(CFG_NAME)\.cfg#$(cfg_target)$(MAIN_NAME).cfg#g" \
			-e "s#/usr/sbin/#$(bin_target)#g" \
			$(foreach m,$(modules_dirs),\
				-e "s#/usr/lib/$(CFG_NAME)/$(m)\([^_]\)#$($(m)_target)\1#g") \
			-e "s#/usr/share/doc/$(CFG_NAME)/#$(doc_target)#g" \
			-e "s#$(SRC_NAME)#$(MAIN_NAME)#g" \
			< $(SRC_NAME).8 >  \
							$(man_prefix)/$(man_dir)/man8/$(MAIN_NAME).8
		@chmod 644  $(man_prefix)/$(man_dir)/man8/$(MAIN_NAME).8
		@sed -e "s#/etc/$(CFG_NAME)/$(CFG_NAME)\.cfg#$(cfg_target)$(MAIN_NAME).cfg#g" \
			-e "s#/usr/sbin/#$(bin_target)#g" \
			$(foreach m,$(modules_dirs),\
				-e "s#/usr/lib/$(CFG_NAME)/$(m)\([^_]\)#$($(m)_target)\1#g") \
			-e "s#/usr/share/doc/$(CFG_NAME)/#$(doc_target)#g" \
			-e "s#$(SRC_NAME)#$(MAIN_NAME)#g" \
			< $(SRC_NAME).cfg.5 >  \
			$(man_prefix)/$(man_dir)/man5/$(MAIN_NAME).cfg.5
		@chmod 644  $(man_prefix)/$(man_dir)/man5/$(MAIN_NAME).cfg.5

install-man:  install-sr-man install-every-module-man



# libs cleaning targets
.PHONY: clean-libs
clean-libs:
			$(MAKE) -C lib clean

.PHONY: proper-libs realclean-libs distclean-libs maintainer-clean-libs
proper-libs realclean-libs distclean-libs maintainer-clean-libs:
			$(MAKE) -C lib $(patsubst %-libs,%,$@)

# utils cleaning targets

.PHONY: clean-utils
clean-utils:
	@for r in $(C_COMPILE_UTILS) $(C_INSTALL_UTILS) "" ; do \
		if [ -d "$$r" ]; then \
			 $(MAKE) -C "$$r" clean ; \
		fi ; \
	done

.PHONY: proper-utils
.PHONY: distclean-utils
.PHONY: realclean-utils
.PHONY: maintainer-clean-utils
proper-utils realclean-utils distclean-utils maintainer-clean-utils: \
 clean_target=$(patsubst %-utils,%,$@)
proper-utils realclean-utils distclean-utils maintainer-clean-utils:
	@for r in $(C_COMPILE_UTILS) $(C_INSTALL_UTILS) "" ; do \
		if [ -d "$$r" ]; then \
			 $(MAKE) -C "$$r" $(clean_target); \
		fi ; \
	done

# clean extra binary names (common "flavour" names)
clean: clean-extra-names
# clean modules on make clean
clean: clean-modules
# clean utils on make clean
clean: clean-utils
# cleaning in libs always when cleaning sip-router
clean: clean-libs

.PHONY: clean-extra-names
clean-extra-names:
	@rm -f $(filter-out $(MAIN_NAME), sip-router ser kamailio)

# proper/distclean-old a.s.o modules, utils and libs too

proper: clean-extra-names proper-modules proper-utils proper-libs
distclean-old: distclean-modules distclean-utils distclean-libs
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
proper realclean distclean-old maintainer-clean: clean_cfg

# on maintainer clean, remove also the configured module list
maintainer-clean: clean_modules_cfg clean_makefile_vars

.PHONY: proper-all realclean-all distclean-all
proper-all realclean-all distclean-all: cmodules=$(all_modules_lst)
proper-all realclean-all distclean-all: proper


.PHONY: clean_cfg clean-cfg
clean_cfg clean-cfg:
	rm -f config.mak

.PHONY: clean_modules_cfg clean-modules-cfg
clean_modules_cfg clean-modules-cfg:
	rm -f modules.lst

.PHONY: clean_makefile_vars clean-makefile-vars
	rm -f Makefile.vars

# clean everything generated - shortcut on maintainer-clean
.PHONY: pure
pure distclean: maintainer-clean

.PHONY: install_initd_debian install-initd-debian
install_initd_debian install-initd-debian:
	sed -e "s#DAEMON=/usr/sbin/kamailio#DAEMON=$(bin_prefix)/$(bin_dir)$(NAME)#g" \
		-e "s#NAME=kamailio#NAME=$(NAME)#g" \
		-e "s#DESC=Kamailio#DESC=$(NAME)#g" \
		-e "s#HOMEDIR=/var/run/kamailio#HOMEDIR=/var/run/$(NAME)#g" \
		-e "s#DEFAULTS=/etc/default/kamailio#DEFAULTS=/etc/default/$(NAME)#g" \
		-e "s#CFGFILE=/etc/kamailio/kamailio.cfg#CFGFILE=$(cfg_prefix)/$(cfg_dir)$(NAME).cfg#g" \
		< pkg/kamailio/deb/debian/kamailio.init \
		> /etc/init.d/$(NAME)
	chmod +x /etc/init.d/$(NAME)
	sed -e "s#RUN_KAMAILIO=no#RUN_KAMAILIO=yes#g" \
		< pkg/kamailio/deb/debian/kamailio.default \
		> /etc/default/$(NAME)
	mkdir -p /var/run/$(NAME)
	adduser --quiet --system --group --disabled-password \
        --shell /bin/false --gecos "$(NAME)" \
        --home /var/run/$(NAME) $(NAME)
	chown $(NAME):$(NAME) /var/run/$(NAME)

.PHONY: install_initd_centos install-initd-centos
install_initd_centos install-initd-centos:
	sed -e "s#KAM=/usr/sbin/kamailio#KAM=$(bin_prefix)/$(bin_dir)$(NAME)#g" \
		-e "s#PROG=kamailio#PROG=$(NAME)#g" \
		-e "s#DEFAULTS=/etc/default/kamailio#DEFAULTS=/etc/default/$(NAME)#g" \
		-e "s#PID_FILE=/var/run/kamailio.pid#PID_FILE=/var/run/$(NAME).pid#g" \
		-e "s#LOCK_FILE=/var/lock/subsys/kamailio#LOCK_FILE=/var/lock/subsys/$(NAME)#g" \
		-e "s#KAMCFG=/etc/kamailio/kamailio.cfg#KAMCFG=$(cfg_prefix)/$(cfg_dir)$(NAME).cfg#g" \
		< pkg/kamailio/rpm/kamailio.init \
		> /etc/init.d/$(NAME)
	chmod +x /etc/init.d/$(NAME)
	sed -e "s#RUN_KAMAILIO=no#RUN_KAMAILIO=yes#g" \
		-e "s#USER=kamailio#USER=$(NAME)#g" \
		-e "s#GROUP=kamailio#GROUP=$(NAME)#g" \
		< pkg/kamailio/rpm/kamailio.default \
		> /etc/default/$(NAME)
	/usr/sbin/groupadd -r $(NAME)
	/usr/sbin/useradd -r -g $(NAME) -s /bin/false -c "Kamailio Daemon" \
                  -d /var/run/$(NAME) $(NAME)

.PHONY: dbschema
dbschema:
	-@echo "Build database schemas"
	$(MAKE) -C lib/srdb1/schema
	-@echo "Done"

.PHONY: printcdefs
printcdefs:
	@echo -n $(C_DEFS)

.PHONY: printvar
printvar:
	@echo "Content of <$(v)> is:"
	@echo -n $($(v))
	@echo

.PHONY: uninstall
uninstall:
	@echo "-Installation details:"
	@echo " *PREFIX Path is: ${PREFIX}"
	@echo " *BINDIR Path is: ${bin_prefix}/${bin_dir}"
	@echo " *CFGDIR Path is: ${cfg_prefix}/${cfg_dir}"
	@echo " *DOCDIR Path is: ${doc_prefix}/${doc_dir}"
	@echo " *LIBDIR Path is: ${lib_prefix}/${lib_dir}"
	@echo " *MANDIR Path is: ${man_prefix}/${man_dir}"
	@echo " *SHRDIR Path is: ${share_prefix}/${share_dir}"
	@echo " *RUNDIR Path is: $(run_prefix)/$(run_dir)"
	@if [ "${PREFIX}" != "/usr/local" ] ; then \
		echo "-Custom PREFIX Path" ; \
		if [ "${PREFIX}" = "/" -o "${PREFIX}" = "/usr" ] ; then \
			echo "-Custom installation in a system folder" ; \
			echo "-This is advanced installation" ; \
			echo "-You seem to be in control of what files were deployed" ; \
			echo "-Folders listed above should give hints about what to delete" ; \
		else \
			echo "-Uninstall should be just removal of the folder: ${PREFIX}" ; \
			echo "-WARNING: before deleting, be sure ${PREFIX} is not a system directory" ; \
		fi ; \
	else \
		echo "-Run following commands to uninstall:" ; \
		echo ; \
		echo "rm ${bin_prefix}/${bin_dir}${MAIN_NAME}" ; \
		if [ "${FLAVOUR}" = "kamailio" ] ; then \
			echo "rm ${bin_prefix}/${bin_dir}kamctl" ; \
			echo "rm ${bin_prefix}/${bin_dir}kamdbctl" ; \
		fi ; \
		echo "rm ${bin_prefix}/${bin_dir}kamcmd" ; \
		echo "rm ${man_prefix}/${man_dir}man5/$(MAIN_NAME).cfg.5" ; \
		echo "rm ${man_prefix}/${man_dir}man8/$(MAIN_NAME).8" ; \
		if [ "${FLAVOUR}" = "kamailio" ] ; then \
			echo "rm ${man_prefix}/${man_dir}kamctl.8" ; \
			echo "rm ${man_prefix}/${man_dir}kamdbctl.8" ; \
		fi ; \
		echo "rm -rf ${cfg_prefix}/${cfg_dir}" ; \
		echo "rm -rf ${doc_prefix}/${doc_dir}" ; \
		echo "rm -rf ${lib_prefix}/${lib_dir}" ; \
		echo "rm -rf ${share_prefix}/${share_dir}" ; \
		echo "rm -rf $(run_prefix)/$(run_dir)" ; \
		echo ; \
		echo "-WARNING: before running the commands, be sure they don't delete any system directory or file" ; \
	fi ;
	@echo

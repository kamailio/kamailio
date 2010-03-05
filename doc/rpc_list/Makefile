
COREPATH=../..

#include $(COREPATH)/Makefile.defs

CFG2TXT=../scripts/cdefs2doc/dump_rpcs.pl
CFG2DOCBOOK=../scripts/cdefs2doc/dump_rpcs.pl

# output directory for generated txt files
txt_output_dir=.
# output directory for generated docbook xml files
docbook_output_dir=docbook

# list of files contanining rpc defs in the following format:
# <filename>:<cfg_grp_name> 
# can be easily filled/updated by pasting the output of:
#  make print-lst |sed  -e 's!^ \?../..\(.*\)!$(COREPATH)\1 \\!'
files_list= $(COREPATH)/core_cmd.c:core \
	$(COREPATH)/modules_s/pdt/pdt.c:pdt \
	$(COREPATH)/modules_s/pike/rpc.c:pike \
	$(COREPATH)/modules_s/presence_b2b/rpc.c:presence_b2b \
	$(COREPATH)/modules_s/gflags/gflags.c:gflags \
	$(COREPATH)/modules_s/dispatcher/ds_rpc.c:dispatcher \
	$(COREPATH)/modules_s/prefix_route/pr_rpc.c:prefix_route \
	$(COREPATH)/modules_s/domain/domain_rpc.c:domain \
	$(COREPATH)/modules_s/cpl-c/cpl_rpc.c:cpl-c \
	$(COREPATH)/modules_s/sl/sl_stats.c:sl \
	$(COREPATH)/modules_s/usrloc/ul_rpc.c:usrloc_s \
	$(COREPATH)/modules_s/ratelimit/ratelimit.c:ratelimit \
	$(COREPATH)/modules/tm/tm.c:tm \
	$(COREPATH)/modules/tls/tls_rpc.c:tls \
	$(COREPATH)/modules/lcr/lcr_rpc.c:lcr \
	$(COREPATH)/modules/debugger/debugger_api.c:debugger \
	$(COREPATH)/modules/ctl/ctl.c:ctl \
	$(COREPATH)/modules/cfg_rpc/cfg_rpc.c:cfg_rpc \
	$(COREPATH)/modules/db_flatstore/flat_rpc.c:db_flatstore \
	$(COREPATH)/modules/mi_rpc/mi_rpc_mod.c:mi_rpc \
	$(COREPATH)/modules_k/htable/htable.c:htable \
	$(COREPATH)/modules_k/usrloc/ul_rpc.c:usrloc_k \
	$(COREPATH)/modules_k/uac/uac_reg.c:uac


# list of excluded groups
grp_exclude=pa
# list of file prefixes to exclude (full path needed)
file_exclude=$(COREPATH)/modules_s/tls/ \
				$(COREPATH)/obsolete/pa/ \
				$(COREPATH)/obsolete/rls/

# special per file group overrides
# format= grp_filename=... ,where filename does not contain the extension
# e.g.:
#     grp_f_tcp_options=tcp
#     grp_f_sctp_options=sctp

# special per group group name overrides
# e.g.:
#      grp_g_maxfwd=mf

# override auto-detected group if set to 1 (else the group is used inside the
# file only if it cannot be aut-odetected)
ifeq ($(group_override),1)
override force_grp=force-
else
override force_grp=
endif

# command used for gcc (contains extra includes)
gcc=gcc -I$(COREPATH)/lib -I/usr/include/libxml2

#filtered files list
flist=$(filter-out $(addsuffix %,$(file_exclude)),\
			$(filter-out $(addprefix %:,$(grp_exclude)),$(files_list)) )

# throws an error if input is not in the format filename:grp
check_fname_grp=$(if $(filter-out 2,$(words $(subst :, ,$(1)))),\
					$(error bad format "$(1)", it should be filename:grp))

# get prereq from file:grp (get_prereq(file:grp) => file)
get_prereq=$(firstword $(subst :, ,$(1)))

# get grp from file:grp (get_grp(file:grp) => grp)
get_listed_grp=$(word 2, $(subst :, ,$(1)))

# get base file name from file:grp: get_bname(file:grp) 
# => basename(file) without extension (e.g. get_bname(foo/bar.c:x) => bar)
# 
get_bname=$(basename $(notdir $(call get_prereq,$(1))))

#get grp from file:grp, using the overrides
get_grp=$(strip $(if $(grp_f_$(call get_bname,$(1))), \
					$(grp_f_$(call get_bname,$(1))),\
					$(if $(grp_g_$(call get_listed_grp,$(1))),\
						$(grp_g_$(call get_listed_grp,$(1))),\
						$(call get_listed_grp,$(1))) ) )

# get target from file:grp (get_target(file:grp) => rpc_grp.txt)
get_target=rpc_$(call get_grp,$(1))

define  mk_rules

$(call check_fname_grp, $(1))

#$$(info generating rpc_$$(call get_grp,$(1)).txt: $$(call get_prereq,$(1)))

$(txt_output_dir)/$$(call get_target,$(1)).txt: \
								$$(call get_prereq,$(1)) Makefile $(CFG2TXT)
	$(CFG2TXT) --file $$< --$(force_grp)grp=$$(call get_grp,$(1)) \
		--gcc="$(gcc)" --txt > "$$@" ||  (rm -f "$$@"; exit 1)

$(docbook_output_dir)/$$(call get_target,$(1)).xml: \
								$$(call get_prereq,$(1)) Makefile $(CFG2TXT)
	$(CFG2DOCBOOK) --file $$< --$(force_grp)grp=$$(call get_grp,$(1)) \
		--gcc="$(gcc)" --docbook > "$$@" ||  (rm -f "$$@"; exit 1)


clean_$$(call get_target,$(1)).txt:
	rm -f "$(txt_output_dir)/$$(call get_target,$(1)).txt"

clean_$$(call get_target,$(1)).xml:
	rm -f "$(docbook_output_dir)/$$(call get_target,$(1)).xml"

txt: $(txt_output_dir)/$$(call get_target,$(1)).txt

docbook: $(docbook_output_dir)/$$(call get_target,$(1)).xml

clean_txt: clean_$$(call get_target,$(1)).txt

clean_docbook: clean_$$(call get_target,$(1)).xml


endef


# help will be the default rule (on-purpose since without having a patched
# GCC:TranslationUnit module, make all won't work)
.PHONY: help
help:
	@echo "To regenerate $(foreach f,$(flist),$(call get_target,$f).{txt,xml})"
	@echo "type: $(MAKE) all ."
	@echo "or to regenerate all the rpc lists by searching all"
	@echo " the source files for definitions, type: $(MAKE) autogen ."
	@echo "NOTE: you need the GCC:TranslationUnit perl module with an "
	@echo "extra patch applied (see $(CFG2TXT) --patch)."

.PHONY: txt
txt:

.PHONY: docbook
docbook:

.PHONY: clean_txt
clean_txt:

.PHONY: clean_docbook
clean_docbook:


.PHONY: all
all: txt $(docbook_output_dir)/rpc_list.xml

.PHONY: clean
clean: clean_txt clean_docbook
	@rm -f $(docbook_output_dir)/rpc_list.xml

.PHONY: proper
proper:
	@rm -f $(txt_output_dir)/rpc_*.txt
	@rm -f $(docbook_output_dir)/rpc_*.xml

repo_ver="sip-router"\
	"git-$(shell  git rev-parse --verify --short=6 HEAD 2>/dev/null)"
ifeq ($(repo_ver),git-)
repo_ver="sip-router unknown"
endif

$(docbook_output_dir)/rpc_list.xml: \
		$(foreach f,$(flist),$(docbook_output_dir)/$(call get_target,$f).xml)
	@echo '<?xml version="1.0" encoding="UTF-8"?>' >$@
	@echo '<!-- this file is autogenerated, do not edit! -->' >>$@
	@echo '<!DOCTYPE section PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN"' >>$@
	@echo '	"http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd">' >>$@
	@echo '<book id="rpc_list"'\
		'xmlns:xi="http://www.w3.org/2001/XInclude">' >>$@
	@echo '	<title>RPC Exports List</title>' >>$@
	@echo '	<info><revhistory><revision>' >>$@
	@echo '		<revnumber>'$(repo_ver)'</revnumber>' >>$@
	@echo '		<revremark>' >>$@
	@echo "			Automatically generated by:">>$@
	@echo "			$(MAKE) -C doc/rpc_list $(MAKECMDGOALS)" >>$@
	@echo '		</revremark>' >>$@
	@echo '	</revision></revhistory></info>' >>$@
	@$(foreach f,$(flist),\
		echo '		<xi:include'\
			'href="'$(call get_target,$f).xml'"/>' \
			>>$@ ; )
	@echo '</book>' >>$@

find_rpc_files_cmd= find $(COREPATH) -type f -name "*.c" \
		-exec grep "rpc_export_t[	 ][a-zA-Z0-9_]*\[\][	 ]*=" /dev/null {} \; \
		| cut -d: -f1

# shell command to generate a file:grp list from a list of files
# grp will be the modulename if the file is in a module directory or
# the file name with the extension and _cmd, cmd_ or _rpc stripped out of
# it.
# output: list of  " "filename":"grpname
gen_file_grp_cmd=\
sed -e "s!\(.*/modules[^/]*/\([^/][^/]*\)/.*\)! \1:\2!" \
    -e "s!^\([^ ].*/\([^/.]*\)[^/]*$$\)!\1:\2!" \
    -e "s!^\([^ :]*\):\(.*\)_cmd[_]*!\1:\2!" \
    -e "s!^\([^ :]*\):\(.*\)cmd[_]*!\1:\2!" \
    -e "s!^\([^ :]\):\(.*\)_rpc[_]*!\1:\2!"

# finds all the files containing cfg defs
.PHONY: find
find:
	@$(find_rpc_files_cmd)

# print the list of the autogenerated files
.PHONY: print-lst
print-lst:
	@$(find_rpc_files_cmd) | $(gen_file_grp_cmd)

# try to generate the docs from all the sources
.PHONY: autogen
autogen:
	LST=`$(find_rpc_files_cmd)| $(gen_file_grp_cmd) | xargs echo` && \
	[ -n "$$LST" ] && $(MAKE) all files_list="$$LST"

$(foreach f,$(flist),$(eval $(call mk_rules,$(f))))

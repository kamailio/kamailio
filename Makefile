# $Id$
#
# sip_router makefile
#
# WARNING: requires gmake (GNU Make)
#  Arch supported: Linux, FreeBSD, SunOS (tested on Solaris 6), WinNT (cygwin)

auto_gen=lex.yy.c cfg.tab.c   #lexx, yacc etc

#include  source related defs
include Makefile.sources

exclude_modules=CVS mysql pike
static_modules=
static_modules_path=$(addprefix modules/, $(static_modules))
extra_sources=$(wildcard $(addsuffix /*.c, $(static_modules_path)))
extra_objs=$(extra_sources:.c=.o)

static_defs= $(foreach  mod, $(static_modules), \
		-DSTATIC_$(shell echo $(mod) | tr [:lower:] [:upper:]) )
DEFS+=$(static_defs)
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

cfg.tab.c: cfg.y $(ALLDEP)
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


tar: mantainer-clean 
	tar -C .. -zcf ../$(NAME)-$(RELEASE)_src.tar.gz  $(notdir $(CURDIR)) 


install: all mk-install-dirs install-cfg install-bin install-modules \
	install-doc install-man


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

install-cfg:
		$(INSTALL-CFG) ser.cfg $(cfg-prefix)/$(cfg-dir)

install-bin:
		$(INSTALL-BIN) ser $(bin-prefix)/$(bin-dir)


install-modules:
	-@for r in $(modules_full_path) "" ; do \
		if [ -n "$$r" ]; then \
			$(INSTALL-MODULES)  $$r  $(modules-prefix)/$(modules-dir) ; \
		fi ; \
	done 


install-doc:
	$(INSTALL-DOC) README $(doc-prefix)/$(doc-dir)

install-man:
	$(INSTALL-MAN)  ser.8 $(man-prefix)/$(man-dir)/man8
	$(INSTALL-MAN)  ser.cfg.5 $(man-prefix)/$(man-dir)/man5


# $Id$
#
# sip_router makefile
#
# WARNING: requires gmake (GNU Make)
#  Arch supported: Linux, FreeBSD, SunOS (tested on Solaris 6), WinNT (cygwin)

auto_gen=lex.yy.c cfg.tab.c   #lexx, yacc etc

#include  source related defs
include Makefile.sources

exclude_modules=CVS usrloc mysql auth
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
	-@for r in $(modules); do \
		echo  "" ; \
		echo  "" ; \
		$(MAKE) -C $$r ; \
	done

.PHONY: static_modules
static_modules:
	-@echo "Extra objs: $(extra_objs)"
	-@for r in $(static_modules_path); do \
		echo  "" ; \
		echo  "Making static module $r" ; \
		$(MAKE) -C $$r static ; \
	done


	
dbg: ser
	gdb -command debug.gdb


tar: mantainer-clean 
	tar -C .. -zcf ../$(NAME)-$(RELEASE)_src.tar.gz  $(notdir $(CURDIR)) 



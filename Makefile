# $Id$
#
# sip_router makefile
#
# WARNING: requires gmake (GNU Make)
#  Arch supported: Linux, FreeBSD, SunOS (tested on Solaris 6), WinNT

auto_gen=lex.yy.c cfg.tab.c   #lexx, yacc etc
exclude_modules=CVS
sources=$(filter-out $(auto_gen), $(wildcard *.c)) $(auto_gen) 
objs=$(sources:.c=.o)
depends=$(sources:.c=.d)
modules=$(filter-out $(addprefix modules/, $(exclude_modules)), \
						$(wildcard modules/*))

NAME=ser

# compile-time options
# NOCR disables seeking for CRs -- breaks standard but is fast
# recommended: on (speed-up, no implementation really sends CR)
# MACROEATER replaces frequently called parser helper functions
# with macros
# recommanded: on (speed-up)
# STATS allows to print out number of packets processed on CTRL-C; implementation
#  still nasty and reports per-process
DEFS=-DNOCR -DMACROEATER -DSTATS

# platform dependent settings

ARCH = $(shell uname -s)

#common
CC=gcc
LD=gcc
CFLAGS=-O2 -Wcast-align #-Wmissing-prototypes 
LDFLAGS=-Wl,-O2 -Wl,-E
LEX=flex
YACC=bison
YACC_FLAGS=-d -b cfg
# on linux and freebsd keep it empty (e.g. LIBS= )
# on solaris add -lxnet (e.g. LIBS= -lxnet)
LIBS=-lfl -ldl


ifeq ($(ARCH), Linux)

endif
ifeq  ($(ARCH), SunOS)

MAKE=gmake
YACC=yacc
LDFLAGS=-O2
LIBS+=-L/usr/local/lib -lxnet # or -lnsl -lsocket or -lglibc ?

endif
ifeq ($(ARCH), FreeBSD)

MAKE=gmake
YACC=yacc
LIBS= -lfl  #dlopen is in libc

endif
ifneq (,$(findstring CYGWIN, $(ARCH)))

#cygwin is the same as common

endif


MKDEP=gcc -M 

ALLDEP=Makefile

export #export all variables for the sub-makes


#implicit rules
%.o:%.c $(ALLDEP)
	$(CC) $(CFLAGS) $(DEFS) -c $< -o $@

%.d: %.c
	$(MKDEP) $< >$@


# normal rules
$(NAME): $(objs)
	$(LD) $(LDFLAGS) $(objs) $(LIBS) -o $(NAME) 

lex.yy.c: cfg.lex $(ALLDEP)
	$(LEX) $<

cfg.tab.c: cfg.y $(ALLDEP)
	$(YACC) $(YACC_FLAGS) $<



.PHONY: all
all: $(NAME) modules

.PHONY: dep
dep: $(depends)

.PHONY: clean
clean:
	-rm $(objs) $(NAME)
	-for r in $(modules); do $(MAKE) -C $$r clean ; done

.PHONY: modules
modules:
	-for r in $(modules); do \
		$(MAKE) -C $$r ; \
	done


.PHONY: proper
proper: clean 
	-rm $(depends)
	-for r in $(modules); do $(MAKE) -C $$r proper ; done

include $(depends)


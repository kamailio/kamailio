# $Id$
#
# sip_router makefile
#
# WARNING: requires gmake (GNU Make)
#  Arch supported: Linux, FreeBSD, SunOS (tested on Solaris 6)

lex_f=lex.yy.c
yacc_f=cfg.tab.c
sources= $(filter-out $(lex_f) $(yacc_f), $(wildcard *.c)) $(lex_f) \
$(yacc_f) 
objs= $(sources:.c=.o)
depends= $(sources:.c=.d)

NAME=sip_router

# compile-time options
# NOCR disables seeking for CRs -- breaks standard but is fast
# recommended: on (speed-up, no implementation really sends CR)
# MACROEATER replaces frequently called parser helper functions
# with macros
# recommanded: on (speed-up)
DEFS=-DNOCR -DMACROEATER

# platform dependent settings

ARCH = $(shell uname -s)

ifeq ($(ARCH), Linux)

CC=gcc
CFLAGS=-O2 -Wcast-align #-Wmissing-prototypes  -Wall
LEX=flex
YACC=bison
YACC_FLAGS=-d -b cfg
# on linux and freebsd keep it empty (e.g. LIBS= )
# on solaris add -lxnet (e.g. LIBS= -lxnet)
LIBS=-lfl

endif 
ifeq  ($(ARCH), SunOS)

MAKE=gmake
CC=gcc
CFLAGS=-O2 -Wcast-align
LEX=flex
YACC=yacc
YACC_FLAGS=-d -b cfg
LIBS=-lfl -L/usr/local/lib -lxnet # or -lnsl -lsocket or -lglibc ?

endif
ifeq ($(ARCH), FreeBSD)

MAKE=gmake
CC=gcc
CFLAGS=-O2 -Wcast-align
LEX=flex
YACC=yacc
YACC_FLAGS=-d -b cfg
LIBS=-lfl

endif

ifeq ($(ARCH), CYGWIN_NT-4.0)

CC=gcc
CFLAGS=-O2 -Wcast-align #-Wmissing-prototypes  -Wall
LEX=flex
YACC=bison
YACC_FLAGS=-d -b cfg
# on linux and freebsd keep it empty (e.g. LIBS= )
# on solaris add -lxnet (e.g. LIBS= -lxnet)
LIBS=-lfl

endif


MKDEP=gcc -M $(DEFS)

ALLDEP=Makefile

#implicit rules


%.o:%.c $(ALLDEP)
	$(CC) $(CFLAGS) $(DEFS) -c $< -o $@

%.d: %.c
	$(MKDEP) $< >$@

# normal rules
$(NAME): $(objs)
	$(CC) $(CFLAGS) $(objs) -o $(NAME) $(LIBS)

lex.yy.c: cfg.lex $(ALLDEP)
	$(LEX) $<

cfg.tab.c: cfg.y $(ALLDEP)
	$(YACC) $(YACC_FLAGS) $<


.PHONY: all
all: $(NAME)

.PHONY: dep
dep: $(depends)

.PHONY: clean
clean:
	-rm $(objs) $(NAME)

.PHONY: proper
proper: clean
	-rm $(depends)

include $(depends)


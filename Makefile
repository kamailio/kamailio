# $Id$
#
# sip_router makefile
#
# WARNING: requires gmake (GNU Make)
#  Arch supported: Linux, FreeBSD, SunOS (tested on Solaris 6), WinNT (cygwin)

auto_gen=lex.yy.c cfg.tab.c   #lexx, yacc etc
exclude_modules=CVS usrloc
sources=$(filter-out $(auto_gen), $(wildcard *.c)) $(auto_gen)
objs=$(sources:.c=.o)
depends=$(sources:.c=.d)
modules=$(filter-out $(addprefix modules/, $(exclude_modules)), \
						$(wildcard modules/*))

NAME=ser

# compile-time options
# 
# STATS allows to print out number of packets processed on CTRL-C; 
# implementation still nasty and reports per-process
# NO_DEBUG turns off some of the debug messages (DBG(...)).
# NO_LOG completely turns of all the logging (and DBG(...))
# EXTRA_DEBUG compiles in some extra debugging code
# DNS_IP_HACK faster ip address resolver for ip strings (e.g "127.0.0.1")
# SHM_MEM    compiles in shared mem. support, needed by some modules and
#            by USE_SHM_MEM
# PKG_MALLOC uses a faster malloc (exclusive w/ USE_SHM_MEM)
# USE_SHM_MEM all pkg_malloc => shm_malloc (most mallocs use a common sh. mem.
#           segment); don't define PKG_MALLOC if you want this!
# DBG_QM_MALLOC - qm_malloc debug code, will cause pkg_malloc and shm_malloc
#                  to keep and display lot of debuging information: file name,
#                  function, line number of malloc/free call for each block,
#                  extra error checking (trying to free the same pointer
#                  twice, trying to free a pointer alloc'ed with a different
#                  malloc etc.)
DEFS=-DDNS_IP_HACK  -DSHM_MEM \
	 -DPKG_MALLOC #-DDBG_QM_MALLOC 
#-DEXTRA_DEBUG
# -DUSE_SHM_MEM
#-DNO_DEBUG 
#-DPKG_MALLOC
#-DNO_DEBUG#-DSTATS -DNO_DEBUG 
#-DNO_LOG

PROFILE=  -pg #set this if you want profiling
mode = debug
#mode = release

# platform dependent settings

ARCH = $(shell uname -s)

#common
CC=gcc
LD=gcc

ifeq ($(mode), release)
	CFLAGS=-O2 -Wcast-align $(PROFILE) -Winline#-Wmissing-prototypes 
	LDFLAGS=-Wl,-O2 -Wl,-E $(PROFILE)
	# we need -fPIC -DPIC only for shared objects, we don't need them for the 
	# executable file, because it's always loaded at a fixed address
	# -andrei
else
	CFLAGS=-g -fPIC -DPIC -Wcast-align -Winline $(PROFILE)
	LDFLAGS=-g -Wl,-E $(PROFILE)
endif

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
LDFLAGS=-O2 $(PROFILE)
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


MKDEP=gcc -MM $(DEFS)

ALLDEP=Makefile

export #export all variables for the sub-makes


#implicit rules
%.o:%.c $(ALLDEP)
	$(CC) $(CFLAGS) $(DEFS) -c $< -o $@

%.d: %.c $(ALLDEP)
	set -e; $(MKDEP) $< \
	|  sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
	[ -s $@ ] || rm -f $@


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
	-rm $(objs) $(NAME) 2>/dev/null
	-for r in $(modules); do $(MAKE) -C $$r clean ; done

.PHONY: modules
modules:
	-for r in $(modules); do \
		echo  "" ; \
		echo  "" ; \
		$(MAKE) -C $$r ; \
	done


.PHONY: proper
proper: clean 
	-rm $(depends) 2>/dev/null
	-for r in $(modules); do $(MAKE) -C $$r proper ; done

include $(depends)

dbg: ser
	gdb -command debug.gdb

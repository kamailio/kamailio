#common
CC=gcc
LD=gcc
CFLAGS=-O2 -Wcast-align $(PROFILE) -Winline#-Wmissing-prototypes
LDFLAGS=-Wl,-O2 -Wl,-E $(PROFILE)
LEX=flex
YACC=bison
YACC_FLAGS=-d -b cfg
# on linux and freebsd keep it empty (e.g. LIBS= )
# on solaris add -lxnet (e.g. LIBS= -lxnet)
LIBS=-lfl -ldl

MKDEP=gcc -M

ALLDEP=Makefile

DEFS=-DFAKED_PARSER

auto_gen=
sources= $(filter-out $(auto_gen), $(wildcard *.c)) $(auto_gen)
objs= $(sources:.c=.o)
depends= $(sources:.c=.d)

NAME=tm

ALLDEP=Makefile

LDFLAGS=-O2 -shared
LIBS=

ifeq ($(ARCH), SunOS)
LDFLAGS=-O2 -G
endif

#implicit rules
%.o:%.c $(ALLDEP)
	$(CC) $(CFLAGS) $(DEFS) -c $< -o $@

%.d: %.c
	$(MKDEP) $< >$@


$(NAME): $(objs)
	$(LD) $(LDFLAGS) $(objs) $(LIBS) -o $(NAME)


.PHONY: all
all: $(NAME)

.PHONY: dep
dep: $(depends)

clean:
	rm -f *.o
	rm -f tm



# $Id$
#
# print example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

auto_gen=
sources= $(filter-out $(auto_gen), $(wildcard *.c)) $(auto_gen) 
objs= $(sources:.c=.o)
depends= $(sources:.c=.d)

NAME=print.so

LDFLAGS=-O2 -shared
LIBS=

ALLDEP=Makefile ../../Makefile

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

.PHONY: clean
clean:
	-rm $(objs) $(NAME)

.PHONY: proper
proper: clean
	-rm $(depends)

include $(depends)

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

NAME=tm.so

SHLIB_LD_FLAGS= -shared
LIBS= -lpthread

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
	$(LD) $(LDFLAGS) $(SHLIB_LD_FLAGS) $(objs) $(LIBS) -o $(NAME)


.PHONY: all
all: $(NAME)

.PHONY: dep
dep: $(depends)

.PHONY: clean
clean:
	-rm -f $(objs) $(NAME)

.PHONY: proper
proper: clean
	-rm $(depends)

include $(depends)

# $Id$
#
# sip_router makefile
#
# WARNING: requires gmake (GNU Make)
#

sources= $(wildcard *.c)
objs= $(sources:.c=.o)
depends= $(sources:.c=.d)

NAME=sip_router

CC=gcc
CFLAGS=-O2
# on linux and freebsd keep it empty (e.g. LIBS= )
# on solaris add -lxnet (e.g. LIBS= -lxnet)
LIBS=
ALLDEP=Makefile

MKDEP=gcc -M


#implicit rules

%.o:%.c $(ALLDEP)
	$(CC) $(CFLAGS) -c $< -o $@

%.d: %.c
	$(MKDEP) $< >$@

$(NAME): $(objs)
	$(CC) $(CFLAGS) $(LIBS) $(objs) -o $(NAME)

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





# $Id$
#
# sip_router makefile
#

sources= $(wildcard *.c)
objs= $((sources:.c=.o)
depends= $(sources:.c=.d)

NAME=sip_router

CC=gcc
COPTS=-O2
ALLDEP=Makefile

MKDEP=gcc -M


#implicit rules

%.o:%.c $(ALLDEP)
	$(CC) $(COPTS) -c $< -o $@

%.d: %.c
	$(MKDEP) $< >$@

$(NAME): $(objs)
	$(CC) $(COPTS) $(objs) -o $(NAME)

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





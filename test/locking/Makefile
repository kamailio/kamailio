#
# $Id$
#


NAME=locking_test

CC=gcc
CFLAGS= -O2 -Wall
DEFS=
LIBS=
INCLUDE=

OS = $(shell uname -s)


ifeq ($(OS), SunOS)
LIBS+= -lrt
CFLAGS+=-mv8 -Wa,-xarch=v8plus
endif

$(NAME): $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DNO_LOCK -o $@ $< $(LIBS)


$(NAME)_sysv: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DSYSV_SEM -o $@ $(INCLUDE) $< $(LIBS)


$(NAME)_flock: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DFLOCK -o $@ $(INCLUDE) $< $(LIBS)

$(NAME)_posix: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DPOSIX_SEM -o $@ $(INCLUDE) $< $(LIBS) -lpthread

$(NAME)_pmutex: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DPTHREAD_MUTEX -o $@ $(INCLUDE) $< $(LIBS) -lpthread

$(NAME)_fastlock: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DFAST_LOCK -o $@ $(INCLUDE) $< $(LIBS) 

all: $(NAME) $(NAME)_sysv  $(NAME)_posix $(NAME)_pmutex \
		$(NAME)_fastlock

ifneq ($(OS), SunOS)

all: $(NAME)_flock

endif

clean:
	-@rm $(NAME) $(NAME)_sysv $(NAME)_flock $(NAME)_posix \
		$(NAME)_pmutex $(NAME)_fastlock

#
# $Id$
#


NAME=locking_test

CC=gcc
CFLAGS= -O2 -Wall
DEFS=
LIBS=

$(NAME): $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DNO_LOCK -o $@ $< $(LIBS)


$(NAME)_sysv: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DSYSV_SEM -o $@ $< $(LIBS)


$(NAME)_flock: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DFLOCK -o $@ $< $(LIBS)

$(NAME)_posix: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DPOSIX_SEM -o $@ $< $(LIBS) -lpthread

$(NAME)_pmutex: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DPTHREAD_MUTEX -o $@ $< $(LIBS) -lpthread

$(NAME)_fastlock: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DFAST_LOCK -o $@ $< $(LIBS) 

all: $(NAME) $(NAME)_sysv $(NAME)_flock $(NAME)_posix $(NAME)_pmutex \
		$(NAME)_fastlock


clean:
	-@rm $(NAME) $(NAME)_sysv $(NAME)_flock $(NAME)_posix \
		$(NAME)_pmutex $(NAME)_fastlock

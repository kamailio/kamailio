


NAME=locking_test

CC=gcc
CFLAGS= -O2
DEFS=
LIBS=

$(NAME): $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DNO_LOCK -o $@ $< $(LIBS)


$(NAME)_sysv: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DSYSV_SEM -o $@ $< $(LIBS)


$(NAME)_flock: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DFLOCK -o $@ $< $(LIBS)

$(NAME)_lin: $(NAME).c
	gcc $(CFLAGS) $(DEFS) -DLIN_SEM -o $@ $< $(LIBS) -lpthread


all: $(NAME) $(NAME)_sysv $(NAME)_flock $(NAME)_lin


clean:
	rm $(NAME) $(NAME)_sysv $(NAME)_flock $(NAME)_lin

CC = gcc
FILES = *.c ../netstring.c ../jsonrpc_global.c seatest/seatest.c
OUT_EXE = test

build: $(FILES)
	$(CC) -std=gnu99 -pedantic -D TEST -o $(OUT_EXE) $(FILES) -lm -levent

clean:
	rm -f *.o core

rebuild: clean build

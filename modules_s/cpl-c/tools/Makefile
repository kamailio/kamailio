BIN=encrypt
CC=gcc
OPT=-Wall -O9 -I/usr/include/libxml2
EXT_OBJ=../../mysql/mysql.so ../cpl_parser.o ../cpl_db.o ../sub_list.o
LIBS=-lxml2




all: $(BIN)

$(BIN): encrypt.o wraper.o
	$(CC) $(LIBS) $(EXT_OBJ) encrypt.o wraper.o -o $(BIN)

encrypt.o: encrypt.c Makefile
	$(CC) -c $(OPT) -o encrypt.o encrypt.c
	
wraper.o: wraper.c Makefile
	$(CC) -c $(OPT) -o wraper.o wraper.c

clean:
	rm -f *.o
	rm -f $(BIN)

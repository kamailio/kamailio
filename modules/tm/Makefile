CC		= g++
cc		= gcc
CCFLAGS 	= -c
TABLE_DIR          = table
INCLUDE 	= -include $(TABLE_DIR)/h_table.h
LIBS		= -lpthread

all: build link

build:  $(TABLE_DIR)/table.o  tm.o

link:  tm

$(TABLE_DIR)/table.o: Makefile $(TABLE_DIR)/hash_func.c $(TABLE_DIR)/h_table.c $(TABLE_DIR)/h_table.h $(TABLE_DIR)/semaphore.c $(TABLE_DIR)/timer.c
	$(cc)  $(CCFLAGS)  $(TABLE_DIR)/h_table.c -o $@

tm.o: Makefile tm.c
	$(cc)  $(CCFLAGS)  $(INCLUDE) tm.c -o $@

tm: Makefile tm.o $(TABLE_DIR)/table.o
	$(cc)  tm.o $(TABLE_DIR)/table.o $(LIBS)  -o $@

clean:
	rm -f  $(TABLE_DIR)/*.o || return 0
	rm -f  *.o || return 0
	rm -f tm  || return 0

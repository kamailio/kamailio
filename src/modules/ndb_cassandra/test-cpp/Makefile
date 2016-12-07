CC=gcc
CCC=g++
LD=g++
GEN_SRC=cassandra_constants.cpp cassandra_types.cpp Cassandra.cpp thrift_wrapper.cpp test.c 

GEN_OBJ_C=$(patsubst %.c,%.o, $(GEN_SRC))
GEN_OBJ_CC=$(patsubst %.cpp,%.o, $(GEN_SRC))

INC=-I/usr/local/include/thrift -I/usr/local/include/
LIBS=-L/usr/local/lib -lthrift

CFLAGS=-Wall -Werror -g -ansi -pedantic -std=c89
CCFLAGS=-Wall -Werror -g
LDFLAGS=-g -Wall

PROG=test

all: $(PROG)
default: all

$(PROG): $(GEN_OBJ_C) $(GEN_OBJ_CC)
	$(LD) $(LDFLAGS) *.o -o $(PROG) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $(INC) $< $(LIBS)

%.o: %.cpp
	$(CCC) $(CCFLAGS) -o $@ -c $(INC) $< $(LIBS)

clean:
	rm -f *.o
	rm -f $(PROG)
	rm -f *~
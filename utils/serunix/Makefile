
all: serunix

serunix: serunix.o
	gcc -o serunix serunix.o

serunix.o: serunix.c
	gcc -c serunix.c

clean:
	rm -f *~ *.o serunix


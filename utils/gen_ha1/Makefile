
all: gen_ha1

gen_ha1: gen_ha1.o calc.o ../../md5.o
	gcc -o gen_ha1 gen_ha1.o calc.o ../../md5.o

gen_ha1.o: gen_ha1.c
	gcc -c gen_ha1.c

calc.o: calc.c calc.h
	gcc -c calc.c


clean:
	rm -f *~ *.o gen_ha1


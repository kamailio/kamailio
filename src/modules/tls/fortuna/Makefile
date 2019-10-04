COREPATH=../../../../src
include $(COREPATH)/Makefile.defs
include $(COREPATH)/Makefile.targets

.PHONY: all
all: libfortuna.a

libfortuna.a:
	$(CC) -c -fPIC -I. $(CFLAGS) fortuna.c rijndael.c sha256.c random.c
	$(AR) rc $(OUT) fortuna.o rijndael.o sha256.o random.o

.PHONY: clean
clean:
	rm -f *.a
	rm -r *.o

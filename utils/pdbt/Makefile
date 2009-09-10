.phony: all clean install

header=common.h carrier.h dt.h dtm.h pdb_server_backend.h log.h
obj=dt.o dtm.o carrier.o pdb_server_backend.o log.o
pdb_server_obj=pdb_server_backend.o dtm.o log.o
cflags=-Wall -O2 -g
# -march=x86-64
extdep=Makefile

all: pdbt pdb_server

$(obj): %.o : %.c %.h $(header) $(extdep)
	gcc $(cflags) -c $<

pdbt: pdbt.c $(obj) $(header) $(extdep)
	gcc $(cflags) -o $@ $< $(obj)

pdb_server: pdb_server.c $(pdb_server_obj) $(header) $(extdep)
	gcc $(cflags) -o $@ $< $(pdb_server_obj)

clean:
	rm -f *~ *.o pdbt pdb_server

install:
	cp pdbt $(DESTDIR)/usr/bin/
	cp pdb_server $(DESTDIR)/usr/bin/
	cp pdb_server.conf $(DESTDIR)/etc/

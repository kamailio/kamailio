LIBXMLRPC_BASE = /opt/xmlrpc
CFLAGS = `$(LIBXMLRPC_BASE)/bin/xmlrpc-c-config client --cflags`
LIBS = `$(LIBXMLRPC_BASE)/bin/xmlrpc-c-config client --libs`

CC = gcc

.PHONY = pike_top

pike_top: pike_top.c
	$(CC) -g pike_top.c -o pike_top $(CFLAGS) $(LIBS)

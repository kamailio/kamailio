#ifndef SOCKET_UNIX_H_INCLUDED
#define SOCKET_UNIX_H_INCLUDED

void
SocketUnixInit(abyss_bool * const succeededP);

void
SocketUnixTerm(void);

void
SocketUnixCreate(TSocket ** const socketPP);

#endif

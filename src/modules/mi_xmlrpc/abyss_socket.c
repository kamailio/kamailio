/******************************************************************************
**
** socket.c
**
** This file is part of the ABYSS Web server project.
**
** Copyright (C) 2000 by Moez Mahfoudh <mmoez@bigfoot.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
*******************************************************************************/

/*============================================================================
  socket.c
==============================================================================
  Implementation of TSocket class: A generic socket over which one can
  transport an HTTP stream or manage HTTP connections
============================================================================*/

#include <sys/types.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "abyss_mallocvar.h"
#include "abyss_xmlrpc_int.h"
#include <xmlrpc-c/abyss.h>
#ifdef WIN32
  #include "socket_win.h"
#else
  #include "abyss_socket_unix.h"
#endif
#include "abyss_socket.h"


#ifdef WIN32
  #include "socket_win.h"
#else
  #include "abyss_socket_unix.h"
#endif

#include "abyss_socket.h"


static void
socketOsInit(abyss_bool * const succeededP) {

#ifdef WIN32
    SocketWinInit(succeededP);
#else
    SocketUnixInit(succeededP);
#endif
}



static void
socketOsTerm(void) {

#ifdef WIN32
    SocketWinTerm();
#else
    SocketUnixTerm();
#endif
}
    


abyss_bool SocketTraceIsActive;

abyss_bool
SocketInit(void) {
    abyss_bool retval;

    socketOsInit(&retval);

    SocketTraceIsActive = (getenv("ABYSS_TRACE_SOCKET") != NULL);
    if (SocketTraceIsActive)
        fprintf(stderr, "Abyss socket layer will trace socket traffic "
                "due to ABYSS_TRACE_SOCKET environment variable\n");

    return retval;
}



void
SocketTerm(void) {

    socketOsTerm();
}



/* SocketCreate() is not exported to the Abyss user.  It is meant to
   be used by an implementation-specific TSocket generator which is
   exported to the Abyss user, e.g. SocketCreateUnix() in
   socket_unix.c

   The TSocket generator functions are the _only_ user-accessible
   functions that are particular to an implementation.
*/

static uint const socketSignature = 0x060609;

void
SocketCreate(const struct TSocketVtbl * const vtblP,
             void *                     const implP,
             TSocket **                 const socketPP) {

    TSocket * socketP;

    MALLOCVAR(socketP);

    if (socketP) {
        socketP->implP = implP;
        socketP->vtbl = *vtblP;
        socketP->signature = socketSignature;
        *socketPP = socketP;
    }
}



void
SocketClose(TSocket * const socketP) {
    socketP->vtbl.close(socketP);
}



void
SocketDestroy(TSocket * const socketP) {

    assert(socketP->signature == socketSignature);

    socketP->vtbl.destroy(socketP);

    socketP->signature = 0;  /* For debuggability */

    free(socketP);
}



void
SocketWrite(TSocket *             const socketP,
            const unsigned char * const buffer,
            uint32_t              const len,
            abyss_bool *          const failedP) {

    (*socketP->vtbl.write)(socketP, buffer, len, failedP );
}



uint32_t
SocketRead(TSocket *       const socketP, 
           unsigned char * const buffer, 
           uint32_t        const len) {
    
    return (*socketP->vtbl.read)(socketP, (char*)buffer, len);
}



abyss_bool
SocketConnect(TSocket * const socketP,
              TIPAddr * const addrP,
              uint16_t  const portNumber) {

    return (*socketP->vtbl.connect)(socketP, addrP, portNumber);
}



abyss_bool
SocketBind(TSocket * const socketP,
           TIPAddr * const addrP,
           uint16_t  const portNumber) {

    return (*socketP->vtbl.bind)(socketP, addrP, portNumber);
}



abyss_bool
SocketListen(TSocket * const socketP,
             uint32_t  const backlog) {

    return (*socketP->vtbl.listen)(socketP, backlog);
}



void
SocketAccept(TSocket *    const listenSocketP,
             abyss_bool * const connectedP,
             abyss_bool * const failedP,
             TSocket **   const acceptedSocketPP,
             TIPAddr *    const ipAddrP) {

    (*listenSocketP->vtbl.accept)(listenSocketP,
                                  connectedP,
                                  failedP,
                                  acceptedSocketPP,
                                  ipAddrP);
}



uint32_t
SocketWait(TSocket *  const socketP,
           abyss_bool const rd,
           abyss_bool const wr,
           uint32_t   const timems) {

    return (*socketP->vtbl.wait)(socketP, rd, wr, timems);
}



uint32_t
SocketAvailableReadBytes(TSocket * const socketP) {

    return (*socketP->vtbl.availableReadBytes)(socketP);
}



void
SocketGetPeerName(TSocket *    const socketP,
                  TIPAddr *    const ipAddrP,
                  uint16_t *   const portNumberP,
                  abyss_bool * const successP) {

    (*socketP->vtbl.getPeerName)(socketP, ipAddrP, portNumberP, successP);
}



uint32_t
SocketError(TSocket * const socketP) {

    return (*socketP->vtbl.error)(socketP);
}

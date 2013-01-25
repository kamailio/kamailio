/******************************************************************************
**
** socket_unix.c
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

/*=============================================================================
                                 socket_unix.c
===============================================================================
  This is the implementation of TSocket for a standard Unix (POSIX)
  stream socket -- what you create with a socket() C library call.
=============================================================================*/

#include <xmlrpc-c/config.h>

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#if HAVE_SYS_FILIO_H
  #include <sys/filio.h>
#endif
//#if HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
//#endif

#include "abyss_xmlrpc_int.h"
#include "abyss_mallocvar.h"
#include "abyss_trace.h"
#include "abyss_socket.h"
#include <xmlrpc-c/abyss.h>

#include "abyss_socket_unix.h"



struct socketUnix {
/*----------------------------------------------------------------------------
   The properties/state of a TSocket unique to a Unix TSocket.
-----------------------------------------------------------------------------*/
    int fd;
        /* File descriptor of the POSIX socket (such as is created by
           socket() in the C library) on which the TSocket is based.
        */
    abyss_bool userSuppliedFd;
        /* The file descriptor and associated POSIX socket belong to the
           user; we did not create it.
        */
};


void
SocketUnixInit(abyss_bool * const succeededP) {

    *succeededP = TRUE;
}



void
SocketUnixTerm(void) {

}



static SocketCloseImpl              socketClose;
static SocketDestroyImpl            socketDestroy;
static SocketWriteImpl              socketWrite;
static SocketReadImpl               socketRead;
static SocketConnectImpl            socketConnect;
static SocketBindImpl               socketBind;
static SocketListenImpl             socketListen;
static SocketAcceptImpl             socketAccept;
static SocketErrorImpl              socketError;
static SocketWaitImpl               socketWait;
static SocketAvailableReadBytesImpl socketAvailableReadBytes;
static SocketGetPeerNameImpl        socketGetPeerName;


static struct TSocketVtbl const vtbl = {
    &socketClose,
    &socketDestroy,
    &socketWrite,
    &socketRead,
    &socketConnect,
    &socketBind,
    &socketListen,
    &socketAccept,
    &socketError,
    &socketWait,
    &socketAvailableReadBytes,
    &socketGetPeerName
};



void
SocketUnixCreate(TSocket ** const socketPP) {

    struct socketUnix * socketUnixP;

    MALLOCVAR(socketUnixP);

    if (socketUnixP) {
        int rc;
        rc = socket(AF_INET, SOCK_STREAM, 0);
        if (rc < 0)
            *socketPP = NULL;
        else {
            socketUnixP->fd = rc;
            socketUnixP->userSuppliedFd = FALSE;
            
            {
                int32_t n = 1;
                int rc;
                rc = setsockopt(socketUnixP->fd, SOL_SOCKET, SO_REUSEADDR,
                                (char*)&n, sizeof(n));
                if (rc < 0)
                    *socketPP = NULL;
                else
                    SocketCreate(&vtbl, socketUnixP, socketPP);
            }
            if (!*socketPP)
                close(socketUnixP->fd);
        }
        if (!*socketPP)
            free(socketUnixP);
    } else
        *socketPP = NULL;
}



void
SocketUnixCreateFd(int        const fd,
                   TSocket ** const socketPP) {

    struct socketUnix * socketUnixP;

    MALLOCVAR(socketUnixP);

    if (socketUnixP) {
        socketUnixP->fd = fd;
        socketUnixP->userSuppliedFd = TRUE;

        SocketCreate(&vtbl, socketUnixP, socketPP);

        if (!*socketPP)
            free(socketUnixP);
    } else
        *socketPP = NULL;
}


static void
socketClose(TSocket * const socketP) {

    struct socketUnix * const socketUnixP = socketP->implP;

    if (!socketUnixP->userSuppliedFd && socketUnixP->fd ) {
        close(socketUnixP->fd);
        socketUnixP->fd = 0;
    }
}


static void
socketDestroy(TSocket * const socketP) {

    struct socketUnix * const socketUnixP = socketP->implP;

    socketClose(socketP);

    free(socketUnixP);
}



static void
socketWrite(TSocket *             const socketP,
            const unsigned char * const buffer,
            uint32_t              const len,
            abyss_bool *          const failedP) {

    struct socketUnix * const socketUnixP = socketP->implP;

    size_t bytesLeft;
    abyss_bool error;

    assert(sizeof(size_t) >= sizeof(len));

    for (bytesLeft = len, error = FALSE;
         bytesLeft > 0 && !error;
        ) {
        size_t const maxSend = (size_t)(-1) >> 1;

        ssize_t rc;
        
        rc = send(socketUnixP->fd, &buffer[len-bytesLeft],
                  MIN(maxSend, bytesLeft), 0);

        if (SocketTraceIsActive) {
            if (rc < 0)
                fprintf(stderr, "Abyss socket: send() failed.  errno=%d (%s)",
                        errno, strerror(errno));
            else if (rc == 0)
                fprintf(stderr, "Abyss socket: send() failed.  "
                        "Socket closed.\n");
            else
                fprintf(stderr, "Abyss socket: sent %u bytes: '%.*s'\n",
                        (unsigned int)-rc, (int)-rc, &buffer[len-bytesLeft]);
        }
        if (rc <= 0)
            /* 0 means connection closed; < 0 means severe error */
            error = TRUE;
        else
            bytesLeft -= rc;
    }
    *failedP = error;
}



static uint32_t
socketRead(TSocket * const socketP, 
           char *    const buffer, 
           uint32_t  const len) {

    struct socketUnix * const socketUnixP = socketP->implP;

    int rc;
    rc = recv(socketUnixP->fd, buffer, len, 0);
    if (SocketTraceIsActive) {
        if (rc < 0)
            fprintf(stderr, "Abyss socket: recv() failed.  errno=%d (%s)",
                    errno, strerror(errno));
        else 
            fprintf(stderr, "Abyss socket: read %u bytes: '%.*s'\n",
                    len, (int)len, buffer);
    }
    return rc;
}



abyss_bool
socketConnect(TSocket * const socketP,
              TIPAddr * const addrP,
              uint16_t  const portNumber) {

    struct socketUnix * const socketUnixP = socketP->implP;

    struct sockaddr_in name;
    int rc;

    name.sin_family = AF_INET;
    name.sin_port = htons(portNumber);
    name.sin_addr = *addrP;

    rc = connect(socketUnixP->fd, (struct sockaddr *)&name, sizeof(name));

    return rc != -1;
}



abyss_bool
socketBind(TSocket * const socketP,
           TIPAddr * const addrP,
           uint16_t  const portNumber) {

    struct socketUnix * const socketUnixP = socketP->implP;

    struct sockaddr_in name;
    int rc;

    name.sin_family = AF_INET;
    name.sin_port   = htons(portNumber);
    if (addrP)
        name.sin_addr = *addrP;
    else
        name.sin_addr.s_addr = INADDR_ANY;

    rc = bind(socketUnixP->fd, (struct sockaddr *)&name, sizeof(name));

    return (rc != -1);
}



abyss_bool
socketListen(TSocket * const socketP,
             uint32_t  const backlog) {

    struct socketUnix * const socketUnixP = socketP->implP;

    int32_t const minus1 = -1;

    int rc;

    /* Disable the Nagle algorithm to make persistant connections faster */

    setsockopt(socketUnixP->fd, IPPROTO_TCP,TCP_NODELAY,
               &minus1, sizeof(minus1));

    rc = listen(socketUnixP->fd, backlog);

    return (rc != -1);
}



static void
socketAccept(TSocket *    const listenSocketP,
             abyss_bool * const connectedP,
             abyss_bool * const failedP,
             TSocket **   const acceptedSocketPP,
             TIPAddr *    const ipAddrP) {
/*----------------------------------------------------------------------------
   Accept a connection on the listening socket 'listenSocketP'.  Return as
   *acceptedSocketPP the socket for the accepted connection.

   If no connection is waiting on 'listenSocketP', wait until one is.

   If we receive a signal while waiting, return immediately.

   Return *connectedP true iff we accepted a connection.  Return
   *failedP true iff we were unable to accept a connection for some
   reason other than that we were interrupted.  Return both false if
   our wait for a connection was interrupted by a signal.
-----------------------------------------------------------------------------*/
    struct socketUnix * const listenSocketUnixP = listenSocketP->implP;

    abyss_bool connected, failed, interrupted;

    connected  = FALSE;
    failed      = FALSE;
    interrupted = FALSE;

    while (!connected && !failed && !interrupted) {
        struct sockaddr_in sa;
        socklen_t size = sizeof(sa);
        int rc;
        rc = accept(listenSocketUnixP->fd, (struct sockaddr *)&sa, &size);
        if (rc >= 0) {
            int const acceptedFd = rc;
            struct socketUnix * acceptedSocketUnixP;

            MALLOCVAR(acceptedSocketUnixP);

            if (acceptedSocketUnixP) {
                acceptedSocketUnixP->fd = acceptedFd;
                acceptedSocketUnixP->userSuppliedFd = FALSE;
                
                SocketCreate(&vtbl, acceptedSocketUnixP, acceptedSocketPP);
                if (!*acceptedSocketPP)
                    failed = TRUE;
                else {
                    connected = TRUE;
                    *ipAddrP = sa.sin_addr;
                }
                if (failed)
                    free(acceptedSocketUnixP);
            } else
                failed = TRUE;
            if (failed)
                close(acceptedFd);
        } else if (errno == EINTR)
            interrupted = TRUE;
        else
            failed = TRUE;
    }   
    *failedP    = failed;
    *connectedP = connected;
}



static uint32_t
socketWait(TSocket *  const socketP,
           abyss_bool const rd,
           abyss_bool const wr,
           uint32_t   const timems) {

    struct socketUnix * const socketUnixP = socketP->implP;

    fd_set rfds, wfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    if (rd)
        FD_SET(socketUnixP->fd, &rfds);

    if (wr)
        FD_SET(socketUnixP->fd, &wfds);

    tv.tv_sec  = timems / 1000;
    tv.tv_usec = timems % 1000;

    for (;;) {
        int rc;

        rc = select(socketUnixP->fd + 1, &rfds, &wfds, NULL,
                    (timems == TIME_INFINITE ? NULL : &tv));

        switch(rc) {   
        case 0: /* time out */
            return 0;

        case -1:  /* socket error */
            if (errno == EINTR)
                break;
            
            return 0;
            
        default:
            if (FD_ISSET(socketUnixP->fd, &rfds))
                return 1;
            if (FD_ISSET(socketUnixP->fd, &wfds))
                return 2;
            return 0;
        }
    }
}



static uint32_t
socketAvailableReadBytes(TSocket * const socketP) {

    struct socketUnix * const socketUnixP = socketP->implP;

    uint32_t x;
    int rc;

    rc = ioctl(socketUnixP->fd, FIONREAD, &x);

    return rc == 0 ? x : 0;
}



static void
socketGetPeerName(TSocket *    const socketP,
                  TIPAddr *    const ipAddrP,
                  uint16_t *   const portNumberP,
                  abyss_bool * const successP) {

    struct socketUnix * const socketUnixP = socketP->implP;

    socklen_t addrlen;
    int rc;
    struct sockaddr sockAddr;

    addrlen = sizeof(sockAddr);
    
    rc = getpeername(socketUnixP->fd, &sockAddr, &addrlen);

    if (rc < 0) {
        TraceMsg("getpeername() failed.  errno=%d (%s)",
                 errno, strerror(errno));
        *successP = FALSE;
    } else {
        if (addrlen != sizeof(sockAddr)) {
            TraceMsg("getpeername() returned a socket address of the wrong "
                     "size: %u.  Expected %u", addrlen, sizeof(sockAddr));
            *successP = FALSE;
        } else {
            if (sockAddr.sa_family != AF_INET) {
                TraceMsg("Socket does not use the Inet (IP) address "
                         "family.  Instead it uses family %d",
                         sockAddr.sa_family);
                *successP = FALSE;
            } else {
                struct sockaddr_in * const sockAddrInP = (struct sockaddr_in *)
                    &sockAddr;

                *ipAddrP     = sockAddrInP->sin_addr;
                *portNumberP = sockAddrInP->sin_port;

                *successP = TRUE;
            }
        }
    }
}



static uint32_t
socketError(TSocket * const socketP) {

    if (socketP){} /* defeat compiler warning */

    return errno;
}

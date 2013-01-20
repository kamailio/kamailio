#ifndef SOCKET_H_INCLUDED
#define SOCKET_H_INCLUDED

#include <netinet/in.h>

#include <xmlrpc-c/abyss.h>

#include <netinet/in.h>

#define IPB1(x) (((unsigned char *)(&x))[0])
#define IPB2(x) (((unsigned char *)(&x))[1])
#define IPB3(x) (((unsigned char *)(&x))[2])
#define IPB4(x) (((unsigned char *)(&x))[3])

typedef struct in_addr TIPAddr;

typedef void SocketCloseImpl(TSocket * const socketP);

typedef void SocketDestroyImpl(TSocket * const socketP);

typedef void SocketWriteImpl(TSocket *             const socketP,
                             const unsigned char * const buffer,
                             uint32_t              const len,
                             abyss_bool *          const failedP);

typedef uint32_t SocketReadImpl(TSocket * const socketP,
                                char *    const buffer,
                                uint32_t  const len);

typedef abyss_bool SocketConnectImpl(TSocket * const socketP,
                                     TIPAddr * const addrP,
                                     uint16_t  const portNumber);

typedef abyss_bool SocketBindImpl(TSocket * const socketP,
                                  TIPAddr * const addrP,
                                  uint16_t  const portNumber);

typedef abyss_bool SocketListenImpl(TSocket * const socketP,
                                    uint32_t  const backlog);

typedef void SocketAcceptImpl(TSocket *    const listenSocketP,
                              abyss_bool * const connectedP,
                              abyss_bool * const failedP,
                              TSocket **   const acceptedSocketPP,
                              TIPAddr *    const ipAddrP);

typedef uint32_t SocketErrorImpl(TSocket * const socketP);

typedef uint32_t SocketWaitImpl(TSocket *  const socketP,
                                abyss_bool const rd,
                                abyss_bool const wr,
                                uint32_t   const timems);

typedef uint32_t SocketAvailableReadBytesImpl(TSocket * const socketP);

typedef void SocketGetPeerNameImpl(TSocket *    const socketP,
                                   TIPAddr *    const ipAddrP,
                                   uint16_t *   const portNumberP,
                                   abyss_bool * const successP);

struct TSocketVtbl {
    SocketCloseImpl              * close;
    SocketDestroyImpl            * destroy;
    SocketWriteImpl              * write;
    SocketReadImpl               * read;
    SocketConnectImpl            * connect;
    SocketBindImpl               * bind;
    SocketListenImpl             * listen;
    SocketAcceptImpl             * accept;
    SocketErrorImpl              * error;
    SocketWaitImpl               * wait;
    SocketAvailableReadBytesImpl * availableReadBytes;
    SocketGetPeerNameImpl        * getPeerName;
};

struct _TSocket {
    uint               signature;
        /* With both background and foreground use of sockets, and
           background being both fork and pthread, it is very easy to
           screw up socket lifetime and try to destroy twice.  We use
           this signature to help catch such bugs.
        */
    void *             implP;
    struct TSocketVtbl vtbl;
};

#define TIME_INFINITE   0xffffffff

extern abyss_bool SocketTraceIsActive;

abyss_bool
SocketInit(void);

void
SocketTerm(void);

void
SocketClose(TSocket *       const socketP);

void
SocketCreate(const struct TSocketVtbl * const vtblP,
             void *                     const implP,
             TSocket **                 const socketPP);

void
SocketWrite(TSocket *             const socketP,
            const unsigned char * const buffer,
            uint32_t              const len,
            abyss_bool *          const failedP);

uint32_t
SocketRead(TSocket *       const socketP, 
           unsigned char * const buffer, 
           uint32_t        const len);

abyss_bool
SocketConnect(TSocket * const socketP,
              TIPAddr * const addrP,
              uint16_t  const portNumber);

abyss_bool
SocketBind(TSocket * const socketP,
           TIPAddr * const addrP,
           uint16_t  const portNumber);

abyss_bool
SocketListen(TSocket * const socketP,
             uint32_t  const backlog);

void
SocketAccept(TSocket *    const listenSocketP,
             abyss_bool * const connectedP,
             abyss_bool * const failedP,
             TSocket **   const acceptedSocketP,
             TIPAddr *    const ipAddrP);

uint32_t
SocketWait(TSocket *  const socketP,
           abyss_bool const rd,
           abyss_bool const wr,
           uint32_t   const timems);

uint32_t
SocketAvailableReadBytes(TSocket * const socketP);

void
SocketGetPeerName(TSocket *    const socketP,
                  TIPAddr *    const ipAddrP,
                  uint16_t *   const portNumberP,
                  abyss_bool * const successP);

uint32_t
SocketError(TSocket * const socketP);

#endif

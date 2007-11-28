#ifndef CONN_H_INCLUDED
#define CONN_H_INCLUDED

#include <xmlrpc-c/abyss.h>
#include "abyss_socket.h"
#include "abyss_file.h"

#define BUFFER_SIZE 4096 

struct _TConn {
    struct _TConn * nextOutstandingP;
        /* Link to the next connection in the list of outstanding
           connections
        */
    TServer * server;
    uint32_t buffersize;
        /* Index into the connection buffer (buffer[], below) where
           the next byte read on the connection will go.
        */
    uint32_t bufferpos;
        /* Index into the connection buffer (buffer[], below) where
           the next byte to be delivered to the user is.
        */
    uint32_t inbytes,outbytes;  
    TSocket * socketP;
    TIPAddr peerip;
    abyss_bool hasOwnThread;
    TThread * threadP;
    abyss_bool finished;
        /* We have done all the processing there is to do on this
           connection, other than possibly notifying someone that we're
           done.  One thing this signifies is that any thread or process
           that the connection spawned is dead or will be dead soon, so
           one could reasonably wait for it to be dead, e.g. with
           pthread_join().  Note that one can scan a bunch of processes
           for 'finished' status, but sometimes can't scan a bunch of
           threads for liveness.
        */
    const char * trace;
    TThreadProc * job;
    TThreadDoneFn * done;
    char buffer[BUFFER_SIZE];
};

typedef struct _TConn TConn;

TConn * ConnAlloc(void);

void ConnFree(TConn * const connectionP);

void
ConnCreate(TConn **            const connectionPP,
           TServer *           const serverP,
           TSocket *           const connectedSocketP,
           TThreadProc *       const job,
           TThreadDoneFn *     const done,
           enum abyss_foreback const foregroundBackground,
           abyss_bool          const useSigchld,
           const char **       const errorP);

abyss_bool
ConnProcess(TConn * const connectionP);

abyss_bool
ConnKill(TConn * const connectionP);

void
ConnWaitAndRelease(TConn * const connectionP);

abyss_bool
ConnWrite(TConn *      const connectionP,
          const void * const buffer,
          uint32_t     const size);

abyss_bool
ConnRead(TConn *  const c,
         uint32_t const timems);

void
ConnReadInit(TConn * const connectionP);

abyss_bool
ConnReadHeader(TConn * const connectionP,
               char ** const headerP);

abyss_bool
ConnWriteFromFile(TConn *  const connectionP,
                  TFile *  const file,
                  uint64_t const start,
                  uint64_t const end,
                  void *   const buffer,
                  uint32_t const buffersize,
                  uint32_t const rate);

TServer *
ConnServer(TConn * const connectionP);

#endif

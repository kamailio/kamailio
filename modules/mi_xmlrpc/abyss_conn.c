/* Copyright information is at the end of the file. */

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "abyss_mallocvar.h"
#include "abyss_xmlrpc_int.h"
#include <xmlrpc-c/abyss.h>
#include "abyss_socket.h"
#include "abyss_server.h"
#include "abyss_thread.h"

#include "abyss_conn.h"

/*********************************************************************
** Conn
*********************************************************************/

static TThreadProc connJob;

static void
connJob(void * const userHandle) {
/*----------------------------------------------------------------------------
   This is the root function for a thread that processes a connection
   (performs HTTP transactions).
-----------------------------------------------------------------------------*/
    TConn * const connectionP = userHandle;

    (connectionP->job)(connectionP);

    connectionP->finished = TRUE;
        /* Note that if we are running in a forked process, setting
           connectionP->finished has no effect, because it's just our own
           copy of *connectionP.  In this case, Parent must update his own
           copy based on a SIGCHLD signal that the OS will generate right
           after we exit.
        */

    ThreadExit(0);
}



static void
connDone(TConn * const connectionP) {

    /* In the forked case, this is designed to run in the parent
       process after the child has terminated.
    */
    connectionP->finished = TRUE;

    if (connectionP->done)
        connectionP->done(connectionP);
}



static TThreadDoneFn threadDone;

static void
threadDone(void * const userHandle) {

    TConn * const connectionP = userHandle;
    
    connDone(connectionP);
}



static void
makeThread(TConn *             const connectionP,
           enum abyss_foreback const foregroundBackground,
           abyss_bool          const useSigchld,
           const char **       const errorP) {
           
    switch (foregroundBackground) {
    case ABYSS_FOREGROUND:
        connectionP->hasOwnThread = FALSE;
        *errorP = NULL;
        break;
    case ABYSS_BACKGROUND: {
        const char * error;
        connectionP->hasOwnThread = TRUE;
        ThreadCreate(&connectionP->threadP, connectionP,
                     &connJob, &threadDone, useSigchld,
                     &error);
        if (error) {
            xmlrpc_asprintf(errorP, "Unable to create thread to "
                            "process connection.  %s", error);
            xmlrpc_strfree(error);
        } else
            *errorP = NULL;
    } break;
    } /* switch */
}

    

void
ConnCreate(TConn **            const connectionPP,
           TServer *           const serverP,
           TSocket *           const connectedSocketP,
           TThreadProc *       const job,
           TThreadDoneFn *     const done,
           enum abyss_foreback const foregroundBackground,
           abyss_bool          const useSigchld,
           const char **       const errorP) {
/*----------------------------------------------------------------------------
   Create an HTTP connection.

   A connection carries one or more HTTP transactions (request/response).

   'connectedSocketP' transports the requests and responses.

   The connection handles those HTTP requests.

   The connection handles the requests primarily by running the
   function 'job' once.  Some connections can do that autonomously, as
   soon as the connection is created.  Others don't until Caller
   subsequently calls ConnProcess.  Some connections complete the
   processing before ConnProcess return, while others may run the
   connection asynchronously to the creator, in the background, via a
   TThread thread.  'foregroundBackground' determines which.

   'job' calls methods of the connection to get requests and send
   responses.

   Some time after the HTTP transactions are all done, 'done' gets
   called in some context.
-----------------------------------------------------------------------------*/
    TConn * connectionP;

    MALLOCVAR(connectionP);

    if (connectionP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for a connection "
                        "descriptor.");
    else {
        abyss_bool success;
        uint16_t peerPortNumber;

        connectionP->server     = serverP;
        connectionP->socketP    = connectedSocketP;
        connectionP->buffersize = 0;
        connectionP->bufferpos  = 0;
        connectionP->finished   = FALSE;
        connectionP->job        = job;
        connectionP->done       = done;
        connectionP->inbytes    = 0;
        connectionP->outbytes   = 0;
        connectionP->trace      = getenv("ABYSS_TRACE_CONN");

        SocketGetPeerName(connectedSocketP,
                          &connectionP->peerip, &peerPortNumber, &success);

        if (success)
            makeThread(connectionP, foregroundBackground, useSigchld, errorP);
        else
            xmlrpc_asprintf(errorP, "Failed to get peer name from socket.");
    }
    *connectionPP = connectionP;
}



abyss_bool
ConnProcess(TConn * const connectionP) {
/*----------------------------------------------------------------------------
   Drive the main processing of a connection -- run the connection's
   "job" function, which should read HTTP requests from the connection
   and send HTTP responses.

   If we succeed, we guarantee the connection's "done" function will get
   called some time after all processing is complete.  It might be before
   we return or some time after.  If we fail, we guarantee the "done"
   function will not be called.
-----------------------------------------------------------------------------*/
    abyss_bool retval;

    if (connectionP->hasOwnThread) {
        /* There's a background thread to handle this connection.  Set
           it running.
        */
        retval = ThreadRun(connectionP->threadP);
    } else {
        /* No background thread.  We just handle it here while Caller waits. */
        (connectionP->job)(connectionP);
        connDone(connectionP);
        retval = TRUE;
    }
    return retval;
}



void
ConnWaitAndRelease(TConn * const connectionP) {
    if (connectionP->hasOwnThread)
        ThreadWaitAndRelease(connectionP->threadP);
    
    free(connectionP);
}



abyss_bool
ConnKill(TConn * connectionP) {
    connectionP->finished = TRUE;
    return ThreadKill(connectionP->threadP);
}



void
ConnReadInit(TConn * const connectionP) {
    if (connectionP->buffersize>connectionP->bufferpos) {
        connectionP->buffersize -= connectionP->bufferpos;
        memmove(connectionP->buffer,
                connectionP->buffer+connectionP->bufferpos,
                connectionP->buffersize);
        connectionP->bufferpos = 0;
    } else
        connectionP->buffersize=connectionP->bufferpos = 0;

    connectionP->inbytes=connectionP->outbytes = 0;
}



static void
traceBuffer(const char * const label,
            const char * const buffer,
            unsigned int const size) {

    unsigned int nonPrintableCount;
    unsigned int i;
    
    nonPrintableCount = 0;  /* Initial value */
    
    for (i = 0; i < size; ++i) {
        if (!isprint(buffer[i]) && buffer[i] != '\n' && buffer[i] != '\r')
            ++nonPrintableCount;
    }
    if (nonPrintableCount > 0)
        fprintf(stderr, "%s contains %u nonprintable characters.\n", 
                label, nonPrintableCount);
    
    fprintf(stderr, "%s:\n", label);
    fprintf(stderr, "%.*s\n", (int)size, buffer);
}



static void
traceSocketRead(TConn *      const connectionP,
                unsigned int const size) {

    if (connectionP->trace)
        traceBuffer("READ FROM SOCKET:",
                    connectionP->buffer + connectionP->buffersize, size);
}



static void
traceSocketWrite(TConn *      const connectionP,
                 const char * const buffer,
                 unsigned int const size,
                 abyss_bool   const failed) {

    if (connectionP->trace) {
        const char * const label =
            failed ? "FAILED TO WRITE TO SOCKET:" : "WROTE TO SOCKET";
        traceBuffer(label, buffer, size);
    }
}



static uint32_t
bufferSpace(TConn * const connectionP) {
    
    return BUFFER_SIZE - connectionP->buffersize;
}
                    


abyss_bool
ConnRead(TConn *  const connectionP,
         uint32_t const timeout) {
/*----------------------------------------------------------------------------
   Read some stuff on connection *connectionP from the socket.

   Don't wait more than 'timeout' seconds for data to arrive.  Fail if
   nothing arrives within that time.
-----------------------------------------------------------------------------*/
    time_t const deadline = time(NULL) + timeout;

    abyss_bool cantGetData;
    abyss_bool gotData;

    cantGetData = FALSE;
    gotData = FALSE;
    
    while (!gotData && !cantGetData) {
        int const timeLeft = deadline - time(NULL);

        if (timeLeft <= 0)
            cantGetData = TRUE;
        else {
            int rc;
            
            rc = SocketWait(connectionP->socketP, TRUE, FALSE,
                            timeLeft * 1000);
            
            if (rc != 1)
                cantGetData = TRUE;
            else {
                uint32_t bytesAvail;
            
                bytesAvail = SocketAvailableReadBytes(connectionP->socketP);
                
                if (bytesAvail <= 0)
                    cantGetData = TRUE;
                else {
                    uint32_t const bytesToRead =
                        MIN(bytesAvail, bufferSpace(connectionP)-1);

                    uint32_t bytesRead;

                    bytesRead = SocketRead(
                        connectionP->socketP,
                        (unsigned char*)connectionP->buffer + connectionP->buffersize,
                        bytesToRead);
                    if (bytesRead > 0) {
                        traceSocketRead(connectionP, bytesRead);
                        connectionP->inbytes += bytesRead;
                        connectionP->buffersize += bytesRead;
                        connectionP->buffer[connectionP->buffersize] = '\0';
                        gotData = TRUE;
                    }
                }
            }
        }
    }
    if (gotData)
        return TRUE;
    else
        return FALSE;
}


            
abyss_bool
ConnWrite(TConn *      const connectionP,
          const void * const buffer,
          uint32_t     const size) {

    abyss_bool failed;

    SocketWrite(connectionP->socketP, buffer, size, &failed);

    traceSocketWrite(connectionP, buffer, size, failed);

    if (!failed)
        connectionP->outbytes += size;

    return !failed;
}



abyss_bool
ConnWriteFromFile(TConn *  const connectionP,
                  TFile *  const fileP,
                  uint64_t const start,
                  uint64_t const last,
                  void *   const buffer,
                  uint32_t const buffersize,
                  uint32_t const rate) {
/*----------------------------------------------------------------------------
   Write the contents of the file stream *fileP, from offset 'start'
   up through 'last', to the HTTP connection *connectionP.

   Meter the reading so as not to read more than 'rate' bytes per second.

   Use the 'bufferSize' bytes at 'buffer' as an internal buffer for this.
-----------------------------------------------------------------------------*/
    abyss_bool retval;
    uint32_t waittime;
    abyss_bool success;
    uint32_t readChunkSize;

    if (rate > 0) {
        readChunkSize = MIN(buffersize, rate);  /* One second's worth */
        waittime = (1000 * buffersize) / rate;
    } else {
        readChunkSize = buffersize;
        waittime = 0;
    }

    success = FileSeek(fileP, start, SEEK_SET);
    if (!success)
        retval = FALSE;
    else {
        uint64_t const totalBytesToRead = last - start + 1;
        uint64_t bytesread;

        bytesread = 0;  /* initial value */

        while (bytesread < totalBytesToRead) {
            uint64_t const bytesLeft = totalBytesToRead - bytesread;
            uint64_t const bytesToRead = MIN(readChunkSize, bytesLeft);

            uint64_t bytesReadThisTime;

            bytesReadThisTime = FileRead(fileP, buffer, bytesToRead);
            bytesread += bytesReadThisTime;
            
            if (bytesReadThisTime > 0)
                ConnWrite(connectionP, buffer, bytesReadThisTime);
            else
                break;
            
            if (waittime > 0)
                xmlrpc_millisecond_sleep(waittime);
        }
        retval = (bytesread >= totalBytesToRead);
    }
    return retval;
}



static void
processHeaderLine(char *       const start,
                  const char * const headerStart,
                  TConn *      const connectionP,                  
                  time_t       const deadline,
                  abyss_bool * const gotHeaderP,
                  char **      const nextP,
                  abyss_bool * const errorP) {
/*----------------------------------------------------------------------------
  If there's enough data in the buffer at *pP, process a line of HTTP
  header.

  It is part of a header that starts at 'headerStart' and has been
  previously processed up to 'start'.  The data in the buffer is
  terminated by a NUL.

  WE MODIFY THE DATA.
-----------------------------------------------------------------------------*/
    abyss_bool gotHeader;
    char * lfPos;
    char * p;

    p = start;

    gotHeader = FALSE;  /* initial assumption */

    lfPos = strchr(p, LF);
    if (lfPos) {
        if ((*p != LF) && (*p != CR)) {
            /* We're looking at a non-empty line */
            if (*(lfPos+1) == '\0') {
                /* There's nothing in the buffer after the line, so we
                   don't know if there's a continuation line coming.
                   Must read more.
                */
                int const timeLeft = deadline - time(NULL);
                
                *errorP = !ConnRead(connectionP, timeLeft);
            }
            if (!*errorP) {
                p = lfPos; /* Point to LF */
                
                /* If the next line starts with whitespace, it's a
                   continuation line, so blank out the line
                   delimiter (LF or CRLF) so as to join the next
                   line with this one.
                */
                if ((*(p+1) == ' ') || (*(p+1) == '\t')) {
                    if (p > headerStart && *(p-1) == CR)
                        *(p-1) = ' ';
                    *p++ = ' ';
                } else
                    gotHeader = TRUE;
            }
        } else {
            /* We're looking at an empty line (i.e. what marks the
               end of the header)
            */
            p = lfPos;  /* Point to LF */
            gotHeader = TRUE;
        }
    }

    if (gotHeader) {
        /* 'p' points to the final LF */

        /* Replace the LF or the CR in CRLF with NUL, so as to terminate
           the string at 'headerStart' that is the full header.
        */
        if (p > headerStart && *(p-1) == CR)
            *(p-1) = '\0';  /* NUL out CR in CRLF */
        else
            *p = '\0';  /* NUL out LF */

        ++p;  /* Point to next line in buffer */
    }
    *gotHeaderP = gotHeader;
    *nextP = p;
}



abyss_bool
ConnReadHeader(TConn * const connectionP,
               char ** const headerP) {
/*----------------------------------------------------------------------------
   Read an HTTP header on connection *connectionP.

   An HTTP header is basically a line, except that if a line starts
   with white space, it's a continuation of the previous line.  A line
   is delimited by either LF or CRLF.

   In the course of reading, we read at least one character past the
   line delimiter at the end of the header; we may read much more.  We
   leave everything after the header (and its line delimiter) in the
   internal buffer, with the buffer pointer pointing to it.

   We use stuff already in the internal buffer (perhaps left by a
   previous call to this subroutine) before reading any more from from
   the socket.

   Return as *headerP the header value.  This is in the connection's
   internal buffer.  This contains no line delimiters.
-----------------------------------------------------------------------------*/
    uint32_t const deadline = time(NULL) + connectionP->server->srvP->timeout;

    abyss_bool retval;
    char * p;
    char * headerStart;
    abyss_bool error;
    abyss_bool gotHeader;

    p = connectionP->buffer + connectionP->bufferpos;
    headerStart = p;

    gotHeader = FALSE;
    error = FALSE;

    while (!gotHeader && !error) {
        int const timeLeft = deadline - time(NULL);

        if (timeLeft <= 0)
            error = TRUE;
        else {
            if (p >= connectionP->buffer + connectionP->buffersize)
                /* Need more data from the socket to chew on */
                error = !ConnRead(connectionP, timeLeft);

            if (!error) {
                assert(connectionP->buffer + connectionP->buffersize > p);
                processHeaderLine(p, headerStart, connectionP, deadline,
                                  &gotHeader, &p, &error);
            }
        }
    }
    if (gotHeader) {
        /* We've consumed this part of the buffer (but be careful --
           you can't reuse that part of the buffer because the string
           we're returning is in it!
        */
        connectionP->bufferpos += p - headerStart;
        *headerP = headerStart;
        retval = TRUE;
    } else
        retval = FALSE;

    return retval;
}



TServer *
ConnServer(TConn * const connectionP) {
    return connectionP->server;
}



/*******************************************************************************
**
** conn.c
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
******************************************************************************/

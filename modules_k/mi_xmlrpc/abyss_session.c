/******************************************************************************
**
** session.c
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

#include <assert.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include "abyss_xmlrpc_int.h"
#include <xmlrpc-c/abyss.h>
#include "abyss_server.h"
#include "abyss_conn.h"

#include "abyss_session.h"



abyss_bool
SessionRefillBuffer(TSession * const sessionP) {
/*----------------------------------------------------------------------------
   Get the next chunk of data from the connection into the buffer.

   I.e. read data from the socket.
-----------------------------------------------------------------------------*/
    struct _TServer * const srvP = sessionP->conn->server->srvP;
    abyss_bool succeeded;
            
    /* Reset our read buffer & flush data from previous reads. */
    ConnReadInit(sessionP->conn);
    
    /* Read more network data into our buffer.  If we encounter a
       timeout, exit immediately.  We're very forgiving about the
       timeout here.  We allow a full timeout per network read, which
       would allow somebody to keep a connection alive nearly
       indefinitely.  But it's hard to do anything intelligent here
       without very complicated code.
    */
    succeeded = ConnRead(sessionP->conn, srvP->timeout);

    return succeeded;
}



size_t
SessionReadDataAvail(TSession * const sessionP) {

    return sessionP->conn->buffersize - sessionP->conn->bufferpos;

}



void
SessionGetReadData(TSession *    const sessionP, 
                   size_t        const max, 
                   const char ** const outStartP, 
                   size_t *      const outLenP) {
/*----------------------------------------------------------------------------
   Extract some data which the server has read and buffered for the
   session.  Don't get or wait for any data that has not yet arrived.
   Do not return more than 'max'.

   We return a pointer to the first byte as *outStartP, and the length in
   bytes as *outLenP.  The memory pointed to belongs to the session.
-----------------------------------------------------------------------------*/
    uint32_t const bufferPos = sessionP->conn->bufferpos;

    *outStartP = &sessionP->conn->buffer[bufferPos];

    assert(bufferPos <= sessionP->conn->buffersize);

    *outLenP = MIN(max, sessionP->conn->buffersize - bufferPos);

    /* move pointer past the bytes we are returning */
    sessionP->conn->bufferpos += *outLenP;

    assert(sessionP->conn->bufferpos <= sessionP->conn->buffersize);
}



void
SessionGetRequestInfo(TSession *            const sessionP,
                      const TRequestInfo ** const requestInfoPP) {
    
    *requestInfoPP = &sessionP->request_info;
}



abyss_bool
SessionLog(TSession * const sessionP) {

    abyss_bool retval;

    if (!sessionP->validRequest)
        retval = FALSE;
    else {
        const char * const user = sessionP->request_info.user;

        const char * logline;
        char date[30];

        DateToLogString(&sessionP->date, date);

        xmlrpc_asprintf(&logline, "%d.%d.%d.%d - %s - [%s] \"%s\" %d %d",
                        IPB1(sessionP->conn->peerip),
                        IPB2(sessionP->conn->peerip),
                        IPB3(sessionP->conn->peerip),
                        IPB4(sessionP->conn->peerip),
                        user ? user : "",
                        date, 
                        sessionP->request_info.requestline,
                        sessionP->status,
                        sessionP->conn->outbytes
            );
        if (logline) {
            LogWrite(sessionP->conn->server, logline);

            xmlrpc_strfree(logline);
        }
        retval = TRUE;
    }
    return retval;
}




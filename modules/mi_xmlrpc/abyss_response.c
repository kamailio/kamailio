/*=============================================================================
                             response
===============================================================================
  This module contains callbacks from and services for a request handler.

  Copyright information is at the end of the file
=============================================================================*/

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <xmlrpc-c/config.h>
#include "abyss_mallocvar.h"
#include "abyss_xmlrpc_int.h"
#include <xmlrpc-c/abyss.h>

#include "abyss_server.h"
#include "abyss_session.h"
#include "abyss_conn.h"
#include "abyss_token.h"
#include "abyss_date.h"
#include "abyss_data.h"
#include "abyss_info.h"
#include "abyss_http.h"



void
ResponseError(TSession * const sessionP) {

    const char * const reason = HTTPReasonByStatus(sessionP->status);
    const char * errorDocument;

    ResponseAddField(sessionP, "Content-type", "text/html");

    ResponseWriteStart(sessionP);
    
    xmlrpc_asprintf(&errorDocument,
                    "<HTML><HEAD><TITLE>Error %d</TITLE></HEAD>"
                    "<BODY><H1>Error %d</H1><P>%s</P>" SERVER_HTML_INFO 
                    "</BODY></HTML>",
                    sessionP->status, sessionP->status, reason);
    
    ConnWrite(sessionP->conn, errorDocument, strlen(errorDocument)); 

    xmlrpc_strfree(errorDocument);
}



abyss_bool
ResponseChunked(TSession * const sessionP) {
    /* This is only a hope, things will be real only after a call of
       ResponseWriteStart()
    */
    assert(!sessionP->responseStarted);

    sessionP->chunkedwrite =
        (sessionP->version.major > 1) ||
        (sessionP->version.major == 1 && (sessionP->version.minor >= 1));

    sessionP->chunkedwritemode = TRUE;

    return TRUE;
}



void
ResponseStatus(TSession * const sessionP,
               uint16_t   const code) {

    sessionP->status = code;
}



uint16_t
ResponseStatusFromErrno(int const errnoArg) {

    uint16_t code;

    switch (errnoArg) {
    case EACCES:
        code=403;
        break;
    case ENOENT:
        code=404;
        break;
    default:
        code=500;
    }
    return code;
}



void
ResponseStatusErrno(TSession * const sessionP) {

    ResponseStatus(sessionP, ResponseStatusFromErrno(errno));
}



abyss_bool
ResponseAddField(TSession *   const sessionP,
                 const char * const name,
                 const char * const value) {

    return TableAdd(&sessionP->response_headers, name, value);
}



static void
addDateHeader(TSession * const sessionP) {

    char dateValue[64];
    abyss_bool validDate;

    validDate = DateToString(&sessionP->date, dateValue);

    if (sessionP->status >= 200 && validDate)
        ResponseAddField(sessionP, "Date", dateValue);
}



void
ResponseWriteStart(TSession * const sessionP) {

    struct _TServer * const srvP = ConnServer(sessionP->conn)->srvP;

    unsigned int i;

    assert(!sessionP->responseStarted);

    if (sessionP->status == 0) {
        // Handler hasn't set status.  That's an error
        sessionP->status = 500;
    }

    sessionP->responseStarted = TRUE;

    {
        const char * const reason = HTTPReasonByStatus(sessionP->status);
        const char * line;
        xmlrpc_asprintf(&line,"HTTP/1.1 %u %s\r\n", sessionP->status, reason);
        ConnWrite(sessionP->conn, line, strlen(line));
        xmlrpc_strfree(line);
    }

    if (HTTPKeepalive(sessionP)) {
        const char * keepaliveValue;
        
        ResponseAddField(sessionP, "Connection", "Keep-Alive");

        xmlrpc_asprintf(&keepaliveValue, "timeout=%u, max=%u",
                        srvP->keepalivetimeout, srvP->keepalivemaxconn);

        ResponseAddField(sessionP, "Keep-Alive", keepaliveValue);

        xmlrpc_strfree(keepaliveValue);
    } else
        ResponseAddField(sessionP, "Connection", "close");
    
    if (sessionP->chunkedwrite && sessionP->chunkedwritemode)
        ResponseAddField(sessionP, "Transfer-Encoding", "chunked");

    addDateHeader(sessionP);

    /* Generation of the server field */
    if (srvP->advertise)
        ResponseAddField(sessionP, "Server", SERVER_HVERSION);

    /* send all the fields */
    for (i = 0; i < sessionP->response_headers.size; ++i) {
        TTableItem * const ti = &sessionP->response_headers.item[i];
        const char * line;
        xmlrpc_asprintf(&line, "%s: %s\r\n", ti->name, ti->value);
        ConnWrite(sessionP->conn, line, strlen(line));
        xmlrpc_strfree(line);
    }

    ConnWrite(sessionP->conn, "\r\n", 2);  
}



abyss_bool
ResponseWriteBody(TSession *   const sessionP,
                  const char * const data,
                  uint32_t     const len) {

    return HTTPWriteBodyChunk(sessionP, data, len);
}



abyss_bool
ResponseWriteEnd(TSession * const sessionP) {

    return HTTPWriteEndChunk(sessionP);
}



abyss_bool
ResponseContentType(TSession *   const serverP,
                    const char * const type) {

    return ResponseAddField(serverP, "Content-type", type);
}



abyss_bool
ResponseContentLength(TSession * const sessionP,
                      uint64_t   const len) {
    char contentLengthValue[32];
    
    sprintf(contentLengthValue, "%llu", (long long unsigned int)len);

    return ResponseAddField(sessionP, "Content-length", contentLengthValue);
}


/*********************************************************************
** MIMEType
*********************************************************************/

struct MIMEType {
    TList typeList;
    TList extList;
    TPool pool;
};


static MIMEType * globalMimeTypeP = NULL;



MIMEType *
MIMETypeCreate(void) {
 
    MIMEType * MIMETypeP;

    MALLOCVAR(MIMETypeP);

    if (MIMETypeP) {
        ListInit(&MIMETypeP->typeList);
        ListInit(&MIMETypeP->extList);
        PoolCreate(&MIMETypeP->pool, 1024);
    }
    return MIMETypeP;
}



void
MIMETypeDestroy(MIMEType * const MIMETypeP) {

    PoolFree(&MIMETypeP->pool);
}



void
MIMETypeInit(void) {

    if (globalMimeTypeP != NULL)
        abort();

    globalMimeTypeP = MIMETypeCreate();
}



void
MIMETypeTerm(void) {

    if (globalMimeTypeP == NULL)
        abort();

    MIMETypeDestroy(globalMimeTypeP);

    globalMimeTypeP = NULL;
}



static void
mimeTypeAdd(MIMEType *   const MIMETypeP,
            const char * const type,
            const char * const ext,
            abyss_bool * const successP) {
    
    uint16_t index;
    void * mimeTypesItem;
    abyss_bool typeIsInList;

    assert(MIMETypeP != NULL);

    typeIsInList = ListFindString(&MIMETypeP->typeList, type, &index);
    if (typeIsInList)
        mimeTypesItem = MIMETypeP->typeList.item[index];
    else
        mimeTypesItem = (void*)PoolStrdup(&MIMETypeP->pool, type);

    if (mimeTypesItem) {
        abyss_bool extIsInList;
        extIsInList = ListFindString(&MIMETypeP->extList, ext, &index);
        if (extIsInList) {
            MIMETypeP->typeList.item[index] = mimeTypesItem;
            *successP = TRUE;
        } else {
            void * extItem = (void*)PoolStrdup(&MIMETypeP->pool, ext);
            if (extItem) {
                abyss_bool addedToMimeTypes;

                addedToMimeTypes =
                    ListAdd(&MIMETypeP->typeList, mimeTypesItem);
                if (addedToMimeTypes) {
                    abyss_bool addedToExt;
                    
                    addedToExt = ListAdd(&MIMETypeP->extList, extItem);
                    *successP = addedToExt;
                    if (!*successP)
                        ListRemove(&MIMETypeP->typeList);
                } else
                    *successP = FALSE;
                if (!*successP)
                    PoolReturn(&MIMETypeP->pool, extItem);
            } else
                *successP = FALSE;
        }
    } else
        *successP = FALSE;
}




abyss_bool
MIMETypeAdd2(MIMEType *   const MIMETypeArg,
             const char * const type,
             const char * const ext) {

    MIMEType * MIMETypeP = MIMETypeArg ? MIMETypeArg : globalMimeTypeP;

    abyss_bool success;

    if (MIMETypeP == NULL)
        success = FALSE;
    else 
        mimeTypeAdd(MIMETypeP, type, ext, &success);

    return success;
}



abyss_bool
MIMETypeAdd(const char * const type,
            const char * const ext) {

    return MIMETypeAdd2(globalMimeTypeP, type, ext);
}



static const char *
mimeTypeFromExt(MIMEType *   const MIMETypeP,
                const char * const ext) {

    const char * retval;
    uint16_t extindex;
    abyss_bool extIsInList;

    assert(MIMETypeP != NULL);

    extIsInList = ListFindString(&MIMETypeP->extList, ext, &extindex);
    if (!extIsInList)
        retval = NULL;
    else
        retval = MIMETypeP->typeList.item[extindex];
    
    return retval;
}



const char *
MIMETypeFromExt2(MIMEType *   const MIMETypeArg,
                 const char * const ext) {

    const char * retval;

    MIMEType * MIMETypeP = MIMETypeArg ? MIMETypeArg : globalMimeTypeP;

    if (MIMETypeP == NULL)
        retval = NULL;
    else
        retval = mimeTypeFromExt(MIMETypeP, ext);

    return retval;
}



const char *
MIMETypeFromExt(const char * const ext) {

    return MIMETypeFromExt2(globalMimeTypeP, ext);
}



static void
findExtension(const char *  const fileName,
              const char ** const extP) {

    unsigned int extPos = 0;  /* stifle unset variable warning */
        /* Running estimation of where in fileName[] the extension starts */
    abyss_bool extFound;
    unsigned int i;

    /* We're looking for the last dot after the last slash */
    for (i = 0, extFound = FALSE; fileName[i]; ++i) {
        char const c = fileName[i];
        
        if (c == '.') {
            extFound = TRUE;
            extPos = i + 1;
        }
        if (c == '/')
            extFound = FALSE;
    }

    if (extFound)
        *extP = &fileName[extPos];
    else
        *extP = NULL;
}



static const char *
mimeTypeFromFileName(MIMEType *   const MIMETypeP,
                     const char * const fileName) {

    const char * retval;
    const char * ext;

    assert(MIMETypeP != NULL);
    
    findExtension(fileName, &ext);

    if (ext)
        retval = MIMETypeFromExt2(MIMETypeP, ext);
    else
        retval = "application/octet-stream";

    return retval;
}



const char *
MIMETypeFromFileName2(MIMEType *   const MIMETypeArg,
                      const char * const fileName) {

    const char * retval;
    
    MIMEType * MIMETypeP = MIMETypeArg ? MIMETypeArg : globalMimeTypeP;

    if (MIMETypeP == NULL)
        retval = NULL;
    else
        retval = mimeTypeFromFileName(MIMETypeP, fileName);

    return retval;
}



const char *
MIMETypeFromFileName(const char * const fileName) {

    return MIMETypeFromFileName2(globalMimeTypeP, fileName);
}



static abyss_bool
fileContainsText(const char * const fileName) {
/*----------------------------------------------------------------------------
   Return true iff we can read the contents of the file named 'fileName'
   and see that it appears to be composed of plain text characters.
-----------------------------------------------------------------------------*/
    abyss_bool retval;
    abyss_bool fileOpened;
    TFile file;

    fileOpened = FileOpen(&file, fileName, O_BINARY | O_RDONLY);
    if (fileOpened) {
        char const ctlZ = 26;
        unsigned char buffer[80];
        int32_t readRc;
        unsigned int i;

        readRc = FileRead(&file, buffer, sizeof(buffer));
       
        if (readRc >= 0) {
            unsigned int bytesRead = readRc;
            abyss_bool nonTextFound;

            nonTextFound = FALSE;  /* initial value */
    
            for (i = 0; i < bytesRead; ++i) {
                char const c = buffer[i];
                if (c < ' ' && !isspace(c) && c != ctlZ)
                    nonTextFound = TRUE;
            }
            retval = !nonTextFound;
        } else
            retval = FALSE;
        FileClose(&file);
    } else
        retval = FALSE;

    return retval;
}


 
static const char *
mimeTypeGuessFromFile(MIMEType *   const MIMETypeP,
                      const char * const fileName) {

    const char * retval;
    const char * ext;

    findExtension(fileName, &ext);

    retval = NULL;

    if (ext && MIMETypeP)
        retval = MIMETypeFromExt2(MIMETypeP, ext);
    
    if (!retval) {
        if (fileContainsText(fileName))
            retval = "text/plain";
        else
            retval = "application/octet-stream";  
    }
    return retval;
}



const char *
MIMETypeGuessFromFile2(MIMEType *   const MIMETypeArg,
                       const char * const fileName) {

    return mimeTypeGuessFromFile(MIMETypeArg ? MIMETypeArg : globalMimeTypeP,
                                 fileName);
}



const char *
MIMETypeGuessFromFile(const char * const fileName) {

    return mimeTypeGuessFromFile(globalMimeTypeP, fileName);
}

                                  

#ifdef XMLRPC_OLD_VERSION
/*********************************************************************
** Base64
*********************************************************************/

void Base64Encode(char *s,char *d)
{
    /* Conversion table. */
    static char tbl[64] = {
        'A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X',
        'Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n',
        'o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3',
        '4','5','6','7','8','9','+','/'
    };

    uint32_t i,length=strlen(s);
    char *p=d;
    
    /* Transform the 3x8 bits to 4x6 bits, as required by base64. */
    for (i = 0; i < length; i += 3)
    {
        *p++ = tbl[s[0] >> 2];
        *p++ = tbl[((s[0] & 3) << 4) + (s[1] >> 4)];
        *p++ = tbl[((s[1] & 0xf) << 2) + (s[2] >> 6)];
        *p++ = tbl[s[2] & 0x3f];
        s += 3;
    }
    
    /* Pad the result if necessary... */
    if (i == length + 1)
        *(p - 1) = '=';
    else if (i == length + 2)
        *(p - 1) = *(p - 2) = '=';
    
    /* ...and zero-terminate it. */
    *p = '\0';
}
#endif

/******************************************************************************
**
** http.c
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

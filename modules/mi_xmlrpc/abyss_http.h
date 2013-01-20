#ifndef HTTP_H_INCLUDED
#define HTTP_H_INCLUDED

#include "abyss_conn.h"

/*********************************************************************
** Request
*********************************************************************/

abyss_bool RequestValidURI(TSession *r);
abyss_bool RequestValidURIPath(TSession *r);
abyss_bool RequestUnescapeURI(TSession *r);

abyss_bool RequestRead(TSession *r);
void RequestInit(TSession *r,TConn *c);
void RequestFree(TSession *r);

abyss_bool RequestAuth(TSession *r,char *credential,char *user,char *pass);

/*********************************************************************
** HTTP
*********************************************************************/

const char *
HTTPReasonByStatus(uint16_t const code);

int32_t
HTTPRead(TSession *   const sessionP,
         const char * const buffer,
         uint32_t     const len);

abyss_bool
HTTPWriteBodyChunk(TSession *   const sessionP,
                   const char * const buffer,
                   uint32_t     const len);

abyss_bool
HTTPWriteEndChunk(TSession * const sessionP);

abyss_bool
HTTPKeepalive(TSession * const sessionP);

#endif

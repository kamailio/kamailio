/* 
 * $Id$ 
 */

#ifndef __DEFS_H__
#define __DEFS_H__

#define PARANOID

#define AUTH_RESPONSE  "Proxy-Authorization"

#define AUTH_HF_LEN 512


#define DB_URL "sql://janakj:heslo@localhost/ser"
#define DB_TABLE "auth"


/*
 * Helper definitions
 */
#define DIGEST   "Digest"

#define USERNAME  "username"
#define REALM     "realm"
#define NONCE     "nonce"
#define URI       "uri"
#define RESPONSE  "response"
#define CNONCE    "cnonce"
#define OPAQUE    "opaque"
#define QOP       "qop"
#define NC        "nc"
#define ALGORITHM "algorithm"

#define USERNAME_ID   1
#define REALM_ID      2
#define NONCE_ID      3
#define URI_ID        4
#define RESPONSE_ID   5
#define CNONCE_ID     6
#define OPAQUE_ID     7
#define QOP_ID        8
#define NC_ID         9
#define ALGORITHM_ID 10
#define UNKNOWN_ID  256


#define MESSAGE_407 "Proxy Authentication Required"
#define MESSAGE_400 "Bad Request"

#define QOP_STRING "auth"

#endif

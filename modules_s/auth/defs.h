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


#define NONCE_SECRET "4e9rhygt90ofw34e8hiof09tg"
#define NONCE_SECRET_LEN 25


/*
 * Helper definitions
 */

#define MESSAGE_407 "Proxy Authentication Required"
#define MESSAGE_400 "Bad Request"

#endif

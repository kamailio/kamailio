/* 
 * $Id$ 
 */

#ifndef __DEFS_H__
#define __DEFS_H__

#define PARANOID

#define AUTH_RESPONSE  "Proxy-Authorization"

#define AUTH_HF_LEN 512


#define DB_URL "sql://csps:47csps11@dbhost/csps107"
#define DB_TABLE "subscriber"
#define SUBS_USER_COL "user_id"
#define SUBS_REALM_COL "realm"
#define SUBS_HA1_COL "ha1"

#define NONCE_SECRET "4e9rhygt90ofw34e8hiof09tg"
#define NONCE_SECRET_LEN 25

#define GRP_TABLE "grp"
#define GRP_USER "user"
#define GRP_GRP "grp"


/*
 * Helper definitions
 */

#define MESSAGE_407 "Proxy Authentication Required"
#define MESSAGE_400 "Bad Request"

#endif

/*
 * $Id$
 */

#ifndef auth_mod_h
#define auth_mod_h

#include "db.h"
#include "defs.h"
#include "../../str.h"

/*
 * Module parameters variables
 */

extern char* db_url;        /* Database URL */
extern char* user_column;   /* 'user' column name */
extern char* realm_column;  /* 'realm' column name */
extern char* pass_column;   /* 'password' column name */

#ifdef USER_DOMAIN_HACK
extern char* pass_column_2;
#endif

extern str secret;          /* secret phrase used to generate nonce */
extern char* grp_table;     /* 'group' table name */
extern char* grp_user_col;  /* 'user' column name in group table */
extern char* grp_grp_col;   /* "group' column name in group table */
extern int calc_ha1;        /* if set to 1, ha1 is calculated by the server */
extern int nonce_expire;    /* nonce expire interval */
extern int retry_count;     /* How many time a client can retry */
 
extern db_con_t* db_handle; /* Database connection handle */

#endif

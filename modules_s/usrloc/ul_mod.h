/*
 * $Id$
 *
 * Usrlocation module interface
 */

#ifndef UL_MOD_H
#define UL_MOD_H


#include "../../db/db.h"


/*
 * Module parameters
 */

extern char* user_col;
extern char* contact_col;
extern char* expires_col;
extern char* q_col;
extern char* callid_col;
extern char* cseq_col;
extern char* method_col;
extern char* db_url;
extern int   timer_interval;
extern int   write_through;
extern int   use_db;

extern db_con_t* db;   /* Dabase connection handle */


#endif /* UL_MOD_H */

/* 
 * $Id$ 
 */

#ifndef __DBASE_H__
#define __DBASE_H__

#include "../../sr_module.h"
#include "location.h"
#include "contact.h"


typedef int (*query_loc_func_t)  (const char*, const char*, location_t**);
typedef int (*insert_loc_func_t) (const char*, const location_t*);
typedef int (*insert_con_func_t) (const char*, const contact_t*);
typedef int (*delete_loc_func_t) (const char*, const location_t*);
typedef int (*update_loc_func_t) (const char*, const location_t*);
typedef int (*update_con_func_t) (const char*, const contact_t*);


typedef struct db_hooks {
	query_loc_func_t  q_loc;
	insert_loc_func_t i_loc;
	insert_con_func_t i_con;
	delete_loc_func_t d_loc;
	update_loc_func_t u_loc;
	update_con_func_t u_con;
} db_hooks_t;


extern db_hooks_t dbase;


int  init_db(void);
void close_db(void);

#endif

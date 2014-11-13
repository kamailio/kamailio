/* 
 * File:   db.h
 * Author: jaybeepee
 *
 * Created on 14 October 2014, 5:48 PM
 */

#ifndef DB_H
#define	DB_H

#include "../../lib/srdb1/db.h"
#include "../../str.h"
#include "../../ut.h"
#include "route_data.h"

#include <string.h>

#define QUERY_LEN 2048

extern str interconnectroute_db_url;

extern db1_con_t * interconnectroute_dbh;
extern db_func_t interconnectroute_dbf;

void interconnectroute_db_close(void);
int interconnectroute_db_init(void);
int interconnectroute_db_open(void);

int get_orig_route_data(str* a_number, str* b_number, str* leg, str* sc, ix_route_list_t** ix_route_list);
int get_term_route_data(str* a_number, str* b_number, str* leg, str* sc, str* ext_trunk_id, ix_route_list_t** ix_route_list);



#endif	/* DB_H */


#ifndef  UL_DB_LAYER_H
#define UL_DB_LAYER_H

#include "../../lib/srdb1/db.h"
#include "../usrloc/usrloc.h"
#include "udomain.h"
#include "ucontact.h"

#define DB_TYPE_CLUSTER 0
#define DB_TYPE_CLUSTER_STR "cluster"
#define DB_TYPE_SINGLE 1
#define DB_TYPE_SINGLE_STR "single"

typedef struct ul_domain_db {
	str name;
	str url;
	int dbt;
	struct ul_domain_db_list * next;
} ul_domain_db_t;

typedef struct ul_domain_db_list {
	ul_domain_db_t domain;
	struct ul_domain_db_list * next;
} ul_domain_db_list_t;

int ul_db_layer_init();

void ul_db_layer_destroy();

int ul_db_layer_single_connect(udomain_t * domain, str * url);

int ul_db_layer_insert(udomain_t * domain, str * user, str * sipdomain, 
				 db_key_t* _k, db_val_t* _v, int _n);

int ul_db_layer_update(udomain_t * domain, str * user, str * sipdomain, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	         db_key_t* _uk, db_val_t* _uv, int _n, int _un);

int ul_db_layer_replace(udomain_t * domain, str * user, str * sipdomain, 
						db_key_t* _k,	db_val_t* _v, int _n, int _un);

int ul_db_layer_delete(udomain_t * domain, str * user, str * sipdomain, 
				 db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n);

int ul_db_layer_query(udomain_t * domain, str * user, str * sipdomain,
				db_key_t* _k, db_op_t* _op, db_val_t* _v, db_key_t* _c, 
				int _n, int _nc, db_key_t _o, db1_res_t** _r);

int ul_db_layer_raw_query(udomain_t * domain, str* _s, db1_res_t** _r);

int ul_db_layer_fetch_result(udomain_t * domain, db1_res_t** _r, int _n);

int ul_db_layer_free_result(udomain_t * domain, db1_res_t * res);

int ul_add_domain_db(str * d, int t, str * url);

ul_domain_db_t * ul_find_domain(const char * s);

int parse_domain_db(str * _d);

#endif


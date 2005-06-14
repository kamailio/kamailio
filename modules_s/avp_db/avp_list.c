/* 
 * $Id$
 *
 * Functions for work with whole listo of AVPs
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <string.h>
#include "avp_db.h"
#include "avp_list.h"
#include "fifo.h"
#include "unixsock.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../config.h"
#include "../../db/db.h"
#include "../../usr_avp.h"


avp_list_t	**avp_list; 		/* pointer to current list of AVPs*/
avp_list_t	*avp_list_1;		/* list of AVPs and their default values*/
avp_list_t	*avp_list_2;		/* list of AVPs and their default values*/




int init_avp_list(){

	/* Initialize fifo interface */
	if (init_avp_fifo() < 0) {
		LOG(L_ERR, "ERROR: avp_db: init_avp_list(): error while initializing"
				" fifo interface\n");
		return -1;
	}

	/* Initialize unixsock interface */
	if (init_avp_unixsock() < 0) {
		LOG(L_ERR, "ERROR: avp_db: init_avp_list(): error while initializing"
				" unix socket interface\n");
		return -1;
	}

	if (avp_db_init() < 0) return -1;

	/* Initializing avp lists */
	avp_list_1 = (avp_list_t *)shm_malloc(sizeof(avp_list_t));
	if (avp_list_1 == 0) {
		LOG(L_ERR, "ERROR: avp_db: init_avp_list(): "
				"No memory for avp list\n");
	}
	avp_list_1->avps = NULL;

	avp_list_2 = (avp_list_t *)shm_malloc(sizeof(avp_list_t));
	if (avp_list_2 == 0) {
		LOG(L_ERR, "ERROR: avp_db: init_avp_list():"
				" No memory for avp list\n");
	}
	avp_list_2->avps = NULL;


	avp_list = (avp_list_t **)shm_malloc(sizeof(avp_list_t *));
	if (avp_list == 0) {
		LOG(L_ERR, "ERROR: avp_db: init_avp_list():"
				" No memory for pointer to avp list\n");
	}

	if (reload_avp_list() == -1) {
		LOG(L_CRIT, "ERROR: avp_db:init_avp_list():"
				" avp list reload failed\n");
		return -1;
	}
		
	avp_db_close();

	return 0;
}


/* add new entry to avp list */
int avp_install(avp_attr_t* avp, char *name, int type, char *dval){

	avp->name.len = strlen(name);
	avp->name.s = (char *) shm_malloc(avp->name.len);
	if (avp->name.s == NULL) {
		LOG(L_CRIT, "avp_db: avp_install(): Cannot allocate memory for name string\n");
		return -1;
	}
	strncpy(avp->name.s, name, avp->name.len);

	avp->type = type;

	avp->dval.len = strlen(dval);
	avp->dval.s = (char *) shm_malloc(avp->dval.len);
	if (avp->dval.s == NULL) {
		LOG(L_CRIT, "avp_db: avp_install(): Cannot allocate memory for dval string\n");
		return -1;
	}
	strncpy(avp->dval.s, dval, avp->dval.len);

	return 1;
}

/* free memory allocated by avp list */
void avp_list_free(avp_list_t *avp_l){
	avp_attr_t*	cur_avp;

	if (avp_l->avps != NULL){

		DBG("avp_db: avp_list_free\n");
	
		for (cur_avp = avp_l->avps; 
		     cur_avp < avp_l->avps + avp_l->n; 
			 cur_avp++){

			shm_free(cur_avp->name.s);
			shm_free(cur_avp->dval.s);
	
		}
		shm_free(avp_l->avps);
	}
}


/* reload list of AVPs from DB */
int reload_avp_list(){

	int err = -1;

	avp_attr_t*	cur_avp;

	db_key_t  cols[3];
	db_res_t* res;
	db_row_t* cur_row;

	avp_list_t	*new_avp_list;

	DBG("avp_db: reload_avp_list started\n");

	cols[0] = attr_name_column;
	cols[1] = attr_type_column;
	cols[2] = attr_dval_column;


	if (dbf.use_table(db_handle, db_list_table) < 0) {
		LOG(L_ERR, "ERROR: avp_db: reload_avp_list: Unable to change the table\n");
	}


	err = dbf.query(db_handle, NULL, 0, NULL, cols, 0, 3, 0, &res);


	if (err) {
		LOG(L_ERR,"ERROR: avp_db: reload_avp_list: db_query failed.");
		return -1;
	}


	/* Choose new AVPs table and free its old contents */
	if (*avp_list == avp_list_1) {
		avp_list_free(avp_list_2);
		new_avp_list = avp_list_2;
	} else {
		avp_list_free(avp_list_1);
		new_avp_list = avp_list_1;
	}

	/* alloc memory for list of AVPs */
	new_avp_list->avps = shm_malloc(sizeof(avp_attr_t) * RES_ROW_N(res));
	if (new_avp_list->avps == 0) {
		LOG(L_ERR, "ERROR: avp_db: reload_avp_list: Out of memory");
		dbf.free_result(db_handle, res);
		return -1;
	}
	new_avp_list->n = RES_ROW_N(res);


	for (cur_row = res->rows, cur_avp = new_avp_list->avps; 
	     cur_row < res->rows + res->n; 
		 cur_row++, cur_avp++) {
		 
		if (VAL_NULL(ROW_VALUES(cur_row)) || 
		    VAL_NULL(ROW_VALUES(cur_row) + 1) || 
			VAL_NULL(ROW_VALUES(cur_row) + 2)) {
			continue;
		}

		if (avp_install(cur_avp, 
		                (char *)(VAL_STRING(ROW_VALUES(cur_row))),
		                VAL_INT(ROW_VALUES(cur_row) + 1),
		                (char*)VAL_STRING(ROW_VALUES(cur_row) + 2)
						) == -1) {
			LOG(L_ERR, "avp_db: reload_avp_list: AVP list problem\n");
			dbf.free_result(db_handle, res);
			return -1;
		}

		DBG("avp_db: reload_avp_list: AVP '%s'='%s' has been read\n", 
				VAL_STRING(ROW_VALUES(cur_row)),
				VAL_STRING(ROW_VALUES(cur_row) + 2));

	}

	dbf.free_result(db_handle, res);

	*avp_list = new_avp_list;	

	return 1;
}


/* Init array of flags - set all flags*/
void init_avp_use_def(int* a, avp_list_t *avp_l){
	int i;
	for (i=0; i<avp_l->n; i++){
		a[i] = 1;
	}
}

/* reset flag for avp with name name */
void reset_avp_use_def_flag(int* a, avp_list_t *avp_l, str* name){
	int i;
	avp_attr_t*	cur_avp;

	for (i=0, cur_avp=avp_l->avps; 
	     i<avp_l->n; 
		 i++, cur_avp++){

		if ((cur_avp->name.len == name->len) &&
		    !memcmp(cur_avp->name.s, name->s, cur_avp->name.len)) {

			a[i] = 0;
			return;
		}
	}
}

/* add default values of avps which has the flag set */
void add_default_avps(int* a, avp_list_t *avp_l, str* prefix){
	int i, err = -1;

	avp_attr_t*	cur_avp;

	str name_str;
	int_str name, val;
	

	for (i=0, cur_avp=avp_l->avps; 
	     i < avp_l->n; 
		 i++, cur_avp++){

		/* if flag is set */
		if (a[i]){

			name_str.len = prefix->len + cur_avp->name.len;
			name_str.s = pkg_malloc(name_str.len);
			if (name_str.s == 0) {
				LOG(L_ERR, "ERROR: avp_db: add_default_avps: Out of memory");
				return;
			}
			    
			memcpy(name_str.s, prefix->s, prefix->len);
			memcpy(name_str.s + prefix->len, 
			       cur_avp->name.s, cur_avp->name.len);

			name.s = &name_str;
			val.s  = &cur_avp->dval;


			err = add_avp(AVP_NAME_STR | AVP_VAL_STR, name, val);
			if (err != 0) {
				LOG(L_ERR, "ERROR: avp_db: add_default_avps: add_avp failed\n");
				pkg_free(name_str.s);
			}
		
			DBG("avp_db: add_default_avps: AVP '%.*s'='%.*s' has been added\n", name_str.len, 
			    name_str.s,
			    cur_avp->dval.len,
			    cur_avp->dval.s);
		}
	}	
}

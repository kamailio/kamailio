/*
 * $Id$
 *
 * rls db - RLS database support 
 *
 * Copyright (C) 2011 Crocodile RCS Ltd
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"

#include "rls.h"

#define CONT_COPYDB(buf, dest, source)\
	do{ \
	dest.s= (char*)buf+ size;\
	memcpy(dest.s, source, strlen(source));\
	dest.len= strlen(source);\
	size+= strlen(source); \
	} while(0);

/* database connection */
extern db1_con_t *rls_db;
extern db_func_t rls_dbf;

extern void update_a_sub(subs_t *subs_copy );

/******************************************************************************/

shtable_t rls_new_shtable(int hash_size)
{
  LM_ERR( "rls_new_shtable shouldn't be called in RLS_DB_ONLY mode\n" );
  return(NULL);
}

/******************************************************************************/

void rls_destroy_shtable(shtable_t htable, int hash_size)
{
  LM_ERR( "rls_destroy_shtable shouldn't be called in RLS_DB_ONLY mode\n" );
}

/******************************************************************************/

int rls_insert_shtable(shtable_t htable,unsigned int hash_code, subs_t* subs)
{
  LM_ERR( "rls_insert_shtable shouldn't be called in RLS_DB_ONLY mode\n" );
  return(-1);
}

/******************************************************************************/

subs_t* rls_search_shtable(shtable_t htable,str callid,str to_tag,
		str from_tag,unsigned int hash_code)
{
  LM_ERR( "rls_search_shtable shouldn't be called in RLS_DB_ONLY mode\n" );
  return(NULL);
}

/******************************************************************************/

int rls_delete_shtable(shtable_t htable,unsigned int hash_code,str to_tag)
{
  LM_ERR( "rls_delete_shtable shouldn't be called in RLS_DB_ONLY mode\n" );
  return(-1);
}

/******************************************************************************/

int rls_update_shtable(shtable_t htable,unsigned int hash_code, 
		subs_t* subs, int type)
{
  LM_ERR( "rls_update_shtable shouldn't be called in RLS_DB_ONLY mode\n" );
  return(-1);
}

/******************************************************************************/

void rls_update_db_subs_timer(db1_con_t *db,db_func_t dbf, shtable_t hash_table,
	int htable_size, int no_lock, handle_expired_func_t handle_expired_func)
{
  LM_ERR( "rls_update_db_subs_timer shouldn't be called in RLS_DB_ONLY mode\n" );
}

/******************************************************************************/
/******************************************************************************/

int delete_expired_subs_rlsdb( void )

{
	db_key_t query_cols[1];
	db_val_t query_vals[1];
	db_op_t query_ops[1];

	if(rls_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	query_cols[0]= &str_expires_col;
	query_vals[0].type = DB1_INT;
	query_vals[0].nul = 0;
	query_vals[0].val.int_val= (int)time(NULL) - rls_expires_offset;
	query_ops[0]= OP_LT;

	if (rls_dbf.delete(rls_db, query_cols, query_ops, query_vals, 1) < 0)
	{
		LM_ERR("db delete failed for expired subs\n");
		return(-1);
	}

	return(1);
}

/******************************************************************************/

int delete_rlsdb( str *callid, str *to_tag, str *from_tag ) 

{
 	int rval;
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	int n_query_cols = 0;

	if(rls_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	query_cols[n_query_cols] = &str_callid_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *callid;
	n_query_cols++;

	query_cols[n_query_cols] = &str_to_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *to_tag;
	n_query_cols++;

	if (from_tag)
	{
		query_cols[n_query_cols] = &str_from_tag_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val= *from_tag;
		n_query_cols++;
	}

	rval = rls_dbf.delete(rls_db, query_cols, 0, query_vals, n_query_cols);

	if (rval < 0)
	{
		LM_ERR("Can't delete in db\n");
		return(-1);
	}

	return(1);
}

/******************************************************************************/

int update_dialog_notify_rlsdb(subs_t *subs)

{
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t data_cols[3];
	db_val_t data_vals[3];
	int n_query_cols = 0, n_data_cols=0;

	if (subs==NULL) return(-1);

	if(rls_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}

	subs->local_cseq++;
	subs->version++;
	
	query_cols[n_query_cols] = &str_callid_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->callid;
	n_query_cols++;

	query_cols[n_query_cols] = &str_to_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_from_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->from_tag;
	n_query_cols++;

	data_cols[n_data_cols]=&str_local_cseq_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val= subs->local_cseq;
	n_data_cols++;

	data_cols[n_data_cols]=&str_version_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val= subs->version;
	n_data_cols++;

	data_cols[n_data_cols] =&str_status_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val= subs->status;
	n_data_cols++;

	if(rls_dbf.update(rls_db, query_cols, 0, query_vals,
                    data_cols,data_vals,n_query_cols,n_data_cols) < 0)
	{
		LM_ERR("Failed update db\n");
		return(-1);
	}

	return(0);
}

/******************************************************************************/

int update_all_subs_rlsdb(str *from_user, str *from_domain, str *evt)
{
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t result_cols[20];
	int n_query_cols = 0, n_result_cols=0;
	int r_pres_uri_col, r_callid_col, r_to_tag_col, r_from_tag_col;
	int r_event_col, r_expires_col;
	db1_res_t *result= NULL;
 	db_val_t *values;
	db_row_t *rows;
	int nr_rows, size, loop;
	subs_t *dest;
	event_t parsed_event;

	if(rls_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	query_cols[n_query_cols] = &str_watcher_username_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *from_user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_watcher_domain_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *from_domain;
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *evt;
	n_query_cols++;

	result_cols[r_pres_uri_col=n_result_cols++] = &str_presentity_uri_col;
	result_cols[r_callid_col=n_result_cols++] = &str_callid_col;
	result_cols[r_to_tag_col=n_result_cols++] = &str_to_tag_col;
	result_cols[r_from_tag_col=n_result_cols++] = &str_from_tag_col;
	result_cols[r_event_col=n_result_cols++] = &str_event_col;
	result_cols[r_expires_col=n_result_cols++] = &str_expires_col;

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols, 
				n_query_cols, n_result_cols, 0, &result )< 0)
	{
		LM_ERR("Can't query db\n");
		if(result) rls_dbf.free_result(rls_db, result);
		return(-1);
	}

	if(result == NULL) return(-1);

	nr_rows = RES_ROW_N(result);

	/* get the results and fill in return data structure */
	for (loop=0; loop <nr_rows; loop++)
	{
		rows = RES_ROWS(result);
		values = ROW_VALUES(rows);

		size= sizeof(subs_t) +
			( strlen(VAL_STRING(values+r_pres_uri_col))
			+ strlen(VAL_STRING(values+r_to_tag_col))
			+ strlen(VAL_STRING(values+r_from_tag_col))
			+ strlen(VAL_STRING(values+r_callid_col)))*sizeof(char);

		dest= (subs_t*)pkg_malloc(size);
	
		if(dest== NULL)
		{
			LM_ERR( "Can't allocate memory\n" );
			rls_dbf.free_result(rls_db, result);
			return(-1);
		}
		memset(dest, 0, size);
		size= sizeof(subs_t);

		CONT_COPYDB(dest, dest->pres_uri, VAL_STRING(values+r_pres_uri_col))
		CONT_COPYDB(dest, dest->to_tag, VAL_STRING(values+r_to_tag_col))
		CONT_COPYDB(dest, dest->from_tag, VAL_STRING(values+r_from_tag_col))
		CONT_COPYDB(dest, dest->callid, VAL_STRING(values+r_callid_col))

		dest->event = pres_contains_event(evt, &parsed_event);
		if(dest->event == NULL)
		{
			LM_ERR("event not found and set to NULL\n");
		}

		dest->expires= VAL_INT(values+r_expires_col);
		dest->from_user.s= from_user->s;
		dest->from_user.len= from_user->len;
		dest->from_domain.s= from_domain->s;
		dest->from_domain.len= from_domain->len;

		update_a_sub(dest);
	}

	rls_dbf.free_result(rls_db, result);
	return(1);	
}

/******************************************************************************/

int update_dialog_subscribe_rlsdb(subs_t *subs)

{
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t data_cols[2];
	db_val_t data_vals[2];
	int n_query_cols = 0, n_data_cols=0;

	if (subs==NULL) return(-1);

	if(rls_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	query_cols[n_query_cols] = &str_callid_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->callid;
	n_query_cols++;

	query_cols[n_query_cols] = &str_to_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_from_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->from_tag;
	n_query_cols++;

	data_cols[n_data_cols] = &str_expires_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val = subs->expires + (int)time(NULL);
	n_data_cols++;

	data_cols[n_data_cols] = &str_remote_cseq_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val= subs->remote_cseq;
	n_data_cols++;

	if(rls_dbf.update(rls_db, query_cols, 0, query_vals,
                    data_cols,data_vals,n_query_cols,n_data_cols) < 0)
	{
		LM_ERR("Failed update db\n");
		return(-1);
	}
	
	return(0);
}

/******************************************************************************/

int insert_rlsdb( subs_t *s )

{
	db_key_t data_cols[20];
	db_val_t data_vals[20];
	int n_data_cols = 0;

	if (s==NULL) return(-1);

	if(rls_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	data_cols[n_data_cols] = &str_presentity_uri_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val= s->pres_uri;
	n_data_cols++;
	
	data_cols[n_data_cols] = &str_callid_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val= s->callid;
	n_data_cols++;

	data_cols[n_data_cols] = &str_to_tag_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val= s->to_tag;
	n_data_cols++;

	data_cols[n_data_cols] = &str_from_tag_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val= s->from_tag;
	n_data_cols++;

	data_cols[n_data_cols] = &str_to_user_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val = s->to_user;
	n_data_cols++;

	data_cols[n_data_cols] = &str_to_domain_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val = s->to_domain;
	n_data_cols++;
	
	data_cols[n_data_cols] = &str_watcher_username_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val = s->from_user;
	n_data_cols++;

	data_cols[n_data_cols] = &str_watcher_domain_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val = s->from_domain;
	n_data_cols++;

	data_cols[n_data_cols] = &str_event_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val = s->event->name;
	n_data_cols++;	

	data_cols[n_data_cols] = &str_event_id_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val = s->event_id;
	n_data_cols++;

	data_cols[n_data_cols]=&str_local_cseq_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val= s->local_cseq;
	n_data_cols++;

	data_cols[n_data_cols]=&str_remote_cseq_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val= s->remote_cseq;
	n_data_cols++;

	data_cols[n_data_cols] =&str_expires_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val = s->expires + (int)time(NULL);
	n_data_cols++;

	data_cols[n_data_cols] =&str_status_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val= s->status;
	n_data_cols++;

	data_cols[n_data_cols] =&str_reason_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val= s->reason;
	n_data_cols++;

	data_cols[n_data_cols] =&str_record_route_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val = s->record_route;
	n_data_cols++;
	
	data_cols[n_data_cols] =&str_contact_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val = s->contact;
	n_data_cols++;

	data_cols[n_data_cols] =&str_local_contact_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val = s->local_contact;
	n_data_cols++;

	data_cols[n_data_cols] =&str_socket_info_col;
	data_vals[n_data_cols].type = DB1_STR;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.str_val= s->sockinfo_str;
	n_data_cols++;

	data_cols[n_data_cols]=&str_version_col;
	data_vals[n_data_cols].type = DB1_INT;
	data_vals[n_data_cols].nul = 0;
	data_vals[n_data_cols].val.int_val= s->version;
	n_data_cols++;
	
	if(rls_dbf.insert(rls_db, data_cols, data_vals, n_data_cols) < 0)
	{
		LM_ERR("db insert failed\n");
		return(-1);
	}

	return(0);
}

/******************************************************************************/

int get_dialog_subscribe_rlsdb(subs_t *subs) 

{
 	db1_res_t *result= NULL;
	db_key_t query_cols[3];
	db_val_t query_vals[3], *values;
	db_key_t result_cols[5];
	db_row_t *rows;
	int n_query_cols = 0, n_result_cols = 0;
	int nr_rows;
	int r_remote_cseq, r_local_cseq, r_version;
	char *r_pres_uri, *r_record_route;

	if(rls_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if (subs == NULL)
	{
		LM_ERR("null subscriptions\n");
		return(-1);
	}

	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	query_cols[n_query_cols] = &str_callid_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->callid;
	n_query_cols++;

	query_cols[n_query_cols] = &str_to_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_from_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= subs->from_tag;
	n_query_cols++;
	
	result_cols[n_result_cols++] = &str_presentity_uri_col;
	result_cols[n_result_cols++] = &str_remote_cseq_col;
	result_cols[n_result_cols++] = &str_local_cseq_col;
	result_cols[n_result_cols++] = &str_version_col;
	result_cols[n_result_cols++] = &str_record_route_col;

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols, 
			n_query_cols, n_result_cols, 0, &result )< 0)
	{
		LM_ERR("Can't query db\n");
		if(result) rls_dbf.free_result(rls_db, result);
		return(-1);
	}

	if(result == NULL) return(-1);

	nr_rows = RES_ROW_N(result);

	if (nr_rows == 0)
	{
		/* no match - this error might not be for RLS */ 
		LM_INFO( "update_subs_rlsdb: NO MATCH\n" );
		rls_dbf.free_result(rls_db, result);
		return(-1);
	}

	if (nr_rows != 1)
	{
		LM_ERR( "update_subs_rlsdb: TOO MANY MATCHES=%d\n", nr_rows);
		rls_dbf.free_result(rls_db, result);
		return(-1);
	}

	/* get the results and fill in return data structure */
	rows = RES_ROWS(result);
	values = ROW_VALUES(rows);

	r_pres_uri = (char *)VAL_STRING(values+0);
	r_remote_cseq = VAL_INT(values+1);
	r_local_cseq = VAL_INT(values+2);
	r_version = VAL_INT(values+3);
	r_record_route = (char *)VAL_STRING(values+4);

	if(strlen(r_pres_uri) > 0)
	{
		subs->pres_uri.s =
				(char*)pkg_malloc(strlen(r_pres_uri) * sizeof(char));
		if(subs->pres_uri.s==NULL)
		{
			LM_ERR( "Out of Memory\n" );
 			pkg_free(subs->pres_uri.s);
			rls_dbf.free_result(rls_db, result);
			return(-1);
		}
		memcpy(subs->pres_uri.s, 
			r_pres_uri, strlen(r_pres_uri));
		subs->pres_uri.len= strlen(r_pres_uri);
	}

	if ( r_remote_cseq >= subs->remote_cseq)
	{
		LM_DBG("stored cseq= %d\n", r_remote_cseq);
		rls_dbf.free_result(rls_db, result);
		return(401); /*stale cseq code */
	}

	if(strlen(r_record_route) > 0)
	{
		subs->record_route.s =
				(char*)pkg_malloc(strlen(r_record_route) * sizeof(char));
		if(subs->record_route.s==NULL)
		{
			LM_ERR( "Out of Memory\n" );
 			pkg_free(subs->record_route.s);
			rls_dbf.free_result(rls_db, result);
			return(-1);
		}
		memcpy(subs->record_route.s, 
			r_record_route, strlen(r_record_route));
		subs->record_route.len= strlen(r_record_route);
	}

	subs->local_cseq= r_local_cseq;
	subs->version= r_version;

	rls_dbf.free_result(rls_db, result);
	return 1;
}

/******************************************************************************/

subs_t *get_dialog_notify_rlsdb(str callid, str to_tag, str from_tag) 
{
 	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t result_cols[20];
	int n_query_cols = 0, n_result_cols=0;
	int r_pres_uri_col,r_to_user_col,r_to_domain_col;
	int r_from_user_col,r_from_domain_col,r_callid_col;
	int r_to_tag_col,r_from_tag_col,r_socket_info_col;
	int r_event_id_col,r_local_contact_col,r_contact_col;
	int r_record_route_col, r_reason_col;
	int r_event_col, r_local_cseq_col, r_remote_cseq_col;
	int r_status_col, r_version_col;
	int r_expires_col;
	db1_res_t *result= NULL;
 	db_val_t *values;
	db_row_t *rows;
	int nr_rows, size;
	subs_t *dest;
	event_t parsed_event;
	str ev_sname;

	if(rls_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(NULL);
	}

	if(rls_dbf.use_table(rls_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(NULL);
	}
	
	query_cols[n_query_cols] = &str_callid_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= callid;
	n_query_cols++;

	query_cols[n_query_cols] = &str_to_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_from_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= from_tag;
	n_query_cols++;
	
	result_cols[r_pres_uri_col=n_result_cols++] = &str_presentity_uri_col;
	result_cols[r_to_user_col=n_result_cols++] = &str_to_user_col;
	result_cols[r_to_domain_col=n_result_cols++] = &str_to_domain_col;
	result_cols[r_from_user_col=n_result_cols++] = &str_watcher_username_col;
	result_cols[r_from_domain_col=n_result_cols++] = &str_watcher_domain_col;
	result_cols[r_callid_col=n_result_cols++] = &str_callid_col;
	result_cols[r_to_tag_col=n_result_cols++] = &str_to_tag_col;
	result_cols[r_from_tag_col=n_result_cols++] = &str_from_tag_col;
	result_cols[r_socket_info_col=n_result_cols++] = &str_socket_info_col;
	result_cols[r_event_id_col=n_result_cols++] = &str_event_id_col;
	result_cols[r_local_contact_col=n_result_cols++] = &str_local_contact_col;
	result_cols[r_contact_col=n_result_cols++] = &str_contact_col;
	result_cols[r_record_route_col=n_result_cols++] = &str_record_route_col;
	result_cols[r_reason_col=n_result_cols++] = &str_reason_col;
	result_cols[r_event_col=n_result_cols++] = &str_event_col;
	result_cols[r_local_cseq_col=n_result_cols++] = &str_local_cseq_col;
	result_cols[r_remote_cseq_col=n_result_cols++] = &str_remote_cseq_col;
	result_cols[r_status_col=n_result_cols++] = &str_status_col;
	result_cols[r_version_col=n_result_cols++] = &str_version_col;
	result_cols[r_expires_col=n_result_cols++] = &str_expires_col;

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols, 
				n_query_cols, n_result_cols, 0, &result )< 0)
	{
		LM_ERR("Can't query db\n");
		if(result) rls_dbf.free_result(rls_db, result);
		return(NULL);
	}

	if(result == NULL) return(NULL);

	nr_rows = RES_ROW_N(result);

	if (nr_rows == 0)
	{
		/* no match */ 
		LM_INFO( "get_dialog_rlsdb No matching records\n" );
		rls_dbf.free_result(rls_db, result);
		return(NULL);
	}

	if (nr_rows != 1)
	{
		LM_ERR( "get_dialog_rlsdb multiple matching records\n" );
		rls_dbf.free_result(rls_db, result);
		return(NULL);
	}

	/* get the results and fill in return data structure */
	rows = RES_ROWS(result);
	values = ROW_VALUES(rows);

	size= sizeof(subs_t) +
		( strlen(VAL_STRING(values+r_pres_uri_col))
		+ strlen(VAL_STRING(values+r_to_user_col))
		+ strlen(VAL_STRING(values+r_to_domain_col))
		+ strlen(VAL_STRING(values+r_from_user_col))
		+ strlen(VAL_STRING(values+r_from_domain_col))
		+ strlen(VAL_STRING(values+r_to_tag_col))
		+ strlen(VAL_STRING(values+r_from_tag_col))
		+ strlen(VAL_STRING(values+r_callid_col))
		+ strlen(VAL_STRING(values+r_socket_info_col))
		+ strlen(VAL_STRING(values+r_local_contact_col))
		+ strlen(VAL_STRING(values+r_contact_col))
		+ strlen(VAL_STRING(values+r_record_route_col))
		+ strlen(VAL_STRING(values+r_event_id_col))
		+ strlen(VAL_STRING(values+r_reason_col)))*sizeof(char);

	dest= (subs_t*)pkg_malloc(size);
	
	if(dest== NULL)
	{
		LM_ERR( "Can't allocate memory\n" );
		rls_dbf.free_result(rls_db, result);
		return(NULL);
	}
	memset(dest, 0, size);
	size= sizeof(subs_t);

	CONT_COPYDB(dest, dest->pres_uri, VAL_STRING(values+r_pres_uri_col))
	CONT_COPYDB(dest, dest->to_user, VAL_STRING(values+r_to_user_col))
	CONT_COPYDB(dest, dest->to_domain, VAL_STRING(values+r_to_domain_col))
	CONT_COPYDB(dest, dest->from_user, VAL_STRING(values+r_from_user_col))
	CONT_COPYDB(dest, dest->from_domain, VAL_STRING(values+r_from_domain_col))
	CONT_COPYDB(dest, dest->to_tag, VAL_STRING(values+r_to_tag_col))
	CONT_COPYDB(dest, dest->from_tag, VAL_STRING(values+r_from_tag_col))
	CONT_COPYDB(dest, dest->callid, VAL_STRING(values+r_callid_col))
	CONT_COPYDB(dest, dest->sockinfo_str, VAL_STRING(values+r_socket_info_col))
	CONT_COPYDB(dest, dest->local_contact, VAL_STRING(values+r_local_contact_col))
	CONT_COPYDB(dest, dest->contact, VAL_STRING(values+r_contact_col))
	CONT_COPYDB(dest, dest->record_route, VAL_STRING(values+r_record_route_col))

	if(strlen(VAL_STRING(values+r_event_id_col)) > 0)
		CONT_COPYDB(dest, dest->event_id, VAL_STRING(values+r_event_id_col))

	if(strlen(VAL_STRING(values+r_reason_col)) > 0)
		CONT_COPYDB(dest, dest->reason, VAL_STRING(values+r_reason_col))

	ev_sname.s= (char *)VAL_STRING(values+r_event_col);
	ev_sname.len= strlen(ev_sname.s);

	dest->event = pres_contains_event(&ev_sname, &parsed_event);

	if(dest->event == NULL)
		LM_ERR("event not found and set to NULL\n");

	dest->local_cseq= VAL_INT(values+r_local_cseq_col);
	dest->remote_cseq= VAL_INT(values+r_remote_cseq_col);
	dest->status= VAL_INT(values+r_status_col);
	dest->version= VAL_INT(values+r_version_col);
	dest->expires= VAL_INT(values+r_expires_col);

	rls_dbf.free_result(rls_db, result);

	return(dest);
}

/******************************************************************************/


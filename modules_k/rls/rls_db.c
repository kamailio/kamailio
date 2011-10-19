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
extern db1_con_t *rls2_db;
extern db_func_t rls2_dbf;

extern void update_a_sub(subs_t *subs_copy );

static int rlsdb_debug=0;

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

void rls_update_db_subs(db1_con_t *db,db_func_t dbf, shtable_t hash_table,
	int htable_size, int no_lock, handle_expired_func_t handle_expired_func)
{
  LM_ERR( "rls_update_db_subs shouldn't be called in RLS_DB_ONLY mode\n" );
}

/******************************************************************************/
/******************************************************************************/

int delete_expired_subs_rlsdb( void )

{
	db_key_t query_cols[1];
	db_val_t query_vals[1];
	db_op_t query_ops[1];
	int n_query_cols = 0;
	int expires_col, rval;

	if (rlsdb_debug) LM_ERR( "delete_expired_subs_rlsdb\n" );

	if(rls2_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls2_dbf.use_table(rls2_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	query_cols[expires_col= n_query_cols]= &str_expires_col;
	query_vals[expires_col].type = DB1_INT;
	query_vals[expires_col].nul = 0;
	query_vals[expires_col].val.int_val= (int)time(NULL) - rls_expires_offset;
	query_ops[expires_col]= OP_LT;
	n_query_cols++;

	rval=rls2_dbf.delete(rls2_db, query_cols, query_ops, query_vals, n_query_cols);

	if (rval < 0)
	{
		LM_ERR( "db delete failed for expired subs\n" );
	}
/*	else
	{
		LM_ERR( "Done\n" );
	}*/

	return(rval);
}

/******************************************************************************/

int delete_rlsdb( str *callid, str *to_tag, str *from_tag ) 

{
 	int rval;
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	int n_query_cols = 0;
	int callid_col, totag_col, fromtag_col;

	if (rlsdb_debug) 
		LM_ERR( "delete_rlsdb callid=%.*s \nto_tag=%.*s \nfrom_tag=%.*s\n",
		callid?callid->len:0, callid?callid->s:"",
		to_tag?to_tag->len:0, to_tag?to_tag->s:"",
		from_tag?from_tag->len:0, from_tag?from_tag->s:"" ); 

	if(rls2_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls2_dbf.use_table(rls2_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	if (callid)
	{
		query_cols[callid_col= n_query_cols] = &str_callid_col;
		query_vals[callid_col].type = DB1_STR;
		query_vals[callid_col].nul = 0;
		query_vals[callid_col].val.str_val= *callid;
		n_query_cols++;
	}

	if (to_tag)
	{
		query_cols[totag_col= n_query_cols] = &str_to_tag_col;
		query_vals[totag_col].type = DB1_STR;
		query_vals[totag_col].nul = 0;
		query_vals[totag_col].val.str_val= *to_tag;
		n_query_cols++;
	}

	if (from_tag)
	{
		query_cols[fromtag_col= n_query_cols] = &str_from_tag_col;
		query_vals[fromtag_col].type = DB1_STR;
		query_vals[fromtag_col].nul = 0;
		query_vals[fromtag_col].val.str_val= *from_tag;
		n_query_cols++;
	}

	rval = rls2_dbf.delete(rls2_db, query_cols, 0, query_vals, n_query_cols);

	if (rval < 0)
	{
		LM_ERR("Can't delete in db\n");
		return(-1);
	}

	if (rlsdb_debug) LM_ERR( "done\n" );
	return(1);
}

/******************************************************************************/

int update_rlsdb( subs_t *subs, int type)

{
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t data_cols[22];
	db_val_t data_vals[22];
	int n_query_cols = 0, n_data_cols=0;
	int callid_col, totag_col, fromtag_col,status_col, event_id_col;
	int local_cseq_col, remote_cseq_col, expires_col;
	int contact_col,version_col;
		
	
	if (rlsdb_debug) 
		LM_ERR( "update_rlsdb callid=%.*s \nto_tag=%.*s \nfrom_tag=%.*s \ntype=%d\n",
		subs->callid.len, subs->callid.s,
		subs->to_tag.len, subs->to_tag.s,
		subs->from_tag.len, subs->from_tag.s, 
		type );

	if (subs==NULL) return(-1);

	if(rls2_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls2_dbf.use_table(rls2_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	query_cols[callid_col= n_query_cols] = &str_callid_col;
	query_vals[callid_col].type = DB1_STR;
	query_vals[callid_col].nul = 0;
	query_vals[callid_col].val.str_val= subs->callid;
	n_query_cols++;

	query_cols[totag_col= n_query_cols] = &str_to_tag_col;
	query_vals[totag_col].type = DB1_STR;
	query_vals[totag_col].nul = 0;
	query_vals[totag_col].val.str_val= subs->to_tag;
	n_query_cols++;

	query_cols[fromtag_col= n_query_cols] = &str_from_tag_col;
	query_vals[fromtag_col].type = DB1_STR;
	query_vals[fromtag_col].nul = 0;
	query_vals[fromtag_col].val.str_val= subs->from_tag;
	n_query_cols++;

	if(type & REMOTE_TYPE)
	{
		data_cols[expires_col= n_data_cols] =&str_expires_col;
		data_vals[expires_col].type = DB1_INT;
		data_vals[expires_col].nul = 0;
		data_vals[expires_col].val.int_val = subs->expires + (int)time(NULL);
		n_data_cols++;

		data_cols[remote_cseq_col= n_data_cols]=&str_remote_cseq_col;
		data_vals[remote_cseq_col].type = DB1_INT;
		data_vals[remote_cseq_col].nul = 0;
		data_vals[remote_cseq_col].val.int_val= subs->remote_cseq;
		n_data_cols++;
	}
	else
	{
		int retrieved_local_cseq=0;
		db_key_t result_cols[1] = {&str_local_cseq_col};
		db1_res_t *result= NULL;
 		db_val_t *values;
		db_row_t *rows;
		int nr_rows;

	
		if(rls2_dbf.query(rls2_db, query_cols, 0, query_vals, result_cols, 
				n_query_cols, 1, 0, &result )< 0)
		{
			LM_ERR("Can't query db\n");
			if(result) rls2_dbf.free_result(rls2_db, result);
			return(-1);
		}

		if(result == NULL) return(-1);

		nr_rows = RES_ROW_N(result);

		if (nr_rows == 0)
		{
			/* no match */ 
			LM_ERR( "update_rlsdb: NO MATCH\n" );
			rls2_dbf.free_result(rls2_db, result);
			return(-1);
		}

		if (nr_rows != 1)
		{
			LM_ERR( "update_rlsdb: TOO MANY MATCHES=%d\n", nr_rows);
			rls2_dbf.free_result(rls2_db, result);
			return(-1);
		}


		/* get the results and fill in return data structure */
		rows = RES_ROWS(result);
		values = ROW_VALUES(rows);

		retrieved_local_cseq = VAL_INT(values);
		rls2_dbf.free_result(rls2_db, result);

		subs->local_cseq = ++retrieved_local_cseq;
		data_cols[local_cseq_col= n_data_cols]=&str_local_cseq_col;
		data_vals[local_cseq_col].type = DB1_INT;
		data_vals[local_cseq_col].nul = 0;
		data_vals[local_cseq_col].val.int_val= subs->local_cseq;
		n_data_cols++;

		subs->version++;
		data_cols[version_col= n_data_cols]=&str_version_col;
		data_vals[version_col].type = DB1_INT;
		data_vals[version_col].nul = 0;
		data_vals[version_col].val.int_val= subs->version;
		n_data_cols++;
	}
	

	data_cols[contact_col= n_data_cols] =&str_contact_col;
	data_vals[contact_col].type = DB1_STR;
	data_vals[contact_col].nul = 0;
	data_vals[contact_col].val.str_val = subs->contact;
	n_data_cols++;

	data_cols[status_col= n_data_cols] =&str_status_col;
	data_vals[status_col].type = DB1_INT;
	data_vals[status_col].nul = 0;
	data_vals[status_col].val.int_val= subs->status;
	n_data_cols++;

	data_cols[event_id_col= n_data_cols] = &str_event_id_col;
	data_vals[event_id_col].type = DB1_STR;
	data_vals[event_id_col].nul = 0;
	data_vals[event_id_col].val.str_val = subs->event_id;
	n_data_cols++;

	/*if(s->db_flag & NO_UPDATEDB_FLAG)
		s->db_flag= UPDATEDB_FLAG; Note this is a retieve & modify of the flag

	data_cols[db_flag_col= n_data_cols]=&str_db_flag_col;
	data_vals[db_flag_col].type = DB1_INT;
	data_vals[db_flag_col].nul = 0;
	data_vals[db_flag_col].val.int_val= subs->db_flag;
	n_data_cols++;*/

	if(rls2_dbf.update(rls2_db, query_cols, 0, query_vals,
                    data_cols,data_vals,n_query_cols,n_data_cols) < 0)
	{
		LM_ERR("Failed update db\n");
		return(-1);
	}

	if (rlsdb_debug) LM_ERR("Done\n");
	return(0);
}

/******************************************************************************/

int update_all_subs_rlsdb( str *from_user, str *from_domain, str *evt )

{
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t result_cols[22];
	int n_query_cols = 0, n_result_cols=0;
	int callid_col, totag_col, fromtag_col;	
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
	int nr_rows, size, loop;
	subs_t *dest;
	event_t parsed_event;
	str ev_sname;


	if (rlsdb_debug)
		LM_ERR( "update_all_subs_rlsdb from_user=%.*s \nfrom_domain=%.*s evt=%.*s\n",
		from_user?from_user->len:0, from_user?from_user->s:"",
		from_domain?from_domain->len:0, from_domain?from_domain->s:"",
		evt?evt->len:0, evt?evt->s:"" );

	if(rls2_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls2_dbf.use_table(rls2_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	query_cols[callid_col= n_query_cols] = &str_watcher_username_col;
	query_vals[callid_col].type = DB1_STR;
	query_vals[callid_col].nul = 0;
	query_vals[callid_col].val.str_val= *from_user;
	n_query_cols++;

	query_cols[totag_col= n_query_cols] = &str_watcher_domain_col;
	query_vals[totag_col].type = DB1_STR;
	query_vals[totag_col].nul = 0;
	query_vals[totag_col].val.str_val= *from_domain;
	n_query_cols++;

	query_cols[fromtag_col= n_query_cols] = &str_event_col;
	query_vals[fromtag_col].type = DB1_STR;
	query_vals[fromtag_col].nul = 0;
	query_vals[fromtag_col].val.str_val= *evt;
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
	/*result_cols[r_send_on_cback_col=n_result_cols++] = &str_send_on_cback_col;*/
	result_cols[r_expires_col=n_result_cols++] = &str_expires_col;


	if(rls2_dbf.query(rls2_db, query_cols, 0, query_vals, result_cols, 
				n_query_cols, n_result_cols, 0, &result )< 0)
	{
		LM_ERR("Can't query db\n");
		if(result) rls2_dbf.free_result(rls2_db, result);
		return(-1);
	}

	if(result == NULL) return(-1);

	nr_rows = RES_ROW_N(result);

	if (nr_rows == 0)
	{
		/* no match */ 
		LM_ERR( "update_all_subs_rlsdb: NO MATCH\n" );
		rls2_dbf.free_result(rls2_db, result);
		return(-1);
	}

	if (nr_rows != 1)
	{
		LM_ERR( "update_all_subs_rlsdb: TOO MANY MATCHES=%d\n", nr_rows);
		rls2_dbf.free_result(rls2_db, result);
		return(-1);
	}

	/* get the results and fill in return data structure */
	for (loop=0; loop <nr_rows; loop++)
	{
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
			+ strlen(VAL_STRING(values+r_reason_col))+ 1)*sizeof(char);

		dest= (subs_t*)pkg_malloc(size);
	
		if(dest== NULL)
		{
			LM_ERR( "Can't allocate memory\n" );
			/* tidy up and return >>>>>>>>>>>>>>>>>>>> */
			rls2_dbf.free_result(rls2_db, result);
			return(-1);
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
		{
			LM_ERR("event not found and set to NULL\n");
		}

		dest->local_cseq= VAL_INT(values+r_local_cseq_col);
		dest->remote_cseq= VAL_INT(values+r_remote_cseq_col);
		dest->status= VAL_INT(values+r_status_col);
		dest->version= VAL_INT(values+r_version_col);
		/*dest->send_on_cback= VAL_INT(values+r_send_on_cback_col);*/
		dest->expires= VAL_INT(values+r_expires_col);

		/*dest->db_flag= VAL_INT(values+r_db_flag_col);*/
		
		update_a_sub(dest);
	}

	rls2_dbf.free_result(rls2_db, result);
	if (rlsdb_debug) LM_ERR( "Done\n" );
	return(1);	
}

/******************************************************************************/

int update_subs_rlsdb( subs_t *subs )

{
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t data_cols[22];
	db_val_t data_vals[22];
	db_key_t result_cols[10];
	int n_query_cols = 0, n_data_cols=0, n_result_cols=0;
	int callid_col, totag_col, fromtag_col;
	int local_cseq_col, expires_col;	
	int r_remote_cseq=0, r_local_cseq=0, r_version=0;
	char *r_record_route, *r_pres_uri;
	db1_res_t *result= NULL;
 	db_val_t *values;
	db_row_t *rows;
	int nr_rows;

	if (rlsdb_debug) LM_ERR( "update_subs_rlsdb\n" );

	if (subs==NULL) return(-1);

	if(rls2_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls2_dbf.use_table(rls2_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	query_cols[callid_col= n_query_cols] = &str_callid_col;
	query_vals[callid_col].type = DB1_STR;
	query_vals[callid_col].nul = 0;
	query_vals[callid_col].val.str_val= subs->callid;
	n_query_cols++;

	query_cols[totag_col= n_query_cols] = &str_to_tag_col;
	query_vals[totag_col].type = DB1_STR;
	query_vals[totag_col].nul = 0;
	query_vals[totag_col].val.str_val= subs->to_tag;
	n_query_cols++;

	query_cols[fromtag_col= n_query_cols] = &str_from_tag_col;
	query_vals[fromtag_col].type = DB1_STR;
	query_vals[fromtag_col].nul = 0;
	query_vals[fromtag_col].val.str_val= subs->from_tag;
	n_query_cols++;

	result_cols[n_result_cols++] = &str_remote_cseq_col;
	result_cols[n_result_cols++] = &str_local_cseq_col;
	result_cols[n_result_cols++] = &str_version_col;
	result_cols[n_result_cols++] = &str_presentity_uri_col;
	result_cols[n_result_cols++] = &str_record_route_col;

	if(rls2_dbf.query(rls2_db, query_cols, 0, query_vals, result_cols, 
				n_query_cols, n_result_cols, 0, &result )< 0)
	{
		LM_ERR("Can't query db\n");
		if(result) rls2_dbf.free_result(rls2_db, result);
		return(-1);
	}

	if(result == NULL) return(-1);

	nr_rows = RES_ROW_N(result);

	if (nr_rows == 0)
	{
		/* no match */ 
		LM_ERR( "update_subs_rlsdb: NO MATCH\n" );
		rls2_dbf.free_result(rls2_db, result);
		return(-1);
	}

	if (nr_rows != 1)
	{
		LM_ERR( "update_subs_rlsdb: TOO MANY MATCHES=%d\n", nr_rows);
		rls2_dbf.free_result(rls2_db, result);
		return(-1);
	}

	/* get the results and fill in return data structure */
	rows = RES_ROWS(result);
	values = ROW_VALUES(rows);

	r_remote_cseq = VAL_INT(values+0);
	r_local_cseq = VAL_INT(values+1);
	r_version = VAL_INT(values+2);
	r_pres_uri = (char *)VAL_STRING(values+3);
	r_record_route = (char *)VAL_STRING(values+4);
	
	data_cols[expires_col= n_data_cols] = &str_expires_col;
	data_vals[expires_col].type = DB1_INT;
	data_vals[expires_col].nul = 0;
	data_vals[expires_col].val.int_val = subs->expires + (int)time(NULL);
	n_data_cols++;

	if ( r_remote_cseq >= subs->remote_cseq)
	{
		LM_DBG("stored cseq= %d\n", r_remote_cseq);
		return(401); /*stale cseq code */
	}

	data_cols[local_cseq_col= n_data_cols] = &str_remote_cseq_col;
	data_vals[local_cseq_col].type = DB1_INT;
	data_vals[local_cseq_col].nul = 0;
	data_vals[local_cseq_col].val.int_val= subs->remote_cseq;
	n_data_cols++;

	subs->pres_uri.s = (char*)pkg_malloc(strlen(r_pres_uri) * sizeof(char));
	if(subs->pres_uri.s== NULL)
	{
		LM_ERR( "Out of Memory\n" );
		rls2_dbf.free_result(rls2_db, result);
		return(-1);
	}

	memcpy(subs->pres_uri.s, r_pres_uri, strlen(r_pres_uri));
	subs->pres_uri.len= strlen(r_pres_uri);

	if(strlen(r_record_route) > 0)
	{
		subs->record_route.s =
				(char*)pkg_malloc(strlen(r_record_route) * sizeof(char));
		if(subs->record_route.s==NULL)
		{
			LM_ERR( "Out of Memory\n" );
 			pkg_free(subs->pres_uri.s);
			rls2_dbf.free_result(rls2_db, result);
			return(-1);
		}
		memcpy(subs->record_route.s, 
			r_record_route, strlen(r_record_route));
		subs->record_route.len= strlen(r_record_route);
	}

	subs->local_cseq= r_local_cseq;
	subs->version= r_version;

	rls2_dbf.free_result(rls2_db, result);


	/*if(s->db_flag & NO_UPDATEDB_FLAG)
		s->db_flag= UPDATEDB_FLAG; Note this is a retieve & modify of the flag

	data_cols[db_flag_col= n_data_cols]=&str_db_flag_col;
	data_vals[db_flag_col].type = DB1_INT;
	data_vals[db_flag_col].nul = 0;
	data_vals[db_flag_col].val.int_val= s->db_flag;
	n_data_cols++;*/

	if(rls2_dbf.update(rls2_db, query_cols, 0, query_vals,
                    data_cols,data_vals,n_query_cols,n_data_cols) < 0)
	{
		LM_ERR("Failed update db\n");
		return(-1);
	}
	
	if (rlsdb_debug) LM_ERR("Done\n" );
	return(0);
}

/******************************************************************************/

int insert_rlsdb( subs_t *s )

{
	db_key_t data_cols[22];
	db_val_t data_vals[22];
	int n_data_cols = 0;
	int pres_uri_col, to_user_col, to_domain_col, from_user_col, from_domain_col;
	int callid_col, totag_col, fromtag_col, event_col,status_col, event_id_col;
	int local_cseq_col, remote_cseq_col, expires_col, record_route_col;
	int contact_col, local_contact_col, version_col,socket_info_col,reason_col;
		
	if (rlsdb_debug)
		LM_ERR( "insert_rlsdb callid=%.*s \nto_tag=%.*s \nfrom_tag=%.*s\n",
		s->callid.len, s->callid.s,
		s->to_tag.len, s->to_tag.s,
		s->from_tag.len, s->from_tag.s );

	if (s==NULL) return(-1);

	if(rls2_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls2_dbf.use_table(rls2_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	data_cols[pres_uri_col= n_data_cols] = &str_presentity_uri_col;
	data_vals[pres_uri_col].type = DB1_STR;
	data_vals[pres_uri_col].nul = 0;
	data_vals[pres_uri_col].val.str_val= s->pres_uri;
	n_data_cols++;
	
	data_cols[callid_col= n_data_cols] = &str_callid_col;
	data_vals[callid_col].type = DB1_STR;
	data_vals[callid_col].nul = 0;
	data_vals[callid_col].val.str_val= s->callid;
	n_data_cols++;

	data_cols[totag_col= n_data_cols] = &str_to_tag_col;
	data_vals[totag_col].type = DB1_STR;
	data_vals[totag_col].nul = 0;
	data_vals[totag_col].val.str_val= s->to_tag;
	n_data_cols++;

	data_cols[fromtag_col= n_data_cols] = &str_from_tag_col;
	data_vals[fromtag_col].type = DB1_STR;
	data_vals[fromtag_col].nul = 0;
	data_vals[fromtag_col].val.str_val= s->from_tag;
	n_data_cols++;

	data_cols[to_user_col= n_data_cols] = &str_to_user_col;
	data_vals[to_user_col].type = DB1_STR;
	data_vals[to_user_col].nul = 0;
	data_vals[to_user_col].val.str_val = s->to_user;
	n_data_cols++;

	data_cols[to_domain_col= n_data_cols] = &str_to_domain_col;
	data_vals[to_domain_col].type = DB1_STR;
	data_vals[to_domain_col].nul = 0;
	data_vals[to_domain_col].val.str_val = s->to_domain;
	n_data_cols++;
	
	data_cols[from_user_col= n_data_cols] = &str_watcher_username_col;
	data_vals[from_user_col].type = DB1_STR;
	data_vals[from_user_col].nul = 0;
	data_vals[from_user_col].val.str_val = s->from_user;
	n_data_cols++;

	data_cols[from_domain_col= n_data_cols] = &str_watcher_domain_col;
	data_vals[from_domain_col].type = DB1_STR;
	data_vals[from_domain_col].nul = 0;
	data_vals[from_domain_col].val.str_val = s->from_domain;
	n_data_cols++;

	data_cols[event_col= n_data_cols] = &str_event_col;
	data_vals[event_col].type = DB1_STR;
	data_vals[event_col].nul = 0;
	data_vals[event_col].val.str_val = s->event->name;
	n_data_cols++;	

	data_cols[event_id_col= n_data_cols] = &str_event_id_col;
	data_vals[event_id_col].type = DB1_STR;
	data_vals[event_id_col].nul = 0;
	data_vals[event_id_col].val.str_val = s->event_id;
	n_data_cols++;

	data_cols[local_cseq_col= n_data_cols]=&str_local_cseq_col;
	data_vals[local_cseq_col].type = DB1_INT;
	data_vals[local_cseq_col].nul = 0;
	data_vals[local_cseq_col].val.int_val= s->local_cseq;
	n_data_cols++;

	data_cols[remote_cseq_col= n_data_cols]=&str_remote_cseq_col;
	data_vals[remote_cseq_col].type = DB1_INT;
	data_vals[remote_cseq_col].nul = 0;
	data_vals[remote_cseq_col].val.int_val= s->remote_cseq;
	n_data_cols++;

	data_cols[expires_col= n_data_cols] =&str_expires_col;
	data_vals[expires_col].type = DB1_INT;
	data_vals[expires_col].nul = 0;
	data_vals[expires_col].val.int_val = s->expires + (int)time(NULL);
	n_data_cols++;

	data_cols[status_col= n_data_cols] =&str_status_col;
	data_vals[status_col].type = DB1_INT;
	data_vals[status_col].nul = 0;
	data_vals[status_col].val.int_val= s->status;
	n_data_cols++;

	data_cols[reason_col= n_data_cols] =&str_reason_col;
	data_vals[reason_col].type = DB1_STR;
	data_vals[reason_col].nul = 0;
	data_vals[reason_col].val.str_val= s->reason;
	n_data_cols++;

	data_cols[record_route_col= n_data_cols] =&str_record_route_col;
	data_vals[record_route_col].type = DB1_STR;
	data_vals[record_route_col].nul = 0;
	data_vals[record_route_col].val.str_val = s->record_route;
	n_data_cols++;
	
	data_cols[contact_col= n_data_cols] =&str_contact_col;
	data_vals[contact_col].type = DB1_STR;
	data_vals[contact_col].nul = 0;
	data_vals[contact_col].val.str_val = s->contact;
	n_data_cols++;

	data_cols[local_contact_col= n_data_cols] =&str_local_contact_col;
	data_vals[local_contact_col].type = DB1_STR;
	data_vals[local_contact_col].nul = 0;
	data_vals[local_contact_col].val.str_val = s->local_contact;
	n_data_cols++;

	data_cols[socket_info_col= n_data_cols] =&str_socket_info_col;
	data_vals[socket_info_col].type = DB1_STR;
	data_vals[socket_info_col].nul = 0;
	data_vals[socket_info_col].val.str_val= s->sockinfo_str;
	n_data_cols++;

	data_cols[version_col= n_data_cols]=&str_version_col;
	data_vals[version_col].type = DB1_INT;
	data_vals[version_col].nul = 0;
	data_vals[version_col].val.int_val= s->version;
	n_data_cols++;
	
	/*data_cols[send_on_cback_col= n_data_cols]=&str_send_on_cback_col;
	data_vals[send_on_cback_col].type = DB1_INT;
	data_vals[send_on_cback_col].nul = 0;
	data_vals[send_on_cback_col].val.int_val= s->send_on_cback;
	n_data_cols++;

	data_cols[db_flag_col= n_data_cols]=&str_db_flag_col;
	data_vals[db_flag_col].type = DB1_INT;
	data_vals[db_flag_col].nul = 0;
	data_vals[db_flag_col].val.int_val= s->db_flag;
	n_data_cols++;*/

	if(rls2_dbf.insert(rls2_db, data_cols, data_vals, n_data_cols) < 0)
	{
		LM_ERR("db insert failed\n");
		return(-1);
	}

	if (rlsdb_debug) LM_ERR( "Done\n" );
	return(0);
}

/******************************************************************************/

int matches_in_rlsdb( str callid, str to_tag, str from_tag ) 

{
 	db1_res_t *result= NULL;
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t result_cols[1];
	int n_query_cols = 0;
	int callid_col, totag_col, fromtag_col;
	int rval;

	if (rlsdb_debug)
		LM_ERR( "matches_in_rlsdb callid=%.*s \nto_tag=%.*s \nfrom_tag=%.*s\n",
		callid.len, callid.s,
		to_tag.len, to_tag.s,
		from_tag.len, from_tag.s );

	if(rls2_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(rls2_dbf.use_table(rls2_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(-1);
	}
	
	query_cols[callid_col= n_query_cols] = &str_callid_col;
	query_vals[callid_col].type = DB1_STR;
	query_vals[callid_col].nul = 0;
	query_vals[callid_col].val.str_val= callid;
	n_query_cols++;

	query_cols[totag_col= n_query_cols] = &str_to_tag_col;
	query_vals[totag_col].type = DB1_STR;
	query_vals[totag_col].nul = 0;
	query_vals[totag_col].val.str_val= to_tag;
	n_query_cols++;

	query_cols[fromtag_col= n_query_cols] = &str_from_tag_col;
	query_vals[fromtag_col].type = DB1_STR;
	query_vals[fromtag_col].nul = 0;
	query_vals[fromtag_col].val.str_val= from_tag;
	n_query_cols++;
	
	result_cols[0]= &str_callid_col; /* should use id column instead here */
	
	if(rls2_dbf.query(rls2_db, query_cols, 0, query_vals, result_cols, 
			n_query_cols, 1, 0, &result )< 0)
	{
		LM_ERR("Can't query db\n");
		if(result) rls2_dbf.free_result(rls2_db, result);
		return(-1);
	}

	if(result == NULL) return(-1);

	rval = result->n;
	rls2_dbf.free_result(rls2_db, result);
	if (rlsdb_debug) LM_ERR( "Done matches=%d\n", rval );
	return(rval);
}

/******************************************************************************/

subs_t *get_dialog_rlsdb( str callid, str to_tag, str from_tag ) 

{
 	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t result_cols[22];
	int n_query_cols = 0, n_result_cols=0;
	int callid_col, totag_col, fromtag_col;	
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
	int nr_rows, size, loop;
	subs_t *dest;
	event_t parsed_event;
	str ev_sname;

	if (rlsdb_debug)
		LM_ERR( "get_dialog_rlsdb callid=%.*s \nto_tag=%.*s \nfrom_tag=%.*s\n",
		callid.len, callid.s,
		to_tag.len, to_tag.s,
		from_tag.len, from_tag.s );

	if(rls2_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(NULL);
	}

	if(rls2_dbf.use_table(rls2_db, &rlsubs_table)< 0)
	{
		LM_ERR("use table failed\n");
		return(NULL);
	}
	
	query_cols[callid_col= n_query_cols] = &str_callid_col;
	query_vals[callid_col].type = DB1_STR;
	query_vals[callid_col].nul = 0;
	query_vals[callid_col].val.str_val= callid;
	n_query_cols++;

	query_cols[totag_col= n_query_cols] = &str_to_tag_col;
	query_vals[totag_col].type = DB1_STR;
	query_vals[totag_col].nul = 0;
	query_vals[totag_col].val.str_val= to_tag;
	n_query_cols++;

	query_cols[fromtag_col= n_query_cols] = &str_from_tag_col;
	query_vals[fromtag_col].type = DB1_STR;
	query_vals[fromtag_col].nul = 0;
	query_vals[fromtag_col].val.str_val= from_tag;
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
	/*result_cols[r_send_on_cback_col=n_result_cols++] = &str_send_on_cback_col;*/
	result_cols[r_expires_col=n_result_cols++] = &str_expires_col;


	if(rls2_dbf.query(rls2_db, query_cols, 0, query_vals, result_cols, 
				n_query_cols, n_result_cols, 0, &result )< 0)
	{
		LM_ERR("Can't query db\n");
		if(result) rls2_dbf.free_result(rls2_db, result);
		return(NULL);
	}

	if(result == NULL) return(NULL);

	nr_rows = RES_ROW_N(result);

	if (nr_rows == 0)
	{
		/* no match */ 
		LM_ERR( "get_dialog_rlsb No matching records\n" );
		rls2_dbf.free_result(rls2_db, result);
		return(NULL);
	}

	if (nr_rows != 1)
	{
		LM_ERR( "get_dialog_rlsb multiple matching records\n" );
		rls2_dbf.free_result(rls2_db, result);
		return(NULL);
	}

	/* get the results and fill in return data structure */
	for (loop=0; loop <nr_rows; loop++)
	{
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
			+ strlen(VAL_STRING(values+r_reason_col))+ 1)*sizeof(char);

		dest= (subs_t*)pkg_malloc(size);
	
		if(dest== NULL)
		{
			LM_ERR( "Can't allocate memory\n" );
			/* tidy up and return >>>>>>>>>>>>>>>>>>>> */
			rls2_dbf.free_result(rls2_db, result);
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
		{
			LM_ERR("event not found and set to NULL\n");
		}

		dest->local_cseq= VAL_INT(values+r_local_cseq_col);
		dest->remote_cseq= VAL_INT(values+r_remote_cseq_col);
		dest->status= VAL_INT(values+r_status_col);
		dest->version= VAL_INT(values+r_version_col);
		/*dest->send_on_cback= VAL_INT(values+r_send_on_cback_col);*/
		dest->expires= VAL_INT(values+r_expires_col);

		/*dest->db_flag= VAL_INT(values+r_db_flag_col);*/
	}

	rls2_dbf.free_result(rls2_db, result);

	if (rlsdb_debug) LM_ERR( "Done\n" );
	return(dest);
}

/******************************************************************************/

void dump_dialog( subs_t *s )

{
	if (!rlsdb_debug) return;
	LM_ERR( "pres_rui=%.*s\n", s->pres_uri.len, s->pres_uri.s );
	LM_ERR( "to_user=%.*s\n", s->to_user.len, s->to_user.s );
	LM_ERR( "to_domain=%.*s\n", s->to_domain.len, s->to_domain.s );
	LM_ERR( "from_user=%.*s\n", s->from_user.len, s->from_user.s );
	LM_ERR( "from_domain=%.*s\n", s->from_domain.len, s->from_domain.s );
	LM_ERR( "to_tag=%.*s\n", s->to_tag.len, s->to_tag.s );
	LM_ERR( "from_tag=%.*s\n", s->from_tag.len, s->from_tag.s );
	LM_ERR( "callid=%.*s\n", s->callid.len, s->callid.s );
	LM_ERR( "sockinfo_str=%.*s\n", s->sockinfo_str.len, s->sockinfo_str.s );
	LM_ERR( "local_contact=%.*s\n", s->local_contact.len, s->local_contact.s );
	LM_ERR( "contact=%.*s\n", s->contact.len, s->contact.s );
	LM_ERR( "record_route=%.*s\n", s->record_route.len, s->record_route.s );
	LM_ERR( "event_id=%.*s\n", s->event_id.len, s->event_id.s );
	LM_ERR( "reason=%.*s\n", s->reason.len, s->reason.s );
	LM_ERR( "event=%.*s\n", s->event->name.len, s->event->name.s );
	LM_ERR( "local_cseq=%d\n", s-> local_cseq);
	LM_ERR( "remote_cseq=%d\n", s->remote_cseq );
	LM_ERR( "status=%d\n", s->status );
	LM_ERR( "version=%d\n", s->version );
	LM_ERR( "expires=%d\n", s->expires );
	LM_ERR( "send_on_cback=%d\n", s->send_on_cback );
	LM_ERR( "db_flag=%x\n", s->db_flag );
}

/******************************************************************************/


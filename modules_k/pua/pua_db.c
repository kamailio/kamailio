/*
 * $Id$
 *
 * pua db - presence user agent database support 
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

#include "pua.h"

/* database connection */
extern db1_con_t *pua_db;
extern db_func_t pua_dbf;
extern int pua_fetch_rows;

/* database colums */
static str str_id_col = str_init( "id" );
static str str_pres_uri_col = str_init("pres_uri");
static str str_pres_id_col = str_init("pres_id");
static str str_expires_col= str_init("expires");
static str str_flag_col= str_init("flag");
static str str_etag_col= str_init("etag");
static str str_tuple_id_col= str_init("tuple_id");
static str str_watcher_uri_col= str_init("watcher_uri");
static str str_call_id_col= str_init("call_id");
static str str_to_tag_col= str_init("to_tag");
static str str_from_tag_col= str_init("from_tag");
static str str_cseq_col= str_init("cseq");
static str str_event_col= str_init("event");
static str str_record_route_col= str_init("record_route");
static str str_contact_col= str_init("contact");
static str str_remote_contact_col= str_init("remote_contact");
static str str_extra_headers_col= str_init("extra_headers");
static str str_desired_expires_col= str_init("desired_expires");
static str str_version_col = str_init("version");

static int puadb_debug=0;

/******************************************************************************/

void free_results_puadb( db1_res_t *res )

{
	if (res) 
	{
		pua_dbf.free_result(pua_db, res);
		if (puadb_debug) LM_ERR( "free_results_puadb\n" );
	}
}

/******************************************************************************/

void extract_row( db_val_t *values, ua_pres_t *result )

{

	if (result->pres_uri != NULL )
	{
		result->pres_uri->s = (char *)VAL_STRING(values+1);
		result->pres_uri->len = strlen(VAL_STRING(values+1));
	}

	result->id.s = (char *)VAL_STRING(values+2);
	result->id.len = strlen(VAL_STRING(values+2));

	result->event = VAL_INT(values+3);

	result->expires = VAL_INT(values+4);

	result->desired_expires = VAL_INT(values+5);

	result->flag = VAL_INT(values+6);

	/* publish */
	result->etag.s = (char *)VAL_STRING(values+7);
	result->etag.len = strlen(VAL_STRING(values+7));

	result->tuple_id.s = (char *)VAL_STRING(values+8);
	result->tuple_id.len = strlen(VAL_STRING(values+8));

	/* subscribe */
	if (result->watcher_uri != NULL )
	{
		result->watcher_uri->s = (char *)VAL_STRING(values+9);
		result->watcher_uri->len = strlen(VAL_STRING(values+9));
	}

	result->call_id.s = (char *)VAL_STRING(values+10);
	result->call_id.len = strlen(VAL_STRING(values+10));

	result->to_tag.s = (char *)VAL_STRING(values+11);
	result->to_tag.len = strlen(VAL_STRING(values+11));

	result->from_tag.s = (char *)VAL_STRING(values+12);
	result->from_tag.len = strlen(VAL_STRING(values+12));

	result->cseq = VAL_INT(values+13);

	result->record_route.s = (char *)VAL_STRING(values+14);
	result->record_route.len = strlen(VAL_STRING(values+14));

	result->contact.s = (char *)VAL_STRING(values+15);
	result->contact.len = strlen(VAL_STRING(values+15));

	result->remote_contact.s = (char *)VAL_STRING(values+16);
	result->remote_contact.len = strlen(VAL_STRING(values+16));

	result->version = VAL_INT(values+17);

	if (result->extra_headers != NULL )
	{
		result->extra_headers->s = (char *)VAL_STRING(values+18);
		result->extra_headers->len = strlen(VAL_STRING(values+18));
	}

	if (puadb_debug)
	{
		LM_ERR( "pres_uri is %s\n",VAL_STRING(values+1));
		LM_ERR( "pres_id is %s\n",VAL_STRING(values+2));
		LM_ERR( "etag is %s\n",VAL_STRING(values+7));
		LM_ERR( "tuple_id is %s\n",VAL_STRING(values+8));
		LM_ERR( "watcher uri is %s\n",VAL_STRING(values+9));
	}

}

/******************************************************************************/

int clean_puadb( int update_period, int min_expires )

{
	int i, nr_rows;
	db_row_t *rows;
	db_val_t *values;
	db_key_t q_cols[1];
	db1_res_t *res= NULL;
	db_val_t q_vals[1];
	db_op_t  q_ops[1];
	int id;
	time_t now;
	ua_pres_t p;
	str pres_uri={0,0}, watcher_uri={0,0}, extra_headers={0,0};

	if (puadb_debug)
		LM_ERR( "clean_puadb update_period=%d min expires=%d\n", update_period, min_expires );

	memset(&p, 0, sizeof(p));
	p.pres_uri = &pres_uri;
	p.watcher_uri = &watcher_uri;
	p.extra_headers = &extra_headers;

	now = time(NULL);

	/* cols and values used for search query */
	q_cols[0] = &str_expires_col;
	q_vals[0].type = DB1_INT;
	q_vals[0].nul = 0;
	q_vals[0].val.int_val = now+update_period;
	q_ops[0] = OP_LT; 	

	if(db_fetch_query(&pua_dbf, pua_fetch_rows, pua_db, q_cols, q_ops,
				q_vals, NULL, 1, 0, 0, &res) < 0)
	{
		LM_ERR("DB query error\n");
		return(-1);
	}

	nr_rows = RES_ROW_N(res);

	if (nr_rows == 0)
	{
		/* no match */ 
		LM_INFO( "No records matched for clean\n");
		pua_dbf.free_result(pua_db, res);
		return(0);
	}

	do {
		/* get the results and update matching entries */
		rows = RES_ROWS(res);

		for (i=0; i < nr_rows; i++)
		{
			values = ROW_VALUES(rows+i);

			extract_row( values, &p ); 
			id = VAL_INT(values);

			if((p.desired_expires> p.expires + min_expires) || (p.desired_expires== 0 ))
			{
			/*    q_cols[0] = &str_id_col;
				  q_vals[0].type = DB1_INT;
				  q_vals[0].nul = 0;
				  q_vals[0].val.int_val = id;	

				  n_update_cols=0;
				  db_cols[n_update_cols] = &str_expires_col;
				  db_vals[n_update_cols].type = DB1_INT;
				  db_vals[n_update_cols].nul = 0; 
				  db_vals[n_update_cols].val.int_val = p.expires;
				  n_update_cols++;

				  db_cols[n_update_cols] = &str_desired_expires_col;
				  db_vals[n_update_cols].type = DB1_INT;
				  db_vals[n_update_cols].nul = 0; 
				  db_vals[n_update_cols].val.int_val = p.desired_expires;
				  n_update_cols++;

				  if(pua_dbf.update(pua_db, q_cols, 0, q_vals,
				  db_cols,db_vals,1,n_update_cols) < 0)
				  {
				  LM_ERR( "update of db FAILED\n" );
				  }*/

				if(update_pua(&p)< 0)
				{
					LM_ERR("update_pua failed\n");
					break; /* >>>>>>> do we really want to do this here*/
				}
				continue;
			}

			if(p.expires < now - 10)
			{
				LM_DBG("Found expired: uri= %.*s\n", p.pres_uri->len, p.pres_uri->s);
				q_cols[0] = &str_id_col;
				q_vals[0].type = DB1_INT;
				q_vals[0].nul = 0;
				q_vals[0].val.int_val = id;

				if ( pua_dbf.delete(pua_db, q_cols, 0, q_vals, 1) < 0 )
				{
					LM_ERR( "Failed to delete from db\n" ); 
				}			
			}

		} /* for i */
	} while ((db_fetch_next(&pua_dbf, pua_fetch_rows, pua_db, &res)==1)
			&& (RES_ROWS(res)>0));

	pua_dbf.free_result(pua_db, res);
	if (puadb_debug) LM_ERR( "Done\n" );
	return(0);
}

/******************************************************************************/

int matches_in_puadb(ua_pres_t *pres) 

{
	int nr_rows;
	db_key_t q_cols[20];
	db1_res_t *res= NULL;
	db_val_t q_vals[20];
	db_op_t  q_ops[20];
	int n_query_cols= 0;
	int puri_col,pid_col,flag_col,etag_col, event_col;
	int watcher_col;
	int remote_contact_col;	

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	if (pres->id.s && pres->id.len)
	{
		q_cols[pid_col= n_query_cols] = &str_pres_id_col;	
		q_vals[pid_col].type = DB1_STR;
		q_vals[pid_col].nul = 0;
		q_vals[pid_col].val.str_val.s = pres->id.s;
		q_vals[pid_col].val.str_val.len = pres->id.len;
		q_ops[pid_col] = OP_EQ;
		n_query_cols++;
	}

	q_cols[flag_col= n_query_cols] = &str_flag_col;
	q_vals[flag_col].type = DB1_INT;
	q_vals[flag_col].nul = 0;
	q_vals[flag_col].val.int_val = pres->flag;
	q_ops[flag_col] = OP_BITWISE_AND;
	n_query_cols++;

	q_cols[event_col= n_query_cols] = &str_event_col;
	q_vals[event_col].type = DB1_INT;
	q_vals[event_col].nul = 0;
	q_vals[event_col].val.int_val = pres->event;
	q_ops[event_col] = OP_BITWISE_AND;
	n_query_cols++;


	if(pres->watcher_uri)
	{
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;

		if (check_remote_contact != 0)
		{
			q_cols[remote_contact_col= n_query_cols] = &str_remote_contact_col;
			q_vals[remote_contact_col].type = DB1_STR;
			q_vals[remote_contact_col].nul = 0;
			q_vals[remote_contact_col].val.str_val.s = pres->remote_contact.s;
			q_vals[remote_contact_col].val.str_val.len = pres->remote_contact.len;
			q_ops[remote_contact_col] = OP_EQ;
			n_query_cols++;
		}

	}
	else /* no watcher _uri */
	{

		if(pres->etag.s)
		{
			q_cols[etag_col= n_query_cols] = &str_etag_col;
			q_vals[etag_col].type = DB1_STR;
			q_vals[etag_col].nul = 0;
			q_vals[etag_col].val.str_val.s = pres->etag.s;
			q_vals[etag_col].val.str_val.len = pres->etag.len;
			q_ops[etag_col] = OP_EQ;
			n_query_cols++;						
		}
		else
		{
			LM_DBG("no etag restriction\n");
		}
	}


	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(pua_dbf.query(pua_db, q_cols, q_ops, q_vals,
				0,n_query_cols,0,0,&res) < 0)
	{
		LM_ERR("DB query error\n");
		return(-1);
	}

	nr_rows = RES_ROW_N(res);

	pua_dbf.free_result(pua_db, res);
	LM_DBG("Found %d rows\n", nr_rows);
	return(nr_rows);
}

/******************************************************************************/

ua_pres_t* search_puadb(ua_pres_t *pres, ua_pres_t *result, db1_res_t **dbres) 

{
	int nr_rows;
	db_row_t *rows;
	db_key_t q_cols[20];
	db1_res_t *res= NULL;
	db_val_t q_vals[20];
	db_op_t  q_ops[20];
	int n_query_cols= 0;
	int puri_col,pid_col,flag_col,etag_col,event_col;
	int watcher_col;
	int remote_contact_col;	
	db_val_t *values;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(NULL);
	}

	*dbres = NULL;

	if (result==NULL) { LM_ERR( "result is NULL\n" ); return(NULL); }		

	/* cols and values used for search query */ 
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	if (pres->id.s && pres->id.len)
	{
		q_cols[pid_col= n_query_cols] = &str_pres_id_col;	
		q_vals[pid_col].type = DB1_STR;
		q_vals[pid_col].nul = 0;
		q_vals[pid_col].val.str_val.s = pres->id.s;
		q_vals[pid_col].val.str_val.len = pres->id.len;
		q_ops[pid_col] = OP_EQ;
		n_query_cols++;
	}

	q_cols[flag_col= n_query_cols] = &str_flag_col;
	q_vals[flag_col].type = DB1_INT;
	q_vals[flag_col].nul = 0;
	q_vals[flag_col].val.int_val = pres->flag;
	q_ops[flag_col] = OP_BITWISE_AND;
	n_query_cols++;

	q_cols[event_col= n_query_cols] = &str_event_col;
	q_vals[event_col].type = DB1_INT;
	q_vals[event_col].nul = 0;
	q_vals[event_col].val.int_val = pres->event;
	q_ops[event_col] = OP_BITWISE_AND;
	n_query_cols++;

	if(pres->watcher_uri)
	{
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;

		if (check_remote_contact != 0)
		{
			q_cols[remote_contact_col= n_query_cols] = &str_remote_contact_col;
			q_vals[remote_contact_col].type = DB1_STR;
			q_vals[remote_contact_col].nul = 0;
			q_vals[remote_contact_col].val.str_val.s = pres->remote_contact.s;
			q_vals[remote_contact_col].val.str_val.len = pres->remote_contact.len;
			q_ops[remote_contact_col] = OP_EQ;
			n_query_cols++;
		}

	}
	else /* no watcher _uri */
	{

		if(pres->etag.s)
		{
			q_cols[etag_col= n_query_cols] = &str_etag_col;
			q_vals[etag_col].type = DB1_STR;
			q_vals[etag_col].nul = 0;
			q_vals[etag_col].val.str_val.s = pres->etag.s;
			q_vals[etag_col].val.str_val.len = pres->etag.len;
			q_ops[etag_col] = OP_EQ;
			n_query_cols++;						
		}
		else
		{
			LM_DBG("no etag restriction\n");
		}
	}


	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(NULL);
	}

	if(pua_dbf.query(pua_db, q_cols, q_ops, q_vals,
				NULL,n_query_cols,0,0,&res) < 0)
	{
		LM_ERR("DB query error\n");
		return(NULL);
	}

	nr_rows = RES_ROW_N(res);

	if (nr_rows == 0)
	{
		/* no match */ 
		LM_DBG("No rows found\n");
		pua_dbf.free_result(pua_db, res);
		return(NULL);
	}

	if (nr_rows != 1)
	{
		LM_ERR("Too many rows found (%d)\n", nr_rows);
		pua_dbf.free_result(pua_db, res);
		return(NULL);
	}


	/* get the results and fill in return data structure */
	rows = RES_ROWS(res);
	values = ROW_VALUES(rows);

	extract_row( values, result );

	/*pua_dbf.free_result(pua_db, res);*/
	*dbres = res;

	return(result);
}

/******************************************************************************/

ua_pres_t* get_dialog_puadb(ua_pres_t *pres, ua_pres_t *result, db1_res_t **dbres) 

{
	int nr_rows;
	db_row_t *rows;
	db_key_t q_cols[20];
	db1_res_t *res= NULL;
	db_val_t q_vals[20];
	db_op_t  q_ops[20];
	int n_query_cols= 0;
	int puri_col,callid_col;
	int watcher_col, totag_col, fromtag_col, flag_col;	
	db_val_t *values;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(NULL);
	}

	*dbres = NULL;

	if (result==NULL) { LM_ERR( "result is NULL\n" ); return(NULL); }

	/* cols and values used for search query */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	q_cols[flag_col= n_query_cols] = &str_flag_col;
	q_vals[flag_col].type = DB1_INT;
	q_vals[flag_col].nul = 0;
	q_vals[flag_col].val.int_val = pres->flag;
	q_ops[flag_col] = OP_BITWISE_AND;
	n_query_cols++;

	if (pres->watcher_uri)
	{
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;
	}
	else
	{
		LM_ERR( "get_dialog_puadb has NULL watcher_uri\n" );
	}

	q_cols[callid_col= n_query_cols] = &str_call_id_col;	
	q_vals[callid_col].type = DB1_STR;
	q_vals[callid_col].nul = 0;
	q_vals[callid_col].val.str_val.s = pres->call_id.s;
	q_vals[callid_col].val.str_val.len = pres->call_id.len;
	q_ops[callid_col] = OP_EQ;
	n_query_cols++;

	q_cols[totag_col= n_query_cols] = &str_to_tag_col;	
	q_vals[totag_col].type = DB1_STR;
	q_vals[totag_col].nul = 0;
	q_vals[totag_col].val.str_val.s = pres->to_tag.s;
	q_vals[totag_col].val.str_val.len = pres->to_tag.len;
	q_ops[totag_col] = OP_EQ;
	n_query_cols++;

	q_cols[fromtag_col= n_query_cols] = &str_from_tag_col;	
	q_vals[fromtag_col].type = DB1_STR;
	q_vals[fromtag_col].nul = 0;
	q_vals[fromtag_col].val.str_val.s = pres->from_tag.s;
	q_vals[fromtag_col].val.str_val.len = pres->from_tag.len;
	q_ops[fromtag_col] = OP_EQ;
	n_query_cols++;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(NULL);
	}

	if(pua_dbf.query(pua_db, q_cols, q_ops, q_vals,
				0,n_query_cols,0,0,&res) < 0)
	{
		LM_ERR("DB query error\n");
		return(NULL);
	}

	nr_rows = RES_ROW_N(res);

	if (nr_rows == 0)
	{
		/* no match */ 
		LM_DBG("No rows found\n");
		pua_dbf.free_result(pua_db, res);
		return(NULL);
	}

	if (nr_rows != 1)
	{
		LM_ERR("Too many rows found (%d)\n", nr_rows);
		return(NULL);
	}


	/* get the results and fill in return data structure */
	rows = RES_ROWS(res);
	values = ROW_VALUES(rows);

	extract_row( values, result );

	/*pua_dbf.free_result(pua_db, res);*/
	*dbres = res;

	return(result);
}

/******************************************************************************/

int is_dialog_puadb(ua_pres_t *pres) 

{
	int nr_rows;
	db_key_t q_cols[20], res_cols[5];
	db1_res_t *res= NULL;
	db_val_t q_vals[20];
	db_op_t  q_ops[20];
	int n_query_cols= 0, n_res_cols=0;
	int puri_col,callid_col;
	int watcher_col, totag_col, fromtag_col;	

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	if (pres->watcher_uri)
	{	
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;
	}
	else
	{
		LM_ERR( "is_dialog_puadb has NULL watcher_uri\n" );
	}


	q_cols[callid_col= n_query_cols] = &str_call_id_col;	
	q_vals[callid_col].type = DB1_STR;
	q_vals[callid_col].nul = 0;
	q_vals[callid_col].val.str_val.s = pres->call_id.s;
	q_vals[callid_col].val.str_val.len = pres->call_id.len;
	q_ops[callid_col] = OP_EQ;
	n_query_cols++;

	q_cols[totag_col= n_query_cols] = &str_to_tag_col;	
	q_vals[totag_col].type = DB1_STR;
	q_vals[totag_col].nul = 0;
	q_vals[totag_col].val.str_val.s = pres->to_tag.s;
	q_vals[totag_col].val.str_val.len = pres->to_tag.len;
	q_ops[totag_col] = OP_EQ;
	n_query_cols++;

	q_cols[fromtag_col= n_query_cols] = &str_from_tag_col;	
	q_vals[fromtag_col].type = DB1_STR;
	q_vals[fromtag_col].nul = 0;
	q_vals[fromtag_col].val.str_val.s = pres->from_tag.s;
	q_vals[fromtag_col].val.str_val.len = pres->from_tag.len;
	q_ops[fromtag_col] = OP_EQ;
	n_query_cols++;

	/* return the id column, even though don't actually need */
	res_cols[n_res_cols] = &str_id_col;	
	n_res_cols++;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}


	if(pua_dbf.query(pua_db, q_cols, q_ops, q_vals,
				res_cols,n_query_cols,n_res_cols,0,&res) < 0)
	{
		LM_ERR("DB query error\n");
		return(-1);
	}

	nr_rows = RES_ROW_N(res);
	pua_dbf.free_result(pua_db, res);

	if (nr_rows == 0)
	{
		/* no match */ 
		LM_DBG("No rows found.\n");
		return(-1);
	}

	if (nr_rows != 1)
	{
		LM_WARN("Too many rows found (%d)\n", nr_rows);
		/* no need to return here - drop thro */
	}

	/* established dialog */
	if(pres->to_tag.len>0)
		return 0;
	/* temporary dialog */
	return 1;
}

/******************************************************************************/

int get_record_id_puadb(ua_pres_t *pres, str **rec_id ) 

{
	int nr_rows;
	db_row_t *rows;
	db_key_t q_cols[20], res_cols[5];
	db1_res_t *res= NULL;
	db_val_t q_vals[20];
	db_op_t  q_ops[20];
	int n_query_cols=0, n_res_cols=0;
	int puri_col,callid_col;
	int watcher_col, totag_col, fromtag_col;
	int pres_id_col;	
	db_val_t *values;	
	str *id;	

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	if (pres->watcher_uri) 
	{
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;
	}
	else
	{
		LM_ERR( "get_record_id_puadb has NULL watcher_uri\n" );
	}

	q_cols[callid_col= n_query_cols] = &str_call_id_col;	
	q_vals[callid_col].type = DB1_STR;
	q_vals[callid_col].nul = 0;
	q_vals[callid_col].val.str_val.s = pres->call_id.s;
	q_vals[callid_col].val.str_val.len = pres->call_id.len;
	q_ops[callid_col] = OP_EQ;
	n_query_cols++;

	q_cols[totag_col= n_query_cols] = &str_to_tag_col;	
	q_vals[totag_col].type = DB1_STR;
	q_vals[totag_col].nul = 0;
	q_vals[totag_col].val.str_val.s = pres->to_tag.s;
	q_vals[totag_col].val.str_val.len = pres->to_tag.len;
	q_ops[totag_col] = OP_EQ;
	n_query_cols++;

	q_cols[fromtag_col= n_query_cols] = &str_from_tag_col;	
	q_vals[fromtag_col].type = DB1_STR;
	q_vals[fromtag_col].nul = 0;
	q_vals[fromtag_col].val.str_val.s = pres->from_tag.s;
	q_vals[fromtag_col].val.str_val.len = pres->from_tag.len;
	q_ops[fromtag_col] = OP_EQ;
	n_query_cols++;

	res_cols[n_res_cols] = &str_id_col;	
	n_res_cols++;

	res_cols[pres_id_col= n_res_cols] = &str_pres_id_col;	
	n_res_cols++;

	*rec_id = NULL;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(pua_dbf.query(pua_db, q_cols, q_ops, q_vals,
				res_cols,n_query_cols,n_res_cols,0,&res) < 0)
	{
		LM_ERR("DB query error\n");
		return(-1);
	}

	nr_rows = RES_ROW_N(res);

	if (nr_rows == 0)
	{
		/* no match */
		LM_DBG("No rows found. Looking for temporary dialog\n");
		pua_dbf.free_result(pua_db, res);

		q_vals[totag_col].val.str_val.s = "";
		q_vals[totag_col].val.str_val.len = 0;


		if(pua_dbf.query(pua_db, q_cols, q_ops, q_vals,
			res_cols,n_query_cols,n_res_cols,0,&res) < 0)
		{
			LM_ERR("DB query error\n");
			return(-1);
		}

		nr_rows = RES_ROW_N(res);

		if (nr_rows == 0)
		{
			LM_DBG( "Temporary Dialog Not found\n" );
			pua_dbf.free_result(pua_db, res);
			return(0);
		}

		LM_DBG( "Found a temporary Dialog\n" );
	}

	if (nr_rows != 1)
	{
		LM_ERR("Too many rows found (%d)\n", nr_rows);
		pua_dbf.free_result(pua_db, res);
		return(-1);
	}


	/* get the results and fill in return data structure */
	rows = RES_ROWS(res);
	values = ROW_VALUES(rows);

	/* LM_ERR("db_id= %d\n", VAL_INT(values) ); */

	id= (str*)pkg_malloc(sizeof(str));

	if(id== NULL)
	{
		LM_ERR("No more memory\n");
		pua_dbf.free_result(pua_db, res);
		return(-1);
	}

	id->s= (char*)pkg_malloc( strlen(VAL_STRING(values+1)) * sizeof(char));

	if(id->s== NULL)
	{
		LM_ERR("No more memory\n");
		pkg_free(id);
		pua_dbf.free_result(pua_db, res);
		return(-1);
	}

	memcpy(id->s, VAL_STRING(values+1), strlen(VAL_STRING(values+1)) );
	id->len= strlen(VAL_STRING(values+1));

	*rec_id= id;
	pua_dbf.free_result(pua_db, res);

	LM_DBG("Found id=%.*s\n", id->len, id->s);
	return(0);
}

/******************************************************************************/

int delete_temporary_dialog_puadb(ua_pres_t *pres ) 

{
	db_key_t q_cols[20];
	db_val_t q_vals[20];
	db_op_t  q_ops[20];
	int n_query_cols= 0;
	int puri_col,callid_col;
	int watcher_col, totag_col, fromtag_col;
	int rval;	

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	if (pres->watcher_uri)
	{	
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;
	}
	else
	{
		LM_ERR( "delete_temporary_dialog_puadb has NULL watcher_uri\n" );
	}


	q_cols[callid_col= n_query_cols] = &str_call_id_col;	
	q_vals[callid_col].type = DB1_STR;
	q_vals[callid_col].nul = 0;
	q_vals[callid_col].val.str_val.s = pres->call_id.s;
	q_vals[callid_col].val.str_val.len = pres->call_id.len;
	q_ops[callid_col] = OP_EQ;
	n_query_cols++;

	q_cols[totag_col= n_query_cols] = &str_to_tag_col;	
	q_vals[totag_col].type = DB1_STR;
	q_vals[totag_col].nul = 0;
	q_vals[totag_col].val.str_val.s = "";
	q_vals[totag_col].val.str_val.len = 0;
	q_ops[totag_col] = OP_EQ;
	n_query_cols++;

	q_cols[fromtag_col= n_query_cols] = &str_from_tag_col;	
	q_vals[fromtag_col].type = DB1_STR;
	q_vals[fromtag_col].nul = 0;
	q_vals[fromtag_col].val.str_val.s = pres->from_tag.s;
	q_vals[fromtag_col].val.str_val.len = pres->from_tag.len;
	q_ops[fromtag_col] = OP_EQ;
	n_query_cols++;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	rval = pua_dbf.delete(pua_db, q_cols, q_ops, q_vals, n_query_cols);

	if ( rval < 0 ) 
	{
		LM_DBG("No temporary dialog found and deleted\n");
	}
	else
	{
		LM_DBG("Temporary dialog found and deleted\n");
	}

	return(rval);
}

/******************************************************************************/

int delete_puadb(ua_pres_t *pres ) 

{
	db_key_t q_cols[20];
	db_val_t q_vals[20];
	db_op_t  q_ops[20];
	int n_query_cols= 0;
	int puri_col,pid_col,flag_col,etag_col,event_col;
	int watcher_col;
	int remote_contact_col;	
	int rval;		

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	q_cols[pid_col= n_query_cols] = &str_pres_id_col;	
	q_vals[pid_col].type = DB1_STR;
	q_vals[pid_col].nul = 0;
	q_vals[pid_col].val.str_val.s = pres->id.s;
	q_vals[pid_col].val.str_val.len = pres->id.len;
	q_ops[pid_col] = OP_EQ;
	n_query_cols++;

	q_cols[flag_col= n_query_cols] = &str_flag_col;
	q_vals[flag_col].type = DB1_INT;
	q_vals[flag_col].nul = 0;
	q_vals[flag_col].val.int_val = pres->flag;
	q_ops[flag_col] = OP_BITWISE_AND;
	n_query_cols++;

	q_cols[event_col= n_query_cols] = &str_event_col;
	q_vals[event_col].type = DB1_INT;
	q_vals[event_col].nul = 0;
	q_vals[event_col].val.int_val = pres->event;
	q_ops[event_col] = OP_BITWISE_AND;
	n_query_cols++;


	if(pres->watcher_uri)
	{
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;

		if (check_remote_contact != 0)
		{
			q_cols[remote_contact_col= n_query_cols] = &str_remote_contact_col;
			q_vals[remote_contact_col].type = DB1_STR;
			q_vals[remote_contact_col].nul = 0;
			q_vals[remote_contact_col].val.str_val.s = pres->remote_contact.s;
			q_vals[remote_contact_col].val.str_val.len = pres->remote_contact.len;
			q_ops[remote_contact_col] = OP_EQ;
			n_query_cols++;
		}

	}
	else /* no watcher _uri */
	{

		if(pres->etag.s)
		{
			q_cols[etag_col= n_query_cols] = &str_etag_col;
			q_vals[etag_col].type = DB1_STR;
			q_vals[etag_col].nul = 0;
			q_vals[etag_col].val.str_val.s = pres->etag.s;
			q_vals[etag_col].val.str_val.len = pres->etag.len;
			q_ops[etag_col] = OP_EQ;
			n_query_cols++;						
		}

	}


	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	rval = pua_dbf.delete(pua_db, q_cols, q_ops, q_vals, n_query_cols);

	if ( rval < 0 ) 
	{
		LM_DBG("No dialog found and deleted\n");
	}
	else
	{
		LM_DBG("Dialog found and deleted\n");
	}

	return(rval);
}

/******************************************************************************/

int update_contact_puadb(ua_pres_t *pres, str *contact) 

{
	db_key_t q_cols[20], db_cols[5];
	db_val_t q_vals[20], db_vals[5];
	db_op_t  q_ops[20];
	int n_query_cols= 0, n_update_cols=0;
	int puri_col,callid_col;
	int watcher_col, totag_col, fromtag_col;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	if (pres->watcher_uri)
	{
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;
	}
	else
	{
		LM_ERR( "update_contact_puadb has NULL watcher_uri\n" );
	}

	q_cols[callid_col= n_query_cols] = &str_call_id_col;	
	q_vals[callid_col].type = DB1_STR;
	q_vals[callid_col].nul = 0;
	q_vals[callid_col].val.str_val.s = pres->call_id.s;
	q_vals[callid_col].val.str_val.len = pres->call_id.len;
	q_ops[callid_col] = OP_EQ;
	n_query_cols++;

	q_cols[totag_col= n_query_cols] = &str_to_tag_col;	
	q_vals[totag_col].type = DB1_STR;
	q_vals[totag_col].nul = 0;
	q_vals[totag_col].val.str_val.s = pres->to_tag.s;
	q_vals[totag_col].val.str_val.len = pres->to_tag.len;
	q_ops[totag_col] = OP_EQ;
	n_query_cols++;

	q_cols[fromtag_col= n_query_cols] = &str_from_tag_col;	
	q_vals[fromtag_col].type = DB1_STR;
	q_vals[fromtag_col].nul = 0;
	q_vals[fromtag_col].val.str_val.s = pres->from_tag.s;
	q_vals[fromtag_col].val.str_val.len = pres->from_tag.len;
	q_ops[fromtag_col] = OP_EQ;
	n_query_cols++;


	/* we overwrite contact even if not changed */
	db_cols[n_update_cols] = &str_contact_col; /* had remote here, think was a bug */
	db_vals[n_update_cols].type = DB1_STR;
	db_vals[n_update_cols].nul = 0; 
	db_vals[n_update_cols].val.str_val.s = contact->s;
	db_vals[n_update_cols].val.str_val.len = contact->len;
	n_update_cols++;


	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(pua_dbf.update(pua_db, q_cols, q_ops, q_vals,
				db_cols,db_vals,n_query_cols,n_update_cols) < 0)
	{
		LM_ERR("DB update failed\n");
		return(-1);
	}

	return(0);
}


/******************************************************************************/

int update_version_puadb(ua_pres_t *pres, int version ) 

{
	db_key_t q_cols[20], db_cols[5];
	db_val_t q_vals[20], db_vals[5];
	db_op_t  q_ops[20];
	int n_query_cols= 0, n_update_cols=0;
	int puri_col,callid_col;
	int watcher_col, totag_col, fromtag_col;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	if (pres->watcher_uri)
	{	
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;
	}

	q_cols[callid_col= n_query_cols] = &str_call_id_col;	
	q_vals[callid_col].type = DB1_STR;
	q_vals[callid_col].nul = 0;
	q_vals[callid_col].val.str_val.s = pres->call_id.s;
	q_vals[callid_col].val.str_val.len = pres->call_id.len;
	q_ops[callid_col] = OP_EQ;
	n_query_cols++;

	q_cols[totag_col= n_query_cols] = &str_to_tag_col;	
	q_vals[totag_col].type = DB1_STR;
	q_vals[totag_col].nul = 0;
	q_vals[totag_col].val.str_val.s = pres->to_tag.s;
	q_vals[totag_col].val.str_val.len = pres->to_tag.len;
	q_ops[totag_col] = OP_EQ;
	n_query_cols++;

	q_cols[fromtag_col= n_query_cols] = &str_from_tag_col;	
	q_vals[fromtag_col].type = DB1_STR;
	q_vals[fromtag_col].nul = 0;
	q_vals[fromtag_col].val.str_val.s = pres->from_tag.s;
	q_vals[fromtag_col].val.str_val.len = pres->from_tag.len;
	q_ops[fromtag_col] = OP_EQ;
	n_query_cols++;


	/* we overwrite contact even if not changed */
	db_cols[n_update_cols] = &str_version_col;
	db_vals[n_update_cols].type = DB1_INT;
	db_vals[n_update_cols].nul = 0;
	db_vals[n_update_cols].val.int_val = version;
	n_update_cols++;


	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(pua_dbf.update(pua_db, q_cols, q_ops, q_vals,
				db_cols,db_vals,n_query_cols,n_update_cols) < 0)

	{
		LM_ERR("DB update failed\n");
		return(-1);
	}

	return(0);
}


/******************************************************************************/

void update_puadb(ua_pres_t* pres, time_t desired_expires, 
		int expires, str* etag, str *contact)

{
	db_key_t q_cols[20];
	db_key_t db_cols[5];
	db_val_t q_vals[20], db_vals[5];
	db_op_t  q_ops[20];
	int n_query_cols= 0;
	int n_update_cols= 0;
	int puri_col,pid_col,flag_col,etag_col,event_col;
	int watcher_col;
	int remote_contact_col;	

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return;
	}

	/* cols and values used for search query */
	q_cols[puri_col= n_query_cols] = &str_pres_uri_col;
	q_vals[puri_col].type = DB1_STR;
	q_vals[puri_col].nul = 0;
	q_vals[puri_col].val.str_val.s = pres->pres_uri->s;
	q_vals[puri_col].val.str_val.len = pres->pres_uri->len;
	q_ops[puri_col] = OP_EQ; 
	n_query_cols++;

	q_cols[pid_col= n_query_cols] = &str_pres_id_col;	
	q_vals[pid_col].type = DB1_STR;
	q_vals[pid_col].nul = 0;
	q_vals[pid_col].val.str_val.s = pres->id.s;
	q_vals[pid_col].val.str_val.len = pres->id.len;
	q_ops[pid_col] = OP_EQ;
	n_query_cols++;

	q_cols[flag_col= n_query_cols] = &str_flag_col;
	q_vals[flag_col].type = DB1_INT;
	q_vals[flag_col].nul = 0;
	q_vals[flag_col].val.int_val = pres->flag;
	q_ops[flag_col] = OP_BITWISE_AND;
	n_query_cols++;

	q_cols[event_col= n_query_cols] = &str_event_col;
	q_vals[event_col].type = DB1_INT;
	q_vals[event_col].nul = 0;
	q_vals[event_col].val.int_val = pres->event;
	q_ops[event_col] = OP_BITWISE_AND;
	n_query_cols++;


	if(pres->watcher_uri)
	{
		q_cols[watcher_col= n_query_cols] = &str_watcher_uri_col;
		q_vals[watcher_col].type = DB1_STR;
		q_vals[watcher_col].nul = 0;
		q_vals[watcher_col].val.str_val.s = pres->watcher_uri->s;
		q_vals[watcher_col].val.str_val.len = pres->watcher_uri->len;
		q_ops[watcher_col] = OP_EQ;
		n_query_cols++;

		if (check_remote_contact != 0)
		{
			q_cols[remote_contact_col= n_query_cols] = &str_remote_contact_col;
			q_vals[remote_contact_col].type = DB1_STR;
			q_vals[remote_contact_col].nul = 0;
			q_vals[remote_contact_col].val.str_val.s = pres->remote_contact.s;
			q_vals[remote_contact_col].val.str_val.len = pres->remote_contact.len;
			q_ops[remote_contact_col] = OP_EQ;
			n_query_cols++;
		}

	}
	else /* no watcher _uri */
	{

		if(pres->etag.s)
		{
			q_cols[etag_col= n_query_cols] = &str_etag_col;
			q_vals[etag_col].type = DB1_STR;
			q_vals[etag_col].nul = 0;
			q_vals[etag_col].val.str_val.s = pres->etag.s;
			q_vals[etag_col].val.str_val.len = pres->etag.len;
			q_ops[etag_col] = OP_EQ;
			n_query_cols++;						
		}

	}

	db_cols[n_update_cols] = &str_expires_col;
	db_vals[n_update_cols].type = DB1_INT;
	db_vals[n_update_cols].nul = 0; 
	db_vals[n_update_cols].val.int_val = expires + (int)time(NULL);
	n_update_cols++;

	db_cols[n_update_cols] = &str_desired_expires_col;
	db_vals[n_update_cols].type = DB1_INT;
	db_vals[n_update_cols].nul = 0; 
	db_vals[n_update_cols].val.int_val = desired_expires;
	n_update_cols++;

	if(pres->watcher_uri)
	{
		db_cols[n_update_cols] = &str_cseq_col;
		db_vals[n_update_cols].type = DB1_INT;
		db_vals[n_update_cols].nul = 0; 
		db_vals[n_update_cols].val.int_val = pres->cseq+1;
		n_update_cols++;
	}

	if(etag)
	{	
		db_cols[n_update_cols] = &str_etag_col;
		db_vals[n_update_cols].type = DB1_STR;
		db_vals[n_update_cols].nul = 0; 
		db_vals[n_update_cols].val.str_val.s = etag->s;
		db_vals[n_update_cols].val.str_val.len = etag->len;
		n_update_cols++;
	}

	if (contact)
	{
		/* we overwrite contact even if not changed,
		   saves retrieving record to check, when the 
		   record has gotta be updated anyway! */
		db_cols[n_update_cols] = &str_contact_col; /* had remote here think was a bug */
		db_vals[n_update_cols].type = DB1_STR;
		db_vals[n_update_cols].nul = 0; 
		db_vals[n_update_cols].val.str_val.s = contact->s;
		db_vals[n_update_cols].val.str_val.len = contact->len;
		n_update_cols++;
	}

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return;
	}

	if(pua_dbf.update(pua_db, q_cols, q_ops, q_vals,
				db_cols,db_vals,n_query_cols,n_update_cols) < 0)
	{
		LM_ERR("DB update failed\n");
	}
}

/******************************************************************************/

int insert_puadb(ua_pres_t* pres)

{
	db_key_t db_cols[20];
	db_val_t db_vals[20];
	int n_cols= 0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	if (pres->pres_uri)
	{
		db_cols[n_cols] = &str_pres_uri_col;
		db_vals[n_cols].type = DB1_STR;
		db_vals[n_cols].nul = 0;
		db_vals[n_cols].val.str_val.s = pres->pres_uri->s;
		db_vals[n_cols].val.str_val.len = pres->pres_uri->len;
		n_cols++;
	}
	else
	{
		LM_ERR( "insert_puadb called with NULL pres_uri\n" );
	}

	db_cols[n_cols] = &str_pres_id_col;	
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.str_val.s = pres->id.s;
	db_vals[n_cols].val.str_val.len = pres->id.len;
	n_cols++;

	db_cols[n_cols] = &str_event_col;	
	db_vals[n_cols].type = DB1_INT;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.int_val = pres->event;
	n_cols++;

	db_cols[n_cols] = &str_expires_col;
	db_vals[n_cols].type = DB1_INT;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.int_val = pres->expires;
	n_cols++;

	db_cols[n_cols] = &str_desired_expires_col;
	db_vals[n_cols].type = DB1_INT;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.int_val = pres->desired_expires;
	n_cols++;

	db_cols[n_cols] = &str_flag_col;
	db_vals[n_cols].type = DB1_INT;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.int_val = pres->flag;
	n_cols++;


	/* publish */
	db_cols[n_cols] = &str_etag_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = pres->etag.s;
	db_vals[n_cols].val.str_val.len = pres->etag.len;
	n_cols++;

	db_cols[n_cols] = &str_tuple_id_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = pres->tuple_id.s;
	db_vals[n_cols].val.str_val.len = pres->tuple_id.len;
	n_cols++;

	/* subscribe */
	db_cols[n_cols] = &str_watcher_uri_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 

	if (pres->watcher_uri)
	{
		db_vals[n_cols].val.str_val.s = pres->watcher_uri->s;
		db_vals[n_cols].val.str_val.len = pres->watcher_uri->len;
	}
	else
	{
		db_vals[n_cols].val.str_val.s = "";
		db_vals[n_cols].val.str_val.len = 0;
	}

	n_cols++;

	db_cols[n_cols] = &str_call_id_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = pres->call_id.s;
	db_vals[n_cols].val.str_val.len = pres->call_id.len;
	n_cols++;

	db_cols[n_cols] = &str_to_tag_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = pres->to_tag.s;
	db_vals[n_cols].val.str_val.len = pres->to_tag.len;
	n_cols++;

	db_cols[n_cols] = &str_from_tag_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = pres->from_tag.s;
	db_vals[n_cols].val.str_val.len = pres->from_tag.len;
	n_cols++;

	db_cols[n_cols] = &str_cseq_col;
	db_vals[n_cols].type = DB1_INT;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.int_val = pres->cseq;
	n_cols++;

	db_cols[n_cols] = &str_record_route_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = pres->record_route.s;
	db_vals[n_cols].val.str_val.len = pres->record_route.len;
	n_cols++;

	db_cols[n_cols] = &str_contact_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = pres->contact.s;
	db_vals[n_cols].val.str_val.len = pres->contact.len;
	n_cols++;

	db_cols[n_cols] = &str_remote_contact_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = pres->remote_contact.s;
	db_vals[n_cols].val.str_val.len = pres->remote_contact.len;
	n_cols++;

	db_cols[n_cols] = &str_version_col;
	db_vals[n_cols].type = DB1_INT;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.int_val = pres->version;
	n_cols++;

	if (pres->extra_headers)
	{
		db_cols[n_cols] = &str_extra_headers_col;
		db_vals[n_cols].type = DB1_STR;
		db_vals[n_cols].nul = 0; 
		db_vals[n_cols].val.str_val.s = pres->extra_headers->s;
		db_vals[n_cols].val.str_val.len = pres->extra_headers->len;
		n_cols++;
	}
	else
	{
		db_cols[n_cols] = &str_extra_headers_col;
		db_vals[n_cols].type = DB1_STR;
		db_vals[n_cols].nul = 0; 
		db_vals[n_cols].val.str_val.s = "";
		db_vals[n_cols].val.str_val.len = 0;
		n_cols++;
	}


	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if(pua_dbf.insert(pua_db, db_cols, db_vals, n_cols) < 0)  
	{
		LM_ERR("DB insert failed\n");
		return(-1);
	}

	return(0);
}

/******************************************************************************/


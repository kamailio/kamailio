/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "pua_db.h"

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


/******************************************************************************/

void free_results_puadb( db1_res_t *res )

{
	if (res) 
	{
		pua_dbf.free_result(pua_db, res);
		res = NULL;
	}
}

/******************************************************************************/

static void extract_row( db_val_t *values, ua_pres_t *result )

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

	if (pua_dbf.use_table(pua_db, &db_table) < 0) {
	    LM_ERR("error in use_table pua\n");
	    return(-1);
	}

	if(db_fetch_query(&pua_dbf, pua_fetch_rows, pua_db, q_cols, q_ops,
				q_vals, NULL, 1, 0, 0, &res) < 0)
	{
		LM_ERR("DB query error\n");
		return(-1);
	}

	if (res == NULL)
	{
		LM_ERR("bad result\n");
		return(-1);
	}

	if (RES_ROW_N(res) == 0)
	{
		/* no match */ 
		LM_DBG( "No records matched for clean\n");
		pua_dbf.free_result(pua_db, res);
		return(0);
	}

	do {
		nr_rows = RES_ROW_N(res);

		/* get the results and update matching entries */
		rows = RES_ROWS(res);

		for (i=0; i < nr_rows; i++)
		{
			values = ROW_VALUES(rows+i);

			extract_row( values, &p ); 
			id = VAL_INT(values);

			if((p.desired_expires> p.expires + min_expires) || (p.desired_expires== 0 ))
			{
				if(update_pua(&p)< 0)
				{
					LM_ERR("update_pua failed\n");
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

		}
	} while ((db_fetch_next(&pua_dbf, pua_fetch_rows, pua_db, &res)==1)
			&& (RES_ROWS(res)>0));

	pua_dbf.free_result(pua_db, res);
	return(0);
}

/******************************************************************************/

int is_dialog_puadb(ua_pres_t *pres) 

{
	int nr_rows;
	db_key_t q_cols[3], res_cols[1];
	db1_res_t *res= NULL;
	db_val_t q_vals[3];
	int n_query_cols= 0, n_res_cols=0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[n_query_cols] = &str_call_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->call_id;
	n_query_cols++;

	q_cols[n_query_cols] = &str_to_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->to_tag;
	n_query_cols++;

	q_cols[n_query_cols] = &str_from_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->from_tag;
	n_query_cols++;

	/* return the id column, even though don't actually need */
	res_cols[n_res_cols] = &str_id_col;	
	n_res_cols++;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(-1);
	}

	if(pua_dbf.query(pua_db, q_cols, 0, q_vals,
				res_cols,n_query_cols,n_res_cols,0,&res) < 0)
	{
		LM_ERR("DB query error\n");
		return(-1);
	}

	if (res == NULL)
	{
		LM_ERR("bad result\n");
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
	db_key_t q_cols[3], res_cols[2];
	db1_res_t *res= NULL;
	db_val_t q_vals[3];
	int n_query_cols=0, n_res_cols=0;
	db_val_t *values;	
	str *id;
	str to_tag;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[n_query_cols] = &str_call_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->call_id;
	n_query_cols++;

	q_cols[n_query_cols] = &str_from_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->from_tag;
	n_query_cols++;

	q_cols[n_query_cols] = &str_to_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->to_tag;
	n_query_cols++;

	res_cols[n_res_cols] = &str_pres_id_col;	
	n_res_cols++;

	*rec_id = NULL;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(-1);
	}

	if(pua_dbf.query(pua_db, q_cols, 0, q_vals,
				res_cols,n_query_cols,n_res_cols,0,&res) < 0)
	{
		LM_ERR("DB query error\n");
		return(-1);
	}

	if (res == NULL)
	{
		LM_ERR("bad result\n");
		return(-1);
	}

	nr_rows = RES_ROW_N(res);

	switch (nr_rows)
	{
	case 1:
		rows = RES_ROWS(res);
		values = ROW_VALUES(rows);
		break;

	case 0:
		/* no match */
		LM_DBG("No rows found. Looking for temporary dialog\n");
		pua_dbf.free_result(pua_db, res);

		n_query_cols--;

		res_cols[n_res_cols] = &str_to_tag_col;
		n_res_cols++;

		if(pua_dbf.query(pua_db, q_cols, 0, q_vals,
			res_cols,n_query_cols,n_res_cols,0,&res) < 0)
		{
			LM_ERR("DB query error\n");
			return(-1);
		}

		if (res == NULL)
		{
			LM_ERR("bad result\n");
			return(-1);
		}

		nr_rows = RES_ROW_N(res);

		if (nr_rows == 1)
		{
			rows = RES_ROWS(res);
			values = ROW_VALUES(rows);

			to_tag.s = (char *) VAL_STRING(values + 1);
			to_tag.len = strlen(to_tag.s);

			if (to_tag.len == 0 ||
				(to_tag.len > 0
				 && strncmp(to_tag.s, pres->to_tag.s, pres->to_tag.len) == 0))
			{
				LM_DBG( "Found a (possibly temporary) Dialog\n" );
				break;
			}
			else
				LM_WARN("Failed to find temporary dialog for To-tag: %.*s, found To-tag: %.*s\n",
					pres->to_tag.len, pres->to_tag.s, to_tag.len, to_tag.s);
		}

		if (nr_rows <= 1)
		{
			LM_DBG("Dialog not found\n" );
			pua_dbf.free_result(pua_db, res);
			return(0);
		}

		/* Fall-thru */

	default:
		LM_ERR("Too many rows found (%d)\n", nr_rows);
		pua_dbf.free_result(pua_db, res);
		return(-1);
	}

	id= (str*)pkg_malloc(sizeof(str));

	if(id== NULL)
	{
		LM_ERR("No more memory\n");
		pua_dbf.free_result(pua_db, res);
		return(-1);
	}

	id->s= (char*)pkg_malloc( strlen(VAL_STRING(values)) * sizeof(char));

	if(id->s== NULL)
	{
		LM_ERR("No more memory\n");
		pkg_free(id);
		pua_dbf.free_result(pua_db, res);
		return(-1);
	}

	memcpy(id->s, VAL_STRING(values), strlen(VAL_STRING(values)) );
	id->len= strlen(VAL_STRING(values));

	*rec_id= id;
	pua_dbf.free_result(pua_db, res);

	LM_DBG("Found id=%.*s\n", id->len, id->s);
	return(0);
}

/******************************************************************************/
int convert_temporary_dialog_puadb(ua_pres_t *pres)
{
	db_key_t query_cols[18];
	db_val_t query_vals[18];
	int n_query_cols = 0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* The columns I need to query to find the temporary dialog */
        query_cols[n_query_cols] = &str_pres_id_col;
        query_vals[n_query_cols].type = DB1_STR;
        query_vals[n_query_cols].nul = 0;
        query_vals[n_query_cols].val.str_val = pres->id;
        n_query_cols++;

	query_cols[n_query_cols] = &str_pres_uri_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = pres->pres_uri->s;
	query_vals[n_query_cols].val.str_val.len = pres->pres_uri->len;
	n_query_cols++;

	query_cols[n_query_cols] = &str_call_id_col;	
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = pres->call_id;
	n_query_cols++;

	query_cols[n_query_cols] = &str_from_tag_col;	
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = pres->from_tag;
	n_query_cols++;

	/* The columns I need to fill in to convert a temporary dialog to a dialog */
	query_cols[n_query_cols] = &str_expires_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = pres->expires;
	n_query_cols++;

	query_cols[n_query_cols] = &str_desired_expires_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = pres->desired_expires;
	n_query_cols++;

	query_cols[n_query_cols] = &str_flag_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = pres->flag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_to_tag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = pres->to_tag;
	n_query_cols++;

	query_cols[n_query_cols] = &str_cseq_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = pres->cseq;
	n_query_cols++;

	query_cols[n_query_cols] = &str_record_route_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = pres->record_route;
	n_query_cols++;

	query_cols[n_query_cols] = &str_contact_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = pres->contact;
	n_query_cols++;

	query_cols[n_query_cols] = &str_remote_contact_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = pres->remote_contact;
	n_query_cols++;

	query_cols[n_query_cols] = &str_version_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = pres->version;
	n_query_cols++;

	query_cols[n_query_cols] = &str_extra_headers_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	if (pres->extra_headers)
	{
		query_vals[n_query_cols].val.str_val.s = pres->extra_headers->s;
		query_vals[n_query_cols].val.str_val.len = pres->extra_headers->len;
	}
	else
	{
		query_vals[n_query_cols].val.str_val.s = "";
		query_vals[n_query_cols].val.str_val.len = 0;
	}
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;	
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = pres->event;
	n_query_cols++;

	query_cols[n_query_cols] = &str_watcher_uri_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0; 
	query_vals[n_query_cols].val.str_val.s = pres->watcher_uri->s;
	query_vals[n_query_cols].val.str_val.len = pres->watcher_uri->len;
	n_query_cols++;

	query_cols[n_query_cols] = &str_etag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0; 
	query_vals[n_query_cols].val.str_val.s = 0;
	query_vals[n_query_cols].val.str_val.len = 0;
	n_query_cols++;

	query_cols[n_query_cols] = &str_tuple_id_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0; 
	query_vals[n_query_cols].val.str_val.s = 0;
	query_vals[n_query_cols].val.str_val.len = 0;
	n_query_cols++;

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(-1);
	}

	if (pua_dbf.replace != NULL)
	{
		if (pua_dbf.replace(pua_db, query_cols, query_vals, n_query_cols,
					4, 0) < 0)
		{
			LM_ERR("Failed replace db\n");
			return -1;
		}
	}
	else
	{
		if (pua_dbf.update(pua_db, query_cols, 0, query_vals,
				query_cols + 4, query_vals + 4, 4, n_query_cols - 4) < 0)
		{
			LM_ERR("Failed update db\n");
			return -1;
		}

		LM_DBG("affected_rows: %d\n", pua_dbf.affected_rows(pua_db));
		if (pua_dbf.affected_rows(pua_db) == 0)
		{
			if (pua_dbf.insert(pua_db, query_cols, query_vals, n_query_cols) < 0)
			{
				LM_ERR("Failed insert db\n");
				return -1;
			}
		}
	}


	shm_free(pres->remote_contact.s);
	shm_free(pres);

	return 1;
}

/******************************************************************************/

int insert_record_puadb(ua_pres_t* pres)

{
	db_key_t db_cols[18];
	db_val_t db_vals[18];
	int n_cols= 0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	db_cols[n_cols] = &str_pres_uri_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.str_val.s = pres->pres_uri->s;
	db_vals[n_cols].val.str_val.len = pres->pres_uri->len;
	n_cols++;

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

	db_cols[n_cols] = &str_watcher_uri_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = "";
	db_vals[n_cols].val.str_val.len = 0;
	n_cols++;

	db_cols[n_cols] = &str_call_id_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = "";
	db_vals[n_cols].val.str_val.len = 0;
	n_cols++;

	db_cols[n_cols] = &str_to_tag_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = "";
	db_vals[n_cols].val.str_val.len = 0;
	n_cols++;

	db_cols[n_cols] = &str_from_tag_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = "";
	db_vals[n_cols].val.str_val.len = 0;
	n_cols++;

	db_cols[n_cols] = &str_cseq_col;
	db_vals[n_cols].type = DB1_INT;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.int_val = 0;
	n_cols++;

	db_cols[n_cols] = &str_record_route_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = "";
	db_vals[n_cols].val.str_val.len = 0;
	n_cols++;

	db_cols[n_cols] = &str_contact_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = "";
	db_vals[n_cols].val.str_val.len = 0;
	n_cols++;

	db_cols[n_cols] = &str_remote_contact_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = "";
	db_vals[n_cols].val.str_val.len = 0;
	n_cols++;

	db_cols[n_cols] = &str_version_col;
	db_vals[n_cols].type = DB1_INT;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.int_val = pres->version;
	n_cols++;

	db_cols[n_cols] = &str_extra_headers_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	if (pres->extra_headers)
	{
		db_vals[n_cols].val.str_val.s = pres->extra_headers->s;
		db_vals[n_cols].val.str_val.len = pres->extra_headers->len;
	}
	else
	{
		db_vals[n_cols].val.str_val.s = "";
		db_vals[n_cols].val.str_val.len = 0;
	}
	n_cols++;

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

ua_pres_t *get_record_puadb(str pres_id, str *etag, ua_pres_t *result, db1_res_t **dbres)
{
	db_key_t q_cols[2];
	db_val_t q_vals[2], *values;
	db_row_t *rows;
	db1_res_t *res;
	int n_query_cols = 0, nr_rows;
	db_query_f query_fn = pua_dbf.query_lock ? pua_dbf.query_lock : pua_dbf.query;

	q_cols[n_query_cols] = &str_pres_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres_id;
	n_query_cols++;

	if (etag != NULL)
	{
		q_cols[n_query_cols] = &str_etag_col;
		q_vals[n_query_cols].type = DB1_STR;
		q_vals[n_query_cols].nul = 0;
		q_vals[n_query_cols].val.str_val.s = etag->s;
		q_vals[n_query_cols].val.str_val.len = etag->len;
		n_query_cols++;
	}

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(NULL);
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(NULL);
	}

	if(query_fn(pua_db, q_cols, 0, q_vals,
				NULL,n_query_cols,0,0,&res) < 0)
	{
		LM_ERR("DB query error\n");
		return(NULL);
	}

	if (res == NULL)
	{
		LM_ERR("bad result\n");
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

	*dbres = res;

	return(result);
}

/******************************************************************************/

int delete_record_puadb(ua_pres_t *pres)
{
	db_key_t q_cols[2];
	db_val_t q_vals[2];
	int n_query_cols = 0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	q_cols[n_query_cols] = &str_pres_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->id;
	n_query_cols++;

	if (pres->etag.s)
	{
		q_cols[n_query_cols] = &str_etag_col;
		q_vals[n_query_cols].type = DB1_STR;
		q_vals[n_query_cols].nul = 0;
		q_vals[n_query_cols].val.str_val = pres->etag;
		n_query_cols++;
	}

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(-1);
	}

	if (pua_dbf.delete(pua_db, q_cols, 0, q_vals, n_query_cols) < 0) 
	{
		LM_ERR("deleting record\n");
		return -1;
	}

	return 1;
}

/******************************************************************************/

int update_record_puadb(ua_pres_t *pres, int expires, str *etag)
{
	db_key_t q_cols[2], u_cols[3];
	db_val_t q_vals[2], u_vals[3];
	int n_query_cols = 0, n_update_cols = 0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	q_cols[n_query_cols] = &str_pres_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->id;
	n_query_cols++;

	if (pres->etag.s)
	{
		q_cols[n_query_cols] = &str_etag_col;
		q_vals[n_query_cols].type = DB1_STR;
		q_vals[n_query_cols].nul = 0;
		q_vals[n_query_cols].val.str_val = pres->etag;
		n_query_cols++;
	}

	u_cols[n_update_cols] = &str_desired_expires_col;
	u_vals[n_update_cols].type = DB1_INT;
	u_vals[n_update_cols].nul = 0;
	u_vals[n_update_cols].val.int_val = pres->desired_expires;
	n_update_cols++;

	u_cols[n_update_cols] = &str_expires_col;
	u_vals[n_update_cols].type = DB1_INT;
	u_vals[n_update_cols].nul = 0;
	u_vals[n_update_cols].val.int_val = expires + (int) time(NULL);
	n_update_cols++;

	if (etag)
	{
		u_cols[n_update_cols] = &str_etag_col;
		u_vals[n_update_cols].type = DB1_STR;
		u_vals[n_update_cols].nul = 0;
		u_vals[n_update_cols].val.str_val.s = etag->s;
		u_vals[n_update_cols].val.str_val.len = etag->len;
		n_update_cols++;
	}

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(-1);
	}

	if (pua_dbf.update(pua_db, q_cols, 0, q_vals, u_cols, u_vals,
			n_query_cols, n_update_cols) < 0)
	{
		LM_ERR("updating dialog\n");
		return -1;
	}

	if (pua_dbf.affected_rows != NULL)
		return pua_dbf.affected_rows(pua_db);

	return 1;
}

/******************************************************************************/

int insert_dialog_puadb(ua_pres_t* pres)

{
	db_key_t db_cols[18];
	db_val_t db_vals[18];
	int n_cols= 0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	db_cols[n_cols] = &str_pres_uri_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0;
	db_vals[n_cols].val.str_val.s = pres->pres_uri->s;
	db_vals[n_cols].val.str_val.len = pres->pres_uri->len;
	n_cols++;

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

	db_cols[n_cols] = &str_etag_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = "";
	db_vals[n_cols].val.str_val.len = 0;
	n_cols++;

	db_cols[n_cols] = &str_tuple_id_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = "";
	db_vals[n_cols].val.str_val.len = 0;
	n_cols++;

	db_cols[n_cols] = &str_watcher_uri_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	db_vals[n_cols].val.str_val.s = pres->watcher_uri->s;
	db_vals[n_cols].val.str_val.len = pres->watcher_uri->len;
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

	db_cols[n_cols] = &str_extra_headers_col;
	db_vals[n_cols].type = DB1_STR;
	db_vals[n_cols].nul = 0; 
	if (pres->extra_headers)
	{
		db_vals[n_cols].val.str_val.s = pres->extra_headers->s;
		db_vals[n_cols].val.str_val.len = pres->extra_headers->len;
	}
	else
	{
		db_vals[n_cols].val.str_val.s = "";
		db_vals[n_cols].val.str_val.len = 0;
	}
	n_cols++;

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

ua_pres_t *get_dialog_puadb(str pres_id, str *pres_uri, ua_pres_t *result, db1_res_t **dbres)
{
	db_key_t q_cols[2];
	db_val_t q_vals[2], *values;
	db_row_t *rows;
	db1_res_t *res;
	int n_query_cols = 0, nr_rows;
	db_query_f query_fn = pua_dbf.query_lock ? pua_dbf.query_lock : pua_dbf.query;

	if (pres_uri == NULL)
	{
		LM_ERR("Attempting to search for a dialog without specifying pres_uri\n");
		return(NULL);
	}

	q_cols[n_query_cols] = &str_pres_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres_id;
	n_query_cols++;

	q_cols[n_query_cols] = &str_pres_uri_col;
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val.s = pres_uri->s;
	q_vals[n_query_cols].val.str_val.len = pres_uri->len;
	n_query_cols++;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(NULL);
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(NULL);
	}

	if(query_fn(pua_db, q_cols, 0, q_vals,
				NULL,n_query_cols,0,0,&res) < 0)
	{
		LM_ERR("DB query error\n");
		return(NULL);
	}

	if (res == NULL)
	{
		LM_ERR("bad result\n");
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
	else if (nr_rows > 1)
	{
		LM_ERR("Too many rows found (%d)... deleting\n", nr_rows);
		pua_dbf.free_result(pua_db, res);

		if (pua_dbf.delete(pua_db, q_cols, 0, q_vals, n_query_cols) < 0) 
			LM_ERR("deleting record(s)\n");

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

int delete_dialog_puadb(ua_pres_t *pres)
{
	db_key_t q_cols[3];
	db_val_t q_vals[3];
	int n_query_cols = 0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	q_cols[n_query_cols] = &str_call_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->call_id;
	n_query_cols++;

	q_cols[n_query_cols] = &str_from_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->from_tag;
	n_query_cols++;

	if (pres->to_tag.len > 0 && pres->to_tag.s != NULL)
	{
		q_cols[n_query_cols] = &str_to_tag_col;	
		q_vals[n_query_cols].type = DB1_STR;
		q_vals[n_query_cols].nul = 0;
		q_vals[n_query_cols].val.str_val = pres->to_tag;
		n_query_cols++;
	}
	
	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(-1);
	}

	if (pua_dbf.delete(pua_db, q_cols, 0, q_vals, n_query_cols) < 0) 
	{
		LM_ERR("deleting record\n");
		return -1;
	}

	return 1;
}

/******************************************************************************/

int update_dialog_puadb(ua_pres_t *pres, int expires, str *contact)
{
	db_key_t q_cols[3], u_cols[4];
	db_val_t q_vals[3], u_vals[4];
	int n_query_cols = 0, n_update_cols = 0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	q_cols[n_query_cols] = &str_call_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->call_id;
	n_query_cols++;

	q_cols[n_query_cols] = &str_from_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->from_tag;
	n_query_cols++;

	q_cols[n_query_cols] = &str_to_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->to_tag;
	n_query_cols++;

	u_cols[n_update_cols] = &str_desired_expires_col;
	u_vals[n_update_cols].type = DB1_INT;
	u_vals[n_update_cols].nul = 0;
	u_vals[n_update_cols].val.int_val = pres->desired_expires;
	n_update_cols++;

	u_cols[n_update_cols] = &str_expires_col;
	u_vals[n_update_cols].type = DB1_INT;
	u_vals[n_update_cols].nul = 0;
	u_vals[n_update_cols].val.int_val = expires + (int) time(NULL);
	n_update_cols++;

	u_cols[n_update_cols] = &str_cseq_col;
	u_vals[n_update_cols].type = DB1_INT;
	u_vals[n_update_cols].nul = 0;
	u_vals[n_update_cols].val.int_val = pres->cseq;
	n_update_cols++;

	u_cols[n_update_cols] = &str_remote_contact_col;
	u_vals[n_update_cols].type = DB1_STR;
	u_vals[n_update_cols].nul = 0;
	u_vals[n_update_cols].val.str_val.s = contact->s;
	u_vals[n_update_cols].val.str_val.len = contact->len;
	n_update_cols++;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(-1);
	}

	if (pua_dbf.update(pua_db, q_cols, 0, q_vals, u_cols, u_vals,
			n_query_cols, n_update_cols) < 0)
	{
		LM_ERR("updating dialog\n");
		return -1;
	}

	return 1;
}

/******************************************************************************/

int update_contact_puadb(ua_pres_t *pres, str *contact) 

{
	db_key_t q_cols[3], db_cols[1];
	db_val_t q_vals[3], db_vals[1];
	int n_query_cols= 0, n_update_cols=0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[n_query_cols] = &str_call_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->call_id;
	n_query_cols++;

	q_cols[n_query_cols] = &str_to_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->to_tag;
	n_query_cols++;

	q_cols[n_query_cols] = &str_from_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->from_tag;
	n_query_cols++;

	/* we overwrite contact even if not changed */
	db_cols[n_update_cols] = &str_remote_contact_col;
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

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(-1);
	}

	if(pua_dbf.update(pua_db, q_cols, 0, q_vals,
				db_cols,db_vals,n_query_cols,n_update_cols) < 0)
	{
		LM_ERR("DB update failed\n");
		return(-1);
	}

	return(0);
}


/******************************************************************************/

int update_version_puadb(ua_pres_t *pres) 

{
	db_key_t q_cols[3], db_cols[1];
	db_val_t q_vals[3], db_vals[1];
	int n_query_cols= 0, n_update_cols=0;

	if (pres==NULL)
	{
		LM_ERR("called with NULL param\n");
		return(-1);
	}

	/* cols and values used for search query */
	q_cols[n_query_cols] = &str_call_id_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->call_id;
	n_query_cols++;

	q_cols[n_query_cols] = &str_to_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->to_tag;
	n_query_cols++;

	q_cols[n_query_cols] = &str_from_tag_col;	
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val = pres->from_tag;
	n_query_cols++;

	/* we overwrite contact even if not changed */
	db_cols[n_update_cols] = &str_version_col;
	db_vals[n_update_cols].type = DB1_INT;
	db_vals[n_update_cols].nul = 0;
	db_vals[n_update_cols].val.int_val = pres->version;
	n_update_cols++;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return(-1);
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(-1);
	}

	if(pua_dbf.update(pua_db, q_cols, 0, q_vals,
				db_cols,db_vals,n_query_cols,n_update_cols) < 0)

	{
		LM_ERR("DB update failed\n");
		return(-1);
	}

	return(0);
}

/******************************************************************************/

list_entry_t *get_subs_list_puadb(str *did)
{
	list_entry_t *list = NULL;
	db_key_t q_cols[1], res_cols[1];
	db1_res_t *res= NULL;
	db_val_t q_vals[1];
	int n_query_cols= 0, n_res_cols = 0;

	/* cols and values used for search query */
	q_cols[n_query_cols] = &str_pres_id_col;
	q_vals[n_query_cols].type = DB1_STR;
	q_vals[n_query_cols].nul = 0;
	q_vals[n_query_cols].val.str_val.s = did->s;
	q_vals[n_query_cols].val.str_val.len = did->len;
	n_query_cols++;

	res_cols[n_res_cols] = &str_pres_uri_col;
	n_res_cols++;

	if(pua_db == NULL)
	{
		LM_ERR("null database connection\n");
		return list;
	}

	if (pua_dbf.use_table(pua_db, &db_table) < 0)
	{
		LM_ERR("error in use_table pua\n");
		return(list);
	}

	if(db_fetch_query(&pua_dbf, pua_fetch_rows, pua_db, q_cols, 0,
				q_vals, res_cols, n_query_cols, n_res_cols, 0, &res) < 0)
	{
		LM_ERR("DB query error\n");
		return list;
	}

	if (res == NULL)
	{
		LM_ERR("bad result\n");
		return list;
	}

	if (RES_ROW_N(res) == 0)
	{
		LM_INFO( "No records found\n");
		pua_dbf.free_result(pua_db, res);
		return list;
	}

	do {
		int i, nr_rows;
		db_row_t *rows;
		nr_rows = RES_ROW_N(res);
		rows = RES_ROWS(res);

		for (i=0; i < nr_rows; i++)
		{
			str strng, *tmp_str;
			strng.s = (char *) VAL_STRING(ROW_VALUES(rows+i));
			strng.len = strlen(VAL_STRING(ROW_VALUES(rows+i)));

			if ((tmp_str = (str *)pkg_malloc(sizeof(str))) == NULL)
			{
				LM_ERR("out of private memory\n");
				pua_dbf.free_result(pua_db, res);
				return list;
			}
			if ((tmp_str->s = (char *)pkg_malloc(sizeof(char) * strng.len + 1)) == NULL)
			{
				pkg_free(tmp_str);
				LM_ERR("out of private memory\n");
				pua_dbf.free_result(pua_db, res);
				return list;
			}
			memcpy(tmp_str->s, strng.s, strng.len);
			tmp_str->len = strng.len;
			tmp_str->s[tmp_str->len] = '\0';

			list = list_insert(tmp_str, list, NULL);
		}
	} while ((db_fetch_next(&pua_dbf, pua_fetch_rows, pua_db, &res)==1)
			&& (RES_ROWS(res)>0));

	pua_dbf.free_result(pua_db, res);
	
	return list;
}

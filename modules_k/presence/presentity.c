/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-08-15  initial version (anca)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../db/db.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../str.h"
#include "../alias_db/alias_db.h"
#include "presentity.h"
#include "presence.h" 
#include "notify.h"
#include "publish.h"

extern int use_db;
extern char* presentity_table;
extern db_con_t* pa_db;
extern db_func_t pa_dbf;

presentity_t* new_presentity( str* domain,str* user,int expires, 
		ev_t* event, str* etag, str* sender)
{
	presentity_t *presentity;
	int size;
	
	/* alocating memory for presentity */
	size = sizeof(presentity_t)+ domain->len+ user->len+ etag->len + 1;
	if(sender)
		size+= sizeof(str)+ sender->len* sizeof(char);
	
	presentity = (presentity_t*)pkg_malloc(size);
	if(presentity == NULL)
	{
		LOG(L_ERR, "PRESENCE:new_presentity: No memory left: size=%d\n", size);
		return NULL;
	}
	memset(presentity, 0, size);
	size= sizeof(presentity_t);

	presentity->domain.s = (char*)((char*)presentity+ size);
	strncpy(presentity->domain.s, domain->s, domain->len);
	presentity->domain.len = domain->len;
	size+= domain->len;	
	
	presentity->user.s = (char*)((char*)presentity+size);
	strncpy(presentity->user.s, user->s, user->len);
	presentity->user.len = user->len;
	size+= user->len;

	presentity->etag.s = ((char*)presentity)+ size;
	strncpy(presentity->etag.s, etag->s, etag->len);
	presentity->etag.len = etag->len;
	size+= etag->len;
	
	if(sender)
	{
		presentity->sender= (str*)((char*)presentity+ size);
		size+= sizeof(str);
		presentity->sender->s= (char*)presentity + size;
		memcpy(presentity->sender->s, sender->s, sender->len);
		presentity->sender->len= sender->len;
		size+= sender->len;
	}

	presentity->event= event;
	presentity->expires = expires;
	presentity->received_time= (int)time(NULL);
	return presentity;
    
}

int update_presentity(presentity_t* presentity, str* body, int new_t )
{
	str res_body;
	db_key_t query_cols[8];
	db_op_t  query_ops[8];
	db_val_t query_vals[8], update_vals[4];
	db_key_t result_cols[4], update_keys[4];
	db_res_t *result= NULL;
	int ret_code= 0;
	int n_query_cols = 0;
	int n_result_cols = 0;
	int n_update_cols = 0;
	int body_col;
	char* status = NULL;

	if( !use_db )
		return 0;

	query_cols[n_query_cols] = "domain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = presentity->domain.s;
	query_vals[n_query_cols].val.str_val.len = presentity->domain.len;
	n_query_cols++;
	
	query_cols[n_query_cols] = "username";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = presentity->user.s;
	query_vals[n_query_cols].val.str_val.len = presentity->user.len;
	n_query_cols++;

	query_cols[n_query_cols] = "event";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->event->stored_name;
	n_query_cols++;

	query_cols[n_query_cols] = "etag";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = presentity->etag.s;
	query_vals[n_query_cols].val.str_val.len = presentity->etag.len;
	n_query_cols++;

	if(presentity->expires == 0) 
	{
		query_db_notify( &presentity->user, &presentity->domain, 
				presentity->event, NULL, &presentity->etag, presentity->sender);
	
		if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
		{
			LOG(L_ERR, "PRESENCE:update_presentity: Error in use_table\n");
			goto error;
		}
		DBG("PRESENCE:update_presentity: expires =0 -> deleting"
				" from database\n");
		if(pa_dbf.delete(pa_db, query_cols, 0 ,query_vals,n_query_cols)< 0 )
		{
			DBG( "PRESENCE:update_presentity: ERROR cleaning"
					" unsubscribed messages\n");
		}
		DBG("PRESENCE:update_presentity:delete from db %.*s\n",
				presentity->user.len,presentity->user.s );
		return 1;
	}

	if(new_t) /* daca a fost generat un nou etag insereaza */
	{
		/* insert new record into database */	
				
		query_cols[n_query_cols] = "expires";
		query_vals[n_query_cols].type = DB_INT;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.int_val = presentity->expires+
				(int)time(NULL);
		n_query_cols++;

		query_cols[n_query_cols] = "body";
		query_vals[n_query_cols].type = DB_BLOB;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val.s = body->s;
		query_vals[n_query_cols].val.str_val.len = body->len;
		n_query_cols++;
		
		query_cols[n_query_cols] = "received_time";
		query_vals[n_query_cols].type = DB_INT;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.int_val = presentity->received_time;
		n_query_cols++;

		if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
		{
			LOG(L_ERR, "PRESENCE:update_presentity: Error in use_table\n");
			goto error;
		}

		DBG( "PRESENCE:update_presentity: inserting %d cols into"
				"table\n",
				n_query_cols);
				
		if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) 
		{
			LOG(L_ERR, "PRESENCE:update_presentity: Error while"
					" inserting new presentity\n");
			goto error;
		}
		/* send notify with presence information */
			if (query_db_notify(&presentity->user, &presentity->domain,
						presentity->event, NULL, NULL, presentity->sender)<0)
			{
				DBG(" PRESENCE:update_presentity:Could not send"
						" notify for event presence\n");
			}
	}
	else
	{
		result_cols[body_col=n_result_cols++] = "body" ;
		if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
		{
			LOG(L_ERR, "PRESENCE:update_presentity: Error in use_table\n");
			goto error;
		}

		DBG("PRESENCE:update_presentity: querying presentity  \n");
		if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
			 result_cols, n_query_cols, n_result_cols, 0, &result) < 0) 
		{
			LOG(L_ERR, "PRESENCE:update_presentity: Error while querying"
					" presentity\n");
			goto error;
		}
		if(result== NULL)
			goto error;

		if (result->n > 0)
		{
			n_update_cols= 0;
			update_keys[n_update_cols] = "expires";
			update_vals[n_update_cols].type = DB_INT;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.int_val = presentity->expires + (int)time(NULL);
			n_update_cols++;

			update_keys[n_update_cols] = "received_time";
			update_vals[n_update_cols].type = DB_INT;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.int_val = presentity->received_time;
			n_update_cols++;

			if(body==NULL || body->s==NULL) /* if there is no body update expires value
											   and sender if present */
			{
				if( pa_dbf.update( pa_db,query_cols, query_ops,
						query_vals, update_keys, update_vals, n_query_cols,n_update_cols)<0) 
				{
					LOG( L_ERR , "PRESENCE:update_presentity:ERROR while"
							" updating presence information\n");
					goto error;
				}
				pa_dbf.free_result(pa_db, result);
				return 0;
			}

			db_row_t *row = &result->rows[0];
			db_val_t *row_vals = ROW_VALUES(row);
			res_body.s = row_vals[body_col].val.str_val.s;	
			res_body.len = row_vals[body_col].val.str_val.len;
			
			//	update_xml( &res_body, body);
			/* write the new body*/
			update_keys[n_update_cols] = "body";
			update_vals[n_update_cols].type = DB_BLOB;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.str_val.s = body->s;
			update_vals[n_update_cols].val.str_val.len=body->len;
			n_update_cols++;
		
			if( pa_dbf.update( pa_db,query_cols, query_ops, query_vals,
			update_keys,update_vals, n_query_cols, n_update_cols )<0) 
			{
				LOG( L_ERR , "PRESENCE:update_presentity: ERROR while"
						" updating presence information\n");
				goto error;
			}

			pa_dbf.free_result(pa_db, result);
			result= NULL;

			/* presentity body is updated so send notify to all watchers */
			if (query_db_notify(&presentity->user, &presentity->domain,
						presentity->event, NULL, NULL, presentity->sender)<0)
			{
				LOG(L_ERR," PRESENCE:update_presentity: Could not send Notify\n");
			}
		}  
		else  /* if there isn't no registration with those 3 values */
		{
			LOG(L_DBG, "PRESENCE:update_presentity: No E_Tag match\n");
			ret_code=  412;
		}
	}
	
	if(status)
		xmlFree(status);

	return ret_code;

error:
	LOG(L_ERR, "PRESENCE:update_presentity: ERROR occured\n");
	if(result)
	{
		pa_dbf.free_result(pa_db, result);
		result= NULL;
	}
	if(status)
		xmlFree(status);
	return -1;

}

void free_presentity(presentity_t* presentity)
{
	pkg_free(presentity);
}



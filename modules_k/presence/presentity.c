/*
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *
 * History:
 * --------
 *  2006-08-15  initial version (Anca Vamanu)
 */

/*!
 * \file
 * \brief Kamailio presence module :: Presentity handling
 * \ingroup presence 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../lib/srdb1/db.h"
#include "../../hashes.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../str.h"
#include "../alias_db/alias_db.h"
#include "../../data_lump_rpl.h"
#include "presentity.h"
#include "presence.h" 
#include "notify.h"
#include "publish.h"
#include "hash.h"
#include "utils_func.h"


xmlNodePtr xmlNodeGetNodeByName(xmlNodePtr node, const char *name,
													const char *ns);
static str pu_200_rpl  = str_init("OK");
static str pu_412_rpl  = str_init("Conditional request failed");

extern int pres_fetch_rows;

#define ETAG_LEN  128

char* generate_ETag(int publ_count)
{
	char* etag= NULL;
	int size = 0;

	etag = (char*)pkg_malloc(ETAG_LEN*sizeof(char));
	if(etag ==NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memset(etag, 0, ETAG_LEN*sizeof(char));
	size = snprintf (etag, ETAG_LEN, "%c.%d.%d.%d.%d",prefix, startup_time, pid, counter, publ_count);
	if( size <0 )
	{
		LM_ERR("unsuccessfull snprintf\n ");
		pkg_free(etag);
		return NULL;
	}
	if(size >= ETAG_LEN)
	{
		LM_ERR("buffer size overflown\n");
		pkg_free(etag);
		return NULL;
	}

	etag[size] = '\0';
	LM_DBG("etag= %s / %d\n ",etag, size);
	return etag;

error:
	return NULL;

}

int publ_send200ok(struct sip_msg *msg, int lexpire, str etag)
{
	char buf[128];
	int buf_len= 128, size;
	str hdr_append= {0, 0}, hdr_append2= {0, 0} ;

	LM_DBG("send 200OK reply\n");	
	LM_DBG("etag= %s - len= %d\n", etag.s, etag.len);
	
	hdr_append.s = buf;
	hdr_append.s[0]='\0';
	hdr_append.len = snprintf(hdr_append.s, buf_len, "Expires: %d\r\n",
			((lexpire==0)?0:(lexpire-expires_offset)));
	if(hdr_append.len < 0)
	{
		LM_ERR("unsuccessful snprintf\n");
		goto error;
	}
	if(hdr_append.len >= buf_len)
	{
		LM_ERR("buffer size overflown\n");
		goto error;
	}
	hdr_append.s[hdr_append.len]= '\0';
		
	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	size= sizeof(char)*(20+etag.len) ;
	hdr_append2.s = (char *)pkg_malloc(size);
	if(hdr_append2.s == NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	hdr_append2.s[0]='\0';
	hdr_append2.len = snprintf(hdr_append2.s, size, "SIP-ETag: %s\r\n", etag.s);
	if(hdr_append2.len < 0)
	{
		LM_ERR("unsuccessful snprintf\n ");
		goto error;
	}
	if(hdr_append2.len >= size)
	{
		LM_ERR("buffer size overflown\n");
		goto error;
	}

	hdr_append2.s[hdr_append2.len]= '\0';
	if (add_lump_rpl(msg, hdr_append2.s, hdr_append2.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	if(slb.freply(msg, 200, &pu_200_rpl) < 0)
	{
		LM_ERR("sending reply\n");
		goto error;
	}

	pkg_free(hdr_append2.s);
	return 0;

error:

	if(hdr_append2.s)
		pkg_free(hdr_append2.s);

	return -1;
}	
presentity_t* new_presentity( str* domain,str* user,int expires, 
		pres_ev_t* event, str* etag, str* sender)
{
	presentity_t *presentity= NULL;
	int size, init_len;
	
	/* allocating memory for presentity */
	size = sizeof(presentity_t)+ domain->len+ user->len+ etag->len +1;
	if(sender)
		size+= sizeof(str)+ sender->len* sizeof(char);
	
	init_len= size;

	presentity = (presentity_t*)pkg_malloc(size);
	if(presentity == NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memset(presentity, 0, size);
	size= sizeof(presentity_t);

	presentity->domain.s = (char*)presentity+ size;
	strncpy(presentity->domain.s, domain->s, domain->len);
	presentity->domain.len = domain->len;
	size+= domain->len;	
	
	presentity->user.s = (char*)presentity+size;
	strncpy(presentity->user.s, user->s, user->len);
	presentity->user.len = user->len;
	size+= user->len;

	presentity->etag.s = (char*)presentity+ size;
	memcpy(presentity->etag.s, etag->s, etag->len);
	presentity->etag.s[etag->len]= '\0';
	presentity->etag.len = etag->len;

	size+= etag->len+1;
	
	if(sender)
	{
		presentity->sender= (str*)((char*)presentity+ size);
		size+= sizeof(str);
		presentity->sender->s= (char*)presentity + size;
		memcpy(presentity->sender->s, sender->s, sender->len);
		presentity->sender->len= sender->len;
		size+= sender->len;
	}

	if(size> init_len)
	{
		LM_ERR("buffer size overflow init_len= %d, size= %d\n", init_len, size);
		goto error;
	}
	presentity->event= event;
	presentity->expires = expires;
	presentity->received_time= (int)time(NULL);
	return presentity;
    
error:
	if(presentity)
		pkg_free(presentity);
	return NULL;
}

xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name)
{
	xmlNodePtr cur = node->children;
	while (cur) {
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

int check_if_dialog(str body, int *is_dialog)
{
	xmlDocPtr doc;
	xmlNodePtr node;

	doc = xmlParseMemory(body.s, body.len);
	if(doc== NULL)
	{
		LM_ERR("failed to parse xml document\n");
		return -1;
	}

	node = doc->children;
	node = xmlNodeGetChildByName(node, "dialog");

	if(node == NULL)
		*is_dialog = 0;
	else
		*is_dialog = 1;

	xmlFreeDoc(doc);
	return 0;
}


int update_presentity(struct sip_msg* msg, presentity_t* presentity, str* body,
		int new_t, int* sent_reply, char* sphere)
{
	db_key_t query_cols[12], update_keys[8], result_cols[5];
	db_op_t  query_ops[12];
	db_val_t query_vals[12], update_vals[8];
	db1_res_t *result= NULL;
	int n_query_cols = 0;
	int n_update_cols = 0;
	char* dot= NULL;
	str etag= {0, 0};
	str cur_etag= {0, 0};
	str* rules_doc= NULL;
	str pres_uri= {0, 0};
	int rez_body_col, rez_sender_col, n_result_cols= 0;
	db_row_t *row = NULL ;
	db_val_t *row_vals = NULL;
	str old_body, sender;
	int is_dialog= 0, bla_update_publish= 1;

	*sent_reply= 0;
	if(presentity->event->req_auth)
	{
		/* get rules_document */
		if(presentity->event->get_rules_doc(&presentity->user,
					&presentity->domain, &rules_doc))
		{
			LM_ERR("getting rules doc\n");
			goto error;
		}
	}
	
	if(uandd_to_uri(presentity->user, presentity->domain, &pres_uri)< 0)
	{
		LM_ERR("constructing uri from user and domain\n");
		goto error;
	}


	query_cols[n_query_cols] = &str_domain_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->domain;
	n_query_cols++;
	
	query_cols[n_query_cols] = &str_username_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->event->name;
	n_query_cols++;

	query_cols[n_query_cols] = &str_etag_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->etag;
	n_query_cols++;

	result_cols[rez_body_col= n_result_cols++] = &str_body_col;
	result_cols[rez_sender_col= n_result_cols++] = &str_sender_col;

	if(new_t) 
	{
		/* insert new record in hash_table */

		if ( publ_cache_enabled &&
			insert_phtable(&pres_uri, presentity->event->evp->type, sphere)< 0)
		{
			LM_ERR("inserting record in hash table\n");
			goto error;
		}
		
		/* insert new record into database */	
		query_cols[n_query_cols] = &str_expires_col;
		query_vals[n_query_cols].type = DB1_INT;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.int_val = presentity->expires+
				(int)time(NULL);
		n_query_cols++;
	
		query_cols[n_query_cols] = &str_sender_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		if(presentity->sender)
		{
			query_vals[n_query_cols].val.str_val.s = presentity->sender->s;
			query_vals[n_query_cols].val.str_val.len = presentity->sender->len;
		} else {
			query_vals[n_query_cols].val.str_val.s = "";
			query_vals[n_query_cols].val.str_val.len = 0;
		}
		n_query_cols++;

		query_cols[n_query_cols] = &str_body_col;
		query_vals[n_query_cols].type = DB1_BLOB;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = *body;
		n_query_cols++;
		
		query_cols[n_query_cols] = &str_received_time_col;
		query_vals[n_query_cols].type = DB1_INT;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.int_val = presentity->received_time;
		n_query_cols++;
		
		if (pa_dbf.use_table(pa_db, &presentity_table) < 0) 
		{
			LM_ERR("unsuccessful use_table\n");
			goto error;
		}

		LM_DBG("inserting %d cols into table\n",n_query_cols);
				
		if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) 
		{
			LM_ERR("inserting new record in database\n");
			goto error;
		}
		if( publ_send200ok(msg, presentity->expires, presentity->etag)< 0)
		{
			LM_ERR("sending 200OK\n");
			goto error;
		}
		*sent_reply= 1;
		goto send_notify;
	}
	else
	{	
		if (pa_dbf.use_table(pa_db, &presentity_table) < 0) 
		{
			LM_ERR("unsuccessful sql use table\n");
			goto error;
		}

		if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
			 result_cols, n_query_cols, n_result_cols, 0, &result) < 0) 
		{
			LM_ERR("unsuccessful sql query\n");
			goto error;
		}
		if(result== NULL)
			goto error;

		if (result->n > 0)
		{

			if(EVENT_DIALOG_SLA(presentity->event->evp))
			{
				/* analize if previous body has a dialog */
				row = &result->rows[0];
				row_vals = ROW_VALUES(row);

				old_body.s = (char*)row_vals[rez_body_col].val.string_val;
				old_body.len = strlen(old_body.s);
				if(check_if_dialog(*body, &is_dialog)< 0)
				{
					LM_ERR("failed to check if dialog stored\n");
					goto error;
				}

				if(is_dialog== 1)  /* if the new body has a dialog - overwrite */
					goto after_dialog_check;

				if(check_if_dialog(old_body, &is_dialog)< 0)
				{
					LM_ERR("failed to check if dialog stored\n");
					goto error;
				}

				if(is_dialog==0 ) /* if the old body has no dialog - overwrite */
					goto after_dialog_check;

				sender.s = (char*)row_vals[rez_sender_col].val.string_val;
				sender.len= strlen(sender.s);

				LM_DBG("old_sender = %.*s\n", sender.len, sender.s );
				if(presentity->sender)
				{
					if(!(presentity->sender->len == sender.len && 
					strncmp(presentity->sender->s, sender.s, sender.len)== 0))
						 bla_update_publish= 0;
				}
			}
after_dialog_check:

			pa_dbf.free_result(pa_db, result);
			result= NULL;
			if(presentity->expires == 0) 
			{
				if( publ_send200ok(msg, presentity->expires, presentity->etag)< 0)
				{
					LM_ERR("sending 200OK reply\n");
					goto error;
				}
				*sent_reply= 1;
				if( publ_notify( presentity, pres_uri, body, &presentity->etag, rules_doc)< 0 )
				{
					LM_ERR("while sending notify\n");
					goto error;
				}
				
				if (pa_dbf.use_table(pa_db, &presentity_table) < 0) 
				{
					LM_ERR("unsuccessful sql use table\n");
					goto error;
				}

				LM_DBG("expires =0 -> deleting from database\n");
				if(pa_dbf.delete(pa_db,query_cols,0,query_vals,n_query_cols)<0)
				{
					LM_ERR("unsuccessful sql delete operation");
					goto error;
				}
				LM_DBG("deleted from db %.*s\n",	presentity->user.len,
						presentity->user.s);

				/* delete from hash table */
				if( publ_cache_enabled &&
					delete_phtable(&pres_uri, presentity->event->evp->type)< 0)
				{
					LM_ERR("deleting record from hash table\n");
					goto error;
				}
				goto done;
			}

			n_update_cols= 0;
			/* if event dialog and is_dialog -> if sender not the same as
			 * old sender do not overwrite */
			if( EVENT_DIALOG_SLA(presentity->event->evp) &&  bla_update_publish==0)
			{
				LM_DBG("drop Publish for BLA from a different sender that"
						" wants to overwrite an existing dialog\n");
				LM_DBG("sender = %.*s\n",  presentity->sender->len, presentity->sender->s );
				if( publ_send200ok(msg, presentity->expires, presentity->etag)< 0)
				{
					LM_ERR("sending 200OK reply\n");
					goto error;
				}
				*sent_reply= 1;
				goto done;
			}

			if(presentity->event->etag_not_new== 0)
			{	
				/* generate another etag */
				unsigned int publ_nr;
				str str_publ_nr= {0, 0};

				dot= presentity->etag.s+ presentity->etag.len;
				while(*dot!= '.' && str_publ_nr.len< presentity->etag.len)
				{
					str_publ_nr.len++;
					dot--;
				}
				if(str_publ_nr.len== presentity->etag.len)
				{
					LM_ERR("wrong etag\n");
					goto error;
				}	
				str_publ_nr.s= dot+1;
				str_publ_nr.len--;
	
				if( str2int(&str_publ_nr, &publ_nr)< 0)
				{
					LM_ERR("converting string to int\n");
					goto error;
				}
				etag.s = generate_ETag(publ_nr+1);
				if(etag.s == NULL)
				{
					LM_ERR("while generating etag\n");
					goto error;
				}
				etag.len=(strlen(etag.s));
				
				cur_etag= etag;

				update_keys[n_update_cols] = &str_etag_col;
				update_vals[n_update_cols].type = DB1_STR;
				update_vals[n_update_cols].nul = 0;
				update_vals[n_update_cols].val.str_val = etag;
				n_update_cols++;

			}
			else
				cur_etag= presentity->etag;
			
			update_keys[n_update_cols] = &str_expires_col;
			update_vals[n_update_cols].type = DB1_INT;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.int_val= presentity->expires +
				(int)time(NULL);
			n_update_cols++;

			update_keys[n_update_cols] = &str_received_time_col;
			update_vals[n_update_cols].type = DB1_INT;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.int_val= presentity->received_time;
			n_update_cols++;

			if(body && body->s)
			{
				update_keys[n_update_cols] = &str_body_col;
				update_vals[n_update_cols].type = DB1_BLOB;
				update_vals[n_update_cols].nul = 0;
				update_vals[n_update_cols].val.str_val = *body;
				n_update_cols++;

				/* updated stored sphere */
				if(sphere_enable && 
						presentity->event->evp->type== EVENT_PRESENCE)
				{
					if( publ_cache_enabled &&
							update_phtable(presentity, pres_uri, *body)< 0)
					{
						LM_ERR("failed to update sphere for presentity\n");
						goto error;
					}
				}
			}
			
			if( presentity->sender)
			{
				update_keys[n_update_cols] = &str_sender_col;
				update_vals[n_update_cols].type = DB1_STR;
				update_vals[n_update_cols].nul = 0;
				update_vals[n_update_cols].val.str_val = *presentity->sender;
				n_update_cols++;
			}

			if( pa_dbf.update( pa_db,query_cols, query_ops, query_vals,
					update_keys, update_vals, n_query_cols, n_update_cols )<0) 
			{
				LM_ERR("updating published info in database\n");
				goto error;
			}
			
			/* send 200OK */
			if( publ_send200ok(msg, presentity->expires, cur_etag)< 0)
			{
				LM_ERR("sending 200OK reply\n");
				goto error;
			}
			*sent_reply= 1;
			
			if(etag.s)
				pkg_free(etag.s);
			etag.s= NULL;
			
			if(!body)
				goto done;
		
			goto send_notify;
		}  
		else  /* if there isn't no registration with those 3 values */
		{
			pa_dbf.free_result(pa_db, result);
			result= NULL;
			LM_ERR("No E_Tag match\n");
			if (slb.freply(msg, 412, &pu_412_rpl) < 0)
			{
				LM_ERR("sending '412 Conditional request failed' reply\n");
				goto error;
			}
			*sent_reply= 1;
			goto done;
		}
	}

send_notify:
			
	/* send notify with presence information */
	if (publ_notify(presentity, pres_uri, body, NULL, rules_doc)<0)
	{
		LM_ERR("while sending Notify requests to watchers\n");
		goto error;
	}

done:
	if(rules_doc)
	{
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}
	if(pres_uri.s)
		pkg_free(pres_uri.s);

	return 0;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	if(etag.s)
		pkg_free(etag.s);
	if(rules_doc)
	{
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}
	if(pres_uri.s)
		pkg_free(pres_uri.s);

	return -1;

}

int pres_htable_restore(void)
{
	/* query all records from presentity table and insert records 
	 * in presentity table */
	db_key_t result_cols[6];
	db1_res_t *result= NULL;
	db_row_t *row= NULL ;
	db_val_t *row_vals;
	int  i;
	str user, domain, ev_str, uri, body;
	int n_result_cols= 0;
	int user_col, domain_col, event_col, expires_col, body_col = 0;
	int event;
	event_t ev;
	char* sphere= NULL;

	result_cols[user_col= n_result_cols++]= &str_username_col;
	result_cols[domain_col= n_result_cols++]= &str_domain_col;
	result_cols[event_col= n_result_cols++]= &str_event_col;
	result_cols[expires_col= n_result_cols++]= &str_expires_col;
	if(sphere_enable)
		result_cols[body_col= n_result_cols++]= &str_body_col;

	if (pa_dbf.use_table(pa_db, &presentity_table) < 0)
	{
		LM_ERR("unsuccessful use table sql operation\n");
		goto error;
	}

	static str query_str = str_init("username");
	if (db_fetch_query(&pa_dbf, pres_fetch_rows, pa_db, 0, 0, 0, result_cols,
				0, n_result_cols, &query_str, &result) < 0)
	{
		LM_ERR("querying presentity\n");
		goto error;
	}
	if(result== NULL)
		goto error;

	if(result->n<= 0)
	{
		pa_dbf.free_result(pa_db, result);
		return 0;
	}

	do {
		for(i= 0; i< result->n; i++)
		{
			row = &result->rows[i];
			row_vals = ROW_VALUES(row);

			if(row_vals[expires_col].val.int_val< (int)time(NULL))
				continue;
		
			sphere= NULL;
			user.s= (char*)row_vals[user_col].val.string_val;
			user.len= strlen(user.s);
			domain.s= (char*)row_vals[domain_col].val.string_val;
			domain.len= strlen(domain.s);
			ev_str.s= (char*)row_vals[event_col].val.string_val;
			ev_str.len= strlen(ev_str.s);

			if(event_parser(ev_str.s, ev_str.len, &ev)< 0)
			{
				LM_ERR("parsing event\n");
				free_event_params(ev.params.list, PKG_MEM_TYPE);
				goto error;
			}
			event= ev.type;
			free_event_params(ev.params.list, PKG_MEM_TYPE);

			if(uandd_to_uri(user, domain, &uri)< 0)
			{
				LM_ERR("constructing uri\n");
				goto error;
			}
			/* insert in hash_table*/
	
			if(sphere_enable && event== EVENT_PRESENCE )
			{
				body.s= (char*)row_vals[body_col].val.string_val;
				body.len= strlen(body.s);
				sphere= extract_sphere(body);
			}

			if(insert_phtable(&uri, event, sphere)< 0)
			{
				LM_ERR("inserting record in presentity hash table");
				pkg_free(uri.s);
				if(sphere)
					pkg_free(sphere);
				goto error;
			}
			if(sphere)
				pkg_free(sphere);
			pkg_free(uri.s);
		}
	} while((db_fetch_next(&pa_dbf, pres_fetch_rows, pa_db, &result)==1)
			&& (RES_ROW_N(result)>0));

	pa_dbf.free_result(pa_db, result);

	return 0;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	return -1;	
}

char* extract_sphere(str body)
{

	/* check for a rpid sphere element */
	xmlDocPtr doc= NULL;
	xmlNodePtr node;
	char* cont, *sphere= NULL;
	

	doc= xmlParseMemory(body.s, body.len);
	if(doc== NULL)
	{
		LM_ERR("failed to parse xml body\n");
		return NULL;
	}

	node= xmlNodeGetNodeByName(doc->children, "sphere", "rpid");
	
	if(node== NULL)
		node= xmlNodeGetNodeByName(doc->children, "sphere", "r");

	if(node)
	{
		LM_DBG("found sphere definition\n");
		cont= (char*)xmlNodeGetContent(node);
		if(cont== NULL)
		{
			LM_ERR("failed to extract sphere node content\n");
			goto error;
		}
		sphere= (char*)pkg_malloc((strlen(cont)+ 1)*sizeof(char));
		if(sphere== NULL)
		{
			xmlFree(cont);
			ERR_MEM(PKG_MEM_STR);
		}
		strcpy(sphere, cont);
		xmlFree(cont);
	}
	else
		LM_DBG("didn't find sphere definition\n");

error:
	xmlFreeDoc(doc);

	return sphere;
}

xmlNodePtr xmlNodeGetNodeByName(xmlNodePtr node, const char *name,
													const char *ns)
{
	xmlNodePtr cur = node;
	while (cur) {
		xmlNodePtr match = NULL;
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0) {
			if (!ns || (cur->ns && xmlStrcasecmp(cur->ns->prefix,
							(unsigned char*)ns) == 0))
				return cur;
		}
		match = xmlNodeGetNodeByName(cur->children, name, ns);
		if (match)
			return match;
		cur = cur->next;
	}
	return NULL;
}

char* get_sphere(str* pres_uri)
{
	unsigned int hash_code;
	char* sphere= NULL;
	pres_entry_t* p;
	db_key_t query_cols[6];
	db_val_t query_vals[6];
	db_key_t result_cols[6];
	db1_res_t *result = NULL;
	db_row_t *row= NULL;
	db_val_t *row_vals;
	int n_result_cols = 0;
	int n_query_cols = 0;
	struct sip_uri uri;
	str body;


	if(!sphere_enable)
		return NULL;

	if ( publ_cache_enabled )
	{
		/* search in hash table*/
		hash_code= core_hash(pres_uri, NULL, phtable_size);

		lock_get(&pres_htable[hash_code].lock);

		p= search_phtable(pres_uri, EVENT_PRESENCE, hash_code);

		if(p)
		{
			if(p->sphere)
			{
				sphere= (char*)pkg_malloc(strlen(p->sphere)* sizeof(char));
				if(sphere== NULL)
				{
					lock_release(&pres_htable[hash_code].lock);
					ERR_MEM(PKG_MEM_STR);
				}
				strcpy(sphere, p->sphere);
			}
			lock_release(&pres_htable[hash_code].lock);
			return sphere;
		}
		lock_release(&pres_htable[hash_code].lock);
	}

	if(parse_uri(pres_uri->s, pres_uri->len, &uri)< 0)
	{
		LM_ERR("failed to parse presentity uri\n");
		goto error;
	}

	query_cols[n_query_cols] = &str_domain_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = uri.host;
	n_query_cols++;

	query_cols[n_query_cols] = &str_username_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = uri.user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s= "presence";
	query_vals[n_query_cols].val.str_val.len= 8;
	n_query_cols++;

	result_cols[n_result_cols++] = &str_body_col;
	
	if (pa_dbf.use_table(pa_db, &presentity_table) < 0) 
	{
		LM_ERR("in use_table\n");
		return NULL;
	}

	static str query_str = str_init("received_time");
	if (pa_dbf.query (pa_db, query_cols, 0, query_vals,
		 result_cols, n_query_cols, n_result_cols, &query_str ,  &result) < 0) 
	{
		LM_ERR("failed to query %.*s table\n", presentity_table.len, presentity_table.s);
		if(result)
			pa_dbf.free_result(pa_db, result);
		return NULL;
	}
	
	if(result== NULL)
		return NULL;

	if (result->n<=0 )
	{
		LM_DBG("no published record found in database\n");
		pa_dbf.free_result(pa_db, result);
		return NULL;
	}

	row = &result->rows[result->n-1];
	row_vals = ROW_VALUES(row);
	if(row_vals[0].val.string_val== NULL)
	{
		LM_ERR("NULL notify body record\n");
		goto error;
	}

	body.s= (char*)row_vals[0].val.string_val;
	body.len= strlen(body.s);
	if(body.len== 0)
	{
		LM_ERR("Empty notify body record\n");
		goto error;
	}
	
	sphere= extract_sphere(body);

	pa_dbf.free_result(pa_db, result);

	return sphere;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	return NULL;

}


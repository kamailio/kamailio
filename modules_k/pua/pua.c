/*
 * $Id$
 *
 * pua module - presence user agent module
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
 *  2006-11-29  initial version (anca)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../parser/parse_expires.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../pt.h"
#include "../../db/db.h"
#include "../tm/tm_load.h"
#include "pua.h"
#include "send_publish.h"
#include "send_subscribe.h"
#include "pua_bind.h"
#include "pua_callback.h"

MODULE_VERSION
#define PUA_TABLE_VERSION 2

struct tm_binds tmb;
htable_t* HashT= NULL;
int HASH_SIZE=4;
extern int bind_pua(pua_api_t* api);
int min_expires= 0;
int default_expires=3600;
str db_url = {0, 0};
char* db_table= "pua";
int update_period= 100;
int startup_time = 0;

/* database connection */
db_con_t *pua_db = NULL;
db_func_t pua_dbf;

/* module functions */

static int mod_init(void);
static int child_init(int);
static void destroy(void);

int send_subscribe(subs_info_t*);
int send_publish(publ_info_t*);

int update_pua(ua_pres_t* p, unsigned int hash_code);

int db_store();
int db_restore();
void db_update(unsigned int ticks,void *param);
void hashT_clean(unsigned int ticks,void *param);

static cmd_export_t cmds[]=
{
	{"bind_pua",	     (cmd_function)bind_pua,		 1, 0, 0},
	{"send_publish",     (cmd_function)send_publish,     1, 0, 0},
	{"send_subscribe",   (cmd_function)send_subscribe,   1, 0, 0},
	{"pua_is_dialog",    (cmd_function)is_dialog,		 1, 0, 0},
	{"register_puacb", (cmd_function)register_puacb,     1, 0, 0},
	{0,							0,					     0, 0, 0} 
};

static param_export_t params[]={
	{"hash_size" ,		 INT_PARAM, &HASH_SIZE			 },
	{"db_url" ,			 STR_PARAM, &db_url.s			 },
	{"db_table" ,		 STR_PARAM, &db_table			 },
	{"min_expires",		 INT_PARAM, &min_expires		 },
	{"default_expires",  INT_PARAM, &default_expires     },
	{"update_period",	 INT_PARAM, &update_period	     },
	{0,							 0,			0            }
};

/** module exports */
struct module_exports exports= {
	"pua",						/* module name */
	DEFAULT_DLFLAGS,            /* dlopen flags */
	cmds,						/* exported functions */
	params,						/* exported parameters */
	0,							/* exported statistics */
	0,							/* exported MI functions */
	0,							/* exported pseudo-variables */
	mod_init,					/* module initialization function */
	(response_function) 0,		/* response handling function */
	destroy,					/* destroy function */
	child_init                  /* per-child init function */
};
	
/**#include "../../db/db.h"

 * init module function
 */
static int mod_init(void)
{
	str _s;
	int ver = 0;
	
	load_tm_f  load_tm;

	DBG("PUA: initializing module ...\n");
	
	if(min_expires< 0)
		min_expires= 0;

	if(default_expires< 600)
		default_expires= 3600;

	/* import the TM auto-loading function */
	if((load_tm=(load_tm_f)find_export("load_tm", 0, 0))==NULL)
	{
		LOG(L_ERR, "PUA:mod_init:ERROR:can't import load_tm\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */

	if(load_tm(&tmb)==-1)
	{
		LOG(L_ERR, "PUA:mod_init:ERROR can't load tm functions\n");
		return -1;
	}
	
	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	DBG("PUA:mod_init: db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len, db_url.s);
	
	/* binding to mysql module  */
	if (bind_dbmod(db_url.s, &pua_dbf))
	{
		DBG("PUA:mod_init: ERROR: Database module not found\n");
		return -1;
	}
	if (!DB_CAPABILITY(pua_dbf, DB_CAP_ALL)) {
		LOG(L_ERR,"PUA:mod_init: ERROR Database module does not implement "
		    "all functions needed by the module\n");
		return -1;
	}

	pua_db = pua_dbf.init(db_url.s);
	if (!pua_db)
	{
		LOG(L_ERR,"PUA:mod_init: Error while connecting database\n");
		return -1;
	}
	// verify table version 
	_s.s = db_table;
	_s.len = strlen(db_table);
	 ver =  table_version(&pua_dbf, pua_db, &_s);
	if(ver!=PUA_TABLE_VERSION)
	{
		LOG(L_ERR,"PRESENCE:mod_init: Wrong version v%d for table <%s>,"
				" need v%d\n", ver, _s.s, PUA_TABLE_VERSION);
		return -1;
	}

	if(HASH_SIZE<=1)
		HASH_SIZE= 512;
	else
		HASH_SIZE = 1<<HASH_SIZE;

	HashT= new_htable();
	if(HashT== NULL)
	{
		LOG(L_ERR, "PUA:mod_init: ERROR while creating new hash table\n");
		return -1;
	}
	if(db_restore()< 0)
	{
		LOG(L_ERR, "PUA:mod_init: ERROR while restoring hash_table\n");
		return -1;
	}

	if(update_period<=0)
	{
		DBG("PUA: ERROR: mod_init: wrong clean_period \n");
		return -1;
	}
	if ( init_puacb_list() < 0)
    {
		LOG(L_ERR, "PUA:mod_init: ERROR: callbacks initialization failed\n");
        return -1;
    }

	startup_time = (int) time(NULL);
	
	register_timer(hashT_clean, 0, update_period- 5);

	register_timer(db_update, 0, update_period);


	if(pua_db)
		pua_dbf.close(pua_db);
	pua_db = NULL;

	return 0;
}

static int child_init(int rank)
{
	DBG("PUA: init_child [%d]  pid [%d]\n", rank, getpid());

	if (pua_dbf.init==0)
	{
		LOG(L_CRIT, "Pua:child_init: database not bound\n");
		return -1;
	}
	pua_db = pua_dbf.init(db_url.s);
	if (!pua_db)
	{
		LOG(L_ERR,"Pua:child %d: Error while connecting database\n",
				rank);
		return -1;
	}
	else
	{
		if (pua_dbf.use_table(pua_db, db_table) < 0)  
		{
			LOG(L_ERR, "Pua:child %d: Error in use_table pua\n", rank);
			return -1;
		}
	
		DBG("Pua:child %d: Database connection opened successfully\n", rank);
	}

	return 0;
}	

static void destroy(void)
{	
	DBG("PUA: destroying module ...\n");
	if (puacb_list)
		destroy_puacb_list();

	if(pua_db && HashT)
		db_update(0,0);
	
	if(HashT)
		destroy_htable();

	if(pua_db)
		pua_dbf.close(pua_db);

	return ;
}

int db_restore()
{
	ua_pres_t* p= NULL;
	db_key_t result_cols[13]; 
	db_res_t *res= NULL;
	db_row_t *row = NULL;	
	db_val_t *row_vals= NULL;
	str pres_uri, pres_id;
	str etag, tuple_id;
	str watcher_uri, call_id;
	str to_tag, from_tag;
	int size= 0, i;
	int n_result_cols= 0;
	int puri_col,pid_col,expires_col,flag_col,etag_col,tuple_col;
	int watcher_col,callid_col,totag_col,fromtag_col,cseq_col,event_col;

	result_cols[puri_col=n_result_cols++]	="pres_uri";		
	result_cols[pid_col=n_result_cols++]	="pres_id";	
	result_cols[expires_col=n_result_cols++]="expires";
	result_cols[flag_col=n_result_cols++]	="flag";
	result_cols[etag_col=n_result_cols++]	="etag";
	result_cols[tuple_col=n_result_cols++]	="tuple_id";
	result_cols[watcher_col=n_result_cols++]="watcher_uri";
	result_cols[callid_col=n_result_cols++] ="call_id";
	result_cols[totag_col=n_result_cols++]	="to_tag";
	result_cols[fromtag_col=n_result_cols++]="from_tag";
	result_cols[cseq_col= n_result_cols++]	="cseq";
	result_cols[event_col= n_result_cols++]	="event";
	
	if(!pua_db)
	{
		LOG(L_ERR,"PUA: db_restore: ERROR null database connection\n");
		return -1;
	}

	if(pua_dbf.use_table(pua_db, db_table)< 0)
	{
		LOG(L_ERR, "PUA: db_restore:ERROR in use table\n");
		return -1;
	}

	if(pua_dbf.query(pua_db,0, 0, 0, result_cols,0, n_result_cols, 0,&res)< 0)
	{
		LOG(L_ERR, "PUA: db_restore:ERROR while querrying table\n");
		if(res)
		{
			pua_dbf.free_result(pua_db, res);
			res = NULL;
		}
		return -1;
	}
	if(res== NULL)
		return -1;

	if(res->n<=0)
	{
		LOG(L_INFO, "PUA: db_restore:the query returned no result\n");
		pua_dbf.free_result(pua_db, res);
		res = NULL;
		return 0;
	}

	DBG("PUA: db_restore: found %d db entries\n", res->n);

	for(i =0 ; i< res->n ; i++)
	{
		row = &res->rows[i];
		row_vals = ROW_VALUES(row);
	
		pres_uri.s= row_vals[0].val.str_val.s;
		pres_uri.len = strlen(pres_uri.s);

		pres_id.s= row_vals[1].val.str_val.s;
		pres_id.len = strlen(pres_id.s);

		memset(&etag,		 0, sizeof(str));
		memset(&tuple_id,	 0, sizeof(str));
		memset(&watcher_uri,  0, sizeof(str));
		memset(&call_id,	 0, sizeof(str));
		memset(&to_tag,		 0, sizeof(str));
		memset(&from_tag,	 0, sizeof(str));

		if(row_vals[4].val.str_val.s)
		{
			etag.s= row_vals[etag_col].val.str_val.s;
			etag.len = strlen(etag.s);
	
			tuple_id.s= row_vals[tuple_col].val.str_val.s;
			tuple_id.len = strlen(tuple_id.s);
		}

		if(row_vals[6].val.str_val.s)
		{	
			watcher_uri.s= row_vals[watcher_col].val.str_val.s;
			watcher_uri.len = strlen(watcher_uri.s);
	
			call_id.s= row_vals[callid_col].val.str_val.s;
			call_id.len = strlen(call_id.s);

			to_tag.s= row_vals[totag_col].val.str_val.s;
			to_tag.len = strlen(to_tag.s);

			from_tag.s= row_vals[fromtag_col].val.str_val.s;
			from_tag.len = strlen(from_tag.s);
		}
		
		size= sizeof(ua_pres_t)+ sizeof(str)+ pres_uri.len+ pres_id.len;
		if(etag.len)
			size+= etag.len+ tuple_id.len;
		else
			size+= sizeof(str)+ watcher_uri.len+ call_id.len+ to_tag.len+
				from_tag.len;
		p= (ua_pres_t*)shm_malloc(size);
		if(p== NULL)
		{
			LOG(L_ERR, "PUA: db_restore: Error no more share memmory");
			goto error;
		}
		memset(p, 0, size);
		size= sizeof(ua_pres_t);

		p->pres_uri= (str*)((char*)p+ size);
		size+= sizeof(str);
		p->pres_uri->s= (char*)p + size;
		memcpy(p->pres_uri->s, pres_uri.s, pres_uri.len);
		p->pres_uri->len= pres_uri.len;
		size+= pres_uri.len;

		p->id.s= (char*)p + size;
		memcpy(p->id.s, pres_id.s, pres_id.len);
		p->id.len= pres_id.len;
		size+= pres_id.len;

		p->expires= row_vals[expires_col].val.int_val;
		p->flag|=	row_vals[flag_col].val.int_val;
		p->db_flag|= INSERTDB_FLAG;

		if(etag.len)
		{
			p->etag.s= (char*)p+ size;
			memcpy(p->etag.s, etag.s, etag.len);
			p->etag.len= etag.len;
			size+= etag.len;
			
			p->tuple_id.s= (char*)p + size;
			memcpy(p->tuple_id.s, tuple_id.s, tuple_id.len);
			p->tuple_id.len= tuple_id.len;
			size+= tuple_id.len;
		}
		else
		{
			p->watcher_uri= (str*)((char*)p+ size);
			size+= sizeof(str);

			p->watcher_uri->s= (char*)p+ size;
			memcpy(p->watcher_uri->s, watcher_uri.s, watcher_uri.len);
			p->watcher_uri->len= watcher_uri.len;
			size+= watcher_uri.len;

			p->to_tag.s= (char*)p+ size;
			memcpy(p->to_tag.s, to_tag.s, to_tag.len);
			p->to_tag.len= to_tag.len;
			size+= to_tag.len;

			p->from_tag.s= (char*)p+ size;
			memcpy(p->from_tag.s, from_tag.s, from_tag.len);
			p->from_tag.len= from_tag.len;
			size+= from_tag.len;
	
			p->call_id.s= (char*)p + size;
			memcpy(p->call_id.s, call_id.s, call_id.len);
			p->call_id.len= call_id.len;
			size+= call_id.len;

			p->cseq= row_vals[cseq_col].val.int_val;
		}
		p->event= row_vals[event_col].val.int_val;

		insert_htable(p);
	}
	if(res)
	{
		pua_dbf.free_result(pua_db, res);
		res = NULL;
	}
	
	if(pua_dbf.delete(pua_db, 0, 0 , 0, 0) < 0)
	{
		LOG(L_ERR,"pua:db_restore:ERROR while deleting information from db\n");
		goto error;
	}

	return 0;

error:
	if(res)
		pua_dbf.free_result(pua_db, res);

	if(p)
		shm_free(p);
	return -1;
}

void hashT_clean(unsigned int ticks,void *param)
{
	int i;
	time_t now;
	ua_pres_t* p= NULL, *q= NULL;

	DBG("PUA: hashT_clean ..\n");
	now = time(NULL);
	for(i= 0;i< HASH_SIZE; i++)
	{
		lock_get(&HashT->p_records[i].lock);
		p= HashT->p_records[i].entity->next;
		while(p)
		{	
			if(p->expires- update_period < now )
			{
				if((p->desired_expires> p->expires + min_expires) || 
						(p->desired_expires== 0 ))
				{
					if(update_pua(p, i)< 0)
					{
						LOG(L_ERR, "PUA: hashT_clean: Error while updating\n");
						lock_release(&HashT->p_records[i].lock);
						return;
					}
					p= p->next;
					continue;
				}	
			    if(p->expires < now - 2)
				{
					q= p->next;
					DBG("PUA: hashT_clean: Found expired: uri= %.*s\n", p->pres_uri->len,
							p->pres_uri->s);
					delete_htable(p, i);
					p= q;
				}
				else
					p= p->next;
			}	
			else
				p= p->next;
		}
		lock_release(&HashT->p_records[i].lock);
	}

}
int update_pua(ua_pres_t* p, unsigned int hash_code)
{
	int size= 0;
	ua_pres_t* hentity= NULL;
	str* str_hdr= NULL;
	int expires;

	size= sizeof(ua_pres_t)+ sizeof(str)+ (p->pres_uri->len+ 1)*sizeof(char);
	
	if(p->watcher_uri)
		size+= sizeof(str)+ p->watcher_uri->len;
	else
		size+=  p->id.len;

	hentity= (ua_pres_t*)shm_malloc(size);
	if(hentity== NULL)
	{
		LOG(L_ERR, "PUA: update_pua: ERROR no more share memory\n");
		goto error;
	}
	memset(hentity, 0, size);

	size =  sizeof(ua_pres_t);

	hentity->pres_uri = (str*)((char*)hentity + size);
	size+= sizeof(str);

	hentity->pres_uri->s = (char*)hentity+ size;
	memcpy(hentity->pres_uri->s, p->pres_uri->s ,
			p->pres_uri->len ) ;
	hentity->pres_uri->len= p->pres_uri->len;
	size+= p->pres_uri->len;
		
	if(p->watcher_uri)
	{
		hentity->watcher_uri=(str*)((char*)hentity+ size);
		size+= sizeof(str);
		hentity->watcher_uri->s= (char*)hentity+ size;
		memcpy(hentity->watcher_uri->s, p->watcher_uri->s,p->watcher_uri->len);
		hentity->watcher_uri->len= p->watcher_uri->len;
		size+= p->watcher_uri->len;
	}	
	else
	{	
		hentity->id.s = ((char*)hentity+ size);
		memcpy(hentity->id.s, p->id.s, p->id.len);
		hentity->id.len= p->id.len;
		size+= p->id.len;
	}
	hentity->flag|= p->flag;
	
	if(p->desired_expires== 0)
			expires= 3600;
		else
			expires= p->desired_expires- (int)time(NULL);

	if(p->watcher_uri== NULL)
	{
		str met= {"PUBLISH", 7};
		str_hdr = publ_build_hdr(expires, p->event, &p->etag, NULL, 0);
		if(str_hdr == NULL)
		{
			LOG(L_ERR, "PUA: update_pua: ERROR while building extra_headers\n");
			goto error;
		}
		DBG("PUA: update_pua: str_hdr:\n%.*s\n ", str_hdr->len, str_hdr->s);
		
		
		tmb.t_request(&met,						/* Type of the message */
				p->pres_uri,					/* Request-URI */
				p->pres_uri,					/* To */
				p->pres_uri,					/* From */
				str_hdr,						/* Optional headers */
				0,								/* Message body */
				0,								/* Outbound proxy*/
				publ_cback_func,				/* Callback function */
				(void*)hentity					/* Callback parameter */
				);
		
	}
	else
	{
		str met= {"SUBSCRIBE", 9};
		dlg_t* td= NULL;
		td= pua_build_dlg_t(p);
		if(td== NULL)
		{
			LOG(L_ERR, "PUA:update_pua: Error while building tm dlg_t"
					"structure");		
			goto error;
		};
	
		str_hdr= subs_build_hdr(p->watcher_uri, expires, p->event);
		if(str_hdr== NULL || str_hdr->s== NULL)
		{
			LOG(L_ERR, "PUA:send_subscribe: Error while building extra headers\n");
			pkg_free(td);
			return -1;
		}
		
		tmb.t_request_within
		(&met,
		str_hdr,                               
		0,                           
		td,					                  
		subs_cback_func,				        
		(void*)hentity	
		);

		pkg_free(td);
		td= NULL;
	}	

	pkg_free(str_hdr);
	return 0;

error:
	if(str_hdr)
		pkg_free(str_hdr);
	if(hentity)
		pkg_free(hentity);
	return -1;

}

void db_update(unsigned int ticks,void *param)
{
	ua_pres_t* p= NULL;
	db_key_t q_cols[14];
	db_res_t *res= NULL;
	db_key_t db_cols[3];
	db_val_t q_vals[13], db_vals[3];
	db_op_t  db_ops[1] ;
	int n_query_cols= 0;
	int n_update_cols= 0;
	int i;
	int puri_col,pid_col,expires_col,flag_col,etag_col,tuple_col;
	int watcher_col,callid_col,totag_col,fromtag_col,cseq_col,event_col;
	int no_lock= 0;
	
	if(ticks== 0 && param == NULL)
		no_lock= 1;


	DBG("PUA: db_update...\n");
	/* cols and values used for insert */
	q_cols[puri_col= n_query_cols] ="pres_uri";
	q_vals[puri_col= n_query_cols].type = DB_STR;
	q_vals[puri_col= n_query_cols].nul = 0;
	n_query_cols++;
	
	q_cols[pid_col= n_query_cols] ="pres_id";	
	q_vals[pid_col= n_query_cols].type = DB_STR;
	q_vals[pid_col= n_query_cols].nul = 0;
	n_query_cols++;

	q_cols[flag_col= n_query_cols] ="flag";
	q_vals[flag_col= n_query_cols].type = DB_INT;
	q_vals[flag_col= n_query_cols].nul = 0;
	n_query_cols++;
	q_cols[watcher_col= n_query_cols] ="watcher_uri";
	q_vals[watcher_col= n_query_cols].type = DB_STR;
	q_vals[watcher_col= n_query_cols].nul = 0;
	n_query_cols++;

	q_cols[tuple_col= n_query_cols] ="tuple_id";
	q_vals[tuple_col= n_query_cols].type = DB_STR;
	q_vals[tuple_col= n_query_cols].nul = 0;
	n_query_cols++;

	q_cols[etag_col= n_query_cols] ="etag";
	q_vals[etag_col= n_query_cols].type = DB_STR;
	q_vals[etag_col= n_query_cols].nul = 0;
	n_query_cols++;	

	q_cols[callid_col= n_query_cols] ="call_id";
	q_vals[callid_col= n_query_cols].type = DB_STR;
	q_vals[callid_col= n_query_cols].nul = 0;
	n_query_cols++;

	q_cols[totag_col= n_query_cols] ="to_tag";
	q_vals[totag_col= n_query_cols].type = DB_STR;
	q_vals[totag_col= n_query_cols].nul = 0;
	n_query_cols++;

	q_cols[fromtag_col= n_query_cols] ="from_tag";
	q_vals[fromtag_col= n_query_cols].type = DB_STR;
	q_vals[fromtag_col= n_query_cols].nul = 0;
	n_query_cols++;

	q_cols[cseq_col= n_query_cols]="cseq";
	q_vals[cseq_col= n_query_cols].type = DB_INT;
	q_vals[cseq_col= n_query_cols].nul = 0;
	n_query_cols++;

	q_cols[expires_col= n_query_cols] ="expires";
	q_vals[expires_col= n_query_cols].type = DB_INT;
	q_vals[expires_col= n_query_cols].nul = 0;
	n_query_cols++;

	q_cols[event_col= n_query_cols] ="event";
	q_vals[event_col= n_query_cols].type = DB_INT;
	q_vals[event_col= n_query_cols].nul = 0;
	n_query_cols++;

	/* cols and values used for update */
	db_cols[0]= "expires";
	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	
	db_cols[1]= "cseq";
	db_vals[1].type = DB_INT;
	db_vals[1].nul = 0;

	if(pua_db== NULL)
	{
		LOG(L_ERR,"PUA: db_update: ERROR null database connection\n");
		return;
	}
	if(pua_dbf.use_table(pua_db, db_table)< 0)
	{
		LOG(L_ERR, "PUA: db_update:ERROR in use table\n");
		return ;
	}

	for(i=0; i<HASH_SIZE; i++) 
	{
		if(!no_lock)
			lock_get(&HashT->p_records[i].lock);	
		
		p = HashT->p_records[i].entity->next;
		while(p)
		{
			if(p->expires - (int)time(NULL)< 0)	
			{
				p= p->next;
				continue;
			}

			switch(p->db_flag)
			{
				case NO_UPDATEDB_FLAG:
				{
			
					DBG("PUA: db_update: NO_UPDATEDB_FLAG\n");
					break;			  
				}
				
				case UPDATEDB_FLAG:
				{
					DBG("PUA: db_update: UPDATEDB_FLAG\n ");
					n_update_cols= 1;
					n_query_cols= 3;
					
					q_vals[puri_col].val.str_val = *(p->pres_uri);
					q_vals[pid_col].val.str_val = p->id;
					q_vals[flag_col].val.int_val = p->flag;

					db_vals[0].val.int_val= p->expires;
					
					if(p->watcher_uri)   /* for subscribe */
					{
						q_vals[n_query_cols].val.str_val = *(p->watcher_uri);
						n_query_cols= 4;
					
						db_vals[1].val.int_val= p->cseq	;
						n_update_cols= 2;
					}
					
					DBG("PUA: db_update: Updating ..n_query_cols= %d\t"
						" n_update_cols= %d\n", n_query_cols, n_update_cols);

					if(pua_dbf.query(pua_db, q_cols, 0, q_vals,
								 0, n_query_cols, 0, 0, &res)< 0)
					{
						LOG(L_ERR, "PUA: db_update:ERROR while querying"
								" database");
						lock_release(&HashT->p_records[i].lock);
						if(res)
							pua_dbf.free_result(pua_db, res);	
						return ;
					}
					if(res && res->n> 0)
					{																				
						if(pua_dbf.update(pua_db, q_cols, 0, q_vals, db_cols, 
								db_vals, n_query_cols, n_update_cols)<0)
						{
							LOG(L_ERR, "PUA: db_update: ERROR while updating"
									" in database");
							lock_release(&HashT->p_records[i].lock);	
							pua_dbf.free_result(pua_db, res);
							res= NULL;
							return ;
						}
						pua_dbf.free_result(pua_db, res);
						res= NULL;

					}
					else
					{
						if(res)
						{	
							pua_dbf.free_result(pua_db, res);
							res= NULL;
						}
						DBG("PUA:db_update: UPDATEDB_FLAG and no record"
								" found\n");
					}	
					break;		
				}
				
				case INSERTDB_FLAG:
				{	
					DBG("PUA: db_update: INSERTDB_FLAG\n ");
					q_vals[puri_col].val.str_val = *(p->pres_uri);
					q_vals[pid_col].val.str_val = p->id;
					q_vals[flag_col].val.int_val = p->flag;
					if((p->watcher_uri))
						q_vals[watcher_col].val.str_val = *(p->watcher_uri);
					else
						memset(& q_vals[watcher_col].val.str_val ,0, sizeof(str));
					q_vals[tuple_col].val.str_val = p->tuple_id;
					q_vals[etag_col].val.str_val = p->etag;
					q_vals[callid_col].val.str_val = p->call_id;
					q_vals[totag_col].val.str_val = p->to_tag;
					q_vals[fromtag_col].val.str_val = p->from_tag;
					q_vals[cseq_col].val.int_val= p->cseq;
					q_vals[expires_col].val.int_val = p->expires;
					q_vals[event_col].val.int_val = p->event;
						
					if(pua_dbf.insert(pua_db, q_cols, q_vals, 12)<0)
					{
						LOG(L_ERR, "PUA: db_update: ERROR while inserting"
								" into table pua\n");
						lock_release(&HashT->p_records[i].lock);
						return ;
					}
					break;
				}

			}
			if(!(p->db_flag & NO_UPDATEDB_FLAG))
			{
				p->db_flag= NO_UPDATEDB_FLAG;	
			}
			p= p->next;
		}
		if(!no_lock)
			lock_release(&HashT->p_records[i].lock);	
	}

	db_vals[0].val.int_val= (int)time(NULL);
	db_ops[0]= OP_LT;
	if(pua_dbf.delete(pua_db, db_cols, db_ops, db_vals, 1) < 0)
	{
		LOG(L_ERR,"PUA: db_update: ERROR cleaning expired"
				" information\n");
	}

	return ;

}	
	


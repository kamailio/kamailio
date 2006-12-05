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

MODULE_VERSION

struct tm_binds tmb;
htable_t* HashT= NULL;
int HASH_SIZE=4;
extern int bind_pua(pua_api_t* api);
int min_expires= 0;
int default_expires;
str db_url;
char* db_table= "pua";
int update_period= 30;
int startup_time;

/* database connection */
db_con_t *pua_db = NULL;
db_func_t pua_dbf;

/* module functions */

static int mod_init(void);
static int child_init(int);
static void destroy(void);

int send_subscribe(subs_info_t*);
int send_publish(publ_info_t*);

int db_store(htable_t* H);
int db_restore(htable_t* H);
void db_update(unsigned int ticks,void *param);
void hashT_clean(unsigned int ticks,void *param);

static cmd_export_t cmds[]=
{
	{"bind_pua",	   (cmd_function)bind_pua,		   1, 0, 0},
	{"send_publish",   (cmd_function)send_publish,     1, 0, 0},
	{"send_subscribe", (cmd_function)send_subscribe,   1, 0, 0},
	{0,							0,					   0, 0, 0} 
};

static param_export_t params[]={
	{"hash_size" ,		 INT_PARAM, &HASH_SIZE			 },
	{"db_url" ,			 STR_PARAM, &db_url				 },
	{"db_table" ,		 STR_PARAM, &db_table			 },
	{"min_expires",		 INT_PARAM, &min_expires		 },
	{"default_expires",  INT_PARAM, &default_expires     },
	{"update_period",	 INT_PARAM, &update_period	 },
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
	if(db_restore(HashT)< 0)
	{
		LOG(L_ERR, "PUA:mod_init: ERROR while restoring hash_table\n");
		return -1;
	}

	if(update_period<=0)
	{
		DBG("PUA: ERROR: mod_init: wrong clean_period \n");
		return -1;
	}

	startup_time = (int) time(NULL);
	
	register_timer(hashT_clean, 0, update_period);

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

	db_update(0,0);
	
	if(HashT)
		destroy_htable(HashT);

	return ;
}

int db_restore(htable_t* H)
{
	ua_pres_t* p= NULL;
	db_key_t result_cols[12]; 
	db_res_t *res= NULL;
	db_row_t *row = NULL;	
	db_val_t *row_vals= NULL;
	str pres_uri, pres_id;
	str etag, tuple_id;
	str watcher_uri, call_id;
	str to_tag, from_tag;
	int size= 0, i;
	
	result_cols[0] ="pres_uri";		
	result_cols[1] ="pres_id";	
	result_cols[2] ="expires";
	result_cols[3] ="flag";
	result_cols[4] ="etag";
	result_cols[5] ="tuple_id";
	result_cols[6] ="watcher_uri";
	result_cols[7] ="call_id";
	result_cols[8] ="to_tag";
	result_cols[9] ="from_tag";
	result_cols[10]="cseq";

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

	if(pua_dbf.query(pua_db,0, 0, 0, result_cols,0, 11, 0,&res)< 0)
	{
		LOG(L_ERR, "PUA: db_restore:ERROR while querrying table\n");
		return -1;
	}
	if(res && res->n<=0)
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
			etag.s= row_vals[4].val.str_val.s;
			etag.len = strlen(etag.s);
	
			tuple_id.s= row_vals[5].val.str_val.s;
			tuple_id.len = strlen(tuple_id.s);
		}

		if(row_vals[6].val.str_val.s)
		{	
			watcher_uri.s= row_vals[6].val.str_val.s;
			watcher_uri.len = strlen(watcher_uri.s);
	
			call_id.s= row_vals[7].val.str_val.s;
			call_id.len = strlen(call_id.s);

			to_tag.s= row_vals[8].val.str_val.s;
			to_tag.len = strlen(to_tag.s);

			from_tag.s= row_vals[9].val.str_val.s;
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

		p->expires= row_vals[2].val.int_val;
		p->flag|=	row_vals[3].val.int_val;
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

			p->cseq= row_vals[10].val.int_val;
		}
		
		insert_htable(p, HashT);
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
	ua_pres_t* p= NULL, *q= NULL;

	DBG("PUA: hashT_clean ..\n");

	for(i= 0;i< HASH_SIZE; i++)
	{
		lock_get(&HashT->p_records[i].lock);
		p= HashT->p_records[i].entity->next;
		while(p)
		{	
			if(p->expires< (int)(time)(NULL))
			{
				q= p->next;
				delete_htable(p, HashT);
				p= q;
			}
			else
				p= p->next;
		}
		lock_release(&HashT->p_records[i].lock);
	}

}

void db_update(unsigned int ticks,void *param)
{
	ua_pres_t* p= NULL;
	db_key_t q_cols[13];
	db_res_t *res= NULL;
	db_key_t db_cols[3];
	db_val_t q_vals[13], db_vals[3];
	db_op_t  db_ops[1] ;
	int n_query_cols= 0;
	int n_update_cols= 0;
	int i;
	
	/* cols and values used for insert */
	q_cols[0] ="pres_uri";
	q_vals[0].type = DB_STR;
	q_vals[0].nul = 0;

	q_cols[1] ="pres_id";	
	q_vals[1].type = DB_STR;
	q_vals[1].nul = 0;

	q_cols[2] ="flag";
	q_vals[2].type = DB_INT;
	q_vals[2].nul = 0;

	q_cols[3] ="watcher_uri";
	q_vals[3].type = DB_STR;
	q_vals[3].nul = 0;

	q_cols[4] ="tuple_id";
	q_vals[4].type = DB_STR;
	q_vals[4].nul = 0;

	q_cols[5] ="etag";
	q_vals[5].type = DB_STR;
	q_vals[5].nul = 0;
	
	q_cols[6] ="call_id";
	q_vals[6].type = DB_STR;
	q_vals[6].nul = 0;

	q_cols[7] ="to_tag";
	q_vals[7].type = DB_STR;
	q_vals[7].nul = 0;
	
	q_cols[8] ="from_tag";
	q_vals[8].type = DB_STR;
	q_vals[8].nul = 0;
	
	q_cols[9]="cseq";
	q_vals[9].type = DB_INT;
	q_vals[9].nul = 0;

	q_cols[10] ="expires";
	q_vals[10].type = DB_INT;
	q_vals[10].nul = 0;
	
	/* cols and values used for update */
	db_cols[0]= "expires";
	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	
	db_cols[1]= "cseq";
	db_vals[1].type = DB_INT;
	db_vals[1].nul = 0;

	DBG("PUA:db_update ...\n");
	
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
					
					q_vals[0].val.str_val = *(p->pres_uri);
					q_vals[1].val.str_val = p->id;
					q_vals[2].val.int_val = p->flag;

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
							return ;
						}
					}
					else
					{
						DBG("PUA:db_update: UPDATEDB_FLAG and no record"
								" found\n");
					}	
					break;		
				}
				
				case INSERTDB_FLAG:
				{	
					DBG("PUA: db_update: INSERTDB_FLAG\n ");
					q_vals[0].val.str_val = *(p->pres_uri);
					q_vals[1].val.str_val = p->id;
					q_vals[2].val.int_val = p->flag;
					if((p->watcher_uri))
						q_vals[3].val.str_val = *(p->watcher_uri);
					else
						memset(& q_vals[3].val.str_val ,0, sizeof(str));
					q_vals[4].val.str_val = p->tuple_id;
					q_vals[5].val.str_val = p->etag;
					q_vals[6].val.str_val = p->call_id;
					q_vals[7].val.str_val = p->to_tag;
					q_vals[8].val.str_val = p->from_tag;
					q_vals[9].val.int_val= p->cseq;
					q_vals[10].val.int_val = p->expires;
						
					if(pua_dbf.insert(pua_db, q_cols, q_vals, 11)<0)
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
	


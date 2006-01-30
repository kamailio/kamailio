/**
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-04-07: a structure for both hashes introduced (ramona) 
 * 2003-04-06: db connection closed in mod_init (janakj)
 * 2004-06-07: updated to the new DB api (andrei)
 * 2005-01-26: removed terminating code (ramona)
 *             prefix hash replaced with tree (ramona)
 *             FIFO commands to add/list/delete prefix domains (ramona)
 *             pdt cache per process for fast translation (ramona)
 * 2006-01-30: multi domain support added
 */

/*
 * Prefix-Domains Translation - ser module
 * Ramona Modroiu <ramona@voice-system.ro>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../db/db_op.h"
#include "../../sr_module.h"
#include "../../db/db.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../fifo_server.h"
#include "../../unixsock_server.h"
#include "../../parser/parse_uri.h"
#include "../../timer.h"
#include "../../ut.h"
#include "../../action.h"
#include "../../parser/parse_from.h"
#include "domains.h"
#include "pdtree.h"

MODULE_VERSION


#define NR_KEYS			3

int hs_two_pow = 2;

/** structures containing prefix-domain pairs */
hash_list_t *_dhash = NULL; 
pdt_tree_t *_ptree = NULL; 

time_t last_sync;

/** database connection */
static db_con_t *db_con = NULL;
static db_func_t pdt_dbf;


/** parameters */
static char *db_url = "mysql://root@127.0.0.1/pdt";
char *db_table = "pd_multidomain";
char *sdomain_column = "sdomain";
char *prefix_column  = "prefix";
char *domain_column  = "domain";

/** pstn prefix */
str prefix = {"", 0};
int sync_time = 600;
int clean_time = 900;

static int w_prefix2domain(struct sip_msg* msg, char* str1, char* str2);
static int w_prefix2domain_1(struct sip_msg* msg, char* mode, char* str2);
static int mod_init(void);
static void mod_destroy(void);
static int  child_init(int r);

static int prefix2domain(struct sip_msg*, int mode);
static int get_domainprefix_unixsock(str* msg);
static int pdt_fifo_add(FILE *stream, char *response_file);
static int pdt_fifo_delete(FILE *stream, char *response_file);
static int pdt_fifo_list(FILE *stream, char *response_file);

int update_new_uri(struct sip_msg *msg, int plen, str *d, int mode);
int pdt_load_db();
int pdt_sync_cache();
void pdt_clean_cache(unsigned int ticks, void *param);

static cmd_export_t cmds[]={
	{"prefix2domain", w_prefix2domain,   0, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"prefix2domain", w_prefix2domain_1, 1, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"db_url",         STR_PARAM, &db_url},
	{"db_table",       STR_PARAM, &db_table},
	{"sdomain_column", STR_PARAM, &sdomain_column},
	{"prefix_column",  STR_PARAM, &prefix_column},
	{"domain_column",  STR_PARAM, &domain_column},
	{"prefix",         STR_PARAM, &prefix.s},
	{"hsize_2pow",     INT_PARAM, &hs_two_pow},
	{"sync_time",      INT_PARAM, &sync_time},
	{"clean_time",     INT_PARAM, &clean_time},
	{0, 0, 0}
};

struct module_exports exports = {
	"pdt",
	cmds,
	params,
	0,	
	mod_init,		/* module initialization function */
	0,				/* response function */
	mod_destroy,	/* destroy function */
	child_init		/* per child init function */
};	



/**
 * init module function
 */
static int mod_init(void)
{
	DBG("PDT: initializing...\n");

	if(hs_two_pow<0)
	{
		LOG(L_ERR, "PDT:mod_init: hash_size_two_pow must be"
					" positive and less than %d\n", MAX_HSIZE_TWO_POW);
		return -1;
	}

	prefix.len = strlen(prefix.s);
	
	if(register_fifo_cmd(pdt_fifo_add, "pdt_add", 0)<0)
	{
		LOG(L_ERR,
			"PDT:mod_init: cannot register fifo command 'pdt_add'\n");
		return -1;
	}	
	if(register_fifo_cmd(pdt_fifo_delete, "pdt_delete", 0)<0)
	{
		LOG(L_ERR,
			"PDT:mod_init: cannot register fifo command 'pdt_delete'\n");
		return -1;
	}	

	if(register_fifo_cmd(pdt_fifo_list, "pdt_list", 0)<0)
	{
		LOG(L_ERR,
			"PDT:mod_init: cannot register fifo command 'pdt_list'\n");
		return -1;
	}	


	if(unixsock_register_cmd("get_domainprefix", get_domainprefix_unixsock)<0)
	{
		LOG(L_ERR,
		"PDT:mod_init: cannot register unixsock command 'get_domainprefix'\n");
		return -1;
	}

	/* binding to mysql module */
	if(bind_dbmod(db_url, &pdt_dbf))
	{
		LOG(L_ERR, "PDT:mod_init: Database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(pdt_dbf, DB_CAP_ALL))
	{
		LOG(L_ERR, "PDT: mod_init: Database module does not "
		    "implement all functions needed by the module\n");
		return -1;
	}

	/* open a connection with the database */
	db_con = pdt_dbf.init(db_url);
	if(db_con==NULL)
	{
		LOG(L_ERR,
			"PDT: mod_init: Error while connecting to database\n");        
		return -1;
	}
	
	if (pdt_dbf.use_table(db_con, db_table) < 0)
	{
		LOG(L_ERR, "PDT: mod_init: Error in use_table\n");
		goto error1;
	}
	DBG("PDT: mod_init: Database connection opened successfully\n");
	
	/* init the hash and tree in share memory */
	if( (_dhash = init_hash_list(hs_two_pow)) == NULL)
	{
		LOG(L_ERR, "PDT:mod_init: domain hash could not be allocated\n");	
		goto error1;
	}
	

	/* loading all information from database */
	if(pdt_load_db()!=0)
	{
		LOG(L_ERR, "PDT:mod_init: cannot load info from database\n");	
		goto error2;
	}
		
	pdt_dbf.close(db_con);

	pdt_print_tree(_ptree);
	DBG("PDT:mod_init: -------------------\n");
	pdt_print_hash_list(_dhash);

	last_sync = time(NULL);

	register_timer(pdt_clean_cache, 0, clean_time);

	/* success code */
	return 0;

error2:
	if(_dhash!=NULL)
	{
		free_hash_list(_dhash);
		_dhash = 0;
	}
error1:
	if(db_con!=NULL)
	{
		pdt_dbf.close(db_con);
		db_con = 0;
	}
	return -1;
}	

/* each child get a new connection to the database */
static int child_init(int r)
{
	DBG("PDT:child_init #%d / pid <%d>\n", r, getpid());

	if(r>0)
	{
		if(_dhash==NULL)
		{
			LOG(L_ERR,"PDT:child_init #%d: ERROR no domain hash\n", r);
			return -1;
		}
	
		lock_get(&_dhash->hl_lock);
		_dhash->workers++;
		lock_release(&_dhash->hl_lock);
	} else {
		if(_ptree!=NULL)
		{
			pdt_free_tree(_ptree);
			_ptree = 0;
		}
	}
	
	db_con = pdt_dbf.init(db_url);
	if(db_con==NULL)
	{
	  LOG(L_ERR,"PDT:child_init #%d: Error while connecting database\n",r);
	  return -1;
	}
	
	if (pdt_dbf.use_table(db_con, db_table) < 0)
	{
		LOG(L_ERR, "PDT:child_init #%d: Error in use_table\n", r);
		return -1;
	}
	if(sync_time<=0)
		sync_time = 300;
	sync_time += r%60;

	DBG("PDT:child_init #%d: Database connection opened successfully\n",r);
	
	return 0;
}

static void mod_destroy(void)
{
	DBG("PDT: mod_destroy : Cleaning up\n");
	if (_dhash!=NULL)
		free_hash_list(_dhash);
	if (_ptree!=NULL)
		pdt_free_tree(_ptree);
	if (db_con!=NULL && pdt_dbf.close!=NULL)
		pdt_dbf.close(db_con);
}


static int w_prefix2domain(struct sip_msg* msg, char* str1, char* str2)
{
	return prefix2domain(msg, 0);
}

static int w_prefix2domain_1(struct sip_msg* msg, char* mode, char* str2)
{
	if(mode!=NULL && *mode=='1')
		return prefix2domain(msg, 1);
	else if(mode!=NULL && *mode=='2')
			return prefix2domain(msg, 2);
	else return prefix2domain(msg, 0);
}

/* change the r-uri if it is a PSTN format */
static int prefix2domain(struct sip_msg* msg, int mode)
{
	str *d, p;
	time_t crt_time;
	int plen;
	struct sip_uri uri;
	
	if(msg==NULL)
	{
		LOG(L_ERR,"PDT:prefix2domain: weird error\n");
		return -1;
	}
	
	/* parse the uri, if not yet */
	if(msg->parsed_uri_ok==0)
		if(parse_sip_msg_uri(msg)<0)
		{
			LOG(L_ERR,"PDT:prefix2domain: ERROR while parsing the R-URI\n");
			return -1;
		}

	/* if the user part begin with the prefix for PSTN users, extract the code*/
	if (msg->parsed_uri.user.len<=0)
	{
		DBG("PDT:prefix2domain: user part of the message is empty\n");
		return 1;
	}
	
	if(prefix.len>0 && prefix.len < msg->parsed_uri.user.len
			&& strncasecmp(prefix.s, msg->parsed_uri.user.s, prefix.len)!=0)
	{
		DBG("PDT:prefix2domain: PSTN prefix did not matched\n");
		return 1;
			
	}

	p.s   = msg->parsed_uri.user.s + prefix.len;
	p.len = msg->parsed_uri.user.len - prefix.len;

	pdt_print_tree(_ptree);

	/* check if need for sync */
	crt_time = time(NULL);
	if(last_sync + sync_time < crt_time)
	{
		last_sync = crt_time;
		if(pdt_sync_cache())
		{
			LOG(L_ERR, "PDT:prefix2domain: cannot update the cache\n");
			return -1;
		}
	}

	/* take the domain from  FROM uri as sdomain */
	if(parse_from_header(msg)==-1 ||  msg->from == NULL || get_from(msg)==NULL)
	{
		LOG(L_ERR,
			"prefix_to_domain: ERROR cannot parse FROM header\n");
		return -1;
	}	
		
	memset(&uri, 0, sizeof(struct sip_uri));
	if (parse_uri(get_from(msg)->uri.s, get_from(msg)->uri.len , &uri)<0)
	{
		LOG(L_ERR,"prefix_to_domain: failed to parse From uri\n");
		return -1;
	}
//	pdt_print_tree(_ptree);
	
	/* find the domain that corresponds to this prefix */
	plen = 0;
	if((d=pdt_get_domain(_ptree, &uri.host, &p, &plen))==NULL)
	{
		LOG(L_INFO, "PDT:prefix2domain: no prefix found in [%.*s]\n",
				p.len, p.s);
		return -1;
	}
	
	/* update the new uri */
	if(update_new_uri(msg, plen, d, mode)<0)
	{
		LOG(L_ERR, "PDT:prefix2domain: new_uri cannot be updated\n");
		return -1;
	}
	return 1;
}

/* change the uri according to translation of the prefix */
int update_new_uri(struct sip_msg *msg, int plen, str *d, int mode)
{
	struct action act;
	if(msg==NULL || d==NULL)
	{
		LOG(L_ERR, "PDT:update_new_uri: bad parameters\n");
		return -1;
	}
	
	if(mode==0 || (mode==1 && prefix.len>0))
	{
		act.type = STRIP_T;
		act.p1_type = NUMBER_ST;
		if(mode==0)
			act.p1.number = plen + prefix.len;
		else
			act.p1.number = prefix.len;
		act.next = 0;

		if (do_action(&act, msg) < 0)
		{
			LOG(L_ERR, "PDT:update_new_uri:Error removing prefix\n");
			return -1;
		}
	}
	
	act.type = SET_HOSTPORT_T;
	act.p1_type = STRING_ST;
	act.p1.string = d->s;
	act.next = 0;

	if (do_action(&act, msg) < 0)
	{
		LOG(L_ERR, "PDT:update_new_uri:Error changing domain\n");
		return -1;
	}

	DBG("PDT: update_new_uri: len=%d uri=%.*s\n", msg->new_uri.len, 
			msg->new_uri.len, msg->new_uri.s);
	
	return 0;
}

int pdt_load_db()
{
	db_key_t db_cols[3] = {sdomain_column, prefix_column, domain_column};
	str p, d, sdomain;
	db_res_t* db_res = NULL;
	int i;
	
	
	if(db_con==NULL)
	{
		LOG(L_ERR, "PDT:pdt_load_db: no db connection\n");
		return -1;
	}
		
	if (pdt_dbf.use_table(db_con, db_table) < 0)
	{
		LOG(L_ERR, "PDT:pdt_load_db: Error in use_table\n");
		return -1;
	}
	
	if(pdt_dbf.query(db_con, NULL, NULL, NULL, db_cols,
				0, 3, sdomain_column, &db_res)==0)
	{
		for(i=0; i<RES_ROW_N(db_res); i++)
		{
			/* check for NULL values ?!?! */
			sdomain.s = (char*)(RES_ROWS(db_res)[i].values[0].val.string_val);
			sdomain.len = strlen(sdomain.s);

			p.s = (char*)(RES_ROWS(db_res)[i].values[1].val.string_val);
			p.len = strlen(p.s);
			
			d.s = (char*)(RES_ROWS(db_res)[i].values[2].val.string_val);
			d.len = strlen(d.s);

			if(p.s==NULL || d.s==NULL || sdomain.s==NULL ||
					p.len<=0 || d.len<=0 || sdomain.len<=0)
			{
				LOG(L_ERR, "PDT:pdt_load_db: Error - bad values in db\n");
				goto error;
			}
		
			if(pdt_check_pd(_dhash, &sdomain, &p, &d)==1)
			{
				LOG(L_ERR,
				"PDT:pdt_load_db:sdomain [%.*s]: prefix [%.*s] or domain <%.*s> duplicated\n",
					sdomain.len, sdomain.s, p.len, p.s, d.len, d.s);
				goto error;;
			}

			if(pdt_add_to_tree(&_ptree, &sdomain, &p, &d)<0)
			{
				LOG(L_ERR, "PDT:pdt_load_db: Error adding info to tree\n");
				goto error;
			}
			
			if(pdt_add_to_hash(_dhash, &sdomain, &p, &d)!=0)
			{
				LOG(L_ERR, "PDT:pdt_load_db: Error adding info to hash\n");
				goto error;
			}
 		}
	}
	
	pdt_dbf.free_result(db_con, db_res);
	return 0;

error:
	pdt_dbf.free_result(db_con, db_res);
	return -1;
}

int pdt_sync_cache()
{
	pd_op_t *ito;
	hash_t *it;
	pdt_tree_t *itree;
	
	DBG("PDT:pdt_sync_cache: ...\n");

	if(_dhash==NULL || _ptree==NULL)
	{
		LOG(L_ERR, "PDT:pdt_sync_cache: strange situation\n");
		return -1;
	}
	
	lock_get(&_dhash->hl_lock);
	it = _dhash->hash;
	while(it != NULL)
	{
		itree = pdt_get_tree(_ptree, &it->sdomain);
		if(itree!=NULL && itree->idsync >= it->max_id)
			continue; 

		ito = it->diff;
		while(ito!=NULL && itree->idsync >= ito->id)
			ito = ito->n;
		
		while(ito!=NULL)
		{
			DBG("PDT:pdt_sync_cache: sync op[%d]=%d...\n",
				ito->id, ito->op);
			switch(ito->op)
			{
				case PDT_ADD:
					if(pdt_add_to_tree(&_ptree, &it->sdomain, &ito->cell->prefix,
								&ito->cell->domain)<0)
					{
						LOG(L_ERR, "PDT:pdt_sync_cache: Error to insert into tree\n");
						goto error;
					}
					break;
				case PDT_DELETE:
					if(itree==NULL)
					{
						LOG(L_ERR,
							"PDT:pdt_sync_cache: Error to remove from tree, tree does not exist\n");
						goto error;
					}
					if(pdt_remove_prefix_from_tree(itree, &it->sdomain, &ito->cell->prefix)!=0)
					{
						LOG(L_ERR,
							"PDT:pdt_sync_cache: Error to remove from tree\n");
						goto error;
					}
					break;
				default:
					LOG(L_ERR, "PDT:pdt_sync_cache: unknown operation\n");
			}
			itree->idsync = ito->id;
			ito->count++;
			ito = ito->n;
		}
		
		it = it->next;
	}


	lock_release(&_dhash->hl_lock);
	return 0;
error:
	lock_release(&_dhash->hl_lock);
	return -1;
}

void pdt_clean_cache(unsigned int ticks, void *param)
{
	pd_op_t *ito, *tmp;
	hash_t *it;

	/* DBG("PDT:pdt_clean_cache: ...\n"); */
	
	if(_dhash==NULL)
	{
		LOG(L_ERR, "PDT:pdt_clean_cache: strange situation\n");
		return;
	}
	
	lock_get(&_dhash->hl_lock);

	it = _dhash->hash;
	while(it!=NULL)
	{
		ito = it->diff;
		while(ito!=NULL)
		{
			if(ito->count >= _dhash->workers)
			{
				DBG("PDT:pdt_clean_cache: cleaning sdomain<%.*s> op[%d]=%d...\n",
					it->sdomain.len, it->sdomain.s, ito->id, ito->op);
				free_cell(ito->cell);
				if(ito->p!=NULL)
					(ito->p)->n = ito->n;
				else
					it->diff = ito->n;
				if(ito->n!=NULL)
					(ito->n)->p = ito->p;
				tmp = ito;
				ito = ito->n;
				shm_free(tmp);
			} 
			else
				ito = ito->n;
		}
		it = it->next;	
	}
	lock_release(&_dhash->hl_lock);
	return;
}

/**
 *	Fifo command example:
 * 
 *	---
 *	 :pdt_add:[response_file]\n
 *	 sdomain\n
 *	 prefix\n
 *	 domain\n
 *	 \n
 * 	--
 */
int pdt_fifo_add(FILE *stream, char *response_file)
{
	db_key_t db_keys[NR_KEYS] = {sdomain_column, prefix_column, domain_column};
	db_val_t db_vals[NR_KEYS];
	db_op_t  db_ops[NR_KEYS] = {OP_EQ, OP_EQ};

	char dbuf[256], sdbuf[256], pbuf[256];
	str sd, sp, sdomain;

	if(_dhash==NULL)
	{
		LOG(L_ERR, "PDT:pdt_fifo_add: strange situation\n");
		fifo_reply(response_file, "500 pdt_fifo_add - server error\n");
		return -1;
	}

	/* read sdomain */
	sdomain.s = sdbuf;
	if(!read_line(sdomain.s, 255, stream, &sdomain.len) || sdomain.len==0)	
	{
		LOG(L_ERR, "PDT:pdt_fifo_add: could not read domain\n");
		fifo_reply(response_file, "400 pdt_fifo_add - domain not found\n");
		return 1;
	}
	sdbuf[sdomain.len] = '\0';
	
	/* read prefix */
	sp.s = pbuf;
	if(!read_line(sp.s, 255, stream, &sp.len) || sp.len==0)	
	{
		LOG(L_ERR, "PDT:pdt_fifo_add: could not read prefix\n");
		fifo_reply(response_file, "400 pdt_fifo_add - prefix not found\n");
		return 1;
	}
	pbuf[sp.len] = '\0';

	while(sp.s!=NULL && *sp.s!='\0')
	{
		if(*sp.s < '0' || *sp.s > '9')
		{
			LOG(L_ERR, "PDT:pdt_fifo_add: bad prefix [%s]\n", pbuf);
			fifo_reply(response_file, "400 pdt_fifo_add - bad prefix\n");
			return 1;
		}
		sp.s++;
	}
	sp.s = pbuf;

	/* read domain */
	sd.s = dbuf;
	if(!read_line(sd.s, 255, stream, &sd.len) || sd.len==0)	
	{
		LOG(L_ERR, "PDT:pdt_fifo_add: could not read domain\n");
		fifo_reply(response_file, "400 pdt_fifo_add - domain not found\n");
		return 1;
	}
	dbuf[sd.len] = '\0';

	if(pdt_check_pd(_dhash, &sdomain, &sp, &sd)==1)
	{
		LOG(L_ERR, "PDT:pdt_fifo_add: (sdomain,prefix,domain) exists\n");
		fifo_reply(response_file,
			"400 pdt_fifo_add - (sdomain,prefix,domain) exists already\n");
		return 1;
	}
	db_vals[0].type = DB_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = sdomain.s;
	db_vals[0].val.str_val.len = sdomain.len;

	db_vals[1].type = DB_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = sp.s;
	db_vals[1].val.str_val.len = sp.len;

	db_vals[2].type = DB_STR;
	db_vals[2].nul = 0;
	db_vals[2].val.str_val.s = sd.s;
	db_vals[2].val.str_val.len = sd.len;
	
	/* insert a new domain into database */
	if(pdt_dbf.insert(db_con, db_keys, db_vals, NR_KEYS)<0)
	{
		LOG(L_ERR, "PDT:pdt_fifo_add: error storing new prefix/domain\n");
		fifo_reply(response_file, "430 Cannot store prefix/domain\n");
		return -1;
	}
	
	if(pdt_add_to_hash(_dhash, &sdomain, &sp, &sd)!=0)
	{
		LOG(L_ERR, "PDT:pdt_fifo_add: could not add to cache\n");
		fifo_reply(response_file, "431 could not add to cache\n");
		goto error;
	}

	fifo_reply(response_file, "230 Added <%.*s> [%.*s] <%.*s>\n",
		sdomain.len, sdomain.s, sp.len, sp.s, sd.len, sd.s);

	return 0;
	
error:
	if(pdt_dbf.delete(db_con, db_keys, db_ops, db_vals, NR_KEYS)<0)
		LOG(L_ERR,"PDT:pdt_fifo_add: database/cache are inconsistent\n");
	
	return -1;
}

/**
 *	Fifo command example:
 * 
 *	---
 *	 :pdt_delete:[response_file]\n
 *	 sdomain
 *	 domain\n
 *	 \n
 * 	--
 */
static int pdt_fifo_delete(FILE *stream, char *response_file)
{
	char sdbuf[256], sdomainbuf[256];
	str sd, sdomain;
	int ret;
	
	db_key_t db_keys[2] = {sdomain_column, domain_column};
	db_val_t db_vals[2];
	db_op_t  db_ops[2] = {OP_EQ, OP_EQ};

	if(_dhash==NULL)
	{
		LOG(L_ERR, "PDT:pdt_fifo_delete: strange situation\n");
		fifo_reply(response_file, "500 pdt_fifo_delete - server error\n");
		return -1;
	}

	/* read sdomain */
	sdomain.s = sdomainbuf;
	if(!read_line(sdomain.s, 255, stream, &sdomain.len) || sdomain.len==0)	
	{
		LOG(L_ERR, "PDT:pdt_fifo_delete: could not read sdomain\n");
		fifo_reply(response_file, "400 pdt_fifo_delete - sdomain not found\n");
		return 1;
	}
	sdomainbuf[sdomain.len] = '\0';
	if(*sdomain.s=='\0' || *sdomain.s=='.')
	{
		LOG(L_INFO, "PDT:pdt_fifo_delete: empty sdomain\n");
		fifo_reply(response_file, "400 pdt_fifo_delete - empty pram\n");
		return 1;
	}
	
	/* read domain */
	sd.s = sdbuf;
	if(!read_line(sd.s, 255, stream, &sd.len) || sd.len==0)	
	{
		LOG(L_ERR, "PDT:pdt_fifo_delete: could not read domain\n");
		fifo_reply(response_file, "400 pdt_fifo_delete - domain not found\n");
		return 1;
	}
	sdbuf[sd.len] = '\0';
	if(*sd.s=='\0' || *sd.s=='.')
	{
		LOG(L_INFO, "PDT:pdt_fifo_delete: empty domain\n");
		fifo_reply(response_file, "400 pdt_fifo_delete - empty pram\n");
		return 1;
	}

	if((ret = pdt_remove_from_hash_list(_dhash, &sdomain, &sd))<0)
	{
		DBG("PDT:pdt_fifo_delete: error encountered when deleting domain\n");
		fifo_reply(response_file, "error encountered when deleting domain!\n");
		return -1;
	}
	
	if(ret==1)
	{
		DBG("PDT:pdt_fifo_delete: prefix for sdomain [%.*s]domain [%.*s] not found\n",
				sdomain.len, sdomain.s, sd.len, sd.s);
		fifo_reply(response_file, "404 domain not found!\n");
		return 0;
	}
	
//	pdt_print_hash_list(_dhash);

	/* ret=0 means domain was deleted from cache, so it must be deleted from db */
	db_vals[0].type = DB_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = sdomain.s;
	db_vals[0].val.str_val.len = sdomain.len;
	
	db_vals[1].type = DB_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = sd.s;
	db_vals[1].val.str_val.len = sd.len;

	if(pdt_dbf.delete(db_con, db_keys, db_ops, db_vals, 2)<0)
	{
		LOG(L_ERR,"PDT:pdt_fifo_delete: database/cache are inconsistent\n");
		fifo_reply(response_file, "602 database/cache are inconsistent!\n");
	} 
	else 
		fifo_reply(response_file, "200 domain removed!\n");
		
	return 0;
}

/**
 *	Fifo command example:
 * 
 *	---
 *	 :pdt_list:[response_file]\n
 *	 sdomain\n
 *	 prefix\n
 *	 domain\n
 *	 \n
 * 	--
 *
 * 	- '.' (dot) means NULL value 
 * 	- the comparison operation is 'START WITH' -- if domain is 'a' then
 * 	  all domains starting with 'a' are listed
 */

static int pdt_fifo_list(FILE *stream, char *response_file)
{
	char sdbuf[256], pbuf[256], sdomainbuf[256];
	str sd, sp, sdomain;
	pd_t *it;
	int i;
	FILE *freply=NULL;
	hash_t *h;

	if(_dhash==NULL)
	{
		LOG(L_ERR, "PDT:pdt_fifo_list: strange situation\n");
		fifo_reply(response_file, "500 pdt_fifo_list - server error\n");
		return -1;
	}

	sdomain.s = sdomainbuf;
	if(!read_line(sdomain.s, 255, stream, &sdomain.len) || sdomain.len==0)	
	{
		LOG(L_ERR, "PDT:pdt_fifo_list: could not read domain\n");
		fifo_reply(response_file, "400 pdt_fifo_list - domain not found\n");
		return 1;
	}
	sdomainbuf[sdomain.len] = '\0';
	if(*sdomain.s=='\0' || *sdomain.s=='.')
	{
		sdomain.s   = NULL;
		sdomain.len = 0;
	}
	
	sp.s = pbuf;
	if(!read_line(sp.s, 255, stream, &sp.len) || sp.len==0)	
	{
		LOG(L_ERR, "PDT:pdt_fifo_list: could not read prefix\n");
		fifo_reply(response_file, "400 pdt_fifo_list - prefix not found\n");
		return 1;
	}
	pbuf[sp.len] = '\0';

	if(*sp.s!='\0' && *sp.s!='.')
	{
		while(sp.s!=NULL && *sp.s!='\0')
		{
			if(*sp.s < '0' || *sp.s > '9')
			{
				LOG(L_ERR, "PDT:pdt_fifo_list: bad prefix [%s]\n", pbuf);
				fifo_reply(response_file, "400 pdt_fifo_list - bad prefix\n");
				return 1;
			}
			sp.s++;
		}
		sp.s = pbuf;
	} else {
		sp.s   = NULL;
		sp.len = 0;
	}

	sd.s = sdbuf;
	if(!read_line(sd.s, 255, stream, &sd.len) || sd.len==0)	
	{
		LOG(L_ERR, "PDT:pdt_fifo_list: could not read domain\n");
		fifo_reply(response_file, "400 pdt_fifo_list - domain not found\n");
		return 1;
	}
	sdbuf[sd.len] = '\0';
	if(*sd.s=='\0' || *sd.s=='.')
	{
		sd.s   = NULL;
		sd.len = 0;
	}

	freply = open_reply_pipe(response_file);
	if(freply==NULL)
	{
		LOG(L_ERR, "PDT:pdt_fifo_list: can't open reply fifo '%s'\n",
				response_file);
		return -1;
	}
	
	lock_get(&_dhash->hl_lock);

	h = _dhash->hash;
	while(h!=NULL)
	{
		if(sdomain.s==NULL || 
			(sdomain.s!=NULL && h->sdomain.len>=sdomain.len && 
			 strncmp(h->sdomain.s, sdomain.s, sdomain.len)==0))
		{
			for(i=0; i<h->hash_size; i++)
			{
				it = h->dhash[i];
				while(it!=NULL)
				{
					if((sp.s==NULL && sd.s==NULL)
						|| (sp.s!=NULL && it->prefix.len>=sp.len &&
							strncmp(it->prefix.s, sp.s, sp.len)==0)
						|| (sd.s!=NULL && it->domain.len>=sd.len &&
							strncasecmp(it->domain.s, sd.s, sd.len)==0))
					fprintf(freply, "%.*s %.*s %.*s\n",
						h->sdomain.len, h->sdomain.s,
						it->prefix.len, it->prefix.s,
						it->domain.len, it->domain.s);
					it = it->n;
				}
			}
		}
			h = h->next;
	}

	lock_release(&_dhash->hl_lock);
	
	fprintf(freply, "\n*200 OK\n");
	if(freply!=NULL)
		fclose(freply);

	return 0;
}

static int get_domainprefix_unixsock(str* msg)
{
	return 0;
#if 0
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];
	db_op_t  db_ops[NR_KEYS] = {OP_EQ, OP_EQ};
	code_t code;
	dc_t* cell; 
	str sdomain, sauth;
	int authorized=0;
		
	/* read a line -the domain name parameter- from the fifo */
	if(unixsock_read_line(&sdomain, msg) != 0)	
	{
		unixsock_reply_asciiz("400 Domain expected\n");
		goto send_err;
	}

	/* read a line -the authorization to register new domains- from the fifo */
	if(unixsock_read_line(&sauth, msg) != 0)
	{	
		unixsock_reply_asciiz("400 Authorization expected\n");
		goto send_err;
	}

	sdomain.s[sdomain.len] = '\0';

	/* see what kind of user we have */
	authorized = sauth.s[0]-'0';

	lock_get(&l);

	/* search the domain in the hashtable */
	cell = get_code_from_hash(hash->dhash, hash->hash_size, sdomain.s);
	
	/* the domain is registered */
	if(cell)
	{

		lock_release(&l);
			
		/* domain already in the database */
		unixsock_reply_printf("201 Domain name=%.*s Domain code=%d%d\n",
				      sdomain.len, ZSW(sdomain.s), cell->code, code_terminator);
		unixsock_reply_send();
		return 0;
		
	}
	
	/* domain not registered yet */
	/* user not authorized to register new domains */	
	if(!authorized)
	{
		lock_release(&l);
		unixsock_reply_asciiz("203 Domain name not registered yet\n");
		unixsock_reply_send();
		return 0;
	}

	code = *next_code;
	*next_code = apply_correction(code+1);
		

	/* prepare for insertion into database */
	db_keys[0] = DB_KEY_CODE;
	db_keys[1] = DB_KEY_NAME;

	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = code;

	db_vals[1].type = DB_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = sdomain.s;
	db_vals[1].val.str_val.len = sdomain.len;
	DBG("%d %.*s\n", code, sdomain.len, sdomain.s);
			
	/* insert a new domain into database */
	if(pdt_dbf.insert(db_con, db_keys, db_vals, NR_KEYS)<0)
	{
		/* next available code is still code */
		*next_code = code;
		lock_release(&l);
		LOG(L_ERR, "PDT: get_domaincode: error storing a"
				" new domain\n");
		unixsock_reply_asciiz("204 Cannot register the new domain in a consistent way\n");
		unixsock_reply_send();
		return -1;
	}
	
	/* insert the new domain into hashtables, too */
	cell = new_cell(sdomain.s, code);
	if(add_to_double_hash(hash, cell)<0)
		goto error;		

	lock_release(&l);

	/* user authorized to register new domains */
	unixsock_reply_printf("202 Domain name=%.*s New domain code=%d%d\n",
			      sdomain.len, ZSW(sdomain.s), code, code_terminator);

	unixsock_reply_send();
	return 0;

 error:
	/* next available code is still code */
	*next_code = code;
	/* delete from database */
	if(pdt_dbf.delete(db_con, db_keys, db_ops, db_vals, NR_KEYS)<0)
		LOG(L_ERR,"PDT: get_domaincode: database/share-memory are inconsistent\n");
	lock_release(&l);
	unixsock_reply_asciiz("500 Database/shared-memory are inconsistent\n");
send_err:
	unixsock_reply_send();
	return -1;
#endif
}


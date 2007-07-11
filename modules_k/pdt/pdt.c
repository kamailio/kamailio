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
#include "../../parser/parse_uri.h"
#include "../../timer.h"
#include "../../ut.h"
#include "../../locking.h"
#include "../../action.h"
#include "../../parser/parse_from.h"
#include "domains.h"
#include "pdtree.h"

MODULE_VERSION


#define NR_KEYS			3

int hs_two_pow = 4;

/** structures containing prefix-domain pairs */
hash_list_t **_dhash = NULL; 
pdt_tree_t **_ptree = NULL; 

/** database connection */
static db_con_t *db_con = NULL;
static db_func_t pdt_dbf;


/** parameters */
static char *db_url = "mysql://root@127.0.0.1/pdt";
char *db_table = "pdt";
char *sdomain_column = "sdomain";
char *prefix_column  = "prefix";
char *domain_column  = "domain";

/** pstn prefix */
str prefix = {"", 0};
/* List of allowed chars for a prefix*/
str pdt_char_list = {"0123456789", 10};

/* lock, ref counter and flag used for reloading the date */
static gen_lock_t *pdt_lock = 0;
static volatile int pdt_tree_refcnt = 0;
static volatile int pdt_reload_flag = 0;

static int  w_prefix2domain(struct sip_msg* msg, char* str1, char* str2);
static int  w_prefix2domain_1(struct sip_msg* msg, char* mode, char* str2);
static int  w_prefix2domain_2(struct sip_msg* msg, char* mode, char* sd_en);
static int  mod_init(void);
static void mod_destroy(void);
static int  child_init();
static int  mod_child_init(int r);

static int prefix2domain(struct sip_msg*, int mode, int sd_en);

static struct mi_root* pdt_mi_reload(struct mi_root*, void* param);
static struct mi_root* pdt_mi_add(struct mi_root*, void* param);
static struct mi_root* pdt_mi_delete(struct mi_root*, void* param);
static struct mi_root* pdt_mi_list(struct mi_root*, void* param);

int update_new_uri(struct sip_msg *msg, int plen, str *d, int mode);
int pdt_load_db();

static cmd_export_t cmds[]={
	{"prefix2domain", w_prefix2domain,   0, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"prefix2domain", w_prefix2domain_1, 1, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"prefix2domain", w_prefix2domain_2, 2, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"db_url",         STR_PARAM, &db_url},
	{"db_table",       STR_PARAM, &db_table},
	{"sdomain_column", STR_PARAM, &sdomain_column},
	{"prefix_column",  STR_PARAM, &prefix_column},
	{"domain_column",  STR_PARAM, &domain_column},
	{"prefix",         STR_PARAM, &prefix.s},
	{"char_list",      STR_PARAM, &pdt_char_list.s},
	{"hsize_2pow",     INT_PARAM, &hs_two_pow},
	{0, 0, 0}
};

static mi_export_t mi_cmds[] = {
	{ "pdt_add",     pdt_mi_add,     0,  0,  child_init },
	{ "pdt_reload",  pdt_mi_reload,  0,  0,  0 },
	{ "pdt_delete",  pdt_mi_delete,  0,  0,  0 },
	{ "pdt_list",    pdt_mi_list,    0,  0,  0 },
	{ 0, 0, 0, 0, 0}
};


struct module_exports exports = {
	"pdt",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	mi_cmds,        /* exported MI functions */
	0,              /* exported pseudo-variables */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	mod_child_init  /* per child init function */
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
	pdt_char_list.len = strlen(pdt_char_list.s);
	if(pdt_char_list.len<=0)
	{
		LOG(L_ERR, "PDT:mod_init: invalid pdt char list\n");
		return -1;
	}
	LOG(L_ERR,"PDT:mod_init: pdt_char_list=%s \n",pdt_char_list.s);

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
	
	if ( (pdt_lock=lock_alloc())==0) {
		LOG(L_CRIT,"PDT: mod_init: failed to alloc lock\n");
		goto error1;
	}
	if (lock_init(pdt_lock)==0 ) {
		LOG(L_CRIT,"PDT: mod_init: failed to init lock\n");
		goto error1;
	}
	
	/* pdt hash and tree pointers in shm */
	_dhash = (hash_list_t**)shm_malloc( sizeof(hash_list_t*) );
	if (_dhash==0) {
		LOG(L_CRIT,"PDT:mod_init: failed to get shm mem for dhash\n");
		goto error1;
	}
	*_dhash = 0;
	_ptree = (pdt_tree_t**)shm_malloc( sizeof(pdt_tree_t*) );
	if (_ptree==0) {
		LOG(L_CRIT,"PDT:mod_init: failed to get shm mem for ptree\n");
		goto error1;
	}
	*_dhash = 0;
	

	/* loading all information from database */
	if(pdt_load_db()!=0)
	{
		LOG(L_ERR, "PDT:mod_init: cannot load info from database\n");	
		goto error1;
	}
		
	pdt_dbf.close(db_con);
	db_con = 0;

	pdt_print_tree(*_ptree);
	DBG("PDT:mod_init: -------------------\n");
	pdt_print_hash_list(*_dhash);

	/* success code */
	return 0;

error1:
	if (pdt_lock)
	{
		lock_destroy( pdt_lock );
		lock_dealloc( pdt_lock );
		pdt_lock = 0;
	}
	if(_dhash!=0)
		shm_free(_dhash);
	if(_ptree!=0)
		shm_free(_ptree);

	if(db_con!=NULL)
	{
		pdt_dbf.close(db_con);
		db_con = 0;
	}
	return -1;
}


static int child_init()
{
	db_con = pdt_dbf.init(db_url);
	if(db_con==NULL)
	{
		LOG(L_ERR,"ERROR:PDT:child_init: failed to connect to database\n");
		return -1;
	}

	if (pdt_dbf.use_table(db_con, db_table) < 0)
	{
		LOG(L_ERR, "ERROR:PDT:child_init: use_table failed\n");
		return -1;
	}
	return 0;
}


/* each child get a new connection to the database */
static int mod_child_init(int r)
{
	if ( child_init()!=0 )
		return -1;

	DBG("PDT:mod_child_init #%d: Database connection opened successfully\n",r);

	return 0;
}


static void mod_destroy(void)
{
	DBG("PDT: mod_destroy : Cleaning up\n");
	if (_dhash!=NULL)
	{
		if (*_dhash!=NULL)
			free_hash_list(*_dhash);
		shm_free(_dhash);
	}
	if (_ptree!=NULL)
	{
		if (*_ptree!=NULL)
			pdt_free_tree(*_ptree);
		shm_free(_ptree);
	}
	if (db_con!=NULL && pdt_dbf.close!=NULL)
		pdt_dbf.close(db_con);
		/* destroy lock */
	if (pdt_lock)
	{
		lock_destroy( pdt_lock );
		lock_dealloc( pdt_lock );
		pdt_lock = 0;
	}

}


static int w_prefix2domain(struct sip_msg* msg, char* str1, char* str2)
{
	return prefix2domain(msg, 0, 0);
}

static int w_prefix2domain_1(struct sip_msg* msg, char* mode, char* str2)
{
	if(mode!=NULL && *mode=='1')
		return prefix2domain(msg, 1, 0);
	else if(mode!=NULL && *mode=='2')
			return prefix2domain(msg, 2, 0);
	else return prefix2domain(msg, 0, 0);
}

static int w_prefix2domain_2(struct sip_msg* msg, char* mode, char* sd_en)
{
	int tmp=0;
	
	if((sd_en==NULL) || ((sd_en!=NULL) && (*sd_en!='0') && (*sd_en!='1')
				&& (*sd_en!='2')))
		return -1;
	
    if (*sd_en=='1')
		tmp = 1;
    if (*sd_en=='2')
		tmp = 2;
	
		
	if(mode!=NULL && *mode=='1')
		return prefix2domain(msg, 1, tmp);
	else if(mode!=NULL && *mode=='2')
			return prefix2domain(msg, 2, tmp);
	else return prefix2domain(msg, 0, tmp);
}

/* change the r-uri if it is a PSTN format */
static int prefix2domain(struct sip_msg* msg, int mode, int sd_en)
{
	str *d, p, all={"*",1};
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
		return -1;
	}   
    
	if(prefix.len>0)
	{
		if (msg->parsed_uri.user.len<=prefix.len)
		{
			DBG("PDT:prefix2domain: user part is less than prefix\n");
			return -1;
		}   
		if(strncasecmp(prefix.s, msg->parsed_uri.user.s, prefix.len)!=0)
		{
			DBG("PDT:prefix2domain: PSTN prefix did not matched\n");
			return -1;
		}
	}   
	
	if(prefix.len>0 && prefix.len < msg->parsed_uri.user.len
			&& strncasecmp(prefix.s, msg->parsed_uri.user.s, prefix.len)!=0)
	{
		DBG("PDT:prefix2domain: PSTN prefix did not matched\n");
		return -1;
			
	}

	p.s   = msg->parsed_uri.user.s + prefix.len;
	p.len = msg->parsed_uri.user.len - prefix.len;

again:
	lock_get( pdt_lock );
	if (pdt_reload_flag) {
		lock_release( pdt_lock );
		sleep_us(5);
		goto again;
	}
	pdt_tree_refcnt++;
	lock_release( pdt_lock );

	if(sd_en==2)
	{	
		/* take the domain from  FROM uri as sdomain */
		if(parse_from_header(msg)<0 ||  msg->from == NULL 
				|| get_from(msg)==NULL)
		{
			LOG(L_ERR,
				"prefix_to_domain: ERROR cannot parse FROM header\n");
			goto error;
		}	
		
		memset(&uri, 0, sizeof(struct sip_uri));
		if (parse_uri(get_from(msg)->uri.s, get_from(msg)->uri.len , &uri)<0)
		{
			LOG(L_ERR,"prefix_to_domain: failed to parse From uri\n");
			goto error;
		}
	
		/* find the domain that corresponds to this prefix */
		plen = 0;
		if((d=pdt_get_domain(*_ptree, &uri.host, &p, &plen))==NULL)
		{
			plen = 0;
			if((d=pdt_get_domain(*_ptree, &all, &p, &plen))==NULL)
			{
				LOG(L_INFO, "PDT:prefix2domain: no prefix found in [%.*s]\n",
					p.len, p.s);
				goto error;
			}
		}
	} else if(sd_en==1) {	
		/* take the domain from  FROM uri as sdomain */
		if(parse_from_header(msg)<0 ||  msg->from == NULL
				|| get_from(msg)==NULL)
		{
			LOG(L_ERR,
				"prefix_to_domain: ERROR cannot parse FROM header\n");
			goto error;
		}	
		
		memset(&uri, 0, sizeof(struct sip_uri));
		if (parse_uri(get_from(msg)->uri.s, get_from(msg)->uri.len , &uri)<0)
		{
			LOG(L_ERR,"prefix_to_domain: failed to parse From uri\n");
			goto error;
		}
	
		/* find the domain that corresponds to this prefix */
		plen = 0;
		if((d=pdt_get_domain(*_ptree, &uri.host, &p, &plen))==NULL)
		{
			LOG(L_INFO, "PDT:prefix2domain: no prefix found in [%.*s]\n",
				p.len, p.s);
			goto error;
		}
	} else {
		/* find the domain that corresponds to this prefix */
		plen = 0;
		if((d=pdt_get_domain(*_ptree, &all, &p, &plen))==NULL)
		{
			LOG(L_INFO, "PDT:prefix2domain: no prefix found in [%.*s]\n",
				p.len, p.s);
			goto error;
		}
	}
	
	/* update the new uri */
	if(update_new_uri(msg, plen, d, mode)<0)
	{
		LOG(L_ERR, "PDT:prefix2domain: new_uri cannot be updated\n");
		goto error;
	}

	lock_get( pdt_lock );
	pdt_tree_refcnt--;
	lock_release( pdt_lock );
	return 1;

error:
	lock_get( pdt_lock );
	pdt_tree_refcnt--;
	lock_release( pdt_lock );
	return -1;
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
		act.elem[0].type = NUMBER_ST;
		if(mode==0)
			act.elem[0].u.number = plen + prefix.len;
		else
			act.elem[0].u.number = prefix.len;
		act.next = 0;

		if (do_action(&act, msg) < 0)
		{
			LOG(L_ERR, "PDT:update_new_uri:Error removing prefix\n");
			return -1;
		}
	}
	
	act.type = SET_HOSTPORT_T;
	act.elem[0].type = STRING_ST;
	act.elem[0].u.string = d->s;
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
	int i, ret;
	hash_list_t *_dhash_new = NULL; 
	pdt_tree_t *_ptree_new = NULL; 
	hash_list_t *old_hash = NULL; 
	pdt_tree_t *old_tree = NULL; 
	
	
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
	
	if((ret=pdt_dbf.query(db_con, NULL, NULL, NULL, db_cols,
				0, 3, sdomain_column, &db_res))!=0
			|| RES_ROW_N(db_res)<=0 )
	{
		pdt_dbf.free_result(db_con, db_res);
		if( ret==0)
			return 0;
		else
			return -1;
	}

	/* init the hash and tree in share memory */
	if( (_dhash_new = init_hash_list(hs_two_pow)) == NULL)
	{
		LOG(L_ERR, "PDT:pdt_load_db: domain hash could not be allocated\n");	
		goto error;
	}
	
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
			continue;
		}
		
		if(pdt_check_pd(_dhash_new, &sdomain, &p, &d)==1)
		{
			LOG(L_ERR,
				"PDT:pdt_load_db:sdomain [%.*s]: prefix [%.*s] or domain"
				" <%.*s> duplicated\n",
				sdomain.len, sdomain.s, p.len, p.s, d.len, d.s);
				continue;
		}

		if(pdt_add_to_tree(&_ptree_new, &sdomain, &p, &d)<0)
		{
			LOG(L_ERR, "PDT:pdt_load_db: Error adding info to tree\n");
			goto error;
		}
			
		if(pdt_add_to_hash(_dhash_new, &sdomain, &p, &d)!=0)
		{
			LOG(L_ERR, "PDT:pdt_load_db: Error adding info to hash\n");
			goto error;
		}
 	}
	
	pdt_dbf.free_result(db_con, db_res);


	/* block all readers */
	lock_get( pdt_lock );
	pdt_reload_flag = 1;
	lock_release( pdt_lock );

	while (pdt_tree_refcnt) {
		sleep_us(10);
	}

	old_tree = *_ptree;
	*_ptree = _ptree_new;
	old_hash = *_dhash;
	*_dhash = _dhash_new;

	pdt_reload_flag = 0;

	/* free old data */
	if (old_hash!=NULL)
		free_hash_list(old_hash);
	if (old_tree!=NULL)
		pdt_free_tree(old_tree);

	return 0;

error:
	pdt_dbf.free_result(db_con, db_res);
	if (_dhash_new!=NULL)
		free_hash_list(_dhash_new);
	if (_ptree_new!=NULL)
		pdt_free_tree(_ptree_new);
	return -1;
}

/**************************** MI ***************************/

/**
 * "pdt_reload" syntax :
 * \n
 */
static struct mi_root* pdt_mi_reload(struct mi_root *cmd_tree, void *param)
{
	/* re-loading all information from database */
	if(pdt_load_db()!=0)
	{
		LOG(L_ERR,"PDT:pdt_mi_reload: cannot re-load info from database\n");	
		goto error;
	}
	
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);

error:
	return init_mi_tree( 500, "Failed to reload",16);
}



/**
 * "pdt_add" syntax :
 *   sdomain
 *   prefix
 *   domain
 */
struct mi_root* pdt_mi_add(struct mi_root* cmd_tree, void* param)
{
	db_key_t db_keys[NR_KEYS] = {sdomain_column, prefix_column, domain_column};
	db_val_t db_vals[NR_KEYS];
	db_op_t  db_ops[NR_KEYS] = {OP_EQ, OP_EQ};
	int i= 0;
	str sd, sp, sdomain;
	struct mi_node* node= NULL;

	if(_dhash==NULL)
	{
		LOG(L_ERR, "PDT:pdt_mi_add: strange situation\n");
		return init_mi_tree( 500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* read sdomain */
	node = cmd_tree->node.kids;
	if(node == NULL)
		goto error1;

	sdomain = node->value;
	if(sdomain.s == NULL || sdomain.len== 0)
		return init_mi_tree( 404, "domain not found", 16);

	if(*sdomain.s=='.' )
		 return init_mi_tree( 400, "empty param",11);

	/* read prefix */
	node = node->next;
	if(node == NULL)
		goto error1;

	sp= node->value;
	if(sp.s== NULL || sp.len==0)
	{
		LOG(L_ERR, "PDT:pdt_mi_add: could not read prefix\n");
		return init_mi_tree( 404, "prefix not found", 16);
	}

	if(*sp.s=='.')
		 return init_mi_tree(400, "empty param", 11);

	while(i< sp.len)
	{
		if(strpos(pdt_char_list.s,sp.s[i]) < 0) 
			return init_mi_tree( 400, "bad prefix", 10);
		i++;
	}

	/* read domain */
	node= node->next;
	if(node == NULL || node->next!=NULL)
		goto error1;

	sd= node->value;
	if(sd.s== NULL || sd.len==0)
	{
		LOG(L_ERR, "PDT:pdt_mi_add: could not read domain\n");
		return init_mi_tree( 400, "domain not found", 16);
	}

	if(*sd.s=='.')
		 return init_mi_tree( 400, "empty param", 11);

	
	if(pdt_check_pd(*_dhash, &sdomain, &sp, &sd)==1)
	{
		LOG(L_ERR, "PDT:pdt_mi_add: (sdomain,prefix,domain) exists\n");
		return init_mi_tree( 400,
			"(sdomain,prefix,domain) exists already", 38);
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
		LOG(L_ERR, "PDT:pdt_mi_add: error storing new prefix/domain\n");
		return init_mi_tree( 500,"Cannot store prefix/domain", 26);
	}

	/* re-loading all information from database */
	if(pdt_load_db()!=0)
	{
		LOG(L_ERR,"PDT:pdt_mi_delete: cannot re-load info from database\n");	
		goto error;
	}
	
	DBG("PDT:pdt_mi_add: new prefix added %.*s-%.*s => %.*s\n",
			sdomain.len, sdomain.s, sp.len, sp.s, sd.len, sd.s);
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);

	
error:
	if(pdt_dbf.delete(db_con, db_keys, db_ops, db_vals, NR_KEYS)<0)
		LOG(L_ERR,"PDT:pdt_mi_add: database/cache are inconsistent\n");
	return init_mi_tree( 500, "could not add to cache", 23 );

error1:
	return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

}

/**
 * "pdt_delete" syntax:
 *    sdomain
 *    domain
 */
struct mi_root* pdt_mi_delete(struct mi_root* cmd_tree, void* param)
{
	str sd, sdomain;
	struct mi_node* node= NULL;
	db_key_t db_keys[2] = {sdomain_column, domain_column};
	db_val_t db_vals[2];
	db_op_t  db_ops[2] = {OP_EQ, OP_EQ};

	if(_dhash==NULL)
	{
		LOG(L_ERR, "PDT:pdt_mi_delete: strange situation\n");
		return init_mi_tree( 500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* read sdomain */
	node = cmd_tree->node.kids;
	if(node == NULL)
		goto error;

	sdomain = node->value;
	if(sdomain.s == NULL || sdomain.len== 0)
		return init_mi_tree( 404, "domain not found", 16);

	if( *sdomain.s=='.' )
		 return init_mi_tree( 400, "400 empty param",11);

	/* read domain */
	node= node->next;
	if(node == NULL || node->next!=NULL)
		goto error;

	sd= node->value;
	if(sd.s== NULL || sd.len==0)
	{
		LOG(L_ERR, "PDT:pdt_mi_delete: could not read domain\n");
		return init_mi_tree(404, "domain not found", 16);
	}

	if(*sd.s=='.')
		 return init_mi_tree( 400, "empty param", 11);


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
		LOG(L_ERR,"PDT:pdt_mi_delete: database/cache are inconsistent\n");
		return init_mi_tree( 500, "database/cache are inconsistent", 31 );
	} 
	/* re-loading all information from database */
	if(pdt_load_db()!=0)
	{
		LOG(L_ERR,"PDT:pdt_mi_delete: cannot re-load info from database\n");	
		return init_mi_tree( 500, "cannot reload", 13 );
	}

	DBG("PDT:pdt_mi_delete: prefix for sdomain [%.*s] domain [%.*s] "
			"removed\n", sdomain.len, sdomain.s, sd.len, sd.s);
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
error:
	return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
}


/**
 * "pdt_list" syntax :
 *    sdomain
 *    prefix
 *    domain
 *
 * 	- '.' (dot) means NULL value and will match anything
 * 	- the comparison operation is 'START WITH' -- if domain is 'a' then
 * 	  all domains starting with 'a' are listed
 *
 * 	  Examples
 * 	  pdt_list o 2 .    - lists the entries where sdomain is starting with 'o', 
 * 	                      prefix is starting with '2' and domain is anything
 * 	  
 * 	  pdt_list . 2 open - lists the entries where sdomain is anything, prefix 
 * 	                      starts with '2' and domain starts with 'open'
 */

struct mi_root* pdt_mi_list(struct mi_root* cmd_tree, void* param)
{
	str sd, sp, sdomain;
	pd_t *it;
	unsigned int i= 0;
	hash_t *h;
	struct mi_root* rpl_tree = NULL;
	struct mi_node* rpl = NULL;
	struct mi_node* node = NULL;
	struct mi_attr* attr= NULL;

	DBG("PDT:pdt_mi_list ...\n");
	if(_dhash==NULL)
	{
		LOG(L_ERR, "PDT:pdt_mi_list: empty domain list\n");
		return init_mi_tree( 500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	/* read sdomain */
	sdomain.s = 0;
	sdomain.len = 0;
	sp.s = 0;
	sp.len = 0;
	sd.s = 0;
	sd.len = 0;
	node = cmd_tree->node.kids;
	if(node != NULL)
	{
		sdomain = node->value;
		if(sdomain.s == NULL || sdomain.len== 0)
			return init_mi_tree( 404, "domain not found", 16);

		if(*sdomain.s=='.')
			sdomain.s = 0;

		/* read prefix */
		node = node->next;
		if(node != NULL)
		{
			sp= node->value;
			if(sp.s== NULL || sp.len==0 || *sp.s=='.')
				sp.s = NULL;
			else {
				while(sp.s!=NULL && i!=sp.len)
				{
					if(strpos(pdt_char_list.s,sp.s[i]) < 0)
					{
						LOG(L_ERR, "PDT:pdt_mi_list: bad prefix [%.*s]\n",
							sp.len, sp.s);
						return init_mi_tree( 400, "bad prefix", 10);
					}
					i++;
				}
			}

			/* read domain */
			node= node->next;
			if(node != NULL)
			{
				sd= node->value;
				if(sd.s== NULL || sd.len==0 || *sd.s=='.')
					sd.s = NULL;
			}
		}
	}

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN );
	if(rpl_tree == NULL)
		return 0;
	rpl = &rpl_tree->node;

	lock_get(&(*_dhash)->hl_lock);
	h = (*_dhash)->hash;

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
						||(sp.s==NULL && (sd.s!=NULL && it->domain.len>=sd.len
							&& strncasecmp(it->domain.s, sd.s, sd.len)==0)) 
						|| (sd.s==NULL && (sp.s!=NULL && it->prefix.len>=sp.len
							&& strncmp(it->prefix.s, sp.s, sp.len)==0))
						|| ((sp.s!=NULL && it->prefix.len>=sp.len &&
							strncmp(it->prefix.s, sp.s, sp.len)==0)
						&& (sd.s!=NULL && it->domain.len>=sd.len &&
							strncasecmp(it->domain.s, sd.s, sd.len)==0)))
					{
						node = add_mi_node_child(rpl, 0 ,"PDT", 3, 0, 0);
						if(node == NULL)
							goto error;

						attr = add_mi_attr(node, MI_DUP_VALUE, "SDOMAIN", 7,
							h->sdomain.s, h->sdomain.len);
						if(attr == NULL)
							goto error;
						attr = add_mi_attr(node, MI_DUP_VALUE, "PREFIX", 6,
							it->prefix.s, it->prefix.len);
						if(attr == NULL)
							goto error;
						
						attr = add_mi_attr(node, MI_DUP_VALUE,"DOMAIN", 6,
							it->domain.s, it->domain.len);
						if(attr == NULL)
							goto error;

					}
					it = it->n;
				}
			}
		}
		h = h->next;
	}

	lock_release(&(*_dhash)->hl_lock);
	
	return rpl_tree;

error:
	lock_release(&(*_dhash)->hl_lock);
	free_mi_tree(rpl_tree);
	return 0;
}


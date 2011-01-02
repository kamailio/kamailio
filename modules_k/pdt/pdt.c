/**
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

#include "../../lib/srdb1/db_op.h"
#include "../../lib/kmi/mi.h"
#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../parser/parse_uri.h"
#include "../../timer.h"
#include "../../ut.h"
#include "../../locking.h"
#include "../../action.h"
#include "../../mod_fix.h"
#include "../../parser/parse_from.h"

#include "pdtree.h"

MODULE_VERSION


#define NR_KEYS			3

int pdt_fetch_rows = 1000;

/** structures containing prefix-domain pairs */
pdt_tree_t **_ptree = NULL; 

/** database connection */
static db1_con_t *db_con = NULL;
static db_func_t pdt_dbf;


/** parameters */
static str db_url = str_init(DEFAULT_DB_URL);
static str db_table = str_init("pdt");
static str sdomain_column = str_init("sdomain");
static str prefix_column  = str_init("prefix");
static str domain_column  = str_init("domain");
static int pdt_check_domain  = 1;

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
static int  child_init(int rank);
static int  mi_child_init(void);
static int prefix2domain(struct sip_msg*, int mode, int sd_en);

static struct mi_root* pdt_mi_reload(struct mi_root*, void* param);
static struct mi_root* pdt_mi_add(struct mi_root*, void* param);
static struct mi_root* pdt_mi_delete(struct mi_root*, void* param);
static struct mi_root* pdt_mi_list(struct mi_root*, void* param);

static int update_new_uri(struct sip_msg *msg, int plen, str *d, int mode);
static int pdt_load_db();

static cmd_export_t cmds[]={
	{"prefix2domain", (cmd_function)w_prefix2domain,   0, 0,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"prefix2domain", (cmd_function)w_prefix2domain_1, 1, fixup_igp_null,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"prefix2domain", (cmd_function)w_prefix2domain_2, 2, fixup_igp_igp,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"db_url",         STR_PARAM, &db_url.s},
	{"db_table",       STR_PARAM, &db_table.s},
	{"sdomain_column", STR_PARAM, &sdomain_column.s},
	{"prefix_column",  STR_PARAM, &prefix_column.s},
	{"domain_column",  STR_PARAM, &domain_column.s},
	{"prefix",         STR_PARAM, &prefix.s},
	{"char_list",      STR_PARAM, &pdt_char_list.s},
	{"fetch_rows",     INT_PARAM, &pdt_fetch_rows},
	{"check_domain",   INT_PARAM, &pdt_check_domain},
	{0, 0, 0}
};

static mi_export_t mi_cmds[] = {
	{ "pdt_add",     pdt_mi_add,     0,  0,  mi_child_init },
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
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	db_url.len = strlen(db_url.s);
	db_table.len = strlen(db_table.s);
	sdomain_column.len = strlen(sdomain_column.s);
	prefix_column.len = strlen(prefix_column.s);
	domain_column.len = strlen(domain_column.s);
	prefix.len = strlen(prefix.s);

	if(pdt_fetch_rows<=0)
		pdt_fetch_rows = 1000;

	pdt_char_list.len = strlen(pdt_char_list.s);
	if(pdt_char_list.len<=0)
	{
		LM_ERR("invalid pdt char list\n");
		return -1;
	}
	LM_INFO("pdt_char_list=%s \n",pdt_char_list.s);

	/* binding to mysql module */
	if(db_bind_mod(&db_url, &pdt_dbf))
	{
		LM_ERR("database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(pdt_dbf, DB_CAP_ALL))
	{
		LM_ERR("database module does not "
		    "implement all functions needed by the module\n");
		return -1;
	}

	/* open a connection with the database */
	db_con = pdt_dbf.init(&db_url);
	if(db_con==NULL)
	{
		LM_ERR("failed to connect to the database\n");        
		return -1;
	}
	
	if (pdt_dbf.use_table(db_con, &db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		goto error1;
	}
	LM_DBG("database connection opened successfully\n");
	
	if ( (pdt_lock=lock_alloc())==0) {
		LM_CRIT("failed to alloc lock\n");
		goto error1;
	}
	if (lock_init(pdt_lock)==0 ) {
		LM_CRIT("failed to init lock\n");
		goto error1;
	}
	
	/* tree pointer in shm */
	_ptree = (pdt_tree_t**)shm_malloc( sizeof(pdt_tree_t*) );
	if (_ptree==0) {
		LM_ERR("out of shm mem for pdtree\n");
		goto error1;
	}
	*_ptree=0;

	/* loading all information from database */
	if(pdt_load_db()!=0)
	{
		LM_ERR("cannot load info from database\n");	
		goto error1;
	}
		
	pdt_dbf.close(db_con);
	db_con = 0;

#if 0
	pdt_print_tree(*_ptree);
#endif

	/* success code */
	return 0;

error1:
	if (pdt_lock)
	{
		lock_destroy( pdt_lock );
		lock_dealloc( pdt_lock );
		pdt_lock = 0;
	}
	if(_ptree!=0)
	{
		shm_free(_ptree);
		_ptree = 0;
	}

	if(db_con!=NULL)
	{
		pdt_dbf.close(db_con);
		db_con = 0;
	}
	return -1;
}


/**
 * mi and worker process init
 */
static int mi_child_init(void)
{
	/* check if db handler is initialized */
	if(db_con!=NULL)
		return 0;

	db_con = pdt_dbf.init(&db_url);
	if(db_con==NULL)
	{
		LM_ERR("failed to connect to database\n");
		return -1;
	}

	if (pdt_dbf.use_table(db_con, &db_table) < 0)
	{
		LM_ERR("use_table failed\n");
		return -1;
	}
	return 0;
}


/* each child get a new connection to the database */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if ( mi_child_init()!=0 )
		return -1;

	LM_DBG("#%d: database connection opened successfully\n", rank);

	return 0;
}


static void mod_destroy(void)
{
	LM_DBG("cleaning up\n");
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
	int m;

	if(fixup_get_ivalue(msg, (gparam_p)mode, &m)!=0)
	{
		LM_ERR("no mode value\n");
		return -1;
	}

	if(m!=1 && m!=2)
		m = 0;

	return prefix2domain(msg, m, 0);
}

static int w_prefix2domain_2(struct sip_msg* msg, char* mode, char* sdm)
{
	int m, s;

	if(fixup_get_ivalue(msg, (gparam_p)mode, &m)!=0)
	{
		LM_ERR("no mode value\n");
		return -1;
	}

	if(m!=1 && m!=2)
		m = 0;

	if(fixup_get_ivalue(msg, (gparam_p)sdm, &s)!=0)
	{
		LM_ERR("no multi-domain mode value\n");
		return -1;
	}

	if(s!=1 && s!=2)
		s = 0;

	return prefix2domain(msg, m, s);
}

/* change the r-uri if it is a PSTN format */
static int prefix2domain(struct sip_msg* msg, int mode, int sd_en)
{
	str *d, p, all={"*",1};
	int plen;
	struct sip_uri uri;
	
	if(msg==NULL)
	{
		LM_ERR("received null msg\n");
		return -1;
	}
	
	/* parse the uri, if not yet */
	if(msg->parsed_uri_ok==0)
		if(parse_sip_msg_uri(msg)<0)
		{
			LM_ERR("failed to parse the R-URI\n");
			return -1;
		}

    /* if the user part begin with the prefix for PSTN users, extract the code*/
	if (msg->parsed_uri.user.len<=0)
	{
		LM_DBG("user part of the message is empty\n");
		return -1;
	}   
    
	if(prefix.len>0)
	{
		if (msg->parsed_uri.user.len<=prefix.len)
		{
			LM_DBG("user part is less than prefix\n");
			return -1;
		}   
		if(strncasecmp(prefix.s, msg->parsed_uri.user.s, prefix.len)!=0)
		{
			LM_DBG("PSTN prefix did not matched\n");
			return -1;
		}
	}   
	
	if(prefix.len>0 && prefix.len < msg->parsed_uri.user.len
			&& strncasecmp(prefix.s, msg->parsed_uri.user.s, prefix.len)!=0)
	{
		LM_DBG("PSTN prefix did not matched\n");
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
			LM_ERR("cannot parse FROM header\n");
			goto error;
		}	
		
		memset(&uri, 0, sizeof(struct sip_uri));
		if (parse_uri(get_from(msg)->uri.s, get_from(msg)->uri.len , &uri)<0)
		{
			LM_ERR("failed to parse From uri\n");
			goto error;
		}
	
		/* find the domain that corresponds to this prefix */
		plen = 0;
		if((d=pdt_get_domain(*_ptree, &uri.host, &p, &plen))==NULL)
		{
			plen = 0;
			if((d=pdt_get_domain(*_ptree, &all, &p, &plen))==NULL)
			{
				LM_INFO("no prefix found in [%.*s]\n", p.len, p.s);
				goto error;
			}
		}
	} else if(sd_en==1) {	
		/* take the domain from  FROM uri as sdomain */
		if(parse_from_header(msg)<0 ||  msg->from == NULL
				|| get_from(msg)==NULL)
		{
			LM_ERR("ERROR cannot parse FROM header\n");
			goto error;
		}	
		
		memset(&uri, 0, sizeof(struct sip_uri));
		if (parse_uri(get_from(msg)->uri.s, get_from(msg)->uri.len , &uri)<0)
		{
			LM_ERR("failed to parse From uri\n");
			goto error;
		}
	
		/* find the domain that corresponds to this prefix */
		plen = 0;
		if((d=pdt_get_domain(*_ptree, &uri.host, &p, &plen))==NULL)
		{
			LM_INFO("no prefix found in [%.*s]\n", p.len, p.s);
			goto error;
		}
	} else {
		/* find the domain that corresponds to this prefix */
		plen = 0;
		if((d=pdt_get_domain(*_ptree, &all, &p, &plen))==NULL)
		{
			LM_INFO("no prefix found in [%.*s]\n", p.len, p.s);
			goto error;
		}
	}
	
	/* update the new uri */
	if(update_new_uri(msg, plen, d, mode)<0)
	{
		LM_ERR("new_uri cannot be updated\n");
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
static int update_new_uri(struct sip_msg *msg, int plen, str *d, int mode)
{
	struct action act;
	struct run_act_ctx ra_ctx;
	if(msg==NULL || d==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	
	if(mode==0 || (mode==1 && prefix.len>0))
	{
		memset(&act, '\0', sizeof(act));
		act.type = STRIP_T;
		act.val[0].type = NUMBER_ST;
		if(mode==0)
			act.val[0].u.number = plen + prefix.len;
		else
			act.val[0].u.number = prefix.len;

		init_run_actions_ctx(&ra_ctx);
		if (do_action(&ra_ctx, &act, msg) < 0)
		{
			LM_ERR("failed to remove prefix\n");
			return -1;
		}
	}
	
	memset(&act, '\0', sizeof(act));
	act.type = SET_HOSTPORT_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = d->s;
	init_run_actions_ctx(&ra_ctx);
	if (do_action(&ra_ctx, &act, msg) < 0)
	{
		LM_ERR("failed to change domain\n");
		return -1;
	}

	LM_DBG("len=%d uri=%.*s\n", msg->new_uri.len, 
			msg->new_uri.len, msg->new_uri.s);
	
	return 0;
}

static int pdt_load_db(void)
{
	db_key_t db_cols[3] = {&sdomain_column, &prefix_column, &domain_column};
	str p, d, sdomain;
	db1_res_t* db_res = NULL;
	int i, ret;
	pdt_tree_t *_ptree_new = NULL; 
	pdt_tree_t *old_tree = NULL; 

	if(db_con==NULL)
	{
		LM_ERR("no db connection\n");
		return -1;
	}
		
	if (pdt_dbf.use_table(db_con, &db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	if (DB_CAPABILITY(pdt_dbf, DB_CAP_FETCH)) {
		if(pdt_dbf.query(db_con,0,0,0,db_cols,0,3,&sdomain_column,0) < 0)
		{
			LM_ERR("Error while querying db\n");
			return -1;
		}
		if(pdt_dbf.fetch_result(db_con, &db_res, pdt_fetch_rows)<0)
		{
			LM_ERR("Error while fetching result\n");
			if (db_res)
				pdt_dbf.free_result(db_con, db_res);
			goto error;
		} else {
			if(RES_ROW_N(db_res)==0)
			{
				return 0;
			}
		}
	} else {
		if((ret=pdt_dbf.query(db_con, NULL, NULL, NULL, db_cols,
				0, 3, &sdomain_column, &db_res))!=0
			|| RES_ROW_N(db_res)<=0 )
		{
			pdt_dbf.free_result(db_con, db_res);
			if( ret==0)
			{
				return 0;
			} else {
				goto error;
			}
		}
	}

	do {
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
				LM_ERR("Error - bad values in db\n");
				continue;
			}
		
			if(pdt_check_domain!=0 && _ptree_new!=NULL
					&& pdt_check_pd(_ptree_new, &sdomain, &p, &d)==1)
			{
				LM_ERR("sdomain [%.*s]: prefix [%.*s] or domain <%.*s> "
					"duplicated\n", sdomain.len, sdomain.s, p.len, p.s,
					d.len, d.s);
				continue;
			}

			if(pdt_add_to_tree(&_ptree_new, &sdomain, &p, &d)<0)
			{
				LM_ERR("Error adding info to tree\n");
				goto error;
			}
	 	}
		if (DB_CAPABILITY(pdt_dbf, DB_CAP_FETCH)) {
			if(pdt_dbf.fetch_result(db_con, &db_res, pdt_fetch_rows)<0) {
				LM_ERR("Error while fetching!\n");
				if (db_res)
					pdt_dbf.free_result(db_con, db_res);
				goto error;
			}
		} else {
			break;
		}
	}  while(RES_ROW_N(db_res)>0);
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

	pdt_reload_flag = 0;

	/* free old data */
	if (old_tree!=NULL)
		pdt_free_tree(old_tree);

	return 0;

error:
	pdt_dbf.free_result(db_con, db_res);
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
		LM_ERR("cannot re-load info from database\n");	
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
	db_key_t db_keys[NR_KEYS] = {&sdomain_column, &prefix_column, &domain_column};
	db_val_t db_vals[NR_KEYS];
	db_op_t  db_ops[NR_KEYS] = {OP_EQ, OP_EQ};
	int i= 0;
	str sd, sp, sdomain;
	struct mi_node* node= NULL;

	if(_ptree==NULL)
	{
		LM_ERR("strange situation\n");
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
		LM_ERR("could not read prefix\n");
		return init_mi_tree( 404, "prefix not found", 16);
	}

	if(*sp.s=='.')
		 return init_mi_tree(400, "empty param", 11);

	while(i< sp.len)
	{
		if(strpos(pdt_char_list.s,sp.s[i]) < 0) 
			return init_mi_tree(400, "bad prefix", 10);
		i++;
	}

	/* read domain */
	node= node->next;
	if(node == NULL || node->next!=NULL)
		goto error1;

	sd= node->value;
	if(sd.s== NULL || sd.len==0)
	{
		LM_ERR("could not read domain\n");
		return init_mi_tree( 400, "domain not found", 16);
	}

	if(*sd.s=='.')
		 return init_mi_tree(400, "empty param", 11);

	
	if(pdt_check_domain!=0 && *_ptree!=NULL
			&& pdt_check_pd(*_ptree, &sdomain, &sp, &sd)==1)
	{
		LM_ERR("(sdomain,prefix,domain) exists\n");
		return init_mi_tree(400,
				"(sdomain,prefix,domain) exists already", 38);
	}
	db_vals[0].type = DB1_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = sdomain.s;
	db_vals[0].val.str_val.len = sdomain.len;

	db_vals[1].type = DB1_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = sp.s;
	db_vals[1].val.str_val.len = sp.len;

	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;
	db_vals[2].val.str_val.s = sd.s;
	db_vals[2].val.str_val.len = sd.len;
	
	/* insert a new domain into database */
	if(pdt_dbf.insert(db_con, db_keys, db_vals, NR_KEYS)<0)
	{
		LM_ERR("failed to store new prefix/domain\n");
		return init_mi_tree( 500,"Cannot store prefix/domain", 26);
	}

	/* re-loading all information from database */
	if(pdt_load_db()!=0)
	{
		LM_ERR("cannot re-load info from database\n");	
		goto error;
	}
	
	LM_DBG("new prefix added %.*s-%.*s => %.*s\n",
			sdomain.len, sdomain.s, sp.len, sp.s, sd.len, sd.s);
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);

	
error:
	if(pdt_dbf.delete(db_con, db_keys, db_ops, db_vals, NR_KEYS)<0)
		LM_ERR("database/cache are inconsistent\n");
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
	db_key_t db_keys[2] = {&sdomain_column, &domain_column};
	db_val_t db_vals[2];
	db_op_t  db_ops[2] = {OP_EQ, OP_EQ};

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
		LM_ERR("could not read domain\n");
		return init_mi_tree(404, "domain not found", 16);
	}

	if(*sd.s=='.')
		 return init_mi_tree( 400, "empty param", 11);


	db_vals[0].type = DB1_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val.s = sdomain.s;
	db_vals[0].val.str_val.len = sdomain.len;
	
	db_vals[1].type = DB1_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = sd.s;
	db_vals[1].val.str_val.len = sd.len;

	if(pdt_dbf.delete(db_con, db_keys, db_ops, db_vals, 2)<0)
	{
		LM_ERR("database/cache are inconsistent\n");
		return init_mi_tree( 500, "database/cache are inconsistent", 31 );
	} 
	/* re-loading all information from database */
	if(pdt_load_db()!=0)
	{
		LM_ERR("cannot re-load info from database\n");	
		return init_mi_tree( 500, "cannot reload", 13 );
	}

	LM_DBG("prefix for sdomain [%.*s] domain [%.*s] "
			"removed\n", sdomain.len, sdomain.s, sd.len, sd.s);
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
error:
	return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
}


int pdt_print_mi_node(pdt_node_t *pt, struct mi_node* rpl, char *code,
		int len, str *sdomain, str *sd, str *sp)
{
	int i;
	struct mi_node* node = NULL;
	struct mi_attr* attr= NULL;

	if(pt==NULL || len>=PDT_MAX_DEPTH)
		return 0;
	
	for(i=0; i<PDT_NODE_SIZE; i++)
	{
		code[len]=pdt_char_list.s[i];
		if(pt[i].domain.s!=NULL)
		{
			if((sp->s==NULL && sd->s==NULL)
				|| (sp->s==NULL && (sd->s!=NULL && pt[i].domain.len==sd->len
						&& strncasecmp(pt[i].domain.s, sd->s, sd->len)==0)) 
				|| (sd->s==NULL && (len+1>=sp->len
						&& strncmp(code, sp->s, sp->len)==0))
				|| ((sp->s!=NULL && len+1>=sp->len
						&& strncmp(code, sp->s, sp->len)==0)
						&& (sd->s!=NULL && pt[i].domain.len>=sd->len
						&& strncasecmp(pt[i].domain.s, sd->s, sd->len)==0)))
			{
				node = add_mi_node_child(rpl, 0, "PDT", 3, 0, 0);
				if(node == NULL)
					goto error;

				attr = add_mi_attr(node, MI_DUP_VALUE, "SDOMAIN", 7,
						sdomain->s, sdomain->len);
				if(attr == NULL)
					goto error;
				attr = add_mi_attr(node, MI_DUP_VALUE, "PREFIX", 6,
							code, len+1);
				if(attr == NULL)
					goto error;
						
				attr = add_mi_attr(node, MI_DUP_VALUE,"DOMAIN", 6,
							pt[i].domain.s, pt[i].domain.len);
				if(attr == NULL)
					goto error;
			}
		}
		if(pdt_print_mi_node(pt[i].child, rpl, code, len+1, sdomain, sd, sp)<0)
			goto error;
	}
	return 0;
error:
	return -1;
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
	pdt_tree_t *pt;
	struct mi_node* node = NULL;
	unsigned int i= 0;
	struct mi_root* rpl_tree = NULL;
	struct mi_node* rpl = NULL;
	static char code_buf[PDT_MAX_DEPTH+1];
	int len;

	if(_ptree==NULL)
	{
		LM_ERR("empty domain list\n");
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
						LM_ERR("bad prefix [%.*s]\n", sp.len, sp.s);
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

	rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if(rpl_tree == NULL)
		return 0;
	rpl = &rpl_tree->node;

	if(*_ptree==0)
		return rpl_tree;

	pt = *_ptree;
	
	while(pt!=NULL)
	{
		if(sdomain.s==NULL || 
			(sdomain.s!=NULL && pt->sdomain.len>=sdomain.len && 
			 strncmp(pt->sdomain.s, sdomain.s, sdomain.len)==0))
		{
			len = 0;
			if(pdt_print_mi_node(pt->head, rpl, code_buf, len, &pt->sdomain,
						&sd, &sp)<0)
				goto error;
		}
		pt = pt->next;
	}
	
	return rpl_tree;

error:
	free_mi_tree(rpl_tree);
	return 0;
}


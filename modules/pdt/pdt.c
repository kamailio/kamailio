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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
 * Ramona Modroiu
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../lib/srdb1/db_op.h"
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
#include "../../rpc.h"
#include "../../rpc_lookup.h"

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

/** translation prefix */
str pdt_prefix = {"", 0};
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
static int  pd_translate(sip_msg_t *msg, str *sdomain, int rmode, int fmode);

static int w_pd_translate(struct sip_msg* msg, char* str1, char* str2);
static int fixup_translate(void** param, int param_no);

static int update_new_uri(struct sip_msg *msg, int plen, str *d, int mode);
static int pdt_init_rpc(void);

static cmd_export_t cmds[]={
	{"prefix2domain", (cmd_function)w_prefix2domain,   0, 0,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"prefix2domain", (cmd_function)w_prefix2domain_1, 1, fixup_igp_null,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"prefix2domain", (cmd_function)w_prefix2domain_2, 2, fixup_igp_igp,
		0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"pd_translate", (cmd_function)w_pd_translate,     2, fixup_translate,
		0, REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"db_url",         PARAM_STR, &db_url},
	{"db_table",       PARAM_STR, &db_table},
	{"sdomain_column", PARAM_STR, &sdomain_column},
	{"prefix_column",  PARAM_STR, &prefix_column},
	{"domain_column",  PARAM_STR, &domain_column},
	{"prefix",         PARAM_STR, &pdt_prefix},
	{"char_list",      PARAM_STR, &pdt_char_list},
	{"fetch_rows",     INT_PARAM, &pdt_fetch_rows},
	{"check_domain",   INT_PARAM, &pdt_check_domain},
	{0, 0, 0}
};


struct module_exports exports = {
	"pdt",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
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

#ifndef PDT_NO_MI
	if(pdt_init_mi(exports.name)<0)
	{
		LM_ERR("cannot register MI commands\n");
		return -1;
	}
#endif

	if(pdt_init_rpc()<0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

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

/* each child get a new connection to the database */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if(pdt_init_db()<0)
	{
		LM_ERR("cannot initialize database connection\n");
		return -1;
	}
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
	str sdall={"*",1};
	return pd_translate(msg, &sdall, 0, 0);
}

static int w_prefix2domain_1(struct sip_msg* msg, char* mode, char* str2)
{
	str sdall={"*",1};
	int md;

	if(fixup_get_ivalue(msg, (gparam_p)mode, &md)!=0)
	{
		LM_ERR("no mode value\n");
		return -1;
	}

	if(md!=1 && md!=2)
		md = 0;

	return pd_translate(msg, &sdall, md, 0);
}

static int w_prefix2domain_2(struct sip_msg* msg, char* mode, char* sdm)
{
	int m, s, f;
	str sdomain={"*",1};
	sip_uri_t *furi;

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

	f = 0;
	if(s==1 || s==2)
	{
		/* take the domain from  FROM uri as sdomain */
		if((furi = parse_from_uri(msg))==NULL)
		{
			LM_ERR("cannot parse FROM header URI\n");
			return -1;
		}
		sdomain = furi->host;
		if(s==2)
			f = 1;
	}
	return pd_translate(msg, &sdomain, m, f);
}

/**
 * @brief change the r-uri domain based on source domain and prefix
 *
 * @param msg the sip message structure
 * @param sdomain the source domain
 * @param rmode the r-uri rewrite mode
 * @param fmode the source domain fallback mode
 * @return 1 if translation is done; -1 otherwise
 */
static int pd_translate(sip_msg_t *msg, str *sdomain, int rmode, int fmode)
{
	str *d, p;
	str sdall={"*",1};
	int plen;
	
	if(msg==NULL)
	{
		LM_ERR("received null msg\n");
		return -1;
	}
	
	if(parse_sip_msg_uri(msg)<0)
	{
		LM_ERR("failed to parse the R-URI\n");
		return -1;
	}

    /* if the user part begin with the prefix, extract the code*/
	if (msg->parsed_uri.user.len<=0)
	{
		LM_DBG("user part of the message is empty\n");
		return -1;
	}   
    
	if(pdt_prefix.len>0)
	{
		if (msg->parsed_uri.user.len<=pdt_prefix.len)
		{
			LM_DBG("user part is less than prefix parameter\n");
			return -1;
		}   
		if(strncasecmp(pdt_prefix.s, msg->parsed_uri.user.s,
					pdt_prefix.len)!=0)
		{
			LM_DBG("prefix parameter did not matched\n");
			return -1;
		}
	}   
	
	p.s   = msg->parsed_uri.user.s + pdt_prefix.len;
	p.len = msg->parsed_uri.user.len - pdt_prefix.len;

again:
	lock_get( pdt_lock );
	if (pdt_reload_flag) {
		lock_release( pdt_lock );
		sleep_us(5);
		goto again;
	}
	pdt_tree_refcnt++;
	lock_release( pdt_lock );


	if((d=pdt_get_domain(*_ptree, sdomain, &p, &plen))==NULL)
	{
		plen = 0;
		if((fmode==0) || (d=pdt_get_domain(*_ptree, &sdall, &p, &plen))==NULL)
		{
			LM_INFO("no prefix PDT prefix matched [%.*s]\n", p.len, p.s);
			goto error;
		}
	}

	
	/* update the new uri */
	if(update_new_uri(msg, plen, d, rmode)<0)
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

/**
 *
 */
static int fixup_translate(void** param, int param_no)
{
	if(param_no==1)
		return fixup_spve_null(param, 1);
	if(param_no==2)
		return fixup_igp_null(param, 1);
	return 0;
}

/**
 *
 */
static int w_pd_translate(sip_msg_t* msg, char* sdomain, char* mode)
{
	int md;
	str sd;

	if(fixup_get_svalue(msg, (gparam_p)sdomain, &sd)!=0)
	{
		LM_ERR("no source domain value\n");
		return -1;
	}


	if(fixup_get_ivalue(msg, (gparam_p)mode, &md)!=0)
	{
		LM_ERR("no multi-domain mode value\n");
		return -1;
	}

	if(md!=1 && md!=2)
		md = 0;

	return pd_translate(msg, &sd, md, 0);
}

/**
 * change the uri according to update mode
 */
static int update_new_uri(struct sip_msg *msg, int plen, str *d, int mode)
{
	struct action act;
	struct run_act_ctx ra_ctx;
	if(msg==NULL || d==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	
	if(mode==0 || (mode==1 && pdt_prefix.len>0))
	{
		memset(&act, '\0', sizeof(act));
		act.type = STRIP_T;
		act.val[0].type = NUMBER_ST;
		if(mode==0)
			act.val[0].u.number = plen + pdt_prefix.len;
		else
			act.val[0].u.number = pdt_prefix.len;

		init_run_actions_ctx(&ra_ctx);
		if (do_action(&ra_ctx, &act, msg) < 0)
		{
			LM_ERR("failed to remove prefix parameter\n");
			return -1;
		}
	}
	
	memset(&act, '\0', sizeof(act));
	act.type = SET_HOSTALL_T;
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

int pdt_init_db(void)
{
	/* db handler initialization */
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

int pdt_load_db(void)
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


/* return the pointer to char list */
str* pdt_get_char_list(void)
{
	return &pdt_char_list;
}

/* return head of pdt trees */
pdt_tree_t **pdt_get_ptree(void)
{
	return _ptree;
}


/*** RPC commands implementation ***/

static const char* pdt_rpc_reload_doc[2] = {
	"Reload PDT database records",
	0
};


/*
 * RPC command to reload pdt db records
 */
static void pdt_rpc_reload(rpc_t* rpc, void* ctx)
{
	if(pdt_load_db()<0) {
		LM_ERR("cannot re-load pdt records from database\n");	
		rpc->fault(ctx, 500, "Reload Failed");
		return;
	}
	return;
}


static const char* pdt_rpc_list_doc[2] = {
	"List PDT memory records",
	0
};


int pdt_rpc_print_node(rpc_t* rpc, void* ctx, void *ih, pdt_node_t *pt, char *code,
		int len, str *sdomain, str *tdomain, str *tprefix)
{
	int i;
	str *cl;
	str prefix;
	void* vh;

	if(pt==NULL || len>=PDT_MAX_DEPTH)
		return 0;
	
	cl = pdt_get_char_list();

	for(i=0; i<cl->len; i++)
	{
		code[len]=cl->s[i];
		if(pt[i].domain.s!=NULL)
		{
			if((tprefix->s==NULL && tdomain->s==NULL)
				|| (tprefix->s==NULL && (tdomain->s!=NULL && pt[i].domain.len==tdomain->len
						&& strncasecmp(pt[i].domain.s, tdomain->s, tdomain->len)==0))
				|| (tdomain->s==NULL && (len+1>=tprefix->len
						&& strncmp(code, tprefix->s, tprefix->len)==0))
				|| ((tprefix->s!=NULL && len+1>=tprefix->len
						&& strncmp(code, tprefix->s, tprefix->len)==0)
						&& (tdomain->s!=NULL && pt[i].domain.len>=tdomain->len
						&& strncasecmp(pt[i].domain.s, tdomain->s, tdomain->len)==0)))
			{
				if(rpc->struct_add(ih, "{",
						"ENTRY", &vh)<0)
				{
					LM_ERR("Internal error creating entry\n");
					return -1;
				}
				prefix.s = code;
				prefix.len = len + 1;
				if(rpc->struct_add(vh, "SS",
						"DOMAIN", &pt[i].domain,
						"PREFIX", &prefix)<0)
				{
					LM_ERR("Internal error filling entry struct\n");
					return -1;
				}
			}
		}
		if(pdt_rpc_print_node(rpc, ctx, ih, pt[i].child, code, len+1, sdomain,
					tdomain, tprefix)<0)
			goto error;
	}
	return 0;
error:
	return -1;
}

/*
 * RPC command to list pdt memory records
 */
/**
 * "pdt.list" parameters:
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
static void pdt_rpc_list(rpc_t* rpc, void* ctx)
{
	str sdomain = {0};
	str tprefix = {0};
	str tdomain = {0};
	pdt_tree_t *pt;
	unsigned int i;
	static char code_buf[PDT_MAX_DEPTH+1];
	int len;
	str *cl;
	pdt_tree_t **ptree;
	void* th;
	void* ih;

	ptree = pdt_get_ptree();

	if(ptree==NULL || *ptree==NULL)
	{
		LM_ERR("empty domain list\n");
		rpc->fault(ctx, 404, "No records");
		return;
	}

	len = rpc->scan(ctx, "*S.SS", &sdomain, &tprefix, &tdomain);
	if(len<0)
	{
		rpc->fault(ctx, 500, "Error Reading Parameters");
		return;
	}
	if(len<1 || sdomain.len==0 || (sdomain.len==1 && sdomain.s[0]=='.')) {
		sdomain.len = 0;
		sdomain.s = 0;
	}
	cl = pdt_get_char_list();
	if(len<2 || tprefix.len==0 || (tprefix.len==1 && tprefix.s[0]=='.')) {
		tprefix.len = 0;
		tprefix.s = 0;
	} else if(tprefix.len>0) {
		/* validate prefix */
		i = 0;
		while(tprefix.s!=NULL && i!=tprefix.len)
		{
			if(strpos(cl->s, tprefix.s[i]) < 0)
			{
				LM_ERR("bad prefix [%.*s]\n", tprefix.len, tprefix.s);
				rpc->fault(ctx, 400, "Bad Prefix");
				return;
			}
			i++;
		}
	}
	if(len<3 || tdomain.len==0 || (tdomain.len==1 && tdomain.s[0]=='.')) {
		tdomain.len = 0;
		tdomain.s = 0;
	}

	pt = *ptree;
	
	if (rpc->add(ctx, "{", &th) < 0)
	{
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	while(pt!=NULL)
	{
		LM_ERR("---- 1 (%d [%.*s])\n", sdomain.len, sdomain.len, sdomain.s);
		if(sdomain.s==NULL || 
			(sdomain.s!=NULL && pt->sdomain.len>=sdomain.len && 
			 strncmp(pt->sdomain.s, sdomain.s, sdomain.len)==0))
		{
		LM_ERR("---- 2\n");
			len = 0;
			if(rpc->struct_add(th, "S{",
					"SDOMAIN", &pt->sdomain,
					"RECORDS",  &ih)<0)
			{
				rpc->fault(ctx, 500, "Internal error creating sdomain structure");
				return;
			}
			if(pdt_rpc_print_node(rpc, ctx, ih, pt->head, code_buf, len, &pt->sdomain,
						&tdomain, &tprefix)<0)
				goto error;
		}
		pt = pt->next;
	}
	return;
error:
	rpc->fault(ctx, 500, "Internal error printing records");
	return;
}


rpc_export_t pdt_rpc_cmds[] = {
	{"pdt.reload", pdt_rpc_reload,
		pdt_rpc_reload_doc, 0},
	{"pdt.list", pdt_rpc_list,
		pdt_rpc_list_doc, 0},
	{0, 0, 0, 0}
};


/**
 * register RPC commands
 */
static int pdt_init_rpc(void)
{
	if (rpc_register_array(pdt_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

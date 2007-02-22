/**
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
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
#include "../../rpc.h"
#include "../../action.h"

#include "domains.h"
#include "pdtree.h"

MODULE_VERSION


#define NR_KEYS			2

int hs_two_pow = 2;

/** structure containing prefix-domain pairs */
pdt_hash_t *_dhash = NULL;
pdt_tree_t *_ptree = NULL;

time_t last_sync;

/** database connection */
static db_con_t *db_con = NULL;
static db_func_t pdt_dbf;


/** parameters */
static char *db_url = DEFAULT_DB_URL;
char *db_table = "pdt";
char *prefix_column = "prefix";
char *domain_column = "domain";

/** pstn prefix */
str prefix = STR_STATIC_INIT("");
int sync_time = 600;
int clean_time = 900;

static int w_prefix2domain(struct sip_msg* msg, char* str1, char* str2);
static int w_prefix2domain_1(struct sip_msg* msg, char* mode, char* str2);
static int mod_init(void);
static void mod_destroy(void);
static int  child_init(int r);

static int prefix2domain(struct sip_msg*, int mode);

int update_new_uri(struct sip_msg *msg, int plen, str *d, int mode);
int pdt_load_db();
int pdt_sync_cache();
void pdt_clean_cache(unsigned int ticks, void *param);

static rpc_export_t pdt_rpc[];

static cmd_export_t cmds[]={
	{"prefix2domain", w_prefix2domain,   0, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"prefix2domain", w_prefix2domain_1, 1, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"db_url",        PARAM_STRING, &db_url},
	{"db_table",      PARAM_STRING, &db_table},
	{"prefix_column", PARAM_STRING, &prefix_column},
	{"domain_column", PARAM_STRING, &domain_column},
	{"prefix",        PARAM_STR,    &prefix},
	{"hsize_2pow",    PARAM_INT,    &hs_two_pow},
	{"sync_time",     PARAM_INT,    &sync_time},
	{"clean_time",    PARAM_INT,    &clean_time},
	{0, 0, 0}
};

struct module_exports exports = {
	"pdt",
	cmds,
	pdt_rpc,         /* RPC methods */
	params,

	mod_init,		/* module initialization function */
	0,				/* response function */
	mod_destroy,	/* destroy function */
	0,				/* oncancel function */
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
	if( (_dhash = pdt_init_hash(hs_two_pow)) == NULL)
	{
		LOG(L_ERR, "PDT:mod_init: domain hash could not be allocated\n");
		goto error1;
	}

	if( (_ptree = pdt_init_tree()) == NULL)
	{
		LOG(L_ERR, "PDT:mod_init: prefix tree could not be allocated\n");
		goto error2;
	}

	/* loading all information from database */
	if(pdt_load_db()!=0)
	{
		LOG(L_ERR, "PDT:mod_init: cannot load info from database\n");
		goto error3;
	}

	pdt_dbf.close(db_con);
	db_con = 0;

	pdt_print_tree(_ptree);
	DBG("PDT:mod_init: -------------------\n");
	pdt_print_hash(_dhash);

	last_sync = time(NULL);

	register_timer(pdt_clean_cache, 0, clean_time);

	/* success code */
	return 0;

error3:
	if(_ptree!=NULL)
	{
		pdt_free_tree(_ptree);
		_ptree = 0;
	}
error2:
	if(_dhash!=NULL)
	{
		pdt_free_hash(_dhash);
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

		lock_get(&_dhash->diff_lock);
		_dhash->workers++;
		lock_release(&_dhash->diff_lock);
	} else {
		if(_ptree!=NULL)
		{
			pdt_free_tree(_ptree);
			_ptree = 0;
		}
	}

	if (r==PROC_MAIN || r==PROC_TCP_MAIN)
		return 0; /* do nothing for the main or tcp_main processes */

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
		pdt_free_hash(_dhash);
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
	str p;
	str *d;
	time_t crt_time;
	int plen;

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

	/* find the domain that corresponds to this prefix */
	plen = 0;
	if((d=pdt_get_domain(_ptree, &p, &plen))==NULL)
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

	memset(&act, 0, sizeof(act));
	if(mode==0 || (mode==1 && prefix.len>0))
	{
		act.type = STRIP_T;
		act.val[0].type = NUMBER_ST;
		if(mode==0)
			act.val[0].u.number = plen + prefix.len;
		else
			act.val[0].u.number = prefix.len;
		act.next = 0;

		if (do_action(&act, msg) < 0)
		{
			LOG(L_ERR, "PDT:update_new_uri:Error removing prefix\n");
			return -1;
		}
	}

	act.type = SET_HOSTPORT_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = d->s;
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
	db_key_t db_cols[] = {prefix_column, domain_column};
	str p, d;
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
				0, 2, prefix_column, &db_res)==0)
	{
		for(i=0; i<RES_ROW_N(db_res); i++)
		{
			/* check for NULL values ?!?! */
			p.s = (char*)(RES_ROWS(db_res)[i].values[0].val.string_val);
			p.len = strlen(p.s);

			d.s = (char*)(RES_ROWS(db_res)[i].values[1].val.string_val);
			d.len = strlen(d.s);

			if(p.s==NULL || d.s==NULL || p.len<=0 || d.len<=0)
			{
				LOG(L_ERR, "PDT:pdt_load_db: Error - bad values in db\n");
				goto error;
			}

			if(pdt_check_pd(_dhash, &p, &d)!=0)
			{
				LOG(L_ERR,
				"PDT:pdt_load_db: prefix [%.*s] or domain <%.*s> duplicated\n",
					p.len, p.s, d.len, d.s);
				goto error;;
			}

			if(pdt_add_to_tree(_ptree, &p, &d)!=0)
			{
				LOG(L_ERR, "PDT:pdt_load_db: Error adding info in tree\n");
				goto error;
			}

			if(pdt_add_to_hash(_dhash, &p, &d)!=0)
			{
				LOG(L_ERR, "PDT:pdt_load_db: Error adding info in hash\n");
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

	DBG("PDT:pdt_sync_cache: ...\n");

	if(_dhash==NULL || _ptree==NULL)
	{
		LOG(L_ERR, "PDT:pdt_sync_cache: strange situation\n");
		return -1;
	}

	lock_get(&_dhash->diff_lock);

	if(_ptree->idsync >= _dhash->max_id)
		goto done;

	ito = _dhash->diff;

	while(ito!=NULL && _ptree->idsync >= ito->id)
		ito = ito->n;

	while(ito!=NULL)
	{
		DBG("PDT:pdt_sync_cache: sync op[%d]=%d...\n",
				ito->id, ito->op);
		switch(ito->op)
		{
			case PDT_ADD:
				if(pdt_add_to_tree(_ptree, &ito->cell->prefix,
							&ito->cell->domain)!=0)
				{
					LOG(L_ERR, "PDT:pdt_sync_cache: Error to insert in tree\n");
					goto error;
				}
				break;
			case PDT_DELETE:
				if(pdt_remove_from_tree(_ptree, &ito->cell->prefix)!=0)
				{
					LOG(L_ERR,
						"PDT:pdt_sync_cache: Error to remove from tree\n");
					goto error;
				}
				break;
			default:
				LOG(L_ERR, "PDT:pdt_sync_cache: unknown operation\n");
		}
		_ptree->idsync = ito->id;
		ito->count++;
		ito = ito->n;
	}

done:
	lock_release(&_dhash->diff_lock);
	return 0;
error:
	lock_release(&_dhash->diff_lock);
	return -1;
}

void pdt_clean_cache(unsigned int ticks, void *param)
{
	pd_op_t *ito, *tmp;

	/* DBG("PDT:pdt_clean_cache: ...\n"); */

	if(_dhash==NULL)
	{
		LOG(L_ERR, "PDT:pdt_clean_cache: strange situation\n");
		return;
	}

	lock_get(&_dhash->diff_lock);

	ito = _dhash->diff;

	while(ito!=NULL)
	{
		if(ito->count >= _dhash->workers)
		{
			DBG("PDT:pdt_clean_cache: cleaning op[%d]=%d...\n",
					ito->id, ito->op);
			free_cell(ito->cell);
			if(ito->p!=NULL)
				(ito->p)->n = ito->n;
			else
				_dhash->diff = ito->n;
			if(ito->n!=NULL)
				(ito->n)->p = ito->p;
			tmp = ito;
			ito = ito->n;
			shm_free(tmp);
		} else
			ito = ito->n;
	}

	lock_release(&_dhash->diff_lock);
	return;
}



static const char* rpc_add_doc[2] = {
	"Add new prefix/domain translation rule.",
	0
};

static void rpc_add(rpc_t* rpc, void* c)
{
	db_key_t db_keys[NR_KEYS] = {prefix_column, domain_column};
	db_val_t db_vals[NR_KEYS];
	db_op_t  db_ops[NR_KEYS] = {OP_EQ, OP_EQ};

	pd_t* cell;
	pd_op_t *ito, *tmp;
	str sd, sp;
	char* t;

	if(_dhash==NULL) {
		LOG(L_ERR, "PDT:pdt_fifo_add: strange situation\n");
		rpc->fault(c, 500, "Server Error");
		return;
	}

	     /* Use 's' to make sure strings are zero terminated */
	if (rpc->scan(c, "ss", &sp.s, &sd.s) < 2) {
		rpc->fault(c, 400, "Invalid Parameter Value");
		return;
	}
	sp.len = strlen(sp.s);
	sd.len = strlen(sd.s);

	t = sp.s;
	while(t!=NULL && *t!='\0') {
		if(*t < '0' || *t > '9') {
			LOG(L_ERR, "PDT:pdt_fifo_add: bad prefix [%s]\n", sp.s);
			rpc->fault(c, 400, "Bad Prefix");
			return;
		}
		t++;
	}

	if(pdt_check_pd(_dhash, &sp, &sd)!=0) {
		LOG(L_ERR, "PDT:pdt_fifo_add: prefix or domain exists\n");
		rpc->fault(c, 400, "Prefix Or Domain Exists");
		return;
	}

	db_vals[0].type = DB_STR;
	db_vals[0].nul = 0;
	db_vals[0].val.str_val = sp;

	db_vals[1].type = DB_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val= sd;

	DBG("PDT:pdt_fifo_add: [%.*s] <%.*s>\n", sp.len, sp.s, sd.len, sd.s);

	     /* insert a new domain into database */
	if(pdt_dbf.insert(db_con, db_keys, db_vals, NR_KEYS)<0) {
		LOG(L_ERR, "PDT:pdt_fifo_add: error storing new prefix/domain\n");
		rpc->fault(c, 430, "Cannot Store Prefix/domain");
		return;
	}

	     /* insert the new domain into hashtables, too */
	cell = new_cell(&sp, &sd);
	if(cell==NULL) {
		LOG(L_ERR, "PDT:pdt_fifo_add: no more shm\n");
		rpc->fault(c, 431, "Out Of Shared Memory");
		goto error1;
	}
	tmp = new_pd_op(cell, 0, PDT_ADD);
	if(tmp==NULL) {
		LOG(L_ERR, "PDT:pdt_fifo_add: no more shm!\n");
		rpc->fault(c, 431, "Out Of Shared Memory");
		goto error2;
	}

	lock_get(&_dhash->diff_lock);

	if(pdt_add_to_hash(_dhash, &sp, &sd)!=0) {
		LOG(L_ERR, "PDT:pdt_fifo_add: could not add to cache\n");
		rpc->fault(c, 431, "Could Not Add To Cache");
		goto error3;
	}

	_dhash->max_id++;
	tmp->id = _dhash->max_id;
	if(_dhash->diff==NULL) {
		_dhash->diff = tmp;
		goto done;
	}
	ito = _dhash->diff;
	while(ito->n!=NULL)
		ito = ito->n;

	ito->n = tmp;
	tmp->p = ito;

 done:
	DBG("PDT:pdt_fifo_add: op[%d]=%d...\n", tmp->id, tmp->op);
	lock_release(&_dhash->diff_lock);
	return;

 error3:
	lock_release(&_dhash->diff_lock);
	free_pd_op(tmp);
 error2:
	free_cell(cell);
 error1:
	if(pdt_dbf.delete(db_con, db_keys, db_ops, db_vals, NR_KEYS)<0)
		LOG(L_ERR,"PDT:pdt_fifo_add: database/cache are inconsistent\n");
}



static const char* rpc_delete_doc[2] = {
	"Delete prefix/domain translation rule.",
	0
};

static void rpc_delete(rpc_t* rpc, void* c)
{
	str sd;
	unsigned int dhash;
	int hash_entry;
	pd_t *it;
	pd_op_t *ito, *tmp;
	db_key_t db_keys[1] = {domain_column};
	db_val_t db_vals[1];
	db_op_t  db_ops[1] = {OP_EQ};

	if(_dhash==NULL) {
		LOG(L_ERR, "PDT:pdt_fifo_delete: strange situation\n");
		rpc->fault(c, 500, "Server Error");
		return;
	}

	     /* Use s to make sure the string is zero terminated */
	if (rpc->scan(c, "s", &sd.s) < 1) {
		rpc->fault(c, 400, "Parameter Missing");
		return;
	}
	sd.len = strlen(sd.s);

	if(*sd.s=='\0') {
		LOG(L_INFO, "PDT:pdt_fifo_delete: empty domain\n");
		rpc->fault(c, 400, "Empty Parameter");
		return;
	}

	dhash = pdt_compute_hash(sd.s);
	hash_entry = get_hash_entry(dhash, _dhash->hash_size);

	lock_get(&_dhash->diff_lock);

	lock_get(&_dhash->dhash[hash_entry].lock);

	it = _dhash->dhash[hash_entry].e;
	while(it!=NULL && it->dhash<=dhash) {
		if(it->dhash==dhash && it->domain.len==sd.len
		   && strncasecmp(it->domain.s, sd.s, sd.len)==0)
			break;
		it = it->n;
	}

	if(it!=NULL) {
		if(it->p!=NULL)
			(it->p)->n = it->n;
		else
			_dhash->dhash[hash_entry].e = it->n;
		if(it->n)
			(it->n)->p = it->p;
	}
	lock_release(&_dhash->dhash[hash_entry].lock);

	if(it!=NULL) {
		tmp = new_pd_op(it, 0, PDT_DELETE);
		if(tmp==NULL) {
			LOG(L_ERR, "PDT:pdt_fifo_delete: no more shm!\n");
			rpc->fault(c, 431, "No Shared Memory Left");
			lock_release(&_dhash->diff_lock);
			return;
		}

		_dhash->max_id++;
		tmp->id = _dhash->max_id;
		if(_dhash->diff==NULL) {
			_dhash->diff = tmp;
			DBG("PDT:pdt_fifo_delete: op[%d]=%d...\n", tmp->id, tmp->op);
			goto done;
		}
		ito = _dhash->diff;
		while(ito->n!=NULL)
			ito = ito->n;

		ito->n = tmp;
		tmp->p = ito;
		DBG("PDT:pdt_fifo_delete: op[%d]=%d...\n", tmp->id, tmp->op);
		dhash = 1;
	} else {
		dhash = 0;
	}

 done:
	lock_release(&_dhash->diff_lock);
	if(dhash==0) {
		DBG("PDT:pdt_fifo_delete: prefix for domain [%s] not found\n", sd.s);
		rpc->fault(c, 404, "Domain Not Found");
	} else {
		db_vals[0].type = DB_STR;
		db_vals[0].nul = 0;
		db_vals[0].val.str_val.s = sd.s;
		db_vals[0].val.str_val.len = sd.len;
		if(pdt_dbf.delete(db_con, db_keys, db_ops, db_vals, 1)<0)
			{
				LOG(L_ERR,"PDT:pdt_fifo_delete: database/cache are inconsistent\n");
				rpc->fault(c, 502, "Database And Cache Are Inconsistent");
			}
	}
}



static const char* rpc_list_doc[2] = {
	"List existin prefix/domain translation rules",
	0
};

/**
 *      Fifo command example:
 *
 *      ---
 *       :pdt_list:[response_file]\n
 *       prefix\n
 *       domain\n
 *       \n
 *      --
 *
 *      - '.' (dot) means NULL value for [prefix] and [domain]
 *      - if both [prefix] and [domain] are NULL, all prefix-domain pairs are listed
 *      - the comparison operation is 'START WITH' -- if domain is 'a' then
 *        all domains starting with 'a' are listed
 */
static void rpc_list(rpc_t* rpc, void* c)
{
	str sd, sp;
	pd_t *it;
	int i;
	char* buf1, *buf2, *t;

	if(_dhash==NULL) {
		LOG(L_ERR, "PDT:pdt_fifo_list: strange situation\n");
		rpc->fault(c, 500, "Server Error");
		return;
	}

	if (rpc->scan(c, "ss", &sp.s, &sd.s) < 2) {
		rpc->fault(c, 400, "Invalid parameter value");
		return;
	}
	sp.len = strlen(sp.s);
	sd.len = strlen(sd.s);

	t = sp.s;
	if(*t!='\0' && *t!='.') {
		while(t!=NULL && *t!='\0') {
			if(*t < '0' || *t > '9') {
				LOG(L_ERR, "PDT:pdt_fifo_add: bad prefix [%s]\n", sp.s);
				rpc->fault(c, 400, "Bad Prefix");
				return;
			}
			t++;
		}
	} else {
		sp.s   = NULL;
		sp.len = 0;
	}

	if(*sd.s=='\0' || *sd.s=='.') {
		sd.s   = NULL;
		sd.len = 0;
	}

	lock_get(&_dhash->diff_lock);

	for(i=0; i<_dhash->hash_size; i++) {
		lock_get(&_dhash->dhash[i].lock);

		it = _dhash->dhash[i].e;
		for (it = _dhash->dhash[i].e; it; it = it->n) {
			if((sp.s==NULL && sd.s==NULL)
			   || (sp.s!=NULL && it->prefix.len>=sp.len &&
			       strncmp(it->prefix.s, sp.s, sp.len)==0)
			   || (sd.s!=NULL && it->domain.len>=sd.len &&
			       strncasecmp(it->domain.s, sd.s, sd.len)==0)) {

				buf1 = pkg_malloc(it->prefix.len + 1);
				if (!buf1) continue;
				memcpy(buf1, it->prefix.s, it->prefix.len);
				buf1[it->prefix.len] = '\0';

				buf2 = pkg_malloc(it->domain.len + 1);
				if (!buf2) {
					pkg_free(buf1);
					continue;
				}
				memcpy(buf2, it->domain.s, it->domain.len);
				buf2[it->domain.len] = '\0';

				rpc->add(c, "ss", buf1, buf2);
				pkg_free(buf1);
				pkg_free(buf2);
			}
		}

		lock_release(&_dhash->dhash[i].lock);
	}

	lock_release(&_dhash->diff_lock);
}


static rpc_export_t pdt_rpc[] = {
	{"pdt.add",    rpc_add,    rpc_add_doc,    0},
	{"pdt.delete", rpc_delete, rpc_delete_doc, 0},
	{"pdt.list",   rpc_list,   rpc_list_doc,   RET_ARRAY},
	{0, 0, 0, 0}
};

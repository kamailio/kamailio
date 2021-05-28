/**
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include "../../lib/srdb1/db_op.h"
#include "../../core/sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/timer.h"
#include "../../core/ut.h"
#include "../../core/locking.h"
#include "../../core/action.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/parse_from.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

#include "mtree.h"
#include "api.h"

MODULE_VERSION


#define NR_KEYS			3

int mt_fetch_rows = 1000;

/** database connection */
static db1_con_t *db_con = NULL;
static db_func_t mt_dbf;

#if 0
INSERT INTO version (table_name, table_version) values ('mtree','1');
CREATE TABLE mtree (
		id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
		tprefix VARCHAR(32) NOT NULL,
		tvalue VARCHAR(128) DEFAULT '' NOT NULL,
		CONSTRAINT tprefix_idx UNIQUE (tprefix)
		) ENGINE=MyISAM;
INSERT INTO version (table_name, table_version) values ('mtrees','1');
CREATE TABLE mtrees (
		id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
		tname VARCHAR(128) NOT NULL,
		tprefix VARCHAR(32) NOT NULL,
		tvalue VARCHAR(128) DEFAULT '' NOT NULL,
		CONSTRAINT tname_tprefix_idx UNIQUE (tname, tprefix)
		) ENGINE=MyISAM;
#endif

/** parameters */
static str db_url = str_init(DEFAULT_DB_URL);
/* default name created by sql scripts is 'mtrees'
 * - don't set it here with default value, only via config param */
static str db_table = str_init("");
static str tname_column   = str_init("tname");
static str tprefix_column = str_init("tprefix");
static str tvalue_column  = str_init("tvalue");

/* List of allowed chars for a prefix*/
str mt_char_list = str_init("0123456789");

static str value_param = str_init("$avp(s:tvalue)");
static str values_param = str_init("$avp(s:tvalues)");
static str dstid_param = str_init("$avp(s:tdstid)");
static str weight_param = str_init("$avp(s:tweight)");
static str count_param = str_init("$avp(s:tcount)");
pv_spec_t pv_value;
pv_spec_t pv_values;
pv_spec_t pv_dstid;
pv_spec_t pv_weight;
pv_spec_t pv_count;
int _mt_tree_type = MT_TREE_SVAL;
int _mt_ignore_duplicates = 0;
int _mt_allow_duplicates = 0;

/* lock, ref counter and flag used for reloading the date */
static gen_lock_t *mt_lock = 0;
static volatile int mt_tree_refcnt = 0;
static volatile int mt_reload_flag = 0;

int mt_param(modparam_t type, void *val);
static int fixup_mt_match(void** param, int param_no);
static int w_mt_match(struct sip_msg* msg, char* str1, char* str2,
		char* str3);

static int  mod_init(void);
static void mod_destroy(void);
static int  child_init(int rank);
static int mtree_init_rpc(void);
static int bind_mtree(mtree_api_t* api);

static int mt_match(sip_msg_t *msg, str *tname, str *tomatch,
		int mval);

static int mt_load_db(m_tree_t *pt);
static int mt_load_db_trees();

static cmd_export_t cmds[]={
	{"mt_match", (cmd_function)w_mt_match, 3, fixup_mt_match,
		0, REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE},
	{"bind_mtree", (cmd_function)bind_mtree, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"mtree",          PARAM_STRING|USE_FUNC_PARAM, (void*)mt_param},
	{"db_url",         PARAM_STR, &db_url},
	{"db_table",       PARAM_STR, &db_table},
	{"tname_column",   PARAM_STR, &tname_column},
	{"tprefix_column", PARAM_STR, &tprefix_column},
	{"tvalue_column",  PARAM_STR, &tvalue_column},
	{"char_list",      PARAM_STR, &mt_char_list},
	{"fetch_rows",     INT_PARAM, &mt_fetch_rows},
	{"pv_value",       PARAM_STR, &value_param},
	{"pv_values",      PARAM_STR, &values_param},
	{"pv_dstid",       PARAM_STR, &dstid_param},
	{"pv_weight",      PARAM_STR, &weight_param},
	{"pv_count",       PARAM_STR, &count_param},
	{"mt_tree_type",   INT_PARAM, &_mt_tree_type},
	{"mt_ignore_duplicates", INT_PARAM, &_mt_ignore_duplicates},
	{"mt_allow_duplicates", INT_PARAM, &_mt_allow_duplicates},
	{0, 0, 0}
};

struct module_exports exports = {
	"mtree",
	DEFAULT_DLFLAGS,/* dlopen flags */
	cmds,		/*·exported·functions·*/
	params,		/*·exported·functions·*/
	0,		/*·exported·RPC·methods·*/
	0,		/* exported pseudo-variables */
	0,		/* response·function */
	mod_init,	/* module initialization function */
	child_init,	/* per child init function */
	mod_destroy	/* destroy function */
};


/**
 * init module function
 */
static int mod_init(void)
{
	m_tree_t *pt = NULL;

	if(mtree_init_rpc()!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(pv_parse_spec(&value_param, &pv_value) < 0
			|| !(pv_is_w(&pv_value)))
	{
		LM_ERR("cannot parse value pv or pv is read-only\n");
		return -1;
	}

	if (pv_parse_spec(&values_param, &pv_values) < 0
			|| pv_values.type != PVT_AVP) {
		LM_ERR("cannot parse values avp\n");
		return -1;
	}

	if(pv_parse_spec(&dstid_param, &pv_dstid) < 0
			|| pv_dstid.type!=PVT_AVP)
	{
		LM_ERR("cannot parse dstid avp\n");
		return -1;
	}

	if(pv_parse_spec(&weight_param, &pv_weight) < 0
			|| pv_weight.type!=PVT_AVP)
	{
		LM_ERR("cannot parse weight avp\n");
		return -1;
	}

	if(pv_parse_spec(&count_param, &pv_count) < 0
			|| !(pv_is_w(&pv_count)))
	{
		LM_ERR("cannot parse count pv or pv is read-only\n");
		return -1;
	}

	if(mt_fetch_rows<=0)
		mt_fetch_rows = 1000;

	if(mt_char_list.len<=0)
	{
		LM_ERR("invalid prefix char list\n");
		return -1;
	}
	LM_DBG("mt_char_list=%s \n", mt_char_list.s);
	mt_char_table_init();

	/* binding to database module */
	if(db_bind_mod(&db_url, &mt_dbf))
	{
		LM_ERR("database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(mt_dbf, DB_CAP_ALL))
	{
		LM_ERR("database module does not "
				"implement all functions needed by the module\n");
		return -1;
	}

	/* open a connection with the database */
	db_con = mt_dbf.init(&db_url);
	if(db_con==NULL)
	{
		LM_ERR("failed to connect to the database\n");
		return -1;
	}

	LM_DBG("database connection opened successfully\n");

	if ( (mt_lock=lock_alloc())==0) {
		LM_CRIT("failed to alloc lock\n");
		goto error1;
	}
	if (lock_init(mt_lock)==0 ) {
		LM_CRIT("failed to init lock\n");
		goto error1;
	}

	if(mt_defined_trees())
	{
		LM_DBG("static trees defined\n");

		pt = mt_get_first_tree();

		while(pt!=NULL)
		{
			LM_DBG("loading from tree <%.*s>\n",
					pt->tname.len, pt->tname.s);

			/* loading all information from database */
			if(mt_load_db(pt)!=0)
			{
				LM_ERR("cannot load info from database\n");
				goto error1;
			}
			pt = pt->next;
		}
		/* reset db_table value */
		db_table.s = "";
		db_table.len = 0;
	} else {
		if(db_table.len<=0)
		{
			LM_ERR("no trees table defined\n");
			goto error1;
		}
		if(mt_init_list_head()<0)
		{
			LM_ERR("unable to init trees list head\n");
			goto error1;
		}
		/* loading all information from database */
		if(mt_load_db_trees()!=0)
		{
			LM_ERR("cannot load trees from database\n");
			goto error1;
		}
	}
	mt_dbf.close(db_con);
	db_con = 0;

#if 0
	mt_print_tree(mt_get_first_tree());
#endif

	/* success code */
	return 0;

error1:
	if (mt_lock)
	{
		lock_destroy( mt_lock );
		lock_dealloc( mt_lock );
		mt_lock = 0;
	}
	mt_destroy_trees();

	if(db_con!=NULL)
		mt_dbf.close(db_con);
	db_con = 0;
	return -1;
}

static int mt_child_init(void)
{
	db_con = mt_dbf.init(&db_url);
	if(db_con==NULL)
	{
		LM_ERR("failed to connect to database\n");
		return -1;
	}

	return 0;
}


/* each child get a new connection to the database */
static int child_init(int rank)
{
	/* skip child init for non-worker process ranks */
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0;

	if ( mt_child_init()!=0 )
		return -1;

	LM_DBG("#%d: database connection opened successfully\n", rank);

	return 0;
}


static void mod_destroy(void)
{
	LM_DBG("cleaning up\n");
	mt_destroy_trees();
	if (db_con!=NULL && mt_dbf.close!=NULL)
		mt_dbf.close(db_con);
	/* destroy lock */
	if (mt_lock)
	{
		lock_destroy( mt_lock );
		lock_dealloc( mt_lock );
		mt_lock = 0;
	}

}

static int fixup_mt_match(void** param, int param_no)
{
	if(param_no==1 || param_no==2) {
		return fixup_spve_null(param, 1);
	}
	if (param_no != 3)	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	return fixup_igp_null(param, 1);
}


/* use tree tn, match var, by mode, output in avp params */
static int mt_match(sip_msg_t *msg, str *tname, str *tomatch,
		int mval)
{
	m_tree_t *tr = NULL;

	if(msg==NULL) {
		LM_ERR("received null msg\n");
		return -1;
	}

again:
	lock_get( mt_lock );
	if (mt_reload_flag) {
		lock_release( mt_lock );
		sleep_us(5);
		goto again;
	}
	mt_tree_refcnt++;
	lock_release( mt_lock );

	tr = mt_get_tree(tname);
	if(tr==NULL) {
		/* no tree with such name*/
		goto error;
	}

	if(mt_match_prefix(msg, tr, tomatch, mval)<0)
	{
		LM_DBG("no prefix found in [%.*s] for [%.*s]\n",
				tname->len, tname->s,
				tomatch->len, tomatch->s);
		goto error;
	}

	lock_get( mt_lock );
	mt_tree_refcnt--;
	lock_release( mt_lock );
	return 1;

error:
	lock_get( mt_lock );
	mt_tree_refcnt--;
	lock_release( mt_lock );
	return -1;
}

static int w_mt_match(struct sip_msg* msg, char* ptn, char* pvar,
		char* pmode)
{
	str tname;
	str tomatch;
	int mval;

	if(msg==NULL)
	{
		LM_ERR("received null msg\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)ptn, &tname)<0)
	{
		LM_ERR("cannot get the tree name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pvar, &tomatch)<0)
	{
		LM_ERR("cannot get the match var\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t*)pmode, &mval)<0)
	{
		LM_ERR("cannot get the mode\n");
		return -1;
	}

	return mt_match(msg, &tname, &tomatch, mval);
}

int mt_param(modparam_t type, void *val)
{
	if(val==NULL)
		goto error;

	return mt_table_spec((char*)val);
error:
	return -1;

}

static int mt_pack_values(m_tree_t *pt, db1_res_t* db_res,
		int row, int cols, str *tvalue)
{
	static char vbuf[4096];
	int c;
	int len;
	char *p;
	str iv;

	len = 0;
	for(c=1; c<cols; c++) {
		if(VAL_NULL(&RES_ROWS(db_res)[row].values[c])) {
			len += 1;
		} else if(RES_ROWS(db_res)[row].values[c].type == DB1_STRING) {
			len += strlen(RES_ROWS(db_res)[row].values[c].val.string_val);
		} else if(RES_ROWS(db_res)[row].values[c].type == DB1_STR) {
			len += RES_ROWS(db_res)[row].values[c].val.str_val.len;
		} else if(RES_ROWS(db_res)[row].values[c].type == DB1_INT) {
			len += 12;
		} else {
			LM_ERR("unsupported data type for column %d\n", c);
			return -1;
		}
	}
	if(len + c>=4096) {
		LM_ERR("too large values (need %d)\n", len+c);
		return -1;
	}
	p = vbuf;
	for(c=1; c<cols; c++) {
		if(VAL_NULL(&RES_ROWS(db_res)[row].values[c])) {
			*p = pt->pack[2];
			p++;
		} else if(RES_ROWS(db_res)[row].values[c].type == DB1_STRING) {
			strcpy(p, RES_ROWS(db_res)[row].values[c].val.string_val);
			p += strlen(RES_ROWS(db_res)[row].values[c].val.string_val);
		} else if(RES_ROWS(db_res)[row].values[c].type == DB1_STR) {
			strncpy(p, RES_ROWS(db_res)[row].values[c].val.str_val.s,
				RES_ROWS(db_res)[row].values[c].val.str_val.len);
			p += RES_ROWS(db_res)[row].values[c].val.str_val.len;
		} else if(RES_ROWS(db_res)[row].values[c].type == DB1_INT) {
			iv.s = sint2str(RES_ROWS(db_res)[row].values[c].val.int_val, &iv.len);
			strncpy(p, iv.s, iv.len);
			p += iv.len;
		}
		if(c+1<cols) {
			*p = pt->pack[1];
			p++;
		}
	}
	tvalue->s = vbuf;
	tvalue->len = p - vbuf;
	LM_DBG("packed: [%.*s]\n", tvalue->len, tvalue->s);
	return 0;
}

static int mt_load_db(m_tree_t *pt)
{
	db_key_t db_cols[MT_MAX_COLS] = {&tprefix_column, &tvalue_column};
	db_key_t key_cols[1];
	db_op_t op[1] = {OP_EQ};
	db_val_t vals[1];
	str tprefix, tvalue;
	db1_res_t* db_res = NULL;
	int i, ret, c;
	m_tree_t new_tree;
	m_tree_t *old_tree = NULL;
	mt_node_t *bk_head = NULL;

	if(pt->ncols>0) {
		for(c=0; c<pt->ncols; c++) {
			db_cols[c] = &pt->scols[c];
		}
	} else {
		db_cols[0] = &tprefix_column;
		db_cols[1] = &tvalue_column;
		c = 2;
	}
	key_cols[0] = &tname_column;
	VAL_TYPE(vals) = DB1_STRING;
	VAL_NULL(vals) = 0;
	VAL_STRING(vals) = pt->tname.s;

	if(db_con==NULL)
	{
		LM_ERR("no db connection\n");
		return -1;
	}

	old_tree = mt_get_tree(&(pt->tname));
	if(old_tree==NULL)
	{
		LM_ERR("tree definition not found [%.*s]\n", pt->tname.len,
				pt->tname.s);
		return -1;
	}
	memcpy(&new_tree, old_tree, sizeof(m_tree_t));
	new_tree.head = 0;
	new_tree.next = 0;
	new_tree.nrnodes = 0;
	new_tree.nritems = 0;
	new_tree.memsize = 0;
	new_tree.reload_count++;
	new_tree.reload_time = (unsigned int)time(NULL);


	if (mt_dbf.use_table(db_con, &old_tree->dbtable) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	if (DB_CAPABILITY(mt_dbf, DB_CAP_FETCH)) {
		if(mt_dbf.query(db_con, key_cols, op, vals, db_cols, pt->multi,
				c, 0, 0) < 0)
		{
			LM_ERR("Error while querying db\n");
			return -1;
		}
		if(mt_dbf.fetch_result(db_con, &db_res, mt_fetch_rows)<0)
		{
			LM_ERR("Error while fetching result\n");
			goto error;
		} else {
			if(RES_ROW_N(db_res)==0)
			{
				goto dbreloaded;
			}
		}
	} else {
		if((ret=mt_dbf.query(db_con, key_cols, op, vals, db_cols,
						pt->multi, 2, 0, &db_res))!=0
				|| RES_ROW_N(db_res)<=0 )
		{
			if(ret==0)
			{
				goto dbreloaded;
			} else {
				goto error;
			}
		}
	}

	if(RES_ROW_N(db_res)>0)
	{
		if(RES_ROWS(db_res)[0].values[0].type != DB1_STRING
				|| RES_ROWS(db_res)[0].values[1].type != DB1_STRING)
		{
			LM_ERR("wrong column types in db table (%d / %d)\n",
					RES_ROWS(db_res)[0].values[0].type,
					RES_ROWS(db_res)[0].values[1].type);
			goto error;
		}
	}

	do {
		for(i=0; i<RES_ROW_N(db_res); i++)
		{
			/* check for NULL values ?!?! */
			tprefix.s = (char*)(RES_ROWS(db_res)[i].values[0].val.string_val);
			tprefix.len = strlen(ZSW(tprefix.s));

			if(c>2) {
				if(mt_pack_values(&new_tree, db_res, i, c, &tvalue)<0) {
					LM_ERR("Error packing values\n");
					goto error;
				}
			} else {
				tvalue.s = (char*)(RES_ROWS(db_res)[i].values[1].val.string_val);
				tvalue.len = strlen(ZSW(tvalue.s));
			}

			if(tprefix.s==NULL || tvalue.s==NULL
					|| tprefix.len<=0 || tvalue.len<=0)
			{
				LM_ERR("Error - bad record in db"
						" (prefix: %p/%d - value: %p/%d)\n",
						tprefix.s, tprefix.len, tvalue.s, tvalue.len);
				continue;
			}

			if(mt_add_to_tree(&new_tree, &tprefix, &tvalue)<0)
			{
				LM_ERR("Error adding info to tree\n");
				goto error;
			}
		}
		if (DB_CAPABILITY(mt_dbf, DB_CAP_FETCH)) {
			if(mt_dbf.fetch_result(db_con, &db_res, mt_fetch_rows)<0) {
				LM_ERR("Error while fetching!\n");
				if (db_res)
					mt_dbf.free_result(db_con, db_res);
				goto error;
			}
		} else {
			break;
		}
	}  while(RES_ROW_N(db_res)>0);

dbreloaded:
	mt_dbf.free_result(db_con, db_res);


	/* block all readers */
	lock_get( mt_lock );
	mt_reload_flag = 1;
	lock_release( mt_lock );

	while (mt_tree_refcnt) {
		sleep_us(10);
	}

	bk_head = old_tree->head;
	old_tree->head = new_tree.head;
	old_tree->nrnodes = new_tree.nrnodes;
	old_tree->nritems = new_tree.nritems;
	old_tree->memsize = new_tree.memsize;
	old_tree->reload_count = new_tree.reload_count;
	old_tree->reload_time  = new_tree.reload_time;

	mt_reload_flag = 0;

	/* free old data */
	if (bk_head!=NULL)
		mt_free_node(bk_head, new_tree.type);

	return 0;

error:
	mt_dbf.free_result(db_con, db_res);
	if (new_tree.head!=NULL)
		mt_free_node(new_tree.head, new_tree.type);
	return -1;
}

static int mt_load_db_trees()
{
	db_key_t db_cols[3] = {&tname_column, &tprefix_column, &tvalue_column};
	str tprefix, tvalue, tname;
	db1_res_t* db_res = NULL;
	int i, ret;
	m_tree_t *new_head = NULL;
	m_tree_t *new_tree = NULL;
	m_tree_t *old_head = NULL;

	if(db_con==NULL)
	{
		LM_ERR("no db connection\n");
		return -1;
	}

	if (mt_dbf.use_table(db_con, &db_table) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	if (DB_CAPABILITY(mt_dbf, DB_CAP_FETCH))
	{
		if(mt_dbf.query(db_con,0,0,0,db_cols,0,3,&tname_column,0) < 0)
		{
			LM_ERR("Error while querying db\n");
			return -1;
		}
		if(mt_dbf.fetch_result(db_con, &db_res, mt_fetch_rows)<0)
		{
			LM_ERR("Error while fetching result\n");
			if (db_res)
				mt_dbf.free_result(db_con, db_res);
			goto error;
		} else {
			if(RES_ROW_N(db_res)==0)
			{
				return 0;
			}
		}
	} else {
		if((ret=mt_dbf.query(db_con, NULL, NULL, NULL, db_cols,
						0, 3, &tname_column, &db_res))!=0
				|| RES_ROW_N(db_res)<=0 )
		{
			mt_dbf.free_result(db_con, db_res);
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
			tname.s = (char*)(RES_ROWS(db_res)[i].values[0].val.string_val);
			tprefix.s = (char*)(RES_ROWS(db_res)[i].values[1].val.string_val);
			tvalue.s = (char*)(RES_ROWS(db_res)[i].values[2].val.string_val);

			if(tprefix.s==NULL || tvalue.s==NULL || tname.s==NULL)
			{
				LM_ERR("Error - null fields in db\n");
				continue;
			}

			tname.len = strlen(tname.s);
			tprefix.len = strlen(tprefix.s);
			tvalue.len = strlen(tvalue.s);

			if(tname.len<=0 || tprefix.len<=0 || tvalue.len<=0)
			{
				LM_ERR("Error - bad values in db\n");
				continue;
			}
			new_tree = mt_add_tree(&new_head, &tname, &db_table, NULL,
							_mt_tree_type, 0);
			if(new_tree==NULL)
			{
				LM_ERR("New tree cannot be initialized\n");
				goto error;
			}
			if(mt_add_to_tree(new_tree, &tprefix, &tvalue)<0)
			{
				LM_ERR("Error adding info to tree\n");
				goto error;
			}
		}
		if (DB_CAPABILITY(mt_dbf, DB_CAP_FETCH)) {
			if(mt_dbf.fetch_result(db_con, &db_res, mt_fetch_rows)<0) {
				LM_ERR("Error while fetching!\n");
				if (db_res)
					mt_dbf.free_result(db_con, db_res);
				goto error;
			}
		} else {
			break;
		}
	} while(RES_ROW_N(db_res)>0);
	mt_dbf.free_result(db_con, db_res);

	/* block all readers */
	lock_get( mt_lock );
	mt_reload_flag = 1;
	lock_release( mt_lock );

	while (mt_tree_refcnt) {
		sleep_us(10);
	}

	old_head = mt_swap_list_head(new_head);

	mt_reload_flag = 0;
	/* free old data */
	if (old_head!=NULL)
		mt_free_tree(old_head);

	return 0;

error:
	mt_dbf.free_result(db_con, db_res);
	if (new_head!=NULL)
		mt_free_tree(new_head);
	return -1;
}


/* RPC commands */
void rpc_mtree_summary(rpc_t* rpc, void* c)
{
	str tname = {0, 0};
	m_tree_t *pt;
	void* th;
	void* ih;
	int found;

	if(!mt_defined_trees())
	{
		rpc->fault(c, 500, "Empty tree list");
		return;
	}

	/* read optional tree name */
	if(rpc->scan(c, "*S", &tname)==0)
	{
		tname.s = NULL;
		tname.len = 0;
	}

	pt = mt_get_first_tree();
	if(pt==NULL)
	{
		rpc->fault(c, 404, "No tree");
		return;
	}

	found = 0;
	while(pt!=NULL)
	{
		if(tname.s==NULL
				|| (tname.s!=NULL && pt->tname.len>=tname.len
					&& strncmp(pt->tname.s, tname.s, tname.len)==0))
		{
			found = 1;
			if (rpc->add(c, "{", &th) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc");
				return;
			}
			if(rpc->struct_add(th, "s{",
						"table", pt->tname.s,
						"item", &ih) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc ih");
				return;
			}
			if(rpc->struct_add(ih, "d", "ttype", pt->type) < 0 ) {
				rpc->fault(c, 500, "Internal error adding type");
				return;
			}
			if(rpc->struct_add(ih, "d", "memsize", pt->memsize) < 0 ) {
				rpc->fault(c, 500, "Internal error adding memsize");
				return;
			}
			if(rpc->struct_add(ih, "d", "nrnodes", pt->nrnodes) < 0 ) {
				rpc->fault(c, 500, "Internal error adding nodes");
				return;
			}
			if(rpc->struct_add(ih, "d", "nritems", pt->nritems) < 0 ) {
				rpc->fault(c, 500, "Internal error adding items");
				return;
			}
			if(rpc->struct_add(ih, "d", "reload_count",
						(int)pt->reload_count) < 0 ) {
				rpc->fault(c, 500, "Internal error adding items");
				return;
			}
			if(rpc->struct_add(ih, "d", "reload_time",
						(int)pt->reload_time) < 0 ) {
				rpc->fault(c, 500, "Internal error adding items");
				return;
			}
		}
		pt = pt->next;
	}

	if(found==0)
	{
		rpc->fault(c, 404, "Tree not found");
		return;
	}

	return;
}

static const char* rpc_mtree_summary_doc[2] = {
	"Print summary of loaded mtree tables",
	0
};

void rpc_mtree_reload(rpc_t* rpc, void* c)
{
	str tname = {0, 0};
	m_tree_t *pt = NULL;
	int treloaded = 0;

	if(db_table.len>0)
	{
		/* re-loading all information from database */
		if(mt_load_db_trees()!=0)
		{
			LM_ERR("cannot re-load mtrees from database\n");
			goto error;
		}
	} else {
		if(!mt_defined_trees())
		{
			LM_ERR("empty mtree list\n");
			goto error;
		}

		/* read tree name */
		if (rpc->scan(c, "S", &tname) != 1) {
			tname.s = 0;
			tname.len = 0;
		} else {
			if(*tname.s=='.') {
				tname.s = 0;
				tname.len = 0;
			}
		}

		pt = mt_get_first_tree();

		while(pt!=NULL)
		{
			if(tname.s==NULL
					|| (tname.s!=NULL && pt->tname.len>=tname.len
						&& strncmp(pt->tname.s, tname.s, tname.len)==0))
			{
				/* re-loading table from database */
				if(mt_load_db(pt)!=0)
				{
					LM_ERR("cannot re-load mtree from database\n");
					goto error;
				}
				treloaded = 1;
			}
			pt = pt->next;
		}
		if(treloaded == 0) {
			rpc->fault(c, 500, "No Mtree Name Matching");
		}
	}

	return;

error:
	rpc->fault(c, 500, "Mtree Reload Failed");
}

static const char* rpc_mtree_reload_doc[2] = {
	"Reload mtrees from database to memory",
	0
};

void rpc_mtree_match(rpc_t* rpc, void* ctx)
{
	str tname = STR_NULL;
	str tomatch = STR_NULL;
	int mode = -1;

	m_tree_t *tr;

	if(!mt_defined_trees())
	{
		rpc->fault(ctx, 500, "Empty tree list.");
		return;
	}

	if (rpc->scan(ctx, ".SSd", &tname, &tomatch, &mode) < 3) {
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}

	if (mode !=0 && mode != 2) {
		rpc->fault(ctx, 500, "Invalid parameter 'mode'");
		return;
	}

again:
	lock_get( mt_lock );
	if (mt_reload_flag) {
		lock_release( mt_lock );
		sleep_us(5);
		goto again;
	}
	mt_tree_refcnt++;
	lock_release( mt_lock );

	tr = mt_get_tree(&tname);
	if(tr==NULL)
	{
		/* no tree with such name*/
		rpc->fault(ctx, 404, "Tree not found");
		goto error;
	}

	if(mt_rpc_match_prefix(rpc, ctx, tr, &tomatch, mode)<0)
	{
		LM_DBG("no prefix found in [%.*s] for [%.*s]\n",
				tname.len, tname.s,
				tomatch.len, tomatch.s);
		rpc->fault(ctx, 404, "Prefix not found");
	}

error:
	lock_get( mt_lock );
	mt_tree_refcnt--;
	lock_release( mt_lock );

}

static const char* rpc_mtree_match_doc[6] = {
	"Match prefix value against mtree",
	"uses three required parameters",
	"tname - tree name",
	"prefix - prefix for matching",
	"mode - mode for matching (0 or 2)",
	0
};


int rpc_mtree_print_node(rpc_t* rpc, void* ctx, m_tree_t *tree, mt_node_t *pt,
		char *code, int len)
{
	int i;
	mt_is_t *tvalues;
	str val;
	void* th = NULL;
	void* ih = NULL;

	if(pt==NULL || len>=MT_MAX_DEPTH)
		return 0;

	for(i=0; i<MT_NODE_SIZE; i++)
	{
		code[len]=mt_char_list.s[i];
		tvalues = pt[i].tvalues;
		if (tvalues != NULL)
		{
			/* add structure node */
			if (rpc->add(ctx, "{", &th) < 0)
			{
				rpc->fault(ctx, 500, "Internal error - node structure");
				return -1;
			}

			val.s = code;
			val.len = len+1;
			if(rpc->struct_add(th, "SS[",
				"tname",	&tree->tname,
				"tprefix", 	&val,
				"tvalue",	&ih)<0)
			{
				rpc->fault(ctx, 500, "Internal error - attribute fields");
				return -1;
			}

			while (tvalues != NULL) {
				if (tree->type == MT_TREE_IVAL) {
					if(rpc->array_add(ih, "u",
								(unsigned long)tvalues->tvalue.n)<0) {
						rpc->fault(ctx, 500, "Internal error - int val");
						return -1;
					}
				} else {
					if(rpc->array_add(ih, "S", &tvalues->tvalue.s)<0) {
						rpc->fault(ctx, 500, "Internal error - str val");
						return -1;
					}
				}
				tvalues = tvalues->next;
			}
		}
		if(rpc_mtree_print_node(rpc, ctx, tree, pt[i].child, code, len+1)<0)
			goto error;
	}
	return 0;
error:
	return -1;
}

/**
 * "mtree.list" syntax :
 *    tname
 *
 * 	- '.' (dot) means NULL value and will match anything
 */
void rpc_mtree_list(rpc_t* rpc, void* ctx)
{
	str tname = {0, 0};
	m_tree_t *pt;
	static char code_buf[MT_MAX_DEPTH+1];
	int len;

	if(!mt_defined_trees())
	{
		rpc->fault(ctx, 500, "Empty tree list.");
		return;
	}

	if(rpc->scan(ctx, "*.S", &tname)!=1) {
		tname.s = NULL;
		tname.len = 0;
	}

	pt = mt_get_first_tree();

	while(pt!=NULL)
	{
		if(tname.s==NULL ||
				(tname.s!=NULL && pt->tname.len>=tname.len &&
					strncmp(pt->tname.s, tname.s, tname.len)==0))
		{
			len = 0;
			code_buf[0] = '\0';
			if(rpc_mtree_print_node(rpc, ctx, pt, pt->head, code_buf, len)<0) {
				goto error;
			}
		}
		pt = pt->next;
	}

	return;

error:
	LM_ERR("failed to build rpc response\n");
	return;
}

static const char* rpc_mtree_list_doc[6] = {
	"List the content of one or all trees",
	"Parameters:",
	"tname - tree name (optional)",
	0
};


rpc_export_t mtree_rpc[] = {
	{"mtree.summary", rpc_mtree_summary, rpc_mtree_summary_doc, RET_ARRAY},
	{"mtree.reload", rpc_mtree_reload, rpc_mtree_reload_doc, 0},
	{"mtree.match", rpc_mtree_match, rpc_mtree_match_doc, 0},
	{"mtree.list", rpc_mtree_list, rpc_mtree_list_doc, RET_ARRAY},
	{0, 0, 0, 0}
};

static int mtree_init_rpc(void)
{
	if (rpc_register_array(mtree_rpc) != 0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}


/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_mtree_exports[] = {
	{ str_init("mtree"), str_init("mt_match"),
		SR_KEMIP_INT, mt_match,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 * load mtree module API
 */
static int bind_mtree(mtree_api_t* api)
{
	if (!api) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}
	api->mt_match = mt_match;

	return 0;
}


/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_mtree_exports);
	return 0;
}

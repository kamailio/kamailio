/**
 *
 * Copyright (C) 2015 Federico Cabiddu (federico.cabiddu@gmail.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/route.h"
#include "../../core/script_cb.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/registrar/api.h"
#include "../../core/dset.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

#include "ts_hash.h"
#include "ts_handlers.h"
#include "ts_append.h"
#include "ts_store.h"
#include "ts_rpc.h"

MODULE_VERSION


/** TM bind **/
struct tm_binds _tmb;
/** REGISTRAR bind **/
registrar_api_t _regapi;

/** parameters */
static int hash_size = 2048;
int use_domain = 0;

/** module functions */
static int mod_init(void);
static void destroy(void);

static int w_ts_append_to(struct sip_msg* msg, char *idx, char *lbl, char *d);
static int w_ts_append_to2(struct sip_msg* msg, char *idx, char *lbl, char *d, char *ruri);
static int fixup_ts_append_to(void** param, int param_no);
static int w_ts_append(struct sip_msg* _msg, char *_table, char *_ruri);
static int fixup_ts_append(void** param, int param_no);

static int w_ts_store(struct sip_msg* msg, char *p1, char *p2);
static int w_ts_store1(struct sip_msg* msg, char *_ruri, char *p2);

stat_var *stored_ruris;
stat_var *stored_transactions;
stat_var *total_ruris;
stat_var *total_transactions;
stat_var *added_branches;

static cmd_export_t cmds[]={
	{"ts_append_to", (cmd_function)w_ts_append_to,  3,
		fixup_ts_append_to, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"ts_append_to", (cmd_function)w_ts_append_to2,  4,
		fixup_ts_append_to, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"ts_append", (cmd_function)w_ts_append,  2,
		fixup_ts_append, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"ts_store", (cmd_function)w_ts_store,  0,
		0 , 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"ts_store", (cmd_function)w_ts_store1,  1,
		fixup_spve_null , 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"hash_size",	INT_PARAM,	&hash_size},
	{"use_domain",	INT_PARAM,	&use_domain},
	{0,0,0}
};

#ifdef STATISTICS
/*! \brief We expose internal variables via the statistic framework below.*/
stat_export_t mod_stats[] = {
	{"stored_ruris",	STAT_NO_RESET, 	&stored_ruris  },
	{"stored_transactions",	STAT_NO_RESET, 	&stored_transactions  },
	{"total_ruris",		STAT_NO_RESET, 	&total_ruris  },
	{"total_transactions",	STAT_NO_RESET, 	&total_transactions  },
	{"added_branches",	STAT_NO_RESET, 	&added_branches  },
	{0, 0, 0}
};
#endif

/** module exports */
struct module_exports exports = {
	"tsilo",
	DEFAULT_DLFLAGS,/* dlopen flags */
	cmds,        	/* Exported functions */
	params,      	/* Exported parameters */
#ifdef STATISTICS
	mod_stats,   	/* exported statistics */
#else
	0,
#endif
	0,           	/* exported MI functions */
	0,     		/* exported pseudo-variables */
	0,           	/* extra processes */
	mod_init,    	/* module initialization function */
	0,
	destroy, 	/* destroy function */
	0,  		/* Per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	unsigned int n;

	/* register the RPC methods */
	if(rpc_register_array(rpc_methods)!=0)
	{
        LM_ERR("failed to register RPC commands\n");
        return -1;
	}
	/* load the TM API */
	if (load_tm_api(&_tmb)!=0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

	/* load the REGISTRAR API */
	if (registrar_load_api(&_regapi) != 0) {
		LM_ERR("cannot load REGISTRAR API\n");
		return -1;
	}

	/* sanitize hash_size */
	if (hash_size < 1){
		LM_WARN("hash_size is smaller "
			"than 1  -> rounding from %d to 1\n",
			hash_size);
		hash_size = 1;
	}

	/* initialize the hash table */
	for( n=0 ; n<(8*sizeof(n)) ; n++) {
		if (hash_size==(1<<n))
			break;
		if (n && hash_size<(1<<n)) {
			LM_WARN("hash_size is not a power "
				"of 2 as it should be -> rounding from %d to %d (n=%d)\n",
				hash_size, 1<<(n-1), n);
			hash_size = 1<<(n-1);
			break;
		}
	}

	LM_DBG("creating table with size %d", hash_size);
	if ( init_ts_table(hash_size)<0 ) {
		LM_ERR("failed to create hash table\n");
		return -1;
	}

#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats(exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
	return 0;
#endif

}

/**
 * destroy function
 */
static void destroy(void)
{
	destroy_ts_table();
	return;
}

static int ts_check_uri(str *uri)
{
	struct sip_uri ruri;
	if (parse_uri(uri->s, uri->len, &ruri)!=0)
	{
		LM_ERR("bad uri [%.*s]\n",
				uri->len,
				uri->s);
		return -1;
	}
	return 0;
}
/**
 *
 */
static int fixup_ts_append_to(void** param, int param_no)
{
	if (param_no==1 || param_no==2) {
		return fixup_igp_null(param, 1);
	}

	if (param_no==3) {
		if(strlen((char*)*param)<=1 && (*(char*)(*param)==0 || *(char*)(*param)=='0')) {
			*param = (void*)0;
			LM_ERR("empty table name\n");
			return -1;
		}
	}

	if (param_no==4) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}

static int fixup_ts_append(void** param, int param_no)
{
	if (param_no==1) {
		if(strlen((char*)*param)<=1 && (*(char*)(*param)==0 || *(char*)(*param)=='0')) {
			*param = (void*)0;
			LM_ERR("empty table name\n");
			return -1;
		}
	}

	if (param_no==2 || param_no==3) {
		return fixup_spve_null(param, 1);
	}

	return 0;
}

/**
 *
 */
static int w_ts_append(struct sip_msg* _msg, char *_table, char *_ruri)
{
	str tmp  = STR_NULL;
	str ruri = STR_NULL;
	int rc;

	if(_ruri==NULL || (fixup_get_svalue(_msg, (gparam_p)_ruri, &tmp)!=0 || tmp.len<=0)) {
		LM_ERR("invalid ruri parameter\n");
		return -1;
	}
	if(ts_check_uri(&tmp)<0)
		return -1;

	if (pkg_str_dup(&ruri, &tmp) < 0)
		return -1;

	rc = ts_append(_msg, &ruri, _table);

	pkg_free(ruri.s);

	return rc;
}

/**
 *
 */
static int ki_ts_append(sip_msg_t* _msg, str *_table, str *_ruri)
{
	str ruri = STR_NULL;
	int rc;

	if(ts_check_uri(_ruri)<0)
		return -1;

	if (pkg_str_dup(&ruri, _ruri) < 0)
		return -1;

	rc = ts_append(_msg, &ruri, _table->s);

	pkg_free(ruri.s);

	return rc;
}

/**
 *
 */
static int w_ts_append_to(struct sip_msg* msg, char *idx, char *lbl, char *table)
{
	unsigned int tindex;
	unsigned int tlabel;

	if(fixup_get_ivalue(msg, (gparam_p)idx, (int*)&tindex)<0) {
		LM_ERR("cannot get transaction index\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)lbl, (int*)&tlabel)<0) {
		LM_ERR("cannot get transaction label\n");
		return -1;
	}

	return ts_append_to(msg, tindex, tlabel, table, 0);
}

/**
 *
 */
static int ki_ts_append_to(sip_msg_t* _msg, int tindex, int tlabel, str *_table)
{
	return ts_append_to(_msg, (unsigned int)tindex, (unsigned int)tlabel,
			_table->s, 0);
}

/**
 *
 */
static int w_ts_append_to2(struct sip_msg* msg, char *idx, char *lbl, char *table, char *ruri)
{
	unsigned int tindex;
	unsigned int tlabel;
	str suri;

	if(fixup_get_ivalue(msg, (gparam_p)idx, (int*)&tindex)<0) {
		LM_ERR("cannot get transaction index\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)lbl, (int*)&tlabel)<0) {
		LM_ERR("cannot get transaction label\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)ruri, &suri)!=0) {
		LM_ERR("failed to conert r-uri parameter\n");
		return -1;
	}
	if(ts_check_uri(&suri)<0)
		return -1;

	return ts_append_to(msg, tindex, tlabel, table, &suri);
}

/**
 *
 */
static int ki_ts_append_to_uri(sip_msg_t* _msg, int tindex, int tlabel,
		str *_table, str *_uri)
{
	return ts_append_to(_msg, (unsigned int)tindex, (unsigned int)tlabel,
			_table->s, _uri);
}

/**
 *
 */
static int w_ts_store(struct sip_msg* msg, char *p1, char *p2)
{
	return ts_store(msg, 0);
}

/**
 *
 */
static int ki_ts_store(sip_msg_t* msg)
{
	return ts_store(msg, 0);
}

/**
 *
 */
static int w_ts_store1(struct sip_msg* msg, char *_ruri, char *p2)
{
	str suri;

	if(fixup_get_svalue(msg, (gparam_t*)_ruri, &suri)!=0) {
		LM_ERR("failed to conert r-uri parameter\n");
		return -1;
	}
	return ts_store(msg, &suri);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_tsilo_exports[] = {
	{ str_init("tsilo"), str_init("ts_store"),
		SR_KEMIP_INT, ki_ts_store,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tsilo"), str_init("ts_store_uri"),
		SR_KEMIP_INT, ts_store,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tsilo"), str_init("ts_append"),
		SR_KEMIP_INT, ki_ts_append,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tsilo"), str_init("ts_append_to"),
		SR_KEMIP_INT, ki_ts_append_to,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tsilo"), str_init("ts_append_to_uri"),
		SR_KEMIP_INT, ki_ts_append_to_uri,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_tsilo_exports);
	return 0;
}
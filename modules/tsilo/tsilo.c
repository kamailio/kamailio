/**
 *
 * Copyright (C) 2014 Federico Cabiddu (federico.cabiddu@gmail.com)
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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../route.h"
#include "../../script_cb.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/registrar/api.h"
#include "../../modules/usrloc/usrloc.h"
#include "../../dset.h"
#include "../../rpc_lookup.h"

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
/** USRLOC BIND **/
usrloc_api_t _ul;

int use_domain = 0;

/** parameters */
static int hash_size = 2048;

/** module functions */
static int mod_init(void);
static void destroy(void);

static int w_ts_append_to(struct sip_msg* msg, char *idx, char *lbl, char *d);
static int fixup_ts_append_to(void** param, int param_no);
static int w_ts_append(struct sip_msg* _msg, char *_table, char *_ruri);
static int fixup_ts_append(void** param, int param_no);
static int w_ts_store(struct sip_msg* msg);

static cmd_export_t cmds[]={
	{"ts_append_to", (cmd_function)w_ts_append_to,  3,
		fixup_ts_append_to, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"ts_append", (cmd_function)w_ts_append,  2,
		fixup_ts_append, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"ts_store", (cmd_function)w_ts_store,  0,
		0 , 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{0,0,0,0,0}
};

static param_export_t params[]={
	{"hash_size",	INT_PARAM,	&hash_size},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"tsilo",
    DEFAULT_DLFLAGS,
	cmds,
	params,
	0, /* exported statistics */
    0,    /* exported MI functions */
    0,
    0,
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,  /* destroy function */
	0
};

/**
 * init module function
 */
static int mod_init(void)
{
	unsigned int n;
	bind_usrloc_t bind_usrloc;

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
	/* load UL-Bindings */
	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);

	if (!bind_usrloc) {
		LM_ERR("could not load the USRLOC API\n");
		return -1;
	}

	if (bind_usrloc(&_ul) < 0) {
		LM_ERR("could not load the USRLOC API\n");
		return -1;
	}

	use_domain = _ul.use_domain;
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

	return 0;
}

/**
 * destroy function
 */
static void destroy(void)
{
	destroy_ts_table();
	return;
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
	str ruri = {0};

	if(_ruri==NULL || (fixup_get_svalue(_msg, (gparam_p)_ruri, &ruri)!=0 || ruri.len<=0)) {
		LM_ERR("invalid ruri parameter\n");
		return -1;
	}
	return ts_append(_msg, &ruri, _table);
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

	return ts_append_to(msg, tindex, tlabel, table);
}

/**
 *
 */
static int w_ts_store(struct sip_msg* msg)
{
	return ts_store(msg);
}

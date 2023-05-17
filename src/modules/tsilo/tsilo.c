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

#include "../../core/parser/contact/parse_contact.h"

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

static int w_ts_append_to(struct sip_msg *msg, char *idx, char *lbl, char *d);
static int w_ts_append_to2(
		struct sip_msg *msg, char *idx, char *lbl, char *d, char *ruri);
static int fixup_ts_append_to(void **param, int param_no);
static int w_ts_append(struct sip_msg *_msg, char *_table, char *_ruri);
static int fixup_ts_append(void **param, int param_no);
static int w_ts_append_by_contact2(
		struct sip_msg *_msg, char *_table, char *_ruri);
static int w_ts_append_by_contact3(
		struct sip_msg *_msg, char *_table, char *_ruri, char *_contact);
static int fixup_ts_append_by_contact(void **param, int param_no);
static int w_ts_store(struct sip_msg *msg, char *p1, char *p2);
static int w_ts_store1(struct sip_msg *msg, char *_ruri, char *p2);

stat_var *stored_ruris;
stat_var *stored_transactions;
stat_var *total_ruris;
stat_var *total_transactions;
stat_var *added_branches;

static cmd_export_t cmds[] = {
		{"ts_append_to", (cmd_function)w_ts_append_to, 3, fixup_ts_append_to, 0,
				REQUEST_ROUTE | FAILURE_ROUTE},
		{"ts_append_to", (cmd_function)w_ts_append_to2, 4, fixup_ts_append_to,
				0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"ts_append", (cmd_function)w_ts_append, 2, fixup_ts_append, 0,
				REQUEST_ROUTE | FAILURE_ROUTE},
		{"ts_append_by_contact", (cmd_function)w_ts_append_by_contact2,
				2, /* for two parameters */
				fixup_ts_append_by_contact, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"ts_append_by_contact", (cmd_function)w_ts_append_by_contact3,
				3, /* for three parameters */
				fixup_ts_append_by_contact, 0, REQUEST_ROUTE | FAILURE_ROUTE},
		{"ts_store", (cmd_function)w_ts_store, 0, 0, 0,
				REQUEST_ROUTE | FAILURE_ROUTE},
		{"ts_store", (cmd_function)w_ts_store1, 1, fixup_spve_null, 0,
				REQUEST_ROUTE | FAILURE_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {{"hash_size", INT_PARAM, &hash_size},
		{"use_domain", INT_PARAM, &use_domain}, {0, 0, 0}};

#ifdef STATISTICS
/*! \brief We expose internal variables via the statistic framework below.*/
stat_export_t mod_stats[] = {{"stored_ruris", STAT_NO_RESET, &stored_ruris},
		{"stored_transactions", STAT_NO_RESET, &stored_transactions},
		{"total_ruris", STAT_NO_RESET, &total_ruris},
		{"total_transactions", STAT_NO_RESET, &total_transactions},
		{"added_branches", STAT_NO_RESET, &added_branches}, {0, 0, 0}};
#endif

/** module exports */
struct module_exports exports = {
		"tsilo",		 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* exported RPC methods */
		0,				 /* exported pseudo-variables */
		0,				 /* response handling function */
		mod_init,		 /* module initialization function */
		0,				 /* per-child init function */
		destroy			 /* destroy function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	unsigned int n;

	/* register the RPC methods */
	if(rpc_register_array(rpc_methods) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	/* load the TM API */
	if(load_tm_api(&_tmb) != 0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

	/* load the REGISTRAR API */
	if(registrar_load_api(&_regapi) != 0) {
		LM_ERR("cannot load REGISTRAR API\n");
		return -1;
	}

	/* sanitize hash_size */
	if(hash_size < 1) {
		LM_WARN("hash_size is smaller "
				"than 1  -> rounding from %d to 1\n",
				hash_size);
		hash_size = 1;
	}

	/* initialize the hash table */
	for(n = 0; n < (8 * sizeof(n)); n++) {
		if(hash_size == (1 << n))
			break;
		if(n && hash_size < (1 << n)) {
			LM_WARN("hash_size is not a power "
					"of 2 as it should be -> rounding from %d to %d (n=%d)\n",
					hash_size, 1 << (n - 1), n);
			hash_size = 1 << (n - 1);
			break;
		}
	}

	LM_DBG("creating table with size %d", hash_size);
	if(init_ts_table(hash_size) < 0) {
		LM_ERR("failed to create hash table\n");
		return -1;
	}

#ifdef STATISTICS
	/* register statistics */
	if(register_module_stats(exports.name, mod_stats) != 0) {
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
	if(parse_uri(uri->s, uri->len, &ruri) != 0) {
		LM_ERR("bad uri [%.*s]\n", uri->len, uri->s);
		return -1;
	}
	return 0;
}
/**
 *
 */
static int fixup_ts_append_to(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2) {
		return fixup_igp_null(param, 1);
	}

	if(param_no == 3) {
		if(strlen((char *)*param) <= 1
				&& (*(char *)(*param) == 0 || *(char *)(*param) == '0')) {
			*param = (void *)0;
			LM_ERR("empty table name\n");
			return -1;
		}
	}

	if(param_no == 4) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}

static int fixup_ts_append(void **param, int param_no)
{
	if(param_no == 1) {
		if(strlen((char *)*param) <= 1
				&& (*(char *)(*param) == 0 || *(char *)(*param) == '0')) {
			*param = (void *)0;
			LM_ERR("empty table name\n");
			return -1;
		}
	}

	if(param_no == 2 || param_no == 3) {
		return fixup_spve_null(param, 1);
	}

	return 0;
}

static int fixup_ts_append_by_contact(void **param, int param_no)
{
	if(param_no == 1) {
		if(strlen((char *)*param) <= 1
				&& (*(char *)(*param) == 0 || *(char *)(*param) == '0')) {
			*param = (void *)0;
			LM_ERR("empty table name\n");
			return -1;
		}
	}

	if(param_no == 2 || param_no == 3) {
		return fixup_spve_null(param, 1);
	}

	return 0;
}

/**
 *
 */
static int w_ts_append(struct sip_msg *_msg, char *_table, char *_ruri)
{
	str tmp = STR_NULL;
	str ruri = STR_NULL;
	int rc;

	/* we do not want to do append by particular location */
	str contact = STR_NULL;

	if(_ruri == NULL
			|| (fixup_get_svalue(_msg, (gparam_p)_ruri, &tmp) != 0
					|| tmp.len <= 0)) {
		LM_ERR("invalid ruri parameter\n");
		return -1;
	}
	if(ts_check_uri(&tmp) < 0)
		return -1;

	if(pkg_str_dup(&ruri, &tmp) < 0)
		return -1;

	rc = ts_append(_msg, &ruri, &contact, _table);

	pkg_free(ruri.s);

	return rc;
}

/**
 *
 */
static int ki_ts_append(sip_msg_t *_msg, str *_table, str *_ruri)
{
	str ruri = STR_NULL;
	int rc;

	/* we do not want to do append by particular location */
	str contact = STR_NULL;

	if(ts_check_uri(_ruri) < 0)
		return -1;

	if(pkg_str_dup(&ruri, _ruri) < 0)
		return -1;

	rc = ts_append(_msg, &ruri, &contact, _table->s);

	pkg_free(ruri.s);

	return rc;
}

/**
 *
 */
static int w_ts_append_to(
		struct sip_msg *msg, char *idx, char *lbl, char *table)
{
	unsigned int tindex;
	unsigned int tlabel;

	/* we do not want to do append by particular location */
	str contact = STR_NULL;

	if(fixup_get_ivalue(msg, (gparam_p)idx, (int *)&tindex) < 0) {
		LM_ERR("cannot get transaction index\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)lbl, (int *)&tlabel) < 0) {
		LM_ERR("cannot get transaction label\n");
		return -1;
	}

	/* we do not want to do append by particular location here */
	return ts_append_to(msg, tindex, tlabel, table, 0, &contact);
}

/**
 *
 */
static int ki_ts_append_to(sip_msg_t *_msg, int tindex, int tlabel, str *_table)
{
	/* we do not want to do append by particular location */
	str contact = STR_NULL;

	/* we do not want to do append by particular location here */
	return ts_append_to(_msg, (unsigned int)tindex, (unsigned int)tlabel,
			_table->s, 0, &contact);
}

/**
 *
 */
static int w_ts_append_to2(
		struct sip_msg *msg, char *idx, char *lbl, char *table, char *ruri)
{
	unsigned int tindex;
	unsigned int tlabel;
	str suri;

	/* we do not want to do append by particular location */
	str contact = STR_NULL;

	if(fixup_get_ivalue(msg, (gparam_p)idx, (int *)&tindex) < 0) {
		LM_ERR("cannot get transaction index\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)lbl, (int *)&tlabel) < 0) {
		LM_ERR("cannot get transaction label\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)ruri, &suri) != 0) {
		LM_ERR("failed to conert r-uri parameter\n");
		return -1;
	}
	if(ts_check_uri(&suri) < 0)
		return -1;

	/* we do not want to do append by particular location here */
	return ts_append_to(msg, tindex, tlabel, table, &suri, &contact);
}

/**
 *
 */
static int ki_ts_append_to_uri(
		sip_msg_t *_msg, int tindex, int tlabel, str *_table, str *_uri)
{
	/* we do not want to do append by particular location */
	str contact = STR_NULL;

	/* we do not want to do append by particular location here */
	return ts_append_to(_msg, (unsigned int)tindex, (unsigned int)tlabel,
			_table->s, _uri, &contact);
}

/**
 *
 */
static int w_ts_append_by_contact2(
		struct sip_msg *_msg, char *_table, char *_ruri)
{
	str ruri = STR_NULL;
	str ruri_fixed = STR_NULL;

	str contact = STR_NULL;
	str tmp_contact = STR_NULL;
	struct sip_uri curi;

	int rc;

	if(_ruri == NULL) {
		LM_ERR("invalid ruri parameter\n");
		return -1;
	}

	/* parse R-URI */
	if(fixup_get_svalue(_msg, (gparam_t *)_ruri, &ruri_fixed) != 0) {
		LM_ERR("failed to convert r-uri parameter\n");
		return -1;
	}

	if(ruri_fixed.s == NULL || ruri_fixed.len <= 0) {
		LM_ERR("invalid ruri parameter - empty or negative length\n");
		return -1;
	}

	if(pkg_str_dup(&ruri, &ruri_fixed) < 0) {
		LM_ERR("failed to copy r-uri parameter\n");
		return -1;
	}

	if(ts_check_uri(&ruri) < 0) {
		LM_ERR("failed to parse R-URI.\n");
		return -1;
	}

	/* parse Contact header */
	if((!_msg->contact && parse_headers(_msg, HDR_CONTACT_F, 0) != 0)
			|| !_msg->contact) {
		LM_WARN("missing contact header or the value is empty/malformed.\n");
		return -1;
	}
	if(_msg->contact) {
		if(parse_contact(_msg->contact) < 0) {
			LM_WARN("failed to parse Contact header.\n");
			return -1;
		}
		if(parse_uri(((struct contact_body *)_msg->contact->parsed)
							 ->contacts->uri.s,
				   ((struct contact_body *)_msg->contact->parsed)
						   ->contacts->uri.len,
				   &curi)
				!= 0) {
			if(ts_check_uri(&_msg->contact->body) < 0) { /* one more attempt */
				LM_WARN("failed to parse Contact header.\n");
				return -1;
			}
		}

		tmp_contact.len = ((struct contact_body *)_msg->contact->parsed)
								  ->contacts->uri.len;
		tmp_contact.s = (char *)pkg_malloc(tmp_contact.len + 1);
		if(tmp_contact.s == NULL) {
			PKG_MEM_ERROR;
			return -1;
		}
		memcpy(tmp_contact.s,
				((struct contact_body *)_msg->contact->parsed)->contacts->uri.s,
				tmp_contact.len);
		tmp_contact.s[tmp_contact.len] = '\0';

		if(pkg_str_dup(&contact, &tmp_contact) < 0) {
			if(pkg_str_dup(&contact, &_msg->contact->body)
					< 0) { /* one more attempt */
				LM_ERR("problems when calling ts_append_contact(), cannot copy "
					   "Contact parameter.\n");
				return -1;
			}
		}
	}

	/* contact must be of syntax: sip:<user>@<host>:<port> with no parameters list */
	rc = ts_append(_msg, &ruri, &contact, _table);

	/* free previously used memory */
	pkg_free(ruri.s);
	pkg_free(contact.s);
	pkg_free(tmp_contact.s);

	return rc;
}

/**
 *
 */
static int ki_ts_append_by_contact(sip_msg_t *_msg, str *_table, str *_ruri)
{
	str ruri = STR_NULL;
	str contact = STR_NULL;
	str tmp_contact = STR_NULL;
	struct sip_uri curi;
	int rc;

	/* parse R-URI */
	if(ts_check_uri(_ruri) < 0)
		return -1;
	if(pkg_str_dup(&ruri, _ruri) < 0)
		return -1;

	/* parse Contact header */
	if((!_msg->contact && parse_headers(_msg, HDR_CONTACT_F, 0) != 0)
			|| !_msg->contact)
		return -1;

	if(_msg->contact) {
		if(parse_contact(_msg->contact) < 0)
			return -1;
		if(parse_uri(((struct contact_body *)_msg->contact->parsed)
							 ->contacts->uri.s,
				   ((struct contact_body *)_msg->contact->parsed)
						   ->contacts->uri.len,
				   &curi)
				!= 0) {
			if(ts_check_uri(&_msg->contact->body) < 0) /* one more attempt */
				return -1;
		}

		tmp_contact.len = ((struct contact_body *)_msg->contact->parsed)
								  ->contacts->uri.len;
		tmp_contact.s = (char *)pkg_malloc(tmp_contact.len + 1);
		if(tmp_contact.s == NULL) {
			PKG_MEM_ERROR;
			return -1;
		}
		memcpy(tmp_contact.s,
				((struct contact_body *)_msg->contact->parsed)->contacts->uri.s,
				tmp_contact.len);
		tmp_contact.s[tmp_contact.len] = '\0';

		if(pkg_str_dup(&contact, &tmp_contact) < 0) {
			if(pkg_str_dup(&contact, &_msg->contact->body)
					< 0) /* one more attempt */
				return -1;
		}
	}

	/* contact must be of syntax: sip:<user>@<host>:<port> with no parameters list */
	rc = ts_append(_msg, &ruri, &contact, _table->s);

	pkg_free(ruri.s);
	pkg_free(contact.s);
	pkg_free(tmp_contact.s);

	return rc;
}

/**
 *
 */
static int w_ts_append_by_contact3(
		struct sip_msg *_msg, char *_table, char *_ruri, char *_contact)
{
	str ruri = STR_NULL;
	str ruri_fixed = STR_NULL;

	str contact = STR_NULL;
	str contact_fixed = STR_NULL;

	int rc;

	if(_ruri == NULL || _contact == NULL) {
		LM_ERR("invalid ruri or contact parameters\n");
		return -1;
	}
	/* parse R-URI */
	if(fixup_get_svalue(_msg, (gparam_t *)_ruri, &ruri_fixed) != 0) {
		LM_ERR("failed to convert r-uri parameter\n");
		return -1;
	}

	if(ruri_fixed.s == NULL || ruri_fixed.len <= 0) {
		LM_ERR("invalid ruri parameter - empty or negative length\n");
		return -1;
	}

	if(pkg_str_dup(&ruri, &ruri_fixed) < 0) {
		LM_ERR("failed to copy r-uri parameter\n");
		return -1;
	}

	if(ts_check_uri(&ruri) < 0) {
		LM_ERR("failed to parse R-URI.\n");
		return -1;
	}

	/* parse Contact header */
	if(fixup_get_svalue(_msg, (gparam_t *)_contact, &contact_fixed) != 0) {
		LM_ERR("failed to convert contact parameter\n");
		return -1;
	}

	if(contact_fixed.s == NULL || contact_fixed.len <= 0) {
		LM_ERR("invalid contact parameter value\n");
		return -1;
	}

	if(pkg_str_dup(&contact, &contact_fixed) < 0) {
		LM_ERR("failed to copy r-uri parameter\n");
		return -1;
	}

	if(ts_check_uri(&contact) < 0) {
		LM_ERR("failed to parse Contact parameter.\n");
		return -1;
	}

	/* contact must be of syntax: sip:<user>@<host>:<port> with no parameters list */
	rc = ts_append(_msg, &ruri, &contact, _table);

	pkg_free(ruri.s);
	pkg_free(contact.s);

	return rc;
}

/**
 *
 */
static int ki_ts_append_by_contact_uri(
		sip_msg_t *_msg, str *_table, str *_ruri, str *_contact)
{
	str ruri = STR_NULL;
	str contact = STR_NULL;

	int rc;

	/* parse R-URI */
	if(ts_check_uri(_ruri) < 0)
		return -1;
	if(pkg_str_dup(&ruri, _ruri) < 0)
		return -1;

	/* parse Contact header */
	if(ts_check_uri(_contact) < 0)
		return -1;
	if(pkg_str_dup(&contact, _contact) < 0)
		return -1;

	/* contact must be of syntax: sip:<user>@<host>:<port> with no parameters list */
	rc = ts_append(_msg, &ruri, &contact, _table->s);

	pkg_free(ruri.s);
	pkg_free(contact.s);

	return rc;
}

/**
 *
 */
static int w_ts_store(struct sip_msg *msg, char *p1, char *p2)
{
	return ts_store(msg, 0);
}

/**
 *
 */
static int ki_ts_store(sip_msg_t *msg)
{
	return ts_store(msg, 0);
}

/**
 *
 */
static int w_ts_store1(struct sip_msg *msg, char *_ruri, char *p2)
{
	str suri;

	if(fixup_get_svalue(msg, (gparam_t *)_ruri, &suri) != 0) {
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
	{ str_init("tsilo"), str_init("ts_append_by_contact"),
		SR_KEMIP_INT, ki_ts_append_by_contact,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tsilo"), str_init("ts_append_by_contact_uri"),
		SR_KEMIP_INT, ki_ts_append_by_contact_uri,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_tsilo_exports);
	return 0;
}

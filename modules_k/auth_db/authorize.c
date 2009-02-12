/*
 * $Id$
 *
 * Digest Authentication - Database support
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * history:
 * ---------
 * 2003-02-28 scratchpad compatibility abandoned
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2004-06-06 updated to the new DB api, added auth_db_{init,bind,close,ver}
 *             (andrei)
 * 2005-05-31 general definition of AVPs in credentials now accepted - ID AVP,
 *            STRING AVP, AVP aliases (bogdan)
 * 2006-03-01 pseudo variables support for domain name (bogdan)
 */


#include <string.h>
#include "../../ut.h"
#include "../../str.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../parser/hf.h"
#include "../../parser/parser_f.h"
#include "../../usr_avp.h"
#include "../../mod_fix.h"
#include "../../mem/mem.h"
#include "aaa_avps.h"
#include "authdb_mod.h"


static str auth_500_err = str_init("Server Internal Error");


static inline int get_ha1(struct username* _username, str* _domain,
			  const str* _table, char* _ha1, db_res_t** res)
{
	struct aaa_avp *cred;
	db_key_t keys[2];
	db_val_t vals[2];
	db_key_t *col;
	str result;

	int n, nc;

	col = pkg_malloc(sizeof(*col) * (credentials_n + 1));
	if (col == NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}

	keys[0] = &user_column;
	keys[1] = &domain_column;
	/* should we calculate the HA1, and is it calculated with domain? */
	col[0] = (_username->domain.len && !calc_ha1) ?
		(&pass_column_2) : (&pass_column);

	for (n = 0, cred=credentials; cred ; n++, cred=cred->next) {
		col[1 + n] = &cred->attr_name;
	}

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;

	VAL_STR(vals).s = _username->user.s;
	VAL_STR(vals).len = _username->user.len;

	if (_username->domain.len) {
		VAL_STR(vals + 1) = _username->domain;
	} else {
		VAL_STR(vals + 1) = *_domain;
	}

	n = (use_domain ? 2 : 1);
	nc = 1 + credentials_n;
	if (auth_dbf.use_table(auth_db_handle, _table) < 0) {
		LM_ERR("failed to use_table\n");
		pkg_free(col);
		return -1;
	}

	if (auth_dbf.query(auth_db_handle, keys, 0, vals, col, n, nc, 0, res) < 0) {
		LM_ERR("failed to query database\n");
		pkg_free(col);
		return -1;
	}
	pkg_free(col);

	if (RES_ROW_N(*res) == 0) {
		LM_DBG("no result for user \'%.*s@%.*s\'\n",
				_username->user.len, ZSW(_username->user.s),
			(use_domain ? (_domain->len) : 0), ZSW(_domain->s));
		return 1;
	}

	result.s = (char*)ROW_VALUES(RES_ROWS(*res))[0].val.string_val;
	result.len = strlen(result.s);

	if (calc_ha1) {
		/* Only plaintext passwords are stored in database,
		 * we have to calculate HA1 */
		auth_api.calc_HA1(HA_MD5, &_username->whole, _domain, &result,
				0, 0, _ha1);
		LM_DBG("HA1 string calculated: %s\n", _ha1);
	} else {
		memcpy(_ha1, result.s, result.len);
		_ha1[result.len] = '\0';
	}

	return 0;
}


/*
 * Generate AVPs from the database result
 */
static int generate_avps(db_res_t* result)
{
	struct aaa_avp *cred;
	int_str ivalue;
	int i;

	for (cred=credentials, i=1; cred; cred=cred->next, i++) {
		switch (result->col.types[i]) {
		case DB_STR:
			ivalue.s = VAL_STR(&(result->rows[0].values[i]));

			if (VAL_NULL(&(result->rows[0].values[i])) ||
			ivalue.s.s == NULL || ivalue.s.len==0)
				continue;

			if (add_avp(cred->avp_type|AVP_VAL_STR,cred->avp_name,ivalue)!=0){
				LM_ERR("failed to add AVP\n");
				return -1;
			}

			LM_DBG("set string AVP \"%s\"/%d = \"%.*s\"\n",
				(cred->avp_type&AVP_NAME_STR)?cred->avp_name.s.s:"",
				(cred->avp_type&AVP_NAME_STR)?0:cred->avp_name.n,
				ivalue.s.len, ZSW(ivalue.s.s));
			break;
		case DB_STRING:
			ivalue.s.s = (char*)VAL_STRING(&(result->rows[0].values[i]));

			if (VAL_NULL(&(result->rows[0].values[i])) ||
			ivalue.s.s == NULL || (ivalue.s.len=strlen(ivalue.s.s))==0 )
				continue;

			if (add_avp(cred->avp_type|AVP_VAL_STR,cred->avp_name,ivalue)!=0){
				LM_ERR("failed to add AVP\n");
				return -1;
			}

			LM_DBG("set string AVP \"%s\"/%d = \"%.*s\"\n",
				(cred->avp_type&AVP_NAME_STR)?cred->avp_name.s.s:"",
				(cred->avp_type&AVP_NAME_STR)?0:cred->avp_name.n,
				ivalue.s.len, ZSW(ivalue.s.s));
			break;
		case DB_INT:
			if (VAL_NULL(&(result->rows[0].values[i])))
				continue;

			ivalue.n = (int)VAL_INT(&(result->rows[0].values[i]));

			if (add_avp(cred->avp_type, cred->avp_name, ivalue)!=0) {
				LM_ERR("failed to add AVP\n");
				return -1;
			}

			LM_DBG("set int AVP \"%s\"/%d = %d\n",
				(cred->avp_type&AVP_NAME_STR)?cred->avp_name.s.s:"",
				(cred->avp_type&AVP_NAME_STR)?0:cred->avp_name.n,
				ivalue.n);
			break;
		default:
			LM_ERR("subscriber table column `%.*s' has unsuported type. "
				"Only string/str or int columns are supported by"
				"load_credentials.\n", result->col.names[i]->len, result->col.names[i]->s);
			break;
		}
	}

	return 0;
}


/*
 * Authorize digest credentials
 */
static inline int authorize(struct sip_msg* _m, gparam_p _realm,
									char* _table, hdr_types_t _hftype)
{
	char ha1[256];
	int res;
	struct hdr_field* h;
	auth_body_t* cred;
	auth_result_t ret;
	str domain, table;
	db_res_t* result = NULL;

	if(!_table) {
		LM_ERR("invalid table parameter\n");
		return -1;
	}

	table.s = _table;
	table.len = strlen(_table);

	if(fixup_get_svalue(_m, _realm, &domain)!=0)
	{
		LM_ERR("invalid realm parameter\n");
		return AUTH_ERROR;
	}

	if (domain.len==0)
		domain.s = 0;

	ret = auth_api.pre_auth(_m, &domain, _hftype, &h);

	if (ret != DO_AUTHORIZATION)
		return ret;

	cred = (auth_body_t*)h->parsed;

	res = get_ha1(&cred->digest.username, &domain, &table, ha1, &result);
	if (res < 0) {
		/* Error while accessing the database */
		if (slb.send_reply(_m, 500, &auth_500_err) == -1) {
			LM_ERR("failed to send 500 reply\n");
		}
		return ERROR;
	}
	if (res > 0) {
		/* Username not found in the database */
		auth_dbf.free_result(auth_db_handle, result);
		return USER_UNKNOWN;
	}

	/* Recalculate response, it must be same to authorize successfully */
	if (!auth_api.check_response(&(cred->digest),
				&_m->first_line.u.request.method, ha1)) {
		ret = auth_api.post_auth(_m, h);
		if (ret == AUTHORIZED)
			generate_avps(result);
		auth_dbf.free_result(auth_db_handle, result);
		return ret;
	}

	auth_dbf.free_result(auth_db_handle, result);
	return INVALID_PASSWORD;
}


/*
 * Authorize using Proxy-Authorize header field
 */
int proxy_authorize(struct sip_msg* _m, char* _realm, char* _table)
{
	return authorize(_m, (gparam_p)_realm, _table, HDR_PROXYAUTH_T);
}


/*
 * Authorize using WWW-Authorize header field
 */
int www_authorize(struct sip_msg* _m, char* _realm, char* _table)
{
	return authorize(_m, (gparam_p)_realm, _table, HDR_AUTHORIZATION_T);
}

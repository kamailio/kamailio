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
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../parser/hf.h"
#include "../../parser/parser_f.h"
#include "../../usr_avp.h"
#include "../../mod_fix.h"
#include "../../mem/mem.h"
#include "aaa_avps.h"
#include "api.h"
#include "authdb_mod.h"


static inline int get_ha1(struct username* _username, str* _domain,
			  const str* _table, char* _ha1, db1_res_t** res)
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

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB1_STR;
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
static int generate_avps(db1_res_t* result)
{
	struct aaa_avp *cred;
	int_str ivalue;
	int i;

	for (cred=credentials, i=1; cred; cred=cred->next, i++) {
		switch (result->col.types[i]) {
		case DB1_STR:
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
		case DB1_STRING:
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
		case DB1_INT:
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
				"load_credentials.\n", result->col.names[i]->len,
				result->col.names[i]->s);
			break;
		}
	}

	return 0;
}


/*
 * Authorize digest credentials
 */
static int digest_authenticate(struct sip_msg* msg, str *realm,
				str *table, hdr_types_t hftype)
{
	char ha1[256];
	int res;
	struct hdr_field* h;
	auth_body_t* cred;
	db1_res_t* result = NULL;
	int ret;

	cred = 0;
	ret = AUTH_ERROR;

	ret = auth_api.pre_auth(msg, realm, hftype, &h, NULL);
	switch(ret) {
		case NONCE_REUSED:
			LM_DBG("nonce reused");
			ret = AUTH_NONCE_REUSED;
			goto end;
		case STALE_NONCE:
			LM_DBG("stale nonce\n");
			ret = AUTH_STALE_NONCE;
			goto end;
		case NO_CREDENTIALS:
			LM_DBG("no credentials\n");
			ret = AUTH_NO_CREDENTIALS;
			goto end;
		case ERROR:
		case BAD_CREDENTIALS:
			LM_DBG("error or bad credentials\n");
			ret = AUTH_ERROR;
			goto end;
		case CREATE_CHALLENGE:
			LM_ERR("CREATE_CHALLENGE is not a valid state\n");
			ret = AUTH_ERROR;
			goto end;
		case DO_RESYNCHRONIZATION:
			LM_ERR("DO_RESYNCHRONIZATION is not a valid state\n");
			ret = AUTH_ERROR;
			goto end;
		case NOT_AUTHENTICATED:
			LM_DBG("not authenticated\n");
			ret = AUTH_ERROR;
			goto end;
		case DO_AUTHENTICATION:
			break;
		case AUTHENTICATED:
			ret = AUTH_OK;
			goto end;
	}

	cred = (auth_body_t*)h->parsed;

	res = get_ha1(&cred->digest.username, realm, table, ha1, &result);
	if (res < 0) {
		/* Error while accessing the database */
		ret = AUTH_ERROR;
		goto end;
	}
	if (res > 0) {
		/* Username not found in the database */
		ret = AUTH_USER_UNKNOWN;
		goto end;
	}

	/* Recalculate response, it must be same to authorize successfully */
	ret = auth_api.check_response(&(cred->digest),
				&msg->first_line.u.request.method, ha1);
	if(ret==AUTHENTICATED) {
		ret = AUTH_OK;
		switch(auth_api.post_auth(msg, h)) {
			case AUTHENTICATED:
				generate_avps(result);
				break;
			default:
				ret = AUTH_ERROR;
				break;
		}
	} else {
		if(ret==NOT_AUTHENTICATED)
			ret = AUTH_INVALID_PASSWORD;
		else
			ret = AUTH_ERROR;
	}

end:
	if(result)
		auth_dbf.free_result(auth_db_handle, result);
	return ret;
}


/*
 * Authenticate using Proxy-Authorize header field
 */
int proxy_authenticate(struct sip_msg* _m, char* _realm, char* _table)
{
	str srealm;
	str stable;

	if(_table==NULL) {
		LM_ERR("invalid table parameter\n");
		return AUTH_ERROR;
	}

	stable.s   = _table;
	stable.len = strlen(stable.s);

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0) {
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len==0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}
	LM_DBG("realm value [%.*s]\n", srealm.len, srealm.s);

	return digest_authenticate(_m, &srealm, &stable, HDR_PROXYAUTH_T);
}


/*
 * Authenticate using WWW-Authorize header field
 */
int www_authenticate(struct sip_msg* _m, char* _realm, char* _table)
{
	str srealm;
	str stable;

	if(_table==NULL) {
		LM_ERR("invalid table parameter\n");
		return AUTH_ERROR;
	}

	stable.s   = _table;
	stable.len = strlen(stable.s);

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0) {
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len==0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}
	LM_DBG("realm value [%.*s]\n", srealm.len, srealm.s);

	return digest_authenticate(_m, &srealm, &stable, HDR_AUTHORIZATION_T);
}

/**
 * @brief bind functions to AUTH_DB API structure
 */
int bind_auth_db(auth_db_api_t *api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->digest_authenticate = digest_authenticate;

	return 0;
}

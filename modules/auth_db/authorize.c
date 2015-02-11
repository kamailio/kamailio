/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <string.h>
#include "../../ut.h"
#include "../../str.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../parser/hf.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../usr_avp.h"
#include "../../mod_fix.h"
#include "../../mem/mem.h"
#include "api.h"
#include "auth_db_mod.h"
#include "authorize.h"


int fetch_credentials(sip_msg_t *msg, str *user, str* domain, str *table, int flags)
{
	pv_elem_t *cred;
	db_key_t keys[2];
	db_val_t vals[2];
	db_key_t *col;
	db1_res_t *res = NULL;

	int n, nc;

	if(flags&AUTH_DB_SUBS_SKIP_CREDENTIALS) {
		nc = 1;
	} else {
		nc = credentials_n;
	}
	col = pkg_malloc(sizeof(*col) * (nc+1));
	if (col == NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	col[0] = &user_column;

	keys[0] = &user_column;
	keys[1] = &domain_column;

	for (n = 0, cred=credentials; cred ; n++, cred=cred->next) {
		col[n] = &cred->text;
	}

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB1_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;

	n = 1;
	VAL_STR(vals) = *user;

	if (domain && domain->len) {
		VAL_STR(vals + 1) = *domain;
		n = 2;
	}

	if (auth_dbf.use_table(auth_db_handle, table) < 0) {
		LM_ERR("failed to use_table\n");
		pkg_free(col);
		return -1;
	}

	if (auth_dbf.query(auth_db_handle, keys, 0, vals, col, n, nc, 0, &res) < 0) {
		LM_ERR("failed to query database\n");
		pkg_free(col);
		if(res)
			auth_dbf.free_result(auth_db_handle, res);
		return -1;
	}
	pkg_free(col);
	if (RES_ROW_N(res) == 0) {
		if(res)
			auth_dbf.free_result(auth_db_handle, res);
		LM_DBG("no result for user \'%.*s%s%.*s\' in [%.*s]\n",
				user->len, user->s, (n==2)?"@":"",
				(n==2)?domain->len:0, (n==2)?domain->s:"",
				table->len, table->s);
		return -2;
	}
	if(flags&AUTH_DB_SUBS_SKIP_CREDENTIALS) {
		/* there is a result and flag to skip loading credentials is set */
		goto done;
	}
	for (cred=credentials, n=0; cred; cred=cred->next, n++) {
		if (db_val2pv_spec(msg, &RES_ROWS(res)[0].values[n], cred->spec) != 0) {
			if(res)
				auth_dbf.free_result(auth_db_handle, res);
			LM_ERR("Failed to convert value for column %.*s\n",
					RES_NAMES(res)[n]->len, RES_NAMES(res)[n]->s);
			return -3;
		}
	}

done:
	if(res)
		auth_dbf.free_result(auth_db_handle, res);
	return 0;
}

static inline int get_ha1(struct username* _username, str* _domain,
			  const str* _table, char* _ha1, db1_res_t** res)
{
	pv_elem_t *cred;
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
		col[1 + n] = &cred->text;
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
static int generate_avps(struct sip_msg* msg, db1_res_t* db_res)
{
	pv_elem_t *cred;
	int i;

	for (cred=credentials, i=1; cred; cred=cred->next, i++) {
		if (db_val2pv_spec(msg, &RES_ROWS(db_res)[0].values[i], cred->spec) != 0) {
			LM_ERR("Failed to convert value for column %.*s\n",
					RES_NAMES(db_res)[i]->len, RES_NAMES(db_res)[i]->s);
			return -1;
		}
	}
	return 0;
}


/*
 * Authorize digest credentials and set the pointer to used hdr
 */
static int digest_authenticate_hdr(sip_msg_t* msg, str *realm,
				str *table, hdr_types_t hftype, str *method, hdr_field_t **ahdr)
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
	if(ahdr!=NULL) *ahdr = h;

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
	ret = auth_api.check_response(&(cred->digest), method, ha1);
	if(ret==AUTHENTICATED) {
		ret = AUTH_OK;
		switch(auth_api.post_auth(msg, h)) {
			case AUTHENTICATED:
				generate_avps(msg, result);
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
 * Authorize digest credentials
 */
static int digest_authenticate(sip_msg_t* msg, str *realm,
				str *table, hdr_types_t hftype, str *method)
{
	return digest_authenticate_hdr(msg, realm, table, hftype, method, NULL);
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

	return digest_authenticate(_m, &srealm, &stable, HDR_PROXYAUTH_T,
					&_m->first_line.u.request.method);
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

	return digest_authenticate(_m, &srealm, &stable, HDR_AUTHORIZATION_T,
					&_m->first_line.u.request.method);
}

int www_authenticate2(struct sip_msg* _m, char* _realm, char* _table, char *_method)
{
	str srealm;
	str stable;
	str smethod;

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

	if (get_str_fparam(&smethod, _m, (fparam_t*)_method) < 0) {
		LM_ERR("failed to get method value\n");
		return AUTH_ERROR;
	}

	if (smethod.len==0)
	{
		LM_ERR("invalid method parameter - empty value\n");
		return AUTH_ERROR;
	}
	LM_DBG("method value [%.*s]\n", smethod.len, smethod.s);

	return digest_authenticate(_m, &srealm, &stable, HDR_AUTHORIZATION_T,
					&smethod);
}

/*
 * Authenticate using WWW/Proxy-Authorize header field
 */
int auth_check(struct sip_msg* _m, char* _realm, char* _table, char *_flags)
{
	str srealm;
	str stable;
	int iflags;
	int ret;
	hdr_field_t *hdr;
	sip_uri_t *uri = NULL;
	sip_uri_t *turi = NULL;
	sip_uri_t *furi = NULL;

	if ((_m->REQ_METHOD == METHOD_ACK) || (_m->REQ_METHOD == METHOD_CANCEL)) {
		return AUTH_OK;
	}

	if(_m==NULL || _realm==NULL || _table==NULL || _flags==NULL) {
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0) {
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len==0) {
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&stable, _m, (fparam_t*)_table) < 0) {
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (stable.len==0) {
		LM_ERR("invalid table parameter - empty value\n");
		return AUTH_ERROR;
	}

	if(fixup_get_ivalue(_m, (gparam_p)_flags, &iflags)!=0)
	{
		LM_ERR("invalid flags parameter\n");
		return -1;
	}

	LM_DBG("realm [%.*s] table [%.*s] flags [%d]\n", srealm.len, srealm.s,
			stable.len,  stable.s, iflags);

	hdr = NULL;
	if(_m->REQ_METHOD==METHOD_REGISTER)
		ret = digest_authenticate_hdr(_m, &srealm, &stable, HDR_AUTHORIZATION_T,
						&_m->first_line.u.request.method, &hdr);
	else
		ret = digest_authenticate_hdr(_m, &srealm, &stable, HDR_PROXYAUTH_T,
						&_m->first_line.u.request.method, &hdr);

	if(ret==AUTH_OK && hdr!=NULL && (iflags&AUTH_CHECK_ID_F)) {
		srealm = ((auth_body_t*)(hdr->parsed))->digest.username.user;
			
		if((furi=parse_from_uri(_m))==NULL)
			return AUTH_ERROR;
		
		if(_m->REQ_METHOD==METHOD_REGISTER || _m->REQ_METHOD==METHOD_PUBLISH) {
			if((turi=parse_to_uri(_m))==NULL)
				return AUTH_ERROR;
			uri = turi;
		} else {
			uri = furi;
		}
		if(!((iflags&AUTH_CHECK_SKIPFWD_F)
				&& (_m->REQ_METHOD==METHOD_INVITE || _m->REQ_METHOD==METHOD_BYE
					|| _m->REQ_METHOD==METHOD_PRACK || _m->REQ_METHOD==METHOD_UPDATE
					|| _m->REQ_METHOD==METHOD_MESSAGE))) {
			if(srealm.len!=uri->user.len
						|| strncmp(srealm.s, uri->user.s, srealm.len)!=0)
				return AUTH_USER_MISMATCH;
		}

		if(_m->REQ_METHOD==METHOD_REGISTER || _m->REQ_METHOD==METHOD_PUBLISH) {
			/* check from==to */
			if(furi->user.len!=turi->user.len
					|| strncmp(furi->user.s, turi->user.s, furi->user.len)!=0)
				return AUTH_USER_MISMATCH;
			if(use_domain!=0 && (furi->host.len!=turi->host.len
					|| strncmp(furi->host.s, turi->host.s, furi->host.len)!=0))
				return AUTH_USER_MISMATCH;
			/* check r-uri==from for publish */
			if(_m->REQ_METHOD==METHOD_PUBLISH) {
				if(parse_sip_msg_uri(_m)<0)
					return AUTH_ERROR;
				uri = &_m->parsed_uri;
				if(furi->user.len!=uri->user.len
						|| strncmp(furi->user.s, uri->user.s, furi->user.len)!=0)
					return AUTH_USER_MISMATCH;
				if(use_domain!=0 && (furi->host.len!=uri->host.len
						|| strncmp(furi->host.s, uri->host.s, furi->host.len)!=0))
					return AUTH_USER_MISMATCH;
				}
		}
		return AUTH_OK;
	}

	return ret;
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

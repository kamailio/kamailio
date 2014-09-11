/*
 * $Id$
 *
 * Digest Authentication - Database support
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * history:
 * ---------
 * 2003-02-28 scratchpad compatibility abandoned
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2004-06-06 updated to the new DB api, added auth_db_{init,bind,close,ver}
 *             (andrei)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../ut.h"
#include "../../str.h"
#include "../../lib/srdb2/db.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../parser/hf.h"
#include "../../parser/parser_f.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../../config.h"
#include "../../id.h"
#include "../../sr_module.h"
#include "../../modules/auth/api.h"
#include "uid_auth_db_mod.h"


#define IS_NULL(f)	((f).flags & DB_NULL)

static inline int get_ha1(struct username* username, str* did, str* realm,
			  authdb_table_info_t *table_info, char* ha1, db_res_t** res, db_rec_t** row)
{
	str result;
	db_cmd_t *q = NULL;
   
	if (calc_ha1) {
		q = table_info->query_password;
		DBG("querying plain password\n");
	} else {
		if (username->domain.len) {
			q = table_info->query_pass2;
			DBG("querying ha1b\n");
		} else {
			q = table_info->query_pass;
			DBG("querying ha1\n");
		}
	}
    
	q->match[0].v.lstr = username->user;
	q->match[1].v.lstr = *realm;

	if (use_did) q->match[2].v.lstr = *did;

	if (db_exec(res, q) < 0 ) {
		ERR("Error while querying database\n");
		return -1;
	}

	if (*res) *row = db_first(*res);
	else *row = NULL;
	while (*row) {
		if (IS_NULL((*row)->fld[0]) || IS_NULL((*row)->fld[1])) {
			LOG(L_ERR, "auth_db:get_ha1: Credentials for '%.*s'@'%.*s' contain NULL value, skipping\n",
				username->user.len, ZSW(username->user.s), realm->len, ZSW(realm->s));
		} else {
			if ((*row)->fld[1].v.int4 & SRDB_DISABLED) {
				/* disabled rows ignored */
			} else {
				if ((*row)->fld[1].v.int4 & SRDB_LOAD_SER) {
					/* *row = i; */
					break;
				}
			}
		}
		*row = db_next(*res);
	}

	if (!*row) {
		DBG("auth_db:get_ha1: Credentials for '%.*s'@'%.*s' not found\n",
			username->user.len, ZSW(username->user.s), realm->len, ZSW(realm->s));
		return 1;
	}		

	result.s = (*row)->fld[0].v.cstr;
	result.len = strlen(result.s);

	if (calc_ha1) {
		/* Only plaintext passwords are stored in database,
		 * we have to calculate HA1 */
		auth_api.calc_HA1(HA_MD5, &username->whole, realm, &result, 0, 0, ha1);
		DBG("auth_db:get_ha1: HA1 string calculated: %s\n", ha1);
	} else {
		memcpy(ha1, result.s, result.len);
		ha1[result.len] = '\0';
	}

	return 0;
}

/*
 * Calculate the response and compare with the given response string
 * Authorization is successful if this two strings are same
 */
static inline int check_response(dig_cred_t* cred, str* method, char* ha1)
{
	HASHHEX resp, hent;
    
	/*
	 * First, we have to verify that the response received has
	 * the same length as responses created by us
	 */
	if (cred->response.len != 32) {
		DBG("auth_db:check_response: Receive response len != 32\n");
		return 1;
	}
    
	/*
	 * Now, calculate our response from parameters received
	 * from the user agent
	 */
	auth_api.calc_response(ha1, &(cred->nonce), 
				  &(cred->nc), &(cred->cnonce), 
				  &(cred->qop.qop_str), cred->qop.qop_parsed == QOP_AUTHINT,
				  method, &(cred->uri), hent, resp);
    
	DBG("auth_db:check_response: Our result = \'%s\'\n", resp);
    
	/*
	 * And simply compare the strings, the user is
	 * authorized if they match
	 */
	if (!memcmp(resp, cred->response.s, 32)) {
		DBG("auth_db:check_response: Authorization is OK\n");
		return 0;
	} else {
		DBG("auth_db:check_response: Authorization failed\n");
		return 2;
	}
}


/*
 * Generate AVPs from the database result
 */
static int generate_avps(db_res_t* result, db_rec_t *row)
{
	int i;
	int_str iname, ivalue;
	str value;
	char buf[32];
    
	for (i = 2; i < credentials_n + 2; i++) {
		value = row->fld[i].v.lstr;

		if (IS_NULL(row->fld[i]))
			continue;

		switch (row->fld[i].type) {
		case DB_STR:
			value = row->fld[i].v.lstr;
			break;

		case DB_INT:
			value.len = sprintf(buf, "%d", row->fld[i].v.int4);
			value.s = buf;
			break;

		default:
			abort();
			break;
		}

		if (value.s == NULL)
			continue;

		iname.s = credentials[i - 2];
		ivalue.s = value;

		if (add_avp(AVP_NAME_STR | AVP_VAL_STR | AVP_CLASS_USER, iname, ivalue) < 0) {
			LOG(L_ERR, "auth_db:generate_avps: Error while creating AVPs\n");
			return -1;
		}

		DBG("auth_db:generate_avps: set string AVP \'%.*s = %.*s\'\n",
			iname.s.len, ZSW(iname.s.s), value.len, ZSW(value.s));
	}
    
	return 0;
}

/* this is a dirty work around to check the credentials of all users,
 * if the database query returned more then one result
 *
 * Fills res (which must be db_free'd afterwards if the call was succesfull)
 * returns  0 on success, 1 on no match (?)
 *          and -1 on error (memory, db a.s.o).
 * WARNING: if -1 is returned res _must_ _not_ be freed (it's empty)
 *
 */
static inline int check_all_ha1(struct sip_msg* msg, struct hdr_field* hdr, 
		dig_cred_t* dig, str* method, str* did, str* realm, 
		authdb_table_info_t *table_info, db_res_t** res) 
{
	char ha1[256];
	db_rec_t *row;
	str result;
	db_cmd_t *q;
   
	if (calc_ha1) {
		q = table_info->query_password;
		DBG("querying plain password\n");
	}
	else {
	    if (dig->username.domain.len) {
			q = table_info->query_pass2;
			DBG("querying ha1b\n");
		}
		else {
			q = table_info->query_pass;
			DBG("querying ha1\n");
		}
	}
    
	q->match[0].v.lstr = dig->username.user;
	if (dig->username.domain.len) 
		q->match[1].v.lstr = dig->username.domain;
	else
		q->match[1].v.lstr = *realm;

	if (use_did) q->match[2].v.lstr = *did;

	if (db_exec(res, q) < 0 ) {
		ERR("Error while querying database\n");
	}

	if (*res) row = db_first(*res);
	else row = NULL;
	while (row) {
		if (IS_NULL(row->fld[0]) || IS_NULL(row->fld[1])) {
			LOG(L_ERR, "auth_db:check_all_ha1: Credentials for '%.*s'@'%.*s' contain NULL value, skipping\n",
			    dig->username.user.len, ZSW(dig->username.user.s), realm->len, ZSW(realm->s));
		}
		else {
			if (row->fld[1].v.int4 & SRDB_DISABLED) {
				/* disabled rows ignored */
			}
			else {
				if (row->fld[1].v.int4 & SRDB_LOAD_SER) {
					result.s = row->fld[0].v.cstr;
					result.len = strlen(result.s);
					if (calc_ha1) {
						 /* Only plaintext passwords are stored in database,
						  * we have to calculate HA1 */
						auth_api.calc_HA1(HA_MD5, &(dig->username.whole), realm, &result, 0, 0, ha1);
						DBG("auth_db:check_all_ha1: HA1 string calculated: %s\n", ha1);
					} else {
						memcpy(ha1, result.s, result.len);
						ha1[result.len] = '\0';
					}

					if (!check_response(dig, method, ha1)) {
						if (auth_api.post_auth(msg, hdr) == AUTHENTICATED) {
							generate_avps(*res, row);
							return 0;
						}
					}
				}
			}
		}
		row = db_next(*res);
	}

	if (!row) {
		DBG("auth_db:check_all_ha1: Credentials for '%.*s'@'%.*s' not found",
		    dig->username.user.len, ZSW(dig->username.user.s), realm->len, ZSW(realm->s));
	}		
	return 1;


}


/*
 * Authenticate digest credentials
 * Returns:
 *      -3 -- Bad Request
 *      -2 -- Error while checking credentials (such as malformed message or database problem)
 *      -1 -- Authentication failed
 *       1 -- Authentication successful
 */
static inline int authenticate(struct sip_msg* msg, str* realm, authdb_table_info_t *table, hdr_types_t hftype)
{
	char ha1[256];
	int res, ret;
	db_rec_t *row;
	struct hdr_field* h;
	auth_body_t* cred;
	db_res_t* result;
	str did;
    
	cred = 0;
	result = 0;
	ret = -1;
    
	switch(auth_api.pre_auth(msg, realm, hftype, &h, NULL)) {
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
		ret = -3;
		goto end;
	case CREATE_CHALLENGE:
		ERR("auth_db:authenticate: CREATE_CHALLENGE is not a valid state\n");
		ret = -2;
		goto end;
	case DO_RESYNCHRONIZATION:
		ERR("auth_db:authenticate: DO_RESYNCHRONIZATION is not a valid state\n");
		ret = -2;
		goto end;
		
	case NOT_AUTHENTICATED: 
		ret = -1;
		goto end;
		
	case DO_AUTHENTICATION: 
		break;
		
	case AUTHENTICATED:
		ret = 1; 
		goto end;
	}
    
	cred = (auth_body_t*)h->parsed;
	
	if (use_did) {
		if (msg->REQ_METHOD == METHOD_REGISTER) {
			ret = get_to_did(&did, msg);
		} else {
			ret = get_from_did(&did, msg);
		}
		if (ret == 0) {
			did.s = DEFAULT_DID;
			did.len = sizeof(DEFAULT_DID) - 1;
		}
	} else {
		did.len = 0;
		did.s = 0;
	}	
    

	if (check_all) {
		res = check_all_ha1(msg, h, &(cred->digest), &msg->first_line.u.request.method, &did, realm, table, &result);
		if (res < 0) {
			ret = -2;
			goto end;
		}
		else if (res > 0) {
			ret = -1;
			goto end;
		}
		else {
			ret = 1;
			goto end;
		}
    	} else {
		res = get_ha1(&cred->digest.username, &did, realm, table, ha1, &result, &row);
		if (res < 0) {
			ret = -2;
			goto end;
		}
		if (res > 0) {
			/* Username not found in the database */
			ret = -1;
			goto end;
        	}
    	}
    
	/* Recalculate response, it must be same to authorize successfully */
	if (!check_response(&(cred->digest), &msg->first_line.u.request.method, ha1)) {
		switch(auth_api.post_auth(msg, h)) {
		case ERROR:
		case BAD_CREDENTIALS:
			ret = -2; 
			break;
			
		case NOT_AUTHENTICATED: 
			ret = -1; 
			break;
			
		case AUTHENTICATED:
			generate_avps(result, row);
			ret = 1;
			break;
			
		default:
			ret = -1;
			break;
		}
	} else {
		ret = -1;
	}

 end:
	if (result) db_res_free(result);
	if (ret < 0) {
		if (auth_api.build_challenge(msg, (cred ? cred->stale : 0), realm, NULL, NULL, hftype) < 0) {
			ERR("Error while creating challenge\n");
			ret = -2;
		}
	}
	return ret;
}


/*
 * Authenticate using Proxy-Authorize header field
 */
int proxy_authenticate(struct sip_msg* msg, char* p1, char* p2)
{
	str realm;

	if (get_str_fparam(&realm, msg, (fparam_t*)p1) < 0) {
		ERR("Cannot obtain digest realm from parameter '%s'\n", ((fparam_t*)p1)->orig);
		return -1;
	}

	return authenticate(msg, &realm, (authdb_table_info_t*)p2, HDR_PROXYAUTH_T);
}


/*
 * Authorize using WWW-Authorize header field
 */
int www_authenticate(struct sip_msg* msg, char* p1, char* p2)
{
	str realm;

	if (get_str_fparam(&realm, msg, (fparam_t*)p1) < 0) {
		ERR("Cannot obtain digest realm from parameter '%s'\n", ((fparam_t*)p1)->orig);
		return -1;
	}

	return authenticate(msg, &realm, (authdb_table_info_t*)p2, HDR_AUTHORIZATION_T);
}

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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * history:
 * ---------
 * 2003-02-28 scratchpad compatibility abandoned
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2004-06-06 updated to the new DB api, added auth_db_{init,bind,close,ver}
 *             (andrei)
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
#include "../../mem/mem.h"
#include "../../config.h"
#include "../../id.h"
#include "authdb_mod.h"
#include "rfc2617.h"


static inline int get_ha1(struct username* username, str* did, str* realm,
			  str* table, char* ha1, db_res_t** res, int* row)
{
    db_key_t keys[3];
    db_val_t vals[3];
    db_val_t* val;
    db_key_t* col;
    str result;
    int n, nc, i;
    char* t = 0;
    
    val = 0; /* Fixes gcc warning */
    col = pkg_malloc(sizeof(*col) * (credentials_n + 2));
    if (col == NULL) {
	LOG(L_ERR, "auth_db:get_ha1: Error while allocating memory\n");
	return -1;
    }
    
    keys[0] = username_column.s;
    keys[1] = realm_column.s;
    keys[2] = did_column.s;
    col[0] = (username->domain.len && !calc_ha1) ? (pass_column_2.s) : (pass_column.s);
    col[1] = flags_column.s;
    
    for (n = 0; n < credentials_n; n++) {
	col[2 + n] = credentials[n].s;
    }
    
    vals[0].type = vals[1].type = DB_STR;
    vals[0].nul = vals[1].nul = 0;
    vals[0].val.str_val = username->user;
    vals[1].val.str_val = *realm;
    
    if (use_did) {
	vals[2].type = DB_STR;
	vals[2].nul = 0;
	vals[2].val.str_val = *did;
	n = 3;
    } else { 
	n = 2;
    }

    nc = 2 + credentials_n;
    t = as_asciiz(table);
    if (auth_dbf.use_table(auth_db_handle, t) < 0) {
	LOG(L_ERR, "auth_db:get_ha1: Error in use_table\n");
	if (t) pkg_free(t);
	pkg_free(col);
	return -1;
    }
    if (t) pkg_free(t);
    
    if (auth_dbf.query(auth_db_handle, keys, 0, vals, col, n, nc, 0, res) < 0) {
	LOG(L_ERR, "auth_db:get_ha1: Error while querying database\n");
	pkg_free(col);
	return -1;
    }
    pkg_free(col);
    
    for(i = 0; i < (*res)->n; i++) {
	val = ((*res)->rows[i].values);
	
	if (val[0].nul || val[1].nul) {
	    LOG(L_ERR, "auth_db:get_ha1: Credentials for '%.*s'@'%.*s' contain NULL value, skipping\n",
		username->user.len, ZSW(username->user.s), realm->len, ZSW(realm->s));
	    continue;
	}
	
	if (val[1].val.int_val & DB_DISABLED) continue;
	if (val[1].val.int_val & DB_LOAD_SER) {
	    *row = i;
	    break;
	}
    }
    
    if (i == (*res)->n) {
	DBG("auth_db:get_ha1: Credentials for '%.*s'@'%.*s' not found\n",
	    username->user.len, ZSW(username->user.s), realm->len, ZSW(realm->s));
	return 1;
    }		
    
    result.s = (char*)val[0].val.string_val;
    result.len = strlen(result.s);
    
    if (calc_ha1) {
	     /* Only plaintext passwords are stored in database,
	      * we have to calculate HA1 */
	calc_HA1(HA_MD5, &username->whole, realm, &result, 0, 0, ha1);
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
    calc_response(ha1, &(cred->nonce), 
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
static int generate_avps(db_res_t* result, unsigned int row)
{
    int i;
    int_str iname, ivalue;
    str value;
    
    for (i = 2; i < credentials_n + 2; i++) {
	value.s = (char*)VAL_STRING(&(result->rows[row].values[i]));
	
	if (VAL_NULL(&(result->rows[row].values[i]))
	    || value.s == NULL) {
	    continue;
	}
	
	iname.s = credentials[i - 2];
	value.len = strlen(value.s);
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


/*
 * Authenticate digest credentials
 * Returns:
 *      -3 -- Bad Request
 *      -2 -- Error while checking credentials (such as malformed message or database problem)
 *      -1 -- Authentication failed
 *       1 -- Authentication successful
 */
static inline int authenticate(struct sip_msg* msg, str* realm, str* table, hdr_types_t hftype)
{
    char ha1[256];
    int res, row, ret;
    struct hdr_field* h;
    auth_body_t* cred;
    db_res_t* result;
    str did;
    
    cred = 0;
    result = 0;
    ret = -1;
    
    switch(auth_api.pre_auth(msg, realm, hftype, &h)) {
    case ERROR:
    case BAD_CREDENTIALS:
	ret = -3;
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
    if (result) auth_dbf.free_result(auth_db_handle, result);
    if (ret < 0) {
	if (auth_api.build_challenge(msg, (cred ? cred->stale : 0), realm, hftype) < 0) {
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
    str realm, table;

    if (get_str_fparam(&realm, msg, (fparam_t*)p1) < 0) {
	ERR("Cannot obtain digest realm from parameter '%s'\n", ((fparam_t*)p1)->orig);
	return -1;
    }

    if (get_str_fparam(&table, msg, (fparam_t*)p2) < 0) {
	ERR("Cannot obtain table name from parameter '%s'\n", ((fparam_t*)p2)->orig);
	return -1;
    }
    return authenticate(msg, &realm, &table, HDR_PROXYAUTH_T);
}


/*
 * Authorize using WWW-Authorize header field
 */
int www_authenticate(struct sip_msg* msg, char* p1, char* p2)
{
    str realm, table;

    if (get_str_fparam(&realm, msg, (fparam_t*)p1) < 0) {
	ERR("Cannot obtain digest realm from parameter '%s'\n", ((fparam_t*)p1)->orig);
	return -1;
    }

    if (get_str_fparam(&table, msg, (fparam_t*)p2) < 0) {
	ERR("Cannot obtain table name from parameter '%s'\n", ((fparam_t*)p2)->orig);
	return -1;
    }
    return authenticate(msg, &realm, &table, HDR_AUTHORIZATION_T);
}

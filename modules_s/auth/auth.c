/*
 * $Id$
 */

#include "auth.h"
#include "utils.h"
#include "defs.h"
#include "../../forward.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../data_lump_rpl.h"
#include "cred.h"
#include "../../db/db.h"
#include "calc.h"
#include "../../md5global.h"
#include "../../md5.h"
#include "../../md5utils.h"
#include <stdio.h>
#include "../../mem/mem.h"
#include <string.h>
#include "auth_mod.h"
#include "checks.h"
#include "group.h"
#include "nonce.h"
#include "../../ut.h"
#include "../../parser/hf.h" /* HDR_AUTHORIZATION & HDR_PROXYAUTH */


/*
 * Temporary buffer
 */
static char auth_hf[AUTH_HF_LEN];


/*
 * Database connection handle
 */
extern db_con_t* db_handle;


auth_state_t state;


extern int (*sl_reply)(struct sip_msg* _m, char* _str1, char* _str2);


/*
 * Create {WWW,Proxy}-Authenticate header field
 */
static inline void build_auth_hf(char* _realm, char* _buf, int* _len, int _qop, char* _hf_name)
{
	char nonce[NONCE_LEN + 1];

	calc_nonce(nonce, time(NULL) + nonce_expire, state.nonce_retries, &secret);
	nonce[NONCE_LEN] = '\0';

	*_len = snprintf(_buf, AUTH_HF_LEN,
			 "%s: Digest realm=\"%s\", nonce=\"%s\"%s%s"
			 #ifdef PRINT_MD5
			 ", algorithm=MD5"
			 #endif
			 "\r\n", 
			 _hf_name, 
			 _realm, 
			 nonce,
			 (_qop) ? (", qop=\"auth\"") : (""),
			 (state.stale) ? (", stale=true") : ("")
			 );		

	DBG("build_auth_hf(): %s\n", _buf);
}


/*
 * Create a response with given code and reason phrase
 * Optionaly add new headers specified in _hdr
 */
/* FIXME: navratova hodnota */
static int send_resp(struct sip_msg* _m, int _code, char* _reason, char* _hdr, int _hdr_len)
{
	struct lump_rpl* ptr;

	/* Add new headers if there are any */
	if (_hdr) {
		ptr = build_lump_rpl(_hdr, _hdr_len);
		add_lump_rpl(_m, ptr);
	}

	sl_reply(_m, (char*)_code, _reason);
	return 1;
}


static int find_auth_hf(struct sip_msg* _msg, char* _realm, cred_t* _c, int _hf_name)
{
	struct hdr_field* ptr = 0;
	int res;
#ifdef PARANOID
	if ((!_msg) || (!_realm) || (!_c)) {
		LOG(L_ERR, "find_auth_hf(): Invalid parameter value\n");
		return -1;
	}
#endif
	switch(_hf_name) {
	case HDR_AUTHORIZATION: ptr = _msg->authorization; break;
	case HDR_PROXYAUTH: ptr = _msg->proxy_auth; break;
	}
	
	while(ptr) {
		     /* Skip other header field types */
		if (ptr->type == _hf_name) {
			res = hf2cred(ptr, _c); 
			if (res == -1) {
				LOG(L_ERR, "find_auth_hf(): Error while parsing credentials\n");
				return -1;
			}
			     /* Malformed digest field */
			if (res == -2) {
				if (send_resp(_msg, 400, MESSAGE_400, NULL, 0) == -1) {
					LOG(L_ERR, "find_auth_hf(): Error while sending 400 reply\n");
				}
				return -1;
			}
			     /* We only support digest scheme */
			if (_c->scheme == SCHEME_DIGEST) {
				     /* Check the realm */
				if (!memcmp(_realm, _c->realm.s, _c->realm.len)) {
					return 0;
				}
			}
		}
		
		ptr = ptr->next;
	}
	
	return -1;
}


static int get_ha1(char* _user, char* _realm, char* _table, char* _ha1)
{
	db_key_t keys[] = {user_column, realm_column};
	db_val_t vals[] = {{DB_STRING, 0, {.string_val = _user}},
			   {DB_STRING, 0, {.string_val = _realm}}
	};
	db_key_t col[] = {pass_column};
	db_res_t* res;

	const char* pass;

#ifdef USER_DOMAIN_HACK
	char* at;
	char buffer[256];
#endif


#ifdef PARANOID
	if ((!_realm) || (!_user) || (!_ha1) || (!_table)) {
		LOG(L_ERR, "get_ha1(): Invalid parameter value\n");
		return -1;
	}
#endif

#ifdef USER_DOMAIN_HACK
	at = memchr(_user, '@', strlen(_user));
	if (at) {
		DBG("get_ha1(): @ found in username, removing domain part\n");
		memcpy(buffer, _user, at - _user);
		buffer[at - _user] = '\0';
		VAL_STRING(vals) = buffer;
		if (!calc_ha1) {
			col[0] = pass_column_2;
		}
	}
#endif

	db_use_table(db_handle, _table);
	if (db_query(db_handle, keys, vals, col, 2, 1, NULL, &res) == FALSE) {
		LOG(L_ERR, "get_ha1(): Error while querying database\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		DBG("get_ha1(): no result\n");
		db_free_query(db_handle, res);
		return -1;
	}
        pass = ROW_VALUES(RES_ROWS(res))[0].val.string_val;

	if (calc_ha1) {
		     /* Only plaintext passwords are stored in database,
		      * we have to calculate HA1 */
		DigestCalcHA1("md5", _user, _realm, pass, NULL, NULL, _ha1);
		DBG("HA1 string calculated: %s\n", _ha1);
	} else {
		memcpy(_ha1, pass, strlen(pass) + 1);
	}

	db_free_query(db_handle, res);
	return 0;
}



int check_response(cred_t* _cred, str* _method, char* _ha1)
{
	HASHHEX resp;
	HASHHEX hent;
	char* qop;

#ifdef PARANOID
	if ((!_cred) || (!_ha1) || (!_method)) {
		LOG(L_ERR, "check_cred(): Invalid parameter value\n");
		return -1;
	}
#endif
	
	     /* We always send algorithm=MD5 and qop=auth, so we can
	      * hardwire these values here, if th client uses a
	      * different algorithm and qop, it will simply not
	      * authorize him
	      */
	/*	DigestCalcHA1("md5", _cred->username.s, _cred->realm.s, _a1, _cred->nonce.s, _cred->cnonce.s, HA1); */

        switch(_cred->qop) {
	case QOP_AUTH:
		qop = "auth";
		break;
	case QOP_AUTH_INT:
		qop = "auth-int";
		break;
	default:
		qop = "";
		break;
	}
		
	DigestCalcResponse(_ha1, _cred->nonce.s, _cred->nonce_count.s, _cred->cnonce.s, qop, _method->s,
			   _cred->uri.s, hent, resp);

	DBG("check_cred(): Our result = %s\n", resp);

	if (!memcmp(resp, _cred->response.s, 32)) {
		DBG("check_cred(): Authorization is OK\n");
		return 1;
	} else {
		DBG("check_cred(): Authorization failed\n");
		return -1;
	}
}


static inline int authorize(struct sip_msg* _msg, char* _realm, char* _table, int _hf_name)
{
	char ha1[256];
	int res;
	     /* We will have to parse the whole message header
	      * since there may be multible authorization header
	      * fields
	      */
	init_cred(&(state.cred));
	state.stale = 0;
	state.nonce_retries = 0;

#ifdef ACK_CANCEL_HACK
	/* the method is parsed; -jiri
	if (!memcmp(_msg->first_line.u.request.method.s, "ACK", 3)) { */
	if (_msg->REQ_METHOD==METHOD_ACK) {
	        return 1;
	}
#endif

	if (parse_headers(_msg, HDR_EOH) == -1) {
		LOG(L_ERR, "authorize(): Error while parsing message header\n");
		return -1;
	}

	     /* Finds the first occurence of authorization header for the
	      * given realm
	      */
	if (find_auth_hf(_msg, _realm, &(state.cred), _hf_name) == -1) {
		DBG("authorize(): Authorization HF not found or malformed\n");
		return -1;
	}

	     /*
	      * FIXME: For debugging purposes only
	      */
	print_cred(&(state.cred));

	/* FIXME: 400s should be sent from routing scripts, but we will need
	 * variables for that
	 */
	if (validate_cred(&(state.cred)) == -1) {
		if (send_resp(_msg, 400, MESSAGE_400, NULL, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
		}
		return 0;
	}

	if (!check_nonce(state.cred.nonce.s, &secret)) {
		LOG(L_ERR, "authorize(): Invalid nonce value returned, very suspicious\n");
		return -1;
	}
	state.nonce_retries = get_nonce_retry(state.cred.nonce.s);


        if (get_ha1(state.cred.username.s, _realm, _table, ha1) == -1) {
		LOG(L_ERR, "authorize(): Error while obtaining HA1 string for user \"%s\"\n", state.cred.username.s);
		return -1;
	}
		
        res = check_response(&(state.cred), &_msg->first_line.u.request.method, ha1);

	if (res == 1) {  /* response was OK */
		if (nonce_is_stale(state.cred.nonce.s)) {
			/* method is parsed; -Jiri 
			if (!(memcmp(_msg->first_line.u.request.method.s, "ACK", 3)) &&
			    (memcmp(_msg->first_line.u.request.method.s, "CANCEL", 6))) { */
			if (_msg->REQ_METHOD==METHOD_ACK || _msg->REQ_METHOD==METHOD_CANCEL ) {
				     /* Method is ACK or CANCEL, we must accept stale
				      * nonces because there is no way how to challenge
				      * with new nonce (ACK and CANCEL have no responses
				      * associated)
				      */
				return 1;
			} else {
				DBG("authorize(): Response is OK, but nonce is stale\n");
				state.stale = 1;
				return -1;
			}
		} else {
			DBG("authorize(): Authorization OK\n");
			return 1;
		}
	} else {
		DBG("authorize(): Response is different\n");
		return -1;
	}
}



static inline int challenge(struct sip_msg* _msg, char* _realm, int _qop, 
			    int _code, char* _message, char* _challenge_msg)
{
	int auth_hf_len;

	if (state.nonce_retries > retry_count) {
			DBG("challenge(): Retry count exceeded, sending Forbidden\n");
			_code = 403;
			_message = MESSAGE_403;
	} else {
		if (!state.stale) {
			state.nonce_retries++;
		} else {
			state.nonce_retries = 0;
		}

		build_auth_hf(_realm, auth_hf, &auth_hf_len, _qop, _challenge_msg);
	}

	if (send_resp(_msg, _code, _message, auth_hf, auth_hf_len) == -1) {
		LOG(L_ERR, "www_challenge(): Error while sending response\n");
		return -1;
	}
	return 0;

}


/*
 * Challenge a user to send credentials using WWW-Authorize header field
 */
int www_challenge(struct sip_msg* _msg, char* _realm, char* _qop)
{
	return challenge(_msg, _realm, (int)_qop, 401, MESSAGE_401, WWW_AUTH_CHALLENGE);
}


/*
 * Challenge a user to send credentials using Proxy-Authorize header field
 */
int proxy_challenge(struct sip_msg* _msg, char* _realm, char* _qop)
{
	return challenge(_msg, _realm, (int)_qop, 407, MESSAGE_407, PROXY_AUTH_CHALLENGE);
}


int proxy_authorize(struct sip_msg* _msg, char* _realm, char* _table)
{
	return authorize(_msg, _realm, _table, HDR_PROXYAUTH);
}


int www_authorize(struct sip_msg* _msg, char* _realm, char* _table)
{
	return authorize(_msg, _realm, _table, HDR_AUTHORIZATION);
}

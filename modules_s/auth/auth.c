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
#include "db.h"
#include "calc.h"
#include "../../md5global.h"
#include "../../md5.h"
#include "../../md5utils.h"
#include <stdio.h>
#include "../../mem/mem.h"
#include <string.h>

static char auth_hf[AUTH_HF_LEN];


extern db_con_t* db_handle;

static cred_t cred;

extern int (*sl_reply)(struct sip_msg* _m, char* _str1, char* _str2);

static void get_username(str* _s);


static void to_hex(char* _dst, char *_src, int _src_len)
{
	unsigned short i;
	unsigned char j;
    
	for (i = 0; i < _src_len; i++) {
		j = (_src[i] >> 4) & 0xf;
		if (j <= 9)
			_dst[i*2] = (j + '0');
		else
			_dst[i*2] = (j + 'a' - 10);
		j = _src[i] & 0xf;
		if (j <= 9)
			_dst[i*2+1] = (j + '0');
		else
			_dst[i*2+1] = (j + 'a' - 10);
	}
}


/*
 * Calculate nonce value
 * Nonce value consists of time in seconds since 1.1 1970 and
 * secret phrase
 */
static inline void calc_nonce(char* _realm, char* _nonce)
{
	MD5_CTX ctx;
	time_t t;
	char bin[16];
	
	t = time(NULL) / 60;
	to_hex(_nonce, (char*)&t, 8);
	
	MD5Init(&ctx);
	MD5Update(&ctx, _nonce, 8);
	MD5Update(&ctx, ":", 1);
	MD5Update(&ctx, NONCE_SECRET, NONCE_SECRET_LEN);
	MD5Final(bin, &ctx);
	CvtHex(bin, _nonce + 8);

	DBG("calc_nonce(): nonce=%s\n", _nonce);
}


/*
 * Create Proxy-Authenticate header field
 */
static inline void build_proxy_auth_hf(char* _realm, char* _buf, int* _len)
{
	     /* 8 hex chars time value + 32 hex char MD5 + '\0' */
	char nonce[41];
	calc_nonce(_realm, nonce);
	nonce[40] = '\0';

	     /* Currently we support qop=auth only */

	     /*
	      * FIXME: We prefer not to use qop since some clients claim
	      *        to support that but they don't
	      */
	*_len = snprintf(_buf, AUTH_HF_LEN,
			 "Proxy-Authenticate: Digest realm=\"%s\", nonce=\"%s\",algorithm=MD5\r\n", 
			 _realm, nonce);
	DBG("build_proxy_auth_hf(): %s\n", _buf);
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



/*
 * Challenge a user to send credentials
 */
int challenge(struct sip_msg* _msg, char* _realm, char* _str2)
{
	int auth_hf_len;

	build_proxy_auth_hf(_realm, auth_hf, &auth_hf_len);
	if (send_resp(_msg, 407, MESSAGE_407, auth_hf, auth_hf_len) == -1) {
		LOG(L_ERR, "challenge(): Error while sending response\n");
		return -1;
	}
	return 0;
}


static int find_auth_hf(struct sip_msg* _msg, char* _realm, cred_t* _c)
{
	struct hdr_field* ptr;
	int res;
#ifdef PARANOID
	if ((!_msg) || (!_realm) || (!_c)) {
		LOG(L_ERR, "find_auth_hf(): Invalid parameter value\n");
		return -1;
	}
#endif

	ptr = _msg->headers;
	while(ptr) {
		if (!strcasecmp(AUTH_RESPONSE, ptr->name.s)) {
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


static int get_ha1(str* _user, char* _realm, char* _ha1)
{
	db_key_t keys[] = {SUBS_USER_COL, SUBS_REALM_COL};
	db_val_t vals[] = {{DB_STRING, 0, {.string_val = _user->s}},
			   {DB_STRING, 0, {.string_val = _realm}}
	};
	db_key_t col[] = {SUBS_HA1_COL};
	db_res_t* res;

	const char* ha1;

#ifdef PARANOID
	if ((!_realm) || (!_user) || (!_ha1)) {
		LOG(L_ERR, "get_ha1(): Invalid parameter value\n");
		return -1;
	}
#endif
	
	db_use_table(db_handle, DB_TABLE);
	if (db_query(db_handle, keys, vals, col, 2, 1, NULL, &res) == FALSE) {
		LOG(L_ERR, "get_ha1(): Error while querying database\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		DBG("get_ha1(): no result\n");
		db_free_query(db_handle, res);
		return -1;
	}
        ha1 = ROW_VALUES(RES_ROWS(res))[0].val.string_val;
	memcpy(_ha1, ha1, strlen(ha1) + 1);

	db_free_query(db_handle, res);
	return 0;
}



int check_cred(cred_t* _cred, str* _method, char* _ha1)
{
	HASHHEX resp;
	HASHHEX hent;
	char* qop;
	char nonce[33];


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
		
	calc_nonce(_cred->realm.s, nonce);
	DigestCalcResponse(_ha1, nonce, _cred->nonce_count.s, _cred->cnonce.s, qop, _method->s,
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



int authorize(struct sip_msg* _msg, char* _realm, char* str2)
{
	char ha1[256];
	int res;
	     /* We will have to parse the whole message header
	      * since there may be multible authorization header
	      * fields
	      */
	init_cred(&cred);

	if (!memcmp(_msg->first_line.u.request.method.s, "ACK", 3)) {
	        return 1;
	}

	if (parse_headers(_msg, HDR_EOH) == -1) {
		LOG(L_ERR, "authorize(): Error while parsing message header\n");
		return -1;
	}

	     /* Finds the first occurence of authorization header for the
	      * given realm
	      */
	if (find_auth_hf(_msg, _realm, &cred) == -1) {
		DBG("authorize(): Proxy-Authorization HF not found or malformed\n");
		return -1;
	}
	DBG("authorize(): Proxy-Authorization HF with the given realm found\n");

	     /*
	      * FIXME: For debugging purposes only
	      */
	print_cred(&cred);

	if (get_ha1(&cred.username, _realm, ha1) == -1) {
		LOG(L_ERR, "authorize(): Error while getting A1 string for user %s\n", cred.username.s);
		return -1;
	}
	
	if (validate_cred(&cred) == -1) {
		if (send_resp(_msg, 400, MESSAGE_400, NULL, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
		}
		return 0;
	}
	
        res = check_cred(&cred, &_msg->first_line.u.request.method, ha1);
	if (res == -1) {
		LOG(L_ERR, "authorize(): Error while checking credentials\n");
		return -1;
	} else return res;
}



/*
 * Check if the given username matches username in credentials
 */
int is_user(struct sip_msg* _msg, char* _user, char* _str2)
{
	if (!cred.username.len) {
		DBG("is_user(): Username not found in credentials\n");
		return -1;
	}
	if (!memcmp(_user, cred.username.s, cred.username.len)) {
		DBG("is_user(): Username matches\n");
		return 1;
	} else {
		DBG("is_user(): Username differs\n");
		return -1;
	}
}



/*
 * Check if the user specified in credentials is a member
 * of given group
 */
int is_in_group(struct sip_msg* _msg, char* _group, char* _str2)
{
	db_key_t keys[] = {GRP_USER, GRP_GRP};
	db_val_t vals[] = {{DB_STRING, 0, {.string_val = cred.username.s}},
			   {DB_STRING, 0, {.string_val = _group}}
	};
	db_key_t col[] = {GRP_GRP};
	db_res_t* res;

	db_use_table(db_handle, GRP_TABLE);
	if (db_query(db_handle, keys, vals, col, 2, 1, NULL, &res) == FALSE) {
		LOG(L_ERR, "is_in_group(): Error while querying database\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		DBG("is_in_group(): User %s is not in group %s\n", cred.username.s, _group);
		db_free_query(db_handle, res);
		return -1;
	} else {
		DBG("is_in_group(): User %s is member of group %s\n", cred.username.s, _group);
		db_free_query(db_handle, res);
		return 1;
	}
}



static void get_username(str* _s)
{
	char* at, *dcolon, *dc;
	dcolon = find_not_quoted(_s->s, ':');

	if (!dcolon) {
		_s->len = 0;
		return;
	}
	_s->s = dcolon + 1;

	at = strchr(_s->s, '@');
	dc = strchr(_s->s, ':');
	if (at) {
		if ((dc) && (dc < at)) {
			_s->len = dc - dcolon - 1;
			return;
		}
		
		_s->len = at - dcolon - 1;
		/*	_s->s[_s->len] = '\0'; */
	} else {
		_s->len = 0;
	} 
	return;
}



int check_to(struct sip_msg* _msg, char* _str1, char* _str2)
{
	str user;

	if (!_msg->to) {
		LOG(L_ERR, "check_to(): To HF not found\n");
		return -1;
	}

	user.s = _msg->to->body.s;
	user.len = _msg->to->body.len;

	get_username(&user);

	if (!user.len) return -1;

	/* FIXME !! */
	if (!strncasecmp(user.s, cred.username.s,
			 (user.len < cred.username.len) ? (cred.username.len) : (user.len))) {
		DBG("check_to(): auth id and To username are equal\n");
		return 1;
	} else {
		DBG("check_to(): auth id and To username differ\n");
		return 0;
	}
}


int check_from(struct sip_msg* _msg, char* _str1, char* _str2)
{
	str user;

	if (!_msg->from) {
		LOG(L_ERR, "check_from(): From HF not found\n");
		return -1;
	}

	user.s = _msg->from->body.s;
	user.len = _msg->from->body.len;

	get_username(&user);

	if (!user.len) return -1;

	/* FIXME !! */
	if (!strncasecmp(user.s, cred.username.s,
			(user.len < cred.username.len) ? (cred.username.len) : (user.len))) {
		DBG("check_from(): auth id and From username are equal\n");
		return 1;
	} else {
		DBG("check_from(): auth id and From username differ\n");
		return -1;
	}
}

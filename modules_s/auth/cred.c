#include "cred.h"
#include "defs.h"
#include "../../dprint.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#define DIGEST   "Digest"

#define USERNAME  "username"
#define REALM     "realm"
#define NONCE     "nonce"
#define URI       "uri"
#define RESPONSE  "response"
#define CNONCE    "cnonce"
#define OPAQUE    "opaque"
#define QOP       "qop"
#define NC        "nc"
#define ALGORITHM "algorithm"

#define USERNAME_ID   1
#define REALM_ID      2
#define NONCE_ID      3
#define URI_ID        4
#define RESPONSE_ID   5
#define CNONCE_ID     6
#define OPAQUE_ID     7
#define QOP_ID        8
#define NC_ID         9
#define ALGORITHM_ID 10
#define UNKNOWN_ID  256


/*
 * Unquote a string
 */
static void unquote(str* _s)
{
	if (*(_s->s + _s->len - 1) == '\"') {
		*(_s->s + _s->len - 1) = '\0';
		_s->len--;
	}
	if (*(_s->s) == '\"') {
		_s->s++;
		_s->len--;
	}
}
	


/*
 * Unquote selected fields in structure cred
 * It is neccessary for digest calculation
 */
static int unquote_cred(cred_t* _c)
{
#ifdef PARANOID
	if (!_c) return -1;
#endif
	if (_c->username.len) unquote(&(_c->username));
	if (_c->realm.len) unquote(&(_c->realm));
	if (_c->nonce.len) unquote(&(_c->nonce));
	if (_c->uri.len) unquote(&(_c->uri));
	if (_c->response.len) unquote(&(_c->response));
	if (_c->cnonce.len) unquote(&(_c->cnonce));
	return 0;
}


/* 
 * Fixme, should be automaton, but for now..
 */
static int name_parser(char* _s)
{
#ifdef PARANOID
	if (!_s) {
		LOG(L_ERR, "name_parser(): Invalid parameter value\n");
		return -1;
	}
#endif
	
	if (!strcasecmp(_s, USERNAME)) return USERNAME_ID;
	else if (!strcasecmp(_s, REALM)) return REALM_ID;
	else if (!strcasecmp(_s, NONCE)) return NONCE_ID;
	else if (!strcasecmp(_s, URI)) return URI_ID;
	else if (!strcasecmp(_s, RESPONSE)) return RESPONSE_ID;
	else if (!strcasecmp(_s, CNONCE)) return CNONCE_ID;
	else if (!strcasecmp(_s, OPAQUE)) return OPAQUE_ID;
	else if (!strcasecmp(_s, QOP)) return QOP_ID;
	else if (!strcasecmp(_s, NC)) return NC_ID;
	else if (!strcasecmp(_s, ALGORITHM)) return ALGORITHM_ID;
	else return UNKNOWN_ID;
}


static int parse_digest_param(char* _s, cred_t* _c)
{
	char* ptr;
	char* name, *body;
	int id, body_len;
#ifdef PARANOID
	if ((!_s) || (!_c)) {
		LOG(L_ERR, "parse_digest_param(): Invalid parameter value\n");
		return -1;
	}
#endif
	ptr = find_not_quoted(_s, '=');
	if (!ptr){
		LOG(L_ERR, "parse_digest_param(): Malformed digest parameter, = missing\n");
		return -1;
	} else {
		*ptr = '\0';
	}
	name = trim_trailing(_s);
	body = trim(ptr + 1);
	body_len = strlen(body);

	     /* FIXME */
	while ((*(body + body_len - 1) == '\r') || (*(body + body_len - 1) == '\n') || (*(body + body_len -1) == ' ')) body_len--;
	*(body + body_len) = '\0';	

	id = name_parser(_s);

	switch(id) {
	case USERNAME_ID:
		_c->username.s = body;
		_c->username.len = body_len;
		break;

	case REALM_ID:
		_c->realm.s = body;
		_c->realm.len = body_len;
		break;

	case NONCE_ID:
		_c->nonce.s = body;
		_c->nonce.len = body_len;
		break;

	case URI_ID:
		_c->uri.s = body;
		_c->uri.len = body_len;
		break;

	case RESPONSE_ID:
		_c->response.s = body;
		_c->response.len = body_len;
		break;
		
	case CNONCE_ID:
		_c->cnonce.s = body;
		_c->cnonce.len = body_len;
		break;

	case OPAQUE_ID:
		_c->opaque.s = body;
		_c->opaque.len = body_len;
		break;
		
	case QOP_ID:
		if ((!strcasecmp(body, "auth")) || (!strcasecmp(body, "\"auth\""))) {
			_c->qop = QOP_AUTH;
		} else if ((!strcasecmp(body, "auth-int")) || (!strcasecmp(body, "\"auth-int\""))) {
			_c->qop = QOP_AUTH_INT;
		} else {
			_c->qop = QOP_UNKNOWN;
		}
		break;
		
	case NC_ID:
		_c->nonce_count.s = body;
		_c->nonce_count.len = body_len;
		break;

	case ALGORITHM_ID:
		if (!strcasecmp(body, "MD5")) {
			_c->algorithm = ALG_MD5;
		} else if (!strcasecmp(body, "\"MD5\"")) {  /* FIXME: MS Messenger workaround */
			_c->algorithm = ALG_MD5;
		} else {
			_c->algorithm = ALG_UNKNOWN;
		}
		break;

	case UNKNOWN_ID:
		DBG("parse_digest_param(): Unknown parameter: %s\n", name);
		break;
	}	
	return 0;
}


static int parse_digest_params(char* _s, cred_t* _c)
{
	char* ptr;
#ifdef PARANOID
	if ((!_s) || (!_c)) {
		LOG(L_ERR, "parse_digest_params(): Invalid parameter value\n");
		return -1;
	}
#endif
	do {
		ptr = find_not_quoted(_s, ',');
		if (ptr) {
			*ptr = '\0';
		}
		
		if (parse_digest_param(_s, _c) == -1) {
			LOG(L_ERR, "parse_digest_params(): Error while parsing parameter\n");
			return -1;
		}
		if (ptr) {
			_s = eat_lws(ptr + 1);
		}
	} while(ptr);

	unquote_cred(_c);
	
	return 0;
}


int hf2cred(struct hdr_field* _hf, cred_t* _c)
{
	char* s, *ptr;
#ifdef PARANOID
	if ((!_hf) || (!_c)) {
		LOG(L_ERR, "hf2cred(): Invalid parameter value\n");
		return -1;
	}
#endif
	s = trim(_hf->body.s);
	ptr = find_not_quoted(s, ' ');

	if (!ptr) {
		LOG(L_ERR, "hf2cred(): Invalid Proxy-Authorize field\n");
		return -2;
	}
	
	*ptr = '\0';
	if (!strcasecmp(s, DIGEST)) {
		_c->scheme = SCHEME_DIGEST;
		s = eat_lws(ptr + 1);

		if (parse_digest_params(s, _c) == -1) {
			LOG(L_ERR, "hf2cred(): Error while parsing digest parameters\n");
			return -2;
		}
		
		return 0;
	} else {
		_c->scheme = SCHEME_UNKNOWN;
		return 0;
	}
}



int print_cred(cred_t* _c)
{
#ifdef PARANOID
	if (!_c) {
		LOG(L_ERR, "print_cred(): Invalid parameter value\n");
		return -1;
	}
#endif

	printf("===Credentials:\n");

	switch(_c->scheme) {
	case SCHEME_DIGEST:
		printf("   scheme=digest\n");
		break;
	case SCHEME_UNKNOWN:
		printf("   scheme=unknown\n");
		break;
	default:
		printf("   scheme=%d\n", _c->scheme);
		break;
	}

	printf("   username=%s\n", _c->username.s);
	printf("   realm=%s\n", _c->realm.s);
	printf("   nonce=%s\n", _c->nonce.s);
	printf("   uri=%s\n", _c->uri.s);
	printf("   response=%s\n", _c->response.s);

	switch(_c->algorithm) {
	case ALG_MD5:
		printf("   algorithm=MD5\n");
		break;
	case ALG_MD5_SESS:
		printf("   algoritm=MD5-Session\n");
		break;
	case ALG_UNKNOWN:
		printf("   algorithm=unknown\n");
		break;
	default:
		printf("   algorithm=%d\n", _c->algorithm);
		break;
	}

	printf("   cnonce=%s\n", _c->cnonce.s);
	printf("   opaque=%s\n", _c->opaque.s);

	switch(_c->qop) {
	case QOP_AUTH:
		printf("   qop=auth\n");
		break;
	case QOP_AUTH_INT:
		printf("   qop=auth-int\n");
		break;
	case QOP_UNKNOWN:
		printf("   qop=unknown\n");
		break;
	default:
		printf("   qop=%d\n", _c->qop);
		break;
	}

	printf("   nonce_count=%s\n", _c->nonce_count);
	printf("===\n");
	return 0;
}


int init_cred(cred_t* _c)
{
#ifdef PARANOID
	if (!_c) {
		LOG(L_ERR, "init_cred(): Invalid parameter value\n");
		return -1;
	}
#endif
	_c->scheme = SCHEME_UNKNOWN;

	_c->username.s = "";
	_c->username.len = 0;

	_c->realm.s = "";
	_c->realm.len = 0;

	_c->nonce.s = "";
	_c->nonce.len = 0;

	_c->uri.s = "";
	_c->uri.len = 0;

	_c->response.s = "";
	_c->response.len = 0;

	_c->algorithm = ALG_UNSPECIFIED;

	_c->cnonce.s = "";
	_c->cnonce.len = 0;

	_c->opaque.s = "";
	_c->opaque.len = 0;

	_c->qop = QOP_UNDEFINED;

	_c->nonce_count.s = "";
	_c->nonce_count.len = 0;
	return 0;
}



int validate_cred(cred_t* _c)
{
#ifdef PARANOID
	if (!_c) {
		LOG(L_ERR, "validate_cred(): Invalid parameter value\n");
		return -1;
	}
#endif
	
	if (_c->scheme != SCHEME_DIGEST) {
		LOG(L_ERR, "validate_cred(): Unknown scheme\n");
		return -1;
	}

	if (_c->algorithm != ALG_MD5) {
		LOG(L_ERR, "validate_cred(): Unsupported algorithm\n");
		return -1;
	}

	if (_c->username.len == 0) {
		LOG(L_ERR, "validate_cred(): Invalid username\n");
		return -1;
	}

	if (_c->realm.len == 0) {
		LOG(L_ERR, "validate_cred(): Invalid realm\n");
		return -1;
	}

	if (_c->nonce.len == 0) {
		LOG(L_ERR, "validate_cred(): Invalid nonce\n");
		return -1;
	}
		
	if (_c->uri.len == 0) {
		LOG(L_ERR, "validate_cred(): Invalid uri\n");
		return -1;
	}

	if (_c->response.len == 0) {
		LOG(L_ERR, "validate_cred(): Invalid response\n");
		return -1;
	}


	if (_c->qop == QOP_AUTH) {
		if (_c->cnonce.len == 0) {
			LOG(L_ERR, "validate_cred(): Invalid cnonce\n");
			return -1;
		}

		if (_c->nonce_count.len == 0) {
			LOG(L_ERR, "validate_cred(): Invalid nonce_count\n");
			return -1;
		}
	} else if (_c->qop != QOP_UNDEFINED) {
		LOG(L_ERR, "validate_cred(): Unsupported qop value\n");
		return -1;
	}

	return 0;
}

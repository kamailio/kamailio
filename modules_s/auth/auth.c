/* 
 * $Id$ 
 */



#include "auth.h"
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "defs.h"
#include <string.h>
#include "base64.h"

#define SCHEME_BASIC_STR  "basic"
#define SCHEME_DIGEST_STR "digest"
#define BASIC_SCHEME_DELIM ':'
#define BASIC_TABLE "basic"

static int           authenticate_f      (struct sip_msg* _msg, char* _str1, char* _str2);
static int           challenge_f         (struct sip_msg* _msg, char* _str1, char* _str2);
static void          destroy             (void);
static int           get_auth            (struct sip_msg* _msg);
static auth_t        parse_auth          (struct hdr_field* _field);
static inline char*  parse_scheme        (char* _s, auth_t* _au);
static inline int    parse_basic_params  (char* _s, auth_t* _au);
static inline int    parse_digest_params (char* _s, auth_t* _au);
static int           check_auth          (auth_t* _a);
static int           check_basic_auth    (auth_t* _a);
static int           check_digest_auth   (auth_t* _a);
static char*         decode_base64       (char* _s);
static char*         get_password        (const char* _table, char* _username);

static struct module_exports auth_exports = {"auth", 
					     (char*[]) { 
						     "authenticate",
						     "challenge"
					     },
					     (cmd_function[]) {
						     authenticate_f, 
						     challenge_f
					     },
					     (int[]) { 
						     1,
						     1
					     },
					     (fixup_function[]) {
						     NULL, 
						     NULL
					     },
					     3,
					     0,       /* response function */
					     destroy  /* destroy function */
};


struct module_exports* mod_register()
{
	LOG(L_ERR, "%s - registering...\n", auth_exports.name);
	return &auth_exports;
}


static int authenticate_f(struct sip_msg* _msg, char* _str1, char* _str2)
{
	struct hdr_field* h;
	auth_t* a;
	int res;

#ifdef PARANOID
	if (!_msg) return -1;
	if (!_str1) return -1;
#endif
	h = get_auth(_msg);

	if (h) {            /* Authorization found, check it */
		h = remove_crlf(h);
		a = parse_auth(h);
		if (!a) {
			LOG(L_ERR, "authenticate_f(): Error while parsing auth. header field\n");
			pkg_free(h);
			return -1;
		}
		res = check_auth(a);

		pkg_free(h);
		return res;
	} else {
		pkg_free(h);
		return 0;   /* Authorization not found */
	}
}


static int challenge_f(struct sip_msg* _msg, char* _str1, char* _str2)
{
#ifdef PARANOID
	if (!_msg) return -1;
	if (!_str1) return -1;
#endif

	return 1;
}


static void destroy(void)
{
}


/*
 * Returned value is dynamically allocated, use pkg_free
 * to destroy it properly
 */
static struct hdr_field* get_auth(struct sip_msg* _msg)
{
	struct hdr_field* result, *last;
#ifdef PARANOID
	if (!_msg) return NULL
#endif
	last = NULL;
	while(1) {
		if (parse_headers(_msg, HDR_OTHER) == -1) {
			LOG(L_ERR, "get_auth(): Error while parsing headers\n");
			return NULL;
		} else {
			if (last == _msg->last_header) return NULL;
			if (!memcmp(_msg->last_header.name.s, AUTH_RESPONSE)) {
				return duplicate_hf(_msg->last_header);
			} else {
				last = _msg->last_header;
				continue;
			}
		}
	}
}


static auth_t* parse_auth(struct hdr_field* _field)
{
	char* ptr;
	auth_t* result;
	int len, off;

#ifdef PARANOID
	if (!_field) retun NULL;
#endif
	result = pkg_malloc(sizeof(auth_t));
	if (!result) {
		LOG(L_ERR, "parse_auth(): No memory left\n");
		return NULL;
	}

	ptr = eat_lws(_field->body.s);
	ptr = parse_scheme(ptr, result);
	ptr = eat_lws(ptr);        /* FIXME: LWS tady neni, je odstraneno uz pri kopirovani celeho hf */

	if (result->scheme == DIGEST_SCHEME) {
		if (parse_digest_parameters(ptr, result) == FALSE) {
			LOG(L_ERR, "parse_auth(): Error while parsing parameters\n");
			pkg_free(result);
			return NULL;
		} else {
			return result;
		}
	} else if (result->scheme == BASIC_SCHEME) {
		ptr = decode_base64(ptr);
		if (parse_basic_parameters(ptr, result) == FALSE) {
			LOG(L_ERR, "parse_auth(): Error while parsing parameters\n");
			pkg_free(result);
			return NULL;
		} else {
			return result;
		}
	} else {
		LOG(L_ERR, "parse_auth(): Unsupported scheme\n");
		pkg_free(result);
		return NULL;
	}
}


static inline char* parse_scheme(char* _s, auth_t* _au)
{
	char* ptr;
#ifdef PARANOID
	if (!_s) return NULL;
	if (!_au) return _s;
#endif
	ptr = find_lws(_s); /* FIXME: Mame jenom najit mezeru nebo tabelator */
	if (!ptr) {
		LOG(L_ERR, "parse_scheme(): Error while parsing scheme\n");
		return _s;
	}
	
	*ptr++ = '\0';

	if (!strncasecmp(_s, SCHEME_BASIC_STR)) {
		_au->scheme = BASIC_SCHEME;
	} else if (!strncasecmp(_s, SCHEME_DIGEST_STR)) {
		_au->scheme = DIGEST_SCHEME;
	} else {
		_au->scheme = UNKNOWN_SCHEME;
	}
	
	return ptr;
}


static int check_auth(auth_t* _a)
{
#ifdef PARANOID
	if (!_a) return 0;
#endif
	switch(_a->scheme) {
	case BASIC_SCHEME:  return check_basic_auth(_a);
	case DIGEST_SCHEME: return check_digest_auth(_a);
	default:
		LOG(L_ERR, "check_auth(): Unsupported authorization scheme\n");
		return 0;
	}
}


static int check_basic_auth(auth_t* _a)
{
	char* pass;
#ifdef PARANOID
	if (!_a) return 0;
	if (_a->scheme != BASIC_SCHEME) return 0;
#endif
	
	pass = get_password(BASIC_TABLE, _a->username.s);
	if (!strcmp(pass, _a->password.s) {
		return 1;
	} else {
		return 0;
	}
}



static int check_digest_auth(auth_t* _a)
{
#ifdef PARANOID
	if (!_a) return 0;
	if (_a->scheme != DIGEST_SCHEME) return 0;
#endif

}


/*
 * Decode a base64 encoded string
 */
static char* decode_base64(char* _s)
{
	char* buf;
	int len;
#ifdef PARANOID
	if (!_s) return NULL;
#endif
	_s = trim(_s);   /* Remove any leading and trailing white chars */
	len = strlen(s);
	buf = pkg_malloc(len);
	if (!buf) {
		LOG(L_ERR, "decode_base64(): No memory left\n");
		return NULL;
	}
	len = base64decode(_s, len, buf, len);
	if (!len) {
		LOG(L_ERR, "decode_base64(): Error while decoding base64 string\n");
		pkg_free(buf);
		return NULL;
	}
	memcpy(_s, buf, len);
	_s[len] = '\0';
	return _s;
}


static inline char* parse_basic_params(char* _s, auth_t* _au)
{
	char* delim;
#ifdef PARANOID
	if (!_s) return NULL;
	if (!_au) return NULL;
#endif	
	user->next = passwd->next = NULL;
					       
	_s = trim(_s);

	delim = strchr(_s, BASIC_SCHEME_DELIM);
	if (!delim) {
		LOG(L_ERR, "parse_basic_params(): Error while parsing parameters\n");
		return _s;
	}
	*delim = '\0';

	_au->username.s = _s;
	_au->username.len = delim - _s;

	_au->password.s = delim + 1;
	_au->password.len = strlen(delim + 1);

	return _s;
}


static inline char* parse_digest_params(char* _s, auth_t* _au)
{
	char* ptr, *next_param, *body;
	struct hdr_field hf;

#ifdef PARANOID
	if (!_s) return NULL;
	if (!_au) return NULL;
#endif
	do {
		hf = pkg_malloc(sizeof(struct hdr_field));
		if (!hf) {
			LOG(L_ERR, "parse_params(): No memory left\n");
			return _s;
		}

		body = strchr(ptr, '=');
		if (!body) {
			LOG(L_ERR, "parse_params(): Error while parsing parameters\n");
			pkg_free(hf);
			return _s;
		}
		*body++ = '\0';

		hf->name.s = trim(_s);
		hf->name.len = strlen(hf->name.s);
		
		next_param = find_not_quoted(body, ',');

		if (next_param) {
			*next_param++ = '\0';
		}

		hf->body.s = trim(body);
		hf->body.len = strlen(hf->body.s);

		ADD_PARAM(_au, hf);

		if (next_param) {
			_s = eat_lws(next_param);
		}
	} while(next_param);

	return ptr;
}


char* get_password(const char* _table, char* _username)
{
#ifdef PARANOID
	if (!_table) return NULL;
	if (!_username) return NULL;
#endif
}

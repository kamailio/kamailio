/* 
 * $Id$ 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "contact_parser.h"
#include "utils.h"
#include "const.h"
#include "log.h"

/* Character delimiting particular contacts in Contact HF */
#define CONTACT_DELIM ','

/* Character delimiting particular parameters in a contact field */
#define PARAM_DELIM   ';'


/*
 * Declarations
 */
static inline char* find_next_contact (char* _b);
static inline char* find_next_param   (char* _b);
static inline void  parse_param       (char* _b, char** _name, char** _body);
static inline void  parse_params      (char* _b, time_t* _exp, float* _q);


/*
 * Returns pointer to the beginning of next
 * contact separated by a colon
 *
 * PARAMS : char* _b : input buffer
 * RETURNS: char*    : character found, NULL if not found
 */
static inline char* find_next_contact(char* _b)
{
	char* ptr = find_not_quoted(_b, CONTACT_DELIM);
	if (ptr) {
		return (ptr + 1);
	} else {
		return NULL;
	}
}


/*
 * Returns pointer to the beginning of next parameter
 * NULL if there is no parameter
 *
 * PARAMS : char* _b : input buffer
 * RETURNS: char*    : character found, NULL if not found
 */
static inline char* find_next_param(char* _b)
{
	char* ptr = find_not_quoted(_b, PARAM_DELIM);
	if (ptr) {
		return (ptr + 1);
	} else {
		return NULL;
	}
}


static inline void parse_param(char* _b, char** _name, char** _body)
{
	*_name = _b;

	while (*_b) {
		if (*_b == '=') {
			*_b++ = '\0';
			break;
		}
		_b++;
	}

	*_name = trim(*_name);
        *_body = trim(_b);
}


static inline void parse_params(char* _b, time_t* _exp, float* _q)
{
	char* p, *end = NULL, *name, *body;

#ifdef PARANOID
	if ((!_exp) || (!_q)) {
		ERR("Invalid parameter value");
	        return;
	}
#endif

	*_exp = (time_t)(-1);
	*_q = (float)(0);

	if (!_b) return;

	p = find_next_param(_b);
	if (!p) p = _b;
	p = trim(p);

	do {
		if (p) end = find_next_param(p);
		if (end) *(end - 1) = '\0';
		
		parse_param(p, &name, &body);
		if (!strcasecmp(name, "expires")) {
			*_exp = atoi(body);
			if (*_exp != 0) *_exp += time(NULL);
		} else if ((*name == 'q') || (*name == 'Q')) {
			*_q = atof(body);
		}
		if ((*_exp != (time_t)(-1)) && (*_q != (float)(-1))) break;
		p = trim(end);
	} while(end);
}


int parse_contact(char* _s, char** _url, time_t* _expire, float* _q)
{
	char* ptr;
#ifdef PARANOID
	if ((!_s) || (!_url) || (!_expire) || (!_q)) {
	        ERR("Invalid parameter value");
		return FALSE;
	}
#endif

	if (*_s == '<') {
		*_url = ++_s;
		ptr = find_not_quoted(_s, '>');
		if (!ptr) {
			ERR("> not found");
			return FALSE;
		} else {
			*ptr++ = '\0';
		}
	} else {
		*_url = _s;

		ptr = find_not_quoted(_s, ';');
		if (ptr) *ptr++ = '\0';
	}

	parse_params(ptr, _expire, _q);
		
	return TRUE;
}


/* FIXME: Pravdepodobne parsuje spatne pokud SIP URL neni
 * uzavrena v lomenych zavorkach
 */

/* FIXME: automat */
/*
 * Contact header field parser
 */
int parse_contact_hdr(char* _b, location_t* _loc, int _expires, int* _star, const char* _callid, int _cseq)
{
	char *url;
	char* comma;
	time_t expires;
	float q;

#ifdef PARANOID
	if ((!_loc) || (!_b) || (!_star) || (!_callid)) return FALSE;
#endif

	_b = trim(_b);

	do {
		_b = eat_lws(_b);

		     /* Star contact has been found */
		if (*_b == '*') {
			*_star = 1;              /* Set the flag */
			_b = find_next_contact(_b);  /* And continue immediately */
			if (_b) *(_b - 1) = '\0';
			continue;
		}

		_b = eat_name(_b);

		comma = find_next_contact(_b);

		if (comma) *comma = '\0';

		if (parse_contact(_b, &url, &expires, &q) == FALSE) {
		        ERR("Error while parsing contact");
			return FALSE;
		}

		     /* If no binding-specific expires value has been found
		      * use Expires HF or default value
		      */
		if (expires == (time_t)-1) {
			expires = _expires;
		}

		if (add_contact(_loc, url, expires, q, _callid, _cseq) == FALSE) {
			ERR("Error while adding contact");
			return FALSE;
		}
		_b = comma;
	} while(_b);
	return TRUE;
}

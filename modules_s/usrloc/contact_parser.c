/* 
 * $Id$ 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "contact_parser.h"
#include "sipdate.h"
#include "utils.h"
#include "const.h"

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
	char* p;
	char* end;
	char* name, *body;

	*_exp = (time_t)(-1);
	*_q = (float)(-1);

	p = trim(find_next_param(_b));
	if (!p) return;
	do {
		if (p) end = find_next_param(p);
		if (end) *(end - 1) = '\0';
		
		parse_param(p, &name, &body);
		if (!strcasecmp(name, "expire")) {
			if (*body == '\"') {
				parse_SIP_date(body, _exp);
			} else {
				time(_exp);
				*_exp += atoi(body);
			}
		} else if ((*name == 'q') || (*name == 'Q')) {
			*_q = atof(body);
		}
		if ((*_exp != (time_t)(-1)) && (*_q != (float)(-1))) break;
		p = trim(end);
	} while(end);
}


/*
 * Contact header field parser
 */
int parse_contact_field(char* _b, location_t* _loc)
{
	contact_t* res = NULL, *c;
	char* body, *url;
	char* comma;
	time_t expire;
	float q;

#ifdef PARANOID
	if (!_loc) return FALSE;
	if (!_b) return FALSE;
#endif

	body = trim(_b);

	     /* Star contact means forget all registrations and
	      * Expires must be 0, exit immediately
	      */
	//	if (*body == '*') {
	//	_loc->star = 1;
	//	_loc->expires = 0;
	//	return TRUE;
	//}

	do {
		url = eat_name(body);

		comma = find_next_contact(url);
		if (comma) {
			*(comma-1) = '\0';
		}

		parse_params(url, &expire, &q);
		add_contact(_loc, eat_lws(body), ((expire == (time_t)-1) ? DEFAULT_EXPIRES : expire), q, TRUE, FALSE);
		body = comma;
	} while(comma);
	return TRUE;
}



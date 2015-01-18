/* 
 * Generic Parameter Parser
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

/*! \file
 * \brief Parser :: Generic Parameter Parser
 *
 * \ingroup parser
 */

#include <string.h>
#include "../str.h"
#include "../ut.h"
#include "../dprint.h"
#include "../trim.h"
#include "../mem/mem.h"
#include "../mem/shm_mem.h"
#include "parse_param.h"


static inline void parse_event_dialog_class(param_hooks_t* h, param_t* p)
{

	if (!p->name.s) {
		LOG(L_ERR, "ERROR: parse_event_dialog_class: empty value\n");
		return;
	}
	if (!h) {
		LOG(L_CRIT, "BUG: parse_event_dialog_class: NULL param hook pointer\n");
		return;
	}
	switch(p->name.s[0]) {
	case 'c':
	case 'C':
		if ((p->name.len == 7) &&
		    (!strncasecmp(p->name.s + 1, "all-id", 6))) {
			p->type = P_CALL_ID;
			h->event_dialog.call_id = p;
		}
		break;

	case 'f':
	case 'F':
		if ((p->name.len == 8) &&
		    (!strncasecmp(p->name.s + 1, "rom-tag", 7))) {
			p->type = P_FROM_TAG;
			h->event_dialog.from_tag = p;
		}
		break;

	case 't':
	case 'T':
		if ((p->name.len == 6) &&
		    (!strncasecmp(p->name.s + 1, "o-tag", 5))) {
			p->type = P_TO_TAG;
			h->event_dialog.to_tag = p;
		}
		break;

	case 'i':
	case 'I':
		if ((p->name.len == 27) &&
		    (!strncasecmp(p->name.s + 1, "nclude-session-description", 26))) {
			p->type = P_ISD;
			h->event_dialog.include_session_description = p;
		}
		break;

	case 's':
	case 'S':
		if ((p->name.len == 3) &&
		    (!strncasecmp(p->name.s + 1, "la", 2))) {
			p->type = P_SLA;
			h->event_dialog.sla = p;
		}
		break;

	case 'm':
	case 'M':
		if ((p->name.len == 2) &&
		    (!strncasecmp(p->name.s + 1, "a", 1))) {
			p->type = P_MA;
			h->event_dialog.ma = p;
		}
		break;
	}
}


/*! \brief
 * Try to find out parameter name, recognized parameters
 * are q, expires and methods
 */
static inline void parse_contact_class(param_hooks_t* _h, param_t* _p)
{

	if (!_p->name.s) {
		LOG(L_ERR, "ERROR: parse_contact_class: empty value\n");
		return;
	}
	if (!_h) {
		LOG(L_CRIT, "BUG: parse_contact_class: NULL param hook pointer\n");
		return;
	}
	switch(_p->name.s[0]) {
	case 'q':
	case 'Q':
		if (_p->name.len == 1) {
			_p->type = P_Q;
			_h->contact.q = _p;
		}
		break;
		
	case 'e':
	case 'E':
		if ((_p->name.len == 7) &&
		    (!strncasecmp(_p->name.s + 1, "xpires", 6))) {
			_p->type = P_EXPIRES;
			_h->contact.expires = _p;
		}
		break;
		
	case 'm':
	case 'M':
		if ((_p->name.len == 7) &&
		    (!strncasecmp(_p->name.s + 1, "ethods", 6))) {
			_p->type = P_METHODS;
			_h->contact.methods = _p;
		}
		break;

	case 'r':
	case 'R':
		if ((_p->name.len == 8) &&
		    (!strncasecmp(_p->name.s + 1, "eceived", 7))) {
			_p->type = P_RECEIVED;
			_h->contact.received = _p;
		} else if((_p->name.len == 6) &&
		    (!strncasecmp(_p->name.s + 1, "eg-id", 5))) {
			_p->type = P_REG_ID;
			_h->contact.reg_id = _p;
		}
		break;
	case '+':
		if ((_p->name.len == 13) &&
			(!strncasecmp(_p->name.s + 1, "sip.instance", 12))) {
			_p->type = P_INSTANCE;
			_h->contact.instance = _p;
		}
		break;
	case 'o':
	case 'O':
		if ((_p->name.len == 2) &&
		    (!strncasecmp(_p->name.s + 1, "b", 1))) {
			_p->type = P_OB;
			_h->contact.ob = _p;
		}
		break;
	}
}


/*! \brief
 * Try to find out parameter name, recognized parameters
 * are transport, lr, r2, maddr
 */
static inline void parse_uri_class(param_hooks_t* _h, param_t* _p)
{

	if (!_p->name.s) {
		LOG(L_ERR, "ERROR: parse_uri_class: empty value\n");
		return;
	}
	if (!_h) {
		LOG(L_CRIT, "BUG: parse_uri_class: NULL param hook pointer\n");
		return;
	}
	switch(_p->name.s[0]) {
	case 't':
	case 'T':
		if ((_p->name.len == 9) &&
		    (!strncasecmp(_p->name.s + 1, "ransport", 8))) {
			_p->type = P_TRANSPORT;
			_h->uri.transport = _p;
		} else if (_p->name.len == 2) {
			if (((_p->name.s[1] == 't') || (_p->name.s[1] == 'T')) &&
			    ((_p->name.s[2] == 'l') || (_p->name.s[2] == 'L'))) {
				_p->type = P_TTL;
				_h->uri.ttl = _p;
			}
		}
		break;

	case 'l':
	case 'L':
		if ((_p->name.len == 2) && ((_p->name.s[1] == 'r') || (_p->name.s[1] == 'R'))) {
			_p->type = P_LR;
			_h->uri.lr = _p;
		}
		break;

	case 'r':
	case 'R':
		if ((_p->name.len == 2) && (_p->name.s[1] == '2')) {
			_p->type = P_R2;
			_h->uri.r2 = _p;
		}
		break;

	case 'm':
	case 'M':
		if ((_p->name.len == 5) &&
		    (!strncasecmp(_p->name.s + 1, "addr", 4))) {
			_p->type = P_MADDR;
			_h->uri.maddr = _p;
		}
		break;
		
	case 'd':
	case 'D':
		if ((_p->name.len == 5) &&
		    (!strncasecmp(_p->name.s + 1, "stip", 4))) {
			_p->type = P_DSTIP;
			_h->uri.dstip = _p;
		} else if ((_p->name.len == 7) &&
			   (!strncasecmp(_p->name.s + 1, "stport", 6))) {
			_p->type = P_DSTPORT;
			_h->uri.dstport = _p;
		}
		break;
	case 'f':
	case 'F':
		if ((_p->name.len == 4) &&
		    (!strncasecmp(_p->name.s + 1, "tag", 3))) {
			_p->type = P_FTAG;
			_h->uri.ftag = _p;
		}
		break;
	case 'o':
	case 'O':
		if ((_p->name.len == 2) &&
		    (!strncasecmp(_p->name.s + 1, "b", 1))) {
			_p->type = P_OB;
			_h->uri.ob = _p;
		}
		break;
	}

}


/*! \brief
 * Parse quoted string in a parameter body
 * return the string without quotes in _r
 * parameter and update _s to point behind the
 * closing quote
 */
static inline int parse_quoted_param(str* _s, str* _r)
{
	char* end_quote;
	char quote;

	     /* The string must have at least
	      * surrounding quotes
	      */
	if (_s->len < 2) {
		return -1;
	}

	     /* Store the kind of quoting (single or double)
	      * which we're handling with
	      */
	quote = (_s->s)[0];

	     /* Skip opening quote */
	_s->s++;
	_s->len--;


	     /* Find closing quote */
	end_quote = q_memchr(_s->s, quote, _s->len);

	     /* Not found, return error */
	if (!end_quote) {
		return -2;
	}

	     /* Let _r point to the string without
	      * surrounding quotes
	      */
	_r->s = _s->s;
	_r->len = end_quote - _s->s;

	     /* Update _s parameter to point
	      * behind the closing quote
	      */
	_s->len -= (end_quote - _s->s + 1);
	_s->s = end_quote + 1;

	     /* Everything went OK */
	return 0;
}


/*! \brief
 * Parse unquoted token in a parameter body
 * let _r point to the token and update _s
 * to point right behind the token
 */
static inline int parse_token_param(str* _s, str* _r, char separator)
{
	int i;

	     /* There is nothing to parse,
	      * return error
	      */
	if (_s->len == 0) {
		return -1;
	}

	     /* Save the begining of the
	      * token in _r->s
	      */
	_r->s = _s->s;

	     /* Iterate through the
	      * token body
	      */
	for(i = 0; i < _s->len; i++) {

		     /* All these characters
		      * mark end of the token
		      */
		switch(_s->s[i]) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case ',':
			     /* So if you find
			      * any of them
			      * stop iterating
			      */
			goto out;
		default:
			if(_s->s[i] == separator)
				goto out;
		}
	}
 out:
	if (i == 0) {
		return -1;
        }

	     /* Save length of the token */
        _r->len = i;

	     /* Update _s parameter so it points
	      * right behind the end of the token
	      */
	_s->s = _s->s + i;
	_s->len -= i;

	     /* Everything went OK */
	return 0;
}


/*! \brief
 * Parse a parameter name
 */
static inline void parse_param_name(str* _s, pclass_t _c, param_hooks_t* _h, param_t* _p, char separator)
{

	if (!_s->s) {
		DBG("DEBUG: parse_param_name: empty parameter\n");
		return;
	}

	_p->name.s = _s->s;

	while(_s->len) {
		switch(_s->s[0]) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case ',':
		case '=':
			goto out;
		default:
			if (_s->s[0] == separator)
				goto out;
		}
		_s->s++;
		_s->len--;
	}

 out:
	_p->name.len = _s->s - _p->name.s;

	switch(_c) {
	case CLASS_CONTACT: parse_contact_class(_h, _p); break;
	case CLASS_URI:     parse_uri_class(_h, _p);     break;
	case CLASS_EVENT_DIALOG: parse_event_dialog_class(_h, _p); break;
	default: break;
	}
}





/*! \brief
 * Parse body of a parameter. It can be quoted string or
 * a single token.
 */
static inline int parse_param_body(str* _s, param_t* _c, char separator)
{
	if (_s->s[0] == '\"' || _s->s[0] == '\'') {
		if (parse_quoted_param(_s, &(_c->body)) < 0) {
			LOG(L_ERR, "parse_param_body(): Error while parsing quoted string\n");
			return -2;
		}
	} else {
		if (parse_token_param(_s, &(_c->body), separator) < 0) {
			LOG(L_ERR, "parse_param_body(): Error while parsing token\n");
			return -3;
		}
	}

	return 0;
}




/*!  \brief
 * Only parse one parameter
 * Returns:
 * 	t: out parameter
 * 	-1: on error
 * 	0: success, but expect a next paramter
 * 	1: success and exepect no more parameters
 */
static inline int parse_param2(str *_s, pclass_t _c, param_hooks_t *_h, param_t *t, char separator)
{
	memset(t, 0, sizeof(param_t));

	parse_param_name(_s, _c, _h, t, separator);
	trim_leading(_s);
	
	if (_s->len == 0) { /* The last parameter without body */
		t->len = t->name.len;
		goto ok;
	}
	
	if (_s->s[0] == '=') {
		_s->s++;
		_s->len--;
		trim_leading(_s);

		if (_s->len == 0) {
		    /* Be forgiving and accept parameters with missing value,
		     * we just set the length of parameter body to 0. */
		    t->body.s = _s->s;
		    t->body.len = 0;
		} else if (parse_param_body(_s, t, separator) < 0) {
			LOG(L_ERR, "parse_params(): Error while parsing param body\n");
			goto error;
		}

		t->len = _s->s - t->name.s;

		trim_leading(_s);
		if (_s->len == 0) {
			goto ok;
		}
	} else {
		t->len = t->name.len;
	}

	if (_s->s[0] == ',') goto ok; /* To be able to parse header parameters */
	if (_s->s[0] == '>') goto ok; /* To be able to parse URI parameters */

	if (_s->s[0] != separator) {
		LOG(L_ERR, "parse_params(): Invalid character, %c expected\n",
			separator);
		goto error;
	}

	_s->s++;
	_s->len--;
	trim_leading(_s);
	
	if (_s->len == 0) {
		LOG(L_ERR, "parse_params(): Param name missing after %c\n",
				separator);
		goto error;
	}

	return 0; /* expect more params */

ok:
	return 1; /* done with parsing for params */
error:
	return -1;
}

/*!  \brief
 * Only parse one parameter
 * Returns:
 * 	t: out parameter
 * 	-1: on error
 * 	0: success, but expect a next paramter
 * 	1: success and exepect no more parameters
 */
inline int parse_param(str *_s, pclass_t _c, param_hooks_t *_h, param_t *t)
{
	return parse_param2(_s, _c, _h, t, ';');
}

/*! \brief
 * Parse parameters
 * \param _s is string containing parameters, it will be updated to point behind the parameters
 * \param _c is class of parameters
 * \param _h is pointer to structure that will be filled with pointer to well known parameters
 * \param _p pointing to linked list where parsed parameters will be stored
 * \return 0 on success and negative number on an error
 */
int parse_params(str* _s, pclass_t _c, param_hooks_t* _h, param_t** _p)
{
	return parse_params2(_s, _c, _h, _p, ';');
}

/*! \brief
 * Parse parameters with configurable separator
 * \param _s is string containing parameters, it will be updated to point behind the parameters
 * \param _c is class of parameters
 * \param _h is pointer to structure that will be filled with pointer to well known parameters
 * \param _p pointing to linked list where parsed parameters will be stored
 * \param separator single character separator
 * \return 0 on success and negative number on an error
 */
int parse_params2(str* _s, pclass_t _c, param_hooks_t* _h, param_t** _p,
			char separator)
{
	param_t* t;

	if (!_s || !_p) {
		LOG(L_ERR, "parse_params(): Invalid parameter value\n");
		return -1;
	}

	if (_h)
		memset(_h, 0, sizeof(param_hooks_t));
	*_p = 0;

	if (!_s->s) { /* no parameters at all -- we're done */
		DBG("DEBUG: parse_params: empty uri params, skipping\n");
		return 0;
	}
			
	while(1) {
		t = (param_t*)pkg_malloc(sizeof(param_t));
		if (t == 0) {
			LOG(L_ERR, "parse_params(): No memory left\n");
			goto error;
		}

		switch(parse_param2(_s, _c, _h, t, separator)) {
		case 0: break;
		case 1: goto ok;
		default: goto error;
		}

		t->next = *_p;
		*_p = t;
	}

 error:
	if (t) pkg_free(t);
	free_params(*_p);
	*_p = 0;
	return -2;

 ok:
	t->next = *_p;
	*_p = t;
	return 0;
}


/*! \brief
 * Free linked list of parameters
 */
static inline void do_free_params(param_t* _p, int _shm)
{
	param_t* ptr;
	
	while(_p) {
		ptr = _p;
		_p = _p->next;
		if (_shm) shm_free(ptr);
		else pkg_free(ptr);
	}	
}


/*! \brief
 * Free linked list of parameters
 */
void free_params(param_t* _p)
{
	do_free_params(_p, 0);
}


/*! \brief
 * Free linked list of parameters
 */
void shm_free_params(param_t* _p)
{
	do_free_params(_p, 1);
}


/*! \brief
 * Print a parameter structure, just for debugging
 */
static inline void print_param(FILE* _o, param_t* _p)
{
	char* type;

	fprintf(_o, "---param(%p)---\n", _p);
	
	switch(_p->type) {
	case P_OTHER:     type = "P_OTHER";     break;
	case P_Q:         type = "P_Q";         break;
	case P_EXPIRES:   type = "P_EXPIRES";   break;
	case P_METHODS:   type = "P_METHODS";   break;
	case P_TRANSPORT: type = "P_TRANSPORT"; break;
	case P_LR:        type = "P_LR";        break;
	case P_R2:        type = "P_R2";        break;
	case P_MADDR:     type = "P_MADDR";     break;
	case P_TTL:       type = "P_TTL";       break;
	case P_RECEIVED:  type = "P_RECEIVED";  break;
	case P_DSTIP:     type = "P_DSTIP";     break;
	case P_DSTPORT:   type = "P_DSTPORT";   break;
	case P_INSTANCE:  type = "P_INSTANCE";  break;
	case P_FTAG:      type = "P_FTAG";      break;
	case P_CALL_ID:   type = "P_CALL_ID";   break;
	case P_FROM_TAG:  type = "P_FROM_TAG";  break;
	case P_TO_TAG:    type = "P_TO_TAG";    break;
	case P_ISD:       type = "P_ISD";       break;
	case P_SLA:       type = "P_SLA";       break;
	default:          type = "UNKNOWN";     break;
	}
	
	fprintf(_o, "type: %s\n", type);
	fprintf(_o, "name: \'%.*s\'\n", _p->name.len, _p->name.s);
	fprintf(_o, "body: \'%.*s\'\n", _p->body.len, _p->body.s);
	fprintf(_o, "len : %d\n", _p->len);
	fprintf(_o, "---/param---\n");
}


/*! \brief
 * Print linked list of parameters, just for debugging
 */
void print_params(FILE* _o, param_t* _p)
{
	param_t* ptr;
	
	ptr = _p;
	while(ptr) {
		print_param(_o, ptr);
		ptr = ptr->next;
	}
}


/*! \brief
 * Duplicate linked list of parameters
 */
static inline int do_duplicate_params(param_t** _n, param_t* _p, int _shm)
{
	param_t* last, *ptr, *t;

	if (!_n) {
		LOG(L_ERR, "duplicate_params(): Invalid parameter value\n");
		return -1;
	}
	
	last = 0;
	*_n = 0;
	ptr = _p;
	while(ptr) {
		if (_shm) {
			t = (param_t*)shm_malloc(sizeof(param_t));
		} else {
			t = (param_t*)pkg_malloc(sizeof(param_t));
		}
		if (!t) {
			LOG(L_ERR, "duplicate_params(): Invalid parameter value\n");
			goto err;
		}
		memcpy(t, ptr, sizeof(param_t));
		t->next = 0;

		if (!*_n) *_n = t;
		if (last) last->next = t;
		last = t;

		ptr = ptr->next;
	}
	return 0;

 err:
	do_free_params(*_n, _shm);
	return -2;
}


/*! \brief
 * Duplicate linked list of parameters
 */
int duplicate_params(param_t** _n, param_t* _p)
{
	return do_duplicate_params(_n, _p, 0);
}


/*! \brief
 * Duplicate linked list of parameters
 */
int shm_duplicate_params(param_t** _n, param_t* _p)
{
	return do_duplicate_params(_n, _p, 1);
}

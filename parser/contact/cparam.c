#include "cparam.h"
#include "../../mem/mem.h"
#include <stdio.h>         /* printf */
#include "../../ut.h"      /* q_memchr */
#include <string.h>        /* memset */
#include "../../trim.h"


/*
 * Parse quoted string in a parameter body
 * return the string without quotes in _r
 * parameter and update _s to point behind the
 * closing quote
 */
static inline int parse_quoted(str* _s, str* _r)
{
	char* end_quote;

	     /* The string must have at least
	      * surrounding quotes
	      */
	if (_s->len < 2) {
		return -1;
	}

	     /* Skip opening quote */
	_s->s++;
	_s->len--;


	     /* Find closing quote */
	end_quote = q_memchr(_s->s, '\"', _s->len);

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


/*
 * Parse unquoted token in a parameter body
 * let _r point to the token and update _s
 * to point right behind the token
 */
static inline int parse_token(str* _s, str* _r)
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

	     /* Iterate throught the
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
		case ';':
			     /* So if you find
			      * any of them
			      * stop iterating
			      */
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


/*
 * Parse type of a parameter
 */
static inline int parse_param_type(cparam_t* _c)
{
	switch(_c->name.s[0]) {
	case 'q':
	case 'Q':
		if (_c->name.len == 1) {
			_c->type = CP_Q;
		}
		return 0;
		
	case 'e':
	case 'E':
		if ((_c->name.len == 7) &&
		    (!strncasecmp(_c->name.s + 1, "xpires", 6))) {
			_c->type = CP_EXPIRES;
		}
		return 0;
		
	case 'm':
	case 'M':
		if ((_c->name.len == 6) &&
		    (!strncasecmp(_c->name.s + 1, "ethod", 5))) {
			_c->type = CP_METHOD;
		}
		return 0;
	}
	return 0;
}


/*
 * Parse body of a parameter. It can be quoted string or
 * a single token.
 */
static inline int parse_body(str* _s, cparam_t* _c)
{
	if (_s->s[0] == '\"') {
		if (parse_quoted(_s, &(_c->body)) < 0) {
			LOG(L_ERR, "parse_body(): Error while parsing quoted string\n");
			return -2;
		}
	} else {
		if (parse_token(_s, &(_c->body)) < 0) {
			LOG(L_ERR, "parse_body(): Error while parsing token\n");
			return -3;
		}
	}

	return 0;
}


/*
 * Parse a parameter name
 */
static inline int parse_param_name(str* _s, cparam_t* _p)
{
	_p->name.s = _s->s;

	while(_s->len) {
		switch(_s->s[0]) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case ';':
		case ',':
		case '=':
			goto out;
		}
		_s->s++;
		_s->len--;
	}

 out:
	_p->name.len = _s->s - _p->name.s;
	
	if (parse_param_type(_p) < 0) {
		LOG(L_ERR, "parse_param_name(): Error while parsing type\n");
		return -2;
	}
	
	return 0;
}


/*
 * Parse contact parameters
 */
int parse_cparams(str* _s, cparam_t** _p, cparam_t** _q, cparam_t** _e, cparam_t** _m)
{
	cparam_t* c;

	while(1) {
		c = (cparam_t*)pkg_malloc(sizeof(cparam_t));
		if (c == 0) {
			LOG(L_ERR, "parse_cparams(): No memory left\n");
			goto error;
		}
		memset(c, 0, sizeof(cparam_t));

		if (parse_param_name(_s, c) < 0) {
			LOG(L_ERR, "parse_cparams(): Error while parsing param name\n");
			goto error;
		}

		trim_leading(_s);
		
		if (_s->len == 0) { /* The last parameter without body */
			goto ok;
		}
		
		if (_s->s[0] == '=') {
			_s->s++;
			_s->len--;
			trim_leading(_s);

			if (_s->len == 0) {
				LOG(L_ERR, "parse_cparams(): Body missing\n");
				goto error;
			}

			if (parse_body(_s, c) < 0) {
				LOG(L_ERR, "parse_cparams(): Error while parsing param body\n");
				goto error;
			}

			trim_leading(_s);
			if (_s->len == 0) {
				goto ok;
			}
		}

		if (_s->s[0] == ',') goto ok;

		if (_s->s[0] != ';') {
			LOG(L_ERR, "parse_cparams(): Invalid character, ; expected\n");
			goto error;
		}

		_s->s++;
		_s->len--;
		trim_leading(_s);
		
		if (_s->len == 0) {
			LOG(L_ERR, "parse_cparams(): Param name missing after ;\n");
			goto error;
		}

		c->next = *_p;
		*_p = c;
		switch(c->type) {    /* Update hook pointers */
		case CP_Q:       *_q = c; break;
		case CP_EXPIRES: *_e = c; break;
		case CP_METHOD:  *_m = c; break;
		case CP_OTHER:            break;
		}		
	}

 error:
	if (c) pkg_free(c);
	free_cparams(_p);
	return -1;

 ok:
	c->next = *_p;
	*_p = c;
	switch(c->type) {    /* Update hook pointers */
	case CP_Q:       *_q = c; break;
	case CP_EXPIRES: *_e = c; break;
	case CP_METHOD:  *_m = c; break;
	case CP_OTHER:          ; break;
	}
	return 0;
}


/*
 * Free the whole contact parameter list
 */
void free_cparams(cparam_t** _p)
{
	cparam_t* ptr;

	while(*_p) {
		ptr = *_p;
		pkg_free(ptr);
		*_p = (*_p)->next;
	}
}


/*
 * Print contact parameter list
 */
void print_cparams(cparam_t* _p)
{
	cparam_t* ptr;
	char* type;

	ptr = _p;

	while(ptr) {
		printf("...cparam(%p)...\n", ptr);

		switch(ptr->type) {
		case CP_OTHER:   type = "CP_OTHER";   break;
		case CP_Q:       type = "CP_Q";       break;
		case CP_EXPIRES: type = "CP_EXPIRES"; break;
		case CP_METHOD:  type = "CP_METHOD";  break;
		default:         type = "UNKNOWN";    break;
		}

		printf("type: %s\n", type);
		printf("name: \'%.*s\'\n", ptr->name.len, ptr->name.s);
		printf("body: \'%.*s\'\n", ptr->body.len, ptr->body.s);

		printf(".../cparam...\n");

		ptr = ptr->next;
	}
}

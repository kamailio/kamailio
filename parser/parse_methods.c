/*
 * Copyright (c) 2004 Juha Heinanen
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
 */

/*! \file
 * \brief Parser :: Parse Methods
 *
 * \ingroup parser
 */

#include <strings.h>
#include "../dprint.h"
#include "../trim.h"
#include "parse_methods.h"


/*! \brief
 * Check if argument is valid RFC3261 token character.
 */
static int token_char(char _c)
{
 	return 	(_c >= 65 && _c <= 90) ||        /* upper alpha */
 		(_c >= 97 && _c <= 122) ||       /* lower aplha */
 		(_c >= 48 && _c <= 57) ||        /* digits */
 		(_c == '-') || (_c == '.') || (_c == '!') || (_c == '%') ||
 		(_c == '*') || (_c == '_') || (_c == '+') || (_c == '`') ||
 		(_c == '\'') || (_c == '~');
 }



/*! \brief Parse a string containing a method.
 *
 * Parse a method pointed by s & assign its enum bit to method. The string
 * _must_ contain _only_ the method (without trailing or heading whitespace).
 * \return 0 on success, -1 on error
 */
int parse_method_name(const str* const s, enum request_method* const method)
 {
	if (unlikely(!s || !method)) {
		LOG(L_ERR, "Invalid parameter value\n");
		return -1;
	}
	
	if (unlikely(!s->len || (s->s==0))) {
		DBG("No input\n");
		*method = METHOD_OTHER;
		return 0;
	}
	
	switch ((s->s)[0]) {
		/* ordered after probability of aparition on a normal proxy */
		case 'R':
		case 'r':
			if (likely((s->len == 8) &&
					!strncasecmp(s->s + 1, "egister", 7))) {
				*method = METHOD_REGISTER;
				return 0;
			}
			if (likely((s->len==5) && !strncasecmp(s->s + 1, "efer", 4))) {
				*method = METHOD_REFER;
				return 0;
			}
			break;
		case 'A':
		case 'a':
			if (likely((s->len==3) && !strncasecmp(s->s + 1, "ck", 2))) {
				*method = METHOD_ACK;
				return 0;
			}
			break;
		case 'I':
		case 'i':
			if (likely((s->len==6) && !strncasecmp(s->s + 1, "nvite", 5))){
				*method = METHOD_INVITE;
				return 0;
			}
			if (likely((s->len==4) && !strncasecmp(s->s + 1, "nfo", 3))) {
				*method = METHOD_INFO;
				return 0;
			}
			break;
		case 'P':
		case 'p':
			if (likely((s->len==5) && !strncasecmp(s->s + 1, "rack", 4))) {
				*method = METHOD_PRACK;
				return 0;
			}
			if (likely((s->len==7) && !strncasecmp(s->s + 1, "ublish", 6))) {
				*method = METHOD_PUBLISH;
				return 0;
			}
			break;
		case 'C':
		case 'c':
			if (likely((s->len==6) && !strncasecmp(s->s + 1, "ancel", 5))) {
				*method = METHOD_CANCEL;
				return 0;
			}
			break;
		case 'B':
		case 'b':
			if (likely((s->len==3) && !strncasecmp(s->s + 1, "ye", 2))) {
				*method = METHOD_BYE;
				return 0;
			}
			break;
		case 'M':
		case 'm':
			if (likely((s->len==7) && !strncasecmp(s->s + 1, "essage", 6))) {
				*method = METHOD_MESSAGE;
				return 0;
			}
			break;
		case 'O':
		case 'o':
			if (likely((s->len==7) && !strncasecmp(s->s + 1, "ptions", 6))) {
				*method = METHOD_OPTIONS;
				return 0;
			}
			break;
		case 'S':
		case 's':
			if (likely((s->len==9) && !strncasecmp(s->s + 1, "ubscribe", 8))) {
				*method = METHOD_SUBSCRIBE;
				return 0;
			}
			break;
		case 'N':
		case 'n':
			if (likely((s->len==6) && !strncasecmp(s->s + 1, "otify", 5))){
				*method = METHOD_NOTIFY;
				return 0;
			}
			break;
		case 'U':
		case 'u':
			if (likely((s->len==6) && !strncasecmp(s->s + 1, "pdate", 5))){
				*method = METHOD_UPDATE;
				return 0;
			}
			break;
		default:
			break;
	}
	/* unknown method */
	*method = METHOD_OTHER;
	return 0;
}



 /*! \brief
  * Parse a method pointed by _next, assign its enum bit to _method, and update
  * _next past the method. Returns 1 if parse succeeded and 0 otherwise.
  */
static int parse_method_advance(str* const _next, enum request_method* const _method)
 {
	char* end;
	
	 if (unlikely(!_next || !_method)) {
		 LOG(L_ERR, "Invalid parameter value\n");
		 return 0;
	 }
	 
	 if (unlikely(!_next->len || !_next->s)) {
		 DBG("No input\n");
 		*_method = METHOD_OTHER;
		 return 1;
	 }
	end=_next->s+_next->len;
	
	 switch ((_next->s)[0]) {
	 case 'A':
	 case 'a':
		 if ((_next->len > 2) && !strncasecmp(_next->s + 1, "ck", 2)) {
 			*_method = METHOD_ACK;
 			_next->len -= 3;
 			_next->s += 3;
			goto found;
 		} else {
 			goto unknown;
 		}

 	case 'B':
 	case 'b':
 		if ((_next->len > 2) && !strncasecmp(_next->s + 1, "ye", 2)) {
 			*_method = METHOD_BYE;
 			_next->len -= 3;
 			_next->s += 3;
			goto found;
 		} else {
 			goto unknown;
 		}

 	case 'C':
 	case 'c':
 		if ((_next->len > 5) && !strncasecmp(_next->s + 1, "ancel", 5)) {
 			*_method = METHOD_CANCEL;
 			_next->len -= 6;
 			_next->s += 6;
			goto found;
 		} else {
 			goto unknown;
 		}

 	case 'I':
 	case 'i':
 		if ((_next->len > 3) &&
 		    ((*(_next->s + 1) == 'N') || (*(_next->s + 1) == 'n'))) {
 			if (!strncasecmp(_next->s + 2, "fo", 2)) {
 				*_method = METHOD_INFO;
 				_next->len -= 4;
 				_next->s += 4;
				goto found;
 			}

 			if ((_next->len > 5) && !strncasecmp(_next->s + 2, "vite", 4)) {
 				*_method = METHOD_INVITE;
 				_next->len -= 6;
 				_next->s += 6;
				goto found;
 			}
 		}
 		goto unknown;

 	case 'M':
 	case 'm':
 		if ((_next->len > 6) && !strncasecmp(_next->s + 1, "essage", 6)) {
 			*_method = METHOD_MESSAGE;
 			_next->len -= 7;
 			_next->s += 7;
			goto found;
 		} else {
 			goto unknown;
 		}

 	case 'N':
 	case 'n':
 		if ((_next->len > 5) && !strncasecmp(_next->s + 1, "otify", 5)) {
 			*_method = METHOD_NOTIFY;
 			_next->len -= 6;
 			_next->s += 6;
			goto found;
 		} else {
 			goto unknown;
 		}

 	case 'O':
 	case 'o':
 		if ((_next->len > 6) && !strncasecmp(_next->s + 1, "ptions", 6)) {
 			*_method = METHOD_OPTIONS;
 			_next->len -= 7;
 			_next->s += 7;
			goto found;
 		} else {
 			goto unknown;
 		}

 	case 'P':
 	case 'p':
 		if ((_next->len > 4) && !strncasecmp(_next->s + 1, "rack", 4)) {
 			*_method = METHOD_PRACK;
 			_next->len -= 5;
 			_next->s += 5;
			goto found;
 		}
 		if ((_next->len > 6) && !strncasecmp(_next->s + 1, "ublish", 6)) {
 			*_method = METHOD_PUBLISH;
 			_next->len -= 7;
 			_next->s += 7;
			goto found;
 		}
 		goto unknown;

 	case 'R':
 	case 'r':
 		if ((_next->len > 4) &&
 		    ((*(_next->s + 1) == 'E') || (*(_next->s + 1) == 'e'))) {
 			if (!strncasecmp(_next->s + 2, "fer", 3)) {
 				*_method = METHOD_REFER;
 				_next->len -= 5;
 				_next->s += 5;
				goto found;
 			}

 			if ((_next->len > 7) && !strncasecmp(_next->s + 2, "gister", 6)) {
 				*_method = METHOD_REGISTER;
 				_next->len -= 8;
 				_next->s += 8;
				goto found;
 			}
 		}
 		goto unknown;

 	case 'S':
 	case 's':
 		if ((_next->len > 8) && !strncasecmp(_next->s + 1, "ubscribe", 8)) {
 			*_method = METHOD_SUBSCRIBE;
 			_next->len -= 9;
 			_next->s += 9;
			goto found;
 		} else {
 			goto unknown;
 		}

 	case 'U':
 	case 'u':
 		if ((_next->len > 5) && !strncasecmp(_next->s + 1, "pdate", 5)) {
 			*_method = METHOD_UPDATE;
 			_next->len -= 6;
 			_next->s += 6;
			goto found;
 		} else {
 			goto unknown;
 		}

 	default:
 		goto unknown;
 	}

 unknown:
 	if (token_char(*(_next->s))) {
 		do { 
 			_next->s++;
 			_next->len--;
 		} while (_next->len && token_char(*(_next->s)));
 		*_method = METHOD_OTHER;
 		return 1;
 	} else {
 		return 0;
 	}
found:
	/* check if the method really ends here (if not return 0) */
	return (_next->s>=end) || (!token_char(*(_next->s)));
 }
 
 
 /*! \brief
  * Parse comma separated list of methods pointed by _body and assign their
  * enum bits to _methods.  Returns 0 on success and -1 on failure.
  */
int parse_methods(const str* const _body, unsigned int* const _methods)
 {
 	str next;
 	unsigned int method;
	
	method=0; /* fixes silly gcc 4.x warning */
 
	if (!_body || !_methods) {
		LOG(L_ERR, "parse_methods: Invalid parameter value\n");
		return -1;
	}

	next.len = _body->len;
	next.s = _body->s;
 
 	trim_leading(&next);
 
  	*_methods = 0;
 
 	if (next.len == 0) {
 		return 0;
 	}

 	while (1) {
 		if (parse_method_advance(&next, &method)) {
 			*_methods |= method;
 		} else {
 			LOG(L_ERR, "ERROR: parse_methods: Invalid method\n");
 			return -1;
 		}
		
 		trim_leading(&next);
 		if (next.len) {
 			if (next.s[0] == ',') {
 				next.len--;
 				next.s++;
 				trim_leading(&next);
 				if (next.len == 0) {
 					LOG(L_ERR, "ERROR: parse_methods: Method expected\n");
 					return 0;
 				}
 			} else {
 				LOG(L_ERR, "ERROR: parse_methods: Comma expected\n");
 				return -1;
 			}
 		} else {
 			break;
 		}
 	}

 	return 0;
 }

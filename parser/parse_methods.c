/*
 * $Id$
 *
 * Copyright (c) 2004 Juha Heinanen
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
 */

#include <strings.h>
#include "../dprint.h"
#include "../trim.h"
#include "parse_methods.h"


/*
 * Check if argument is valid RFC3261 token character.
 */
static int token_char(char _c)
{
 	return 	(_c >= 65 && _c <= 90) ||        /* upper alpha */
 		(_c >= 97 && _c <= 122) ||       /* lower aplha */
 		(_c >= 48 && _c <= 57) ||        /* digits */
 		(_c == '-') || (_c == '.') || (_c == '!') || (_c == '%') ||
 		(_c == '*') || (_c == '_') || (_c == '+') || (_c == '`') ||
 		(_c == '\'') || (_c == '~') || (_c == '+') || (_c == '`');
 }
 
 
 /*
  * Parse a method pointed by _next, assign its enum bit to _method, and update
  * _next past the method. Returns 1 if parse succeeded and 0 otherwise.
  */
static int parse_method(str* _next, unsigned int* _method) 
 {
	 if (!_next || !_method) {
		 LOG(L_ERR, "parse_method: Invalid parameter value\n");
		 return 0;
	 }
	 
	 if (!_next->len || !_next->s) {
		 DBG("parse_method: No input\n");
		 return 1;
	 }

	 switch ((_next->s)[0]) {
	 case 'A':
	 case 'a':
		 if ((_next->len > 2) && !strncasecmp(_next->s + 1, "ck", 2)) {
 			*_method = METHOD_ACK;
 			_next->len -= 3;
 			_next->s += 3;
 			return 1;
 		} else {
 			goto unknown;
 		}

 	case 'B':
 	case 'b':
 		if ((_next->len > 2) && !strncasecmp(_next->s + 1, "ye", 2)) {
 			*_method = METHOD_BYE;
 			_next->len -= 3;
 			_next->s += 3;
 			return 1;
 		} else {
 			goto unknown;
 		}

 	case 'C':
 	case 'c':
 		if ((_next->len > 5) && !strncasecmp(_next->s + 1, "ancel", 5)) {
 			*_method = METHOD_CANCEL;
 			_next->len -= 6;
 			_next->s += 6;
 			return 1;
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
 				return 1;
 			}

 			if ((_next->len > 5) && !strncasecmp(_next->s + 2, "vite", 4)) {
 				*_method = METHOD_INVITE;
 				_next->len -= 6;
 				_next->s += 6;
 				return 1;
 			}
 		}
 		goto unknown;

 	case 'M':
 	case 'm':
 		if ((_next->len > 6) && !strncasecmp(_next->s + 1, "essage", 6)) {
 			*_method = METHOD_MESSAGE;
 			_next->len -= 7;
 			_next->s += 7;
 			return 1;
 		} else {
 			goto unknown;
 		}

 	case 'N':
 	case 'n':
 		if ((_next->len > 5) && !strncasecmp(_next->s + 1, "otify", 5)) {
 			*_method = METHOD_NOTIFY;
 			_next->len -= 6;
 			_next->s += 6;
 			return 1;
 		} else {
 			goto unknown;
 		}

 	case 'O':
 	case 'o':
 		if ((_next->len > 6) && !strncasecmp(_next->s + 1, "ptions", 6)) {
 			*_method = METHOD_OPTIONS;
 			_next->len -= 7;
 			_next->s += 7;
 			return 1;
 		} else {
 			goto unknown;
 		}

 	case 'P':
 	case 'p':
 		if ((_next->len > 4) && !strncasecmp(_next->s + 1, "rack", 4)) {
 			*_method = METHOD_PRACK;
 			_next->len -= 5;
 			_next->s += 5;
 			return 1;
 		} else {
 			goto unknown;
 		}

 	case 'R':
 	case 'r':
 		if ((_next->len > 4) &&
 		    ((*(_next->s + 1) == 'E') || (*(_next->s + 1) == 'e'))) {
 			if (!strncasecmp(_next->s + 2, "fer", 3)) {
 				*_method = METHOD_REFER;
 				_next->len -= 5;
 				_next->s += 5;
 				return 1;
 			}

 			if ((_next->len > 7) && !strncasecmp(_next->s + 2, "gister", 6)) {
 				*_method = METHOD_REGISTER;
 				_next->len -= 8;
 				_next->s += 8;
 				return 1;
 			}
 		}
 		goto unknown;

 	case 'S':
 	case 's':
 		if ((_next->len > 8) && !strncasecmp(_next->s + 1, "ubscribe", 8)) {
 			*_method = METHOD_SUBSCRIBE;
 			_next->len -= 9;
 			_next->s += 9;
 			return 1;
 		} else {
 			goto unknown;
 		}

 	case 'U':
 	case 'u':
 		if ((_next->len > 5) && !strncasecmp(_next->s + 1, "pdate", 5)) {
 			*_method = METHOD_UPDATE;
 			_next->len -= 6;
 			_next->s += 6;
 			return 1;
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
 		*_method = METHOD_UNKNOWN;
 		return 1;
 	} else {
 		return 0;
 	}
 }
 
 
 /* 
  * Parse comma separated list of methods pointed by _body and assign their
  * enum bits to _methods.  Returns 1 on success and 0 on failure.
  */
 int parse_methods(str* _body, unsigned int* _methods)
 {
 	str next;
 	unsigned int method;
 
	if (!_body || !_methods) {
		LOG(L_ERR, "parse_methods: Invalid parameter value\n");
		return 0;
	}

	next.len = _body->len;
	next.s = _body->s;
 
 	trim_leading(&next);
 
 	if (next.len == 0) {
 		LOG(L_ERR, "ERROR: parse_methods: Empty body\n");
 		return 0;
 	}

  	*_methods = 0;
 
 	while (1) {
 		if (parse_method(&next, &method)) {
 			*_methods |= method;
 		} else {
 			LOG(L_ERR, "ERROR: parse_methods: Invalid method\n");
 			return 0;
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
 				return 0;
 			}
 		} else {
 			break;
 		}
 	}

 	return 1;
 }

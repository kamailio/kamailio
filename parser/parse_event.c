/*
 * $Id$
 *
 * Event header field body parser.
 * The parser was written for Presence Agent module only.
 * it recognize presence package only, no subpackages, no parameters
 * It should be replaced by a more generic parser if subpackages or
 * parameters should be parsed too.
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *
 * History:
 * --------
 * 2003-04-26 ZSW (jiri)
 */


#include "parse_event.h"
#include "../mem/mem.h"    /* pkg_malloc, pkg_free */
#include "../dprint.h"
#include <string.h>        /* memset */
#include "../trim.h"       /* trim_leading */
#include <stdio.h>         /* printf */
#include "../ut.h"


#define PRES_STR "presence"
#define PRES_STR_LEN 8

#define PRES_WINFO_STR "presence.winfo"
#define PRES_WINFO_STR_LEN 14

#define PRES_XCAP_CHANGE_STR "xcap-change"
#define PRES_XCAP_CHANGE_STR_LEN 11

#define PRES_LOCATION_STR "location"
#define PRES_LOCATION_STR_LEN 8



static inline char* skip_token(char* _b, int _l)
{
	int i = 0;

	for(i = 0; i < _l; i++) {
		switch(_b[i]) {
		case ' ':
		case '\r':
		case '\n':
		case '\t':
		case ';':
			return _b + i;
		}
	}

	return _b + _l;
}


static inline int event_parser(char* _s, int _l, event_t* _e)
{
	str tmp;
	char* end;
	char buf[128];

	tmp.s = _s;
	tmp.len = _l;

	trim_leading(&tmp);

	if (tmp.len == 0) {
		LOG(L_ERR, "event_parser(): Empty body\n");
		return -1;
	}

	_e->text.s = tmp.s;

	end = skip_token(tmp.s, tmp.len);

	_e->text.len = end - tmp.s;

	strncpy(buf, tmp.s, tmp.len);
	buf[tmp.len] = 0;

	if ((_e->text.len == PRES_STR_LEN) && 
	    !strncasecmp(PRES_STR, tmp.s, _e->text.len)) {
		_e->parsed = EVENT_PRESENCE;
	} else if ((_e->text.len == PRES_XCAP_CHANGE_STR_LEN) && 
		   !strncasecmp(PRES_XCAP_CHANGE_STR, tmp.s, _e->text.len)) {
		_e->parsed = EVENT_XCAP_CHANGE;
	} else if ((_e->text.len == PRES_LOCATION_STR_LEN) && 
		   !strncasecmp(PRES_LOCATION_STR, tmp.s, _e->text.len)) {
		_e->parsed = EVENT_LOCATION;
	} else if ((_e->text.len == PRES_WINFO_STR_LEN) && 
		   !strncasecmp(PRES_WINFO_STR, tmp.s, _e->text.len)) {
		_e->parsed = EVENT_PRESENCE_WINFO;
	} else {
		_e->parsed = EVENT_OTHER;
	}

	return 0;
}


/*
 * Parse Event header field body
 */
int parse_event(struct hdr_field* _h)
{
	event_t* e;

	if (_h->parsed != 0) {
		return 0;
	}

	e = (event_t*)pkg_malloc(sizeof(event_t));
	if (e == 0) {
		LOG(L_ERR, "parse_event(): No memory left\n");
		return -1;
	}

	memset(e, 0, sizeof(event_t));

	if (event_parser(_h->body.s, _h->body.len, e) < 0) {
		LOG(L_ERR, "parse_event(): Error in event_parser\n");
		pkg_free(e);
		return -2;
	}

	_h->parsed = (void*)e;
	return 0;
}


/*
 * Free all memory
 */
void free_event(event_t** _e)
{
	if (*_e) pkg_free(*_e);
	*_e = 0;
}


/*
 * Print structure, for debugging only
 */
void print_event(event_t* _e)
{
	printf("===Event===\n");
	printf("text  : \'%.*s\'\n", _e->text.len, ZSW(_e->text.s));
	printf("parsed: %s\n", 
	       (_e->parsed == EVENT_PRESENCE) ? ("EVENT_PRESENCE") : ("EVENT_OTHER"));
	printf("===/Event===\n");
}

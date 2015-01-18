/*
 * Event header field body parser.
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
 * \brief Parser :: Event header field body parser.
 *
 * The parser was written for Presence Agent module only.
 * it recognize presence package only, no sub-packages, no parameters
 * It should be replaced by a more generic parser if sub-packages or
 * parameters should be parsed too.
 *
 * \ingroup parser
 */


#include "parse_event.h"
#include "../mem/mem.h"    /* pkg_malloc, pkg_free */
#include "../dprint.h"
#include <string.h>        /* memset */
#include "../trim.h"       /* trim_leading */
#include <stdio.h>         /* printf */
#include "../ut.h"

static struct {
	str name;
	int type;	
} events[] = {
	{STR_STATIC_INIT("presence"),        EVENT_PRESENCE},
	{STR_STATIC_INIT("presence.winfo"),  EVENT_PRESENCE_WINFO},
	{STR_STATIC_INIT("xcap-change"),     EVENT_XCAP_CHANGE},
	{STR_STATIC_INIT("sip-profile"),     EVENT_SIP_PROFILE},
	{STR_STATIC_INIT("message-summary"), EVENT_MESSAGE_SUMMARY},
	{STR_STATIC_INIT("dialog"),          EVENT_DIALOG},
	{STR_STATIC_INIT("ua-profile"),      EVENT_UA_PROFILE},
	/* The following must be the last element in the array */
	{STR_NULL,                           EVENT_OTHER}
};


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


int event_parser(char* s, int len, event_t* e)
{
	int i;
	str tmp;
	char* end;
	param_hooks_t* phooks = NULL;
	enum pclass pclass = CLASS_ANY;

	if (e == NULL) {
		ERR("event_parser: Invalid parameter value\n");
		return -1;
	}

	tmp.s = s;
	tmp.len = len;
	trim_leading(&tmp);

	if (tmp.len == 0) {
		LOG(L_ERR, "event_parser: Empty body\n");
		return -1;
	}

	e->name.s = tmp.s;
	end = skip_token(tmp.s, tmp.len);
	e->name.len = end - tmp.s;

	e->type = EVENT_OTHER;
	for(i = 0; events[i].name.len; i++) {
		if (e->name.len == events[i].name.len &&
			!strncasecmp(e->name.s, events[i].name.s, e->name.len)) {
			e->type = events[i].type;
			break;
		}
	}

	tmp.len -= end - tmp.s;
	tmp.s = end;
	trim_leading(&tmp);

	e->params.list = NULL;
	
	if (tmp.len && (tmp.s[0] == ';')) {
		/* Shift the semicolon and skip any leading whitespace, this is needed
		 * for parse_params to work correctly. */
		tmp.s++; tmp.len--;
		trim_leading(&tmp);
		if (!tmp.len) return 0;

		/* We have parameters to parse */
		if (e->type == EVENT_DIALOG) {
			pclass = CLASS_EVENT_DIALOG;
			phooks = (param_hooks_t*)&e->params.hooks;
		}

		if (parse_params(&tmp, pclass, phooks, &e->params.list) < 0) {
			ERR("event_parser: Error while parsing parameters parameters\n");
			return -1;
		}
	}
	return 0;
}


/*! \brief
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


/*! \brief
 * Free all memory
 */
void free_event(event_t** _e)
{
	if (*_e) {
		if ((*_e)->params.list) free_params((*_e)->params.list);
		pkg_free(*_e);
		*_e = NULL;
	}
}


/*! \brief
 * Print structure, for debugging only
 */
void print_event(event_t* e)
{
	fprintf(stderr, "===Event===\n");
	fprintf(stderr, "name  : \'%.*s\'\n", STR_FMT(&e->name));
	fprintf(stderr, "type: %d\n", e->type);
	if (e->params.list) {
		print_params(stderr, e->params.list);
	}
	fprintf(stderr, "===/Event===\n");
}

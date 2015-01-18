/*
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
 */

/*! \file
 * \brief Parser :: Event header field body parser.
 *
 *
 * \ingroup parser
 */


#ifndef PARSE_EVENT_H
#define PARSE_EVENT_H

#include "../str.h"
#include "hf.h"
#include "parse_param.h"

/* Recognized event types */
enum event_type {
	EVENT_OTHER = 0,
	EVENT_PRESENCE,
	EVENT_PRESENCE_WINFO,
	EVENT_SIP_PROFILE,
	EVENT_XCAP_CHANGE,
	EVENT_DIALOG,
	EVENT_MESSAGE_SUMMARY,
	EVENT_UA_PROFILE
};


struct event_params {
	param_hooks_t hooks; /* Well known dialog package params */
	param_t* list; /* Linked list of all parsed parameters */
};


typedef struct event {
	enum event_type type; /* Parsed variant */
	str name;             /* Original string representation */
	struct event_params params;
} event_t;


/*
 * Parse Event HF body
 */
int parse_event(struct hdr_field* hf);


/*
 * Release memory
 */
void free_event(event_t** e);


/*
 * Print structure, for debugging only
 */
void print_event(event_t* e);

int event_parser(char* s, int l, event_t* e);


#endif /* PARSE_EVENT_H */

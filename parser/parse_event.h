/*
 * $Id$
 *
 * Event header field body parser
 * This parser was written for Presence Agent module only.
 * it recognize presence package only, no subpackages, no parameters
 * It should be replaced by a more generic parser if subpackages or
 * parameters should be parsed too.
 */

#ifndef PARSE_EVENT_H
#define PARSE_EVENT_H

#include "../str.h"
#include "hf.h"

#define EVENT_OTHER    0
#define EVENT_PRESENCE 1


typedef struct event {
	str text;       /* Original string representation */
	int parsed;     /* Parsed variant */
} event_t;


/*
 * Parse Event HF body
 */
int parse_event(struct hdr_field* _h);


/*
 * Release memory
 */
void free_event(event_t** _e);


/*
 * Print structure, for debugging only
 */
void print_event(event_t* _e);


#endif /* PARSE_EVENT_H */

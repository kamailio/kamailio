/*
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#ifndef OPTION_TAGS_H
#define OPTION_TAGS_H

#include <strings.h>
#include "hf.h"
#include "keys.h"

#define F_OPTION_TAG_PATH (1 << 0)
#define F_OPTION_TAG_100REL (1 << 1)
#define F_OPTION_TAG_TIMER (1 << 2)
#define F_OPTION_TAG_EVENTLIST (1 << 3)
#define F_OPTION_TAG_GRUU (1 << 4)
#define F_OPTION_TAG_OUTBOUND (1 << 5)

#define OPTION_TAG_PATH_STR "path"
#define OPTION_TAG_PATH_LEN (sizeof(OPTION_TAG_PATH_STR) - 1)

/* RFC 3262 (PRACK) */
#define OPTION_TAG_100REL_STR "100rel"
#define OPTION_TAG_100REL_LEN (sizeof(OPTION_TAG_100REL_STR) - 1)

/* RFC 4028 */
#define OPTION_TAG_TIMER_STR "timer"
#define OPTION_TAG_TIMER_LEN (sizeof(OPTION_TAG_TIMER_STR) - 1)

/* RFC 4662 (RLS) */
#define OPTION_TAG_EVENTLIST_STR "eventlist"
#define OPTION_TAG_EVENTLIST_LEN (sizeof(OPTION_TAG_EVENTLIST_STR) - 1)

/* RFC 5627 */
#define OPTION_TAG_GRUU_STR "gruu"
#define OPTION_TAG_GRUU_LEN (sizeof(OPTION_TAG_GRUU_STR) - 1)

/* RFC 5626 */
#define OPTION_TAG_OUTBOUND_STR "outbound"
#define OPTION_TAG_OUTBOUND_LEN (sizeof(OPTION_TAG_OUTBOUND_STR) - 1)


struct option_tag_body
{
	hf_parsed_free_f hfree;		  /* function to free the content */
	unsigned int option_tags;	  /* option-tag mask for the current hdr */
	unsigned int option_tags_all; /* option-tag mask for the all hdr
	                                *  - it's set only for the first hdr in
	                                *  sibling list*/
};


inline static int option_tag_is_delim(char val)
{
	return val == ' ' || val == '\t' || val == '\r' || val == '\n'
		   || val == ',';
}

inline static unsigned int option_tag_lower_byte(unsigned char val)
{
	return (unsigned int)val | 0x20U;
}

inline static unsigned int option_tag_read_u32le(const char *val)
{
	return (unsigned int)(unsigned char)val[0]
		   | ((unsigned int)(unsigned char)val[1] << 8)
		   | ((unsigned int)(unsigned char)val[2] << 16)
		   | ((unsigned int)(unsigned char)val[3] << 24);
}

inline static unsigned int option_tag_lower_u32(unsigned int val)
{
	return val | 0x20202020U;
}

/*!
 * Parse HF body containing option-tags.
 */
static inline int parse_option_tag_body(str *body, unsigned int *tags)
{
	register char *p;
	register unsigned int val;
	int len, pos = 0;
	int case_found;

	*tags = 0;

	p = body->s;
	len = body->len;

	while(pos < len) {
		/* skip spaces and commas */
		for(; pos < len && option_tag_is_delim(*p); ++pos, ++p)
			;

		case_found = 0;
		if(pos >= len) {
			break;
		}
		if(len - pos >= 4) {
			val = option_tag_lower_u32(option_tag_read_u32le(p));
			switch(val) {

				/* "path" */
				case _path_:
					if(len - pos == OPTION_TAG_PATH_LEN
							|| (len - pos > OPTION_TAG_PATH_LEN
									&& option_tag_is_delim(
											p[OPTION_TAG_PATH_LEN]))) {
						*tags |= F_OPTION_TAG_PATH;
						pos += OPTION_TAG_PATH_LEN;
						p += OPTION_TAG_PATH_LEN;
						case_found = 1;
					}
					break;

				/* "100rel" */
				case _100r_:
					if(len - pos >= OPTION_TAG_100REL_LEN
							&& option_tag_lower_byte((unsigned char)p[4]) == 'e'
							&& option_tag_lower_byte((unsigned char)p[5]) == 'l'
							&& (len - pos == OPTION_TAG_100REL_LEN
									|| option_tag_is_delim(
											p[OPTION_TAG_100REL_LEN]))) {
						*tags |= F_OPTION_TAG_100REL;
						pos += OPTION_TAG_100REL_LEN;
						p += OPTION_TAG_100REL_LEN;
						case_found = 1;
					}
					break;

				/* "timer" */
				case _time_:
					if(len - pos >= OPTION_TAG_TIMER_LEN
							&& option_tag_lower_byte((unsigned char)p[4]) == 'r'
							&& (len - pos == OPTION_TAG_TIMER_LEN
									|| option_tag_is_delim(
											p[OPTION_TAG_TIMER_LEN]))) {
						*tags |= F_OPTION_TAG_TIMER;
						pos += OPTION_TAG_TIMER_LEN;
						p += OPTION_TAG_TIMER_LEN;
						case_found = 1;
					}
					break;
			}
		}
		if(case_found == 0) {
			/* extra require or unknown */
			if(len - pos >= OPTION_TAG_EVENTLIST_LEN
					&& strncasecmp(p, OPTION_TAG_EVENTLIST_STR,
							   OPTION_TAG_EVENTLIST_LEN)
							   == 0
					&& (len - pos == OPTION_TAG_EVENTLIST_LEN
							|| option_tag_is_delim(
									p[OPTION_TAG_EVENTLIST_LEN]))) {
				*tags |= F_OPTION_TAG_EVENTLIST;
				pos += OPTION_TAG_EVENTLIST_LEN;
				p += OPTION_TAG_EVENTLIST_LEN;
			} else if(len - pos >= OPTION_TAG_GRUU_LEN
					  && strncasecmp(
								 p, OPTION_TAG_GRUU_STR, OPTION_TAG_GRUU_LEN)
								 == 0
					  && (len - pos == OPTION_TAG_GRUU_LEN
							  || option_tag_is_delim(p[OPTION_TAG_GRUU_LEN]))) {
				*tags |= F_OPTION_TAG_GRUU;
				pos += OPTION_TAG_GRUU_LEN;
				p += OPTION_TAG_GRUU_LEN;
			} else if(len - pos >= OPTION_TAG_OUTBOUND_LEN
					  && strncasecmp(p, OPTION_TAG_OUTBOUND_STR,
								 OPTION_TAG_OUTBOUND_LEN)
								 == 0
					  && (len - pos == OPTION_TAG_OUTBOUND_LEN
							  || option_tag_is_delim(
									  p[OPTION_TAG_OUTBOUND_LEN]))) {
				*tags |= F_OPTION_TAG_OUTBOUND;
				pos += OPTION_TAG_OUTBOUND_LEN;
				p += OPTION_TAG_OUTBOUND_LEN;
			} else {
				/* unknown (not needed) - skip element */
				for(; pos < len && !option_tag_is_delim(*p); ++pos, ++p)
					;
			}
		}
	}

	return 0;
}


void hf_free_option_tag(void *parsed);

#endif /* OPTION_TAGS_H */

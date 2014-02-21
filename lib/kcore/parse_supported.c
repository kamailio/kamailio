/*
 * $Id$
 *
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file
 * \brief Supported parser
 * \ingroup parser
 */

#include "../../mem/mem.h"
#include "../../parser/keys.h"
#include "parse_supported.h"

#define _100r_ 0x72303031   /* "100r" for "100rel" */
#define _time_ 0x656d6974   /*!< "time" */

#define IS_DELIM(c) (*(c) == ' ' || *(c) == '\t' || *(c) == '\r' || *(c) == '\n' || *(c) == ',')

/* from parser/parse_hname2.c: */
#define LOWER_BYTE(b) ((b) | 0x20)
#define LOWER_DWORD(d) ((d) | 0x20202020)
#define READ(val) \
	(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))


/*!
 * Parse Supported HF body.
 */
static inline int parse_supported_body(str *body, unsigned int *sup)
{
	register char* p;
	register unsigned int val;
	int len, pos = 0;
	int case_found;

	*sup = 0;

	p = body->s;
	len = body->len;

	while (pos < len) {
		/* skip spaces and commas */
		for (; pos < len && IS_DELIM(p); ++pos, ++p);

		val = LOWER_DWORD(READ(p));
		case_found = 0;
		switch (val) {

			/* "path" */
			case _path_:
				if(pos + 4 <= len && IS_DELIM(p+4)) {
					*sup |= F_SUPPORTED_PATH;
					pos += 5; p += 5;
					case_found = 1;
				}
				break;

			/* "100rel" */
			case _100r_:
				if ( pos+6 <= len
					 && LOWER_BYTE(*(p+4))=='e' && LOWER_BYTE(*(p+5))=='l'
					 && IS_DELIM(p+6)) {
					*sup |= F_SUPPORTED_100REL;
					pos += SUPPORTED_100REL_LEN + 1;
					p   += SUPPORTED_100REL_LEN + 1;
					case_found = 1;
				}
				break;

			/* "timer" */
			case _time_:
				if ( pos+5 <= len && LOWER_BYTE(*(p+4))=='r'
					 && IS_DELIM(p+5) ) {
					*sup |= F_SUPPORTED_TIMER;
					pos += SUPPORTED_TIMER_LEN + 1;
					p   += SUPPORTED_TIMER_LEN + 1;
					case_found = 1;
				}
				break;
		}

		if(case_found==0) {
			/* extra supported or unknown */
			if(pos+SUPPORTED_EVENTLIST_LEN<=len
					&& strncasecmp(p, SUPPORTED_EVENTLIST_STR,
						SUPPORTED_EVENTLIST_LEN)==0
					&& IS_DELIM(p+SUPPORTED_EVENTLIST_LEN) ) {
				*sup |= F_SUPPORTED_EVENTLIST;
				pos += SUPPORTED_EVENTLIST_LEN + 1;
				p   += SUPPORTED_EVENTLIST_LEN + 1;
			} else if(pos+SUPPORTED_GRUU_LEN<=len
					&& strncasecmp(p, SUPPORTED_GRUU_STR,
						SUPPORTED_GRUU_LEN)==0
					&& IS_DELIM(p+SUPPORTED_GRUU_LEN)) {
				*sup |= F_SUPPORTED_GRUU;
				pos += SUPPORTED_GRUU_LEN + 1;
				p   += SUPPORTED_GRUU_LEN + 1;
			} else if(pos+SUPPORTED_OUTBOUND_LEN<=len
					&& strncasecmp(p, SUPPORTED_OUTBOUND_STR,
						SUPPORTED_OUTBOUND_LEN)==0
					&& IS_DELIM(p+SUPPORTED_OUTBOUND_LEN)) {
				*sup |= F_SUPPORTED_OUTBOUND;
				pos += SUPPORTED_OUTBOUND_LEN + 1;
				p   += SUPPORTED_OUTBOUND_LEN + 1;
			} else {
				/* skip element */
				for (; pos < len && !IS_DELIM(p); ++pos, ++p);
			}
		}
	}
	
	return 0;
}


/**
 * wrapper to free the content of parsed supported header
 */
void hf_free_supported(void *parsed)
{
	struct supported_body *sb;
	sb = (struct supported_body*)parsed;
	free_supported(&sb);
}

/*!
 * Parse all Supported headers
 */
int parse_supported( struct sip_msg *msg)
{
	unsigned int supported;
	struct hdr_field  *hdr;
	struct supported_body *sb;

	/* maybe the header is already parsed! */
	if (msg->supported && msg->supported->parsed)
		return 0;

	/* parse to the end in order to get all SUPPORTED headers */
	if (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->supported)
		return -1;

	/* bad luck! :-( - we have to parse them */
	supported = 0;
	for( hdr=msg->supported ; hdr ; hdr=next_sibling_hdr(hdr)) {
		if (hdr->parsed) {
			supported |= ((struct supported_body*)hdr->parsed)->supported;
			continue;
		}

		sb = (struct supported_body*)pkg_malloc(sizeof(struct supported_body));
		if (sb == 0) {
			LM_ERR("out of pkg_memory\n");
			return -1;
		}

		parse_supported_body(&(hdr->body), &(sb->supported));
		sb->hfree = hf_free_supported;
		sb->supported_all = 0;
		hdr->parsed = (void*)sb;
		supported |= sb->supported;
	}

	((struct supported_body*)msg->supported->parsed)->supported_all = 
		supported;
	return 0;
}

/* free supported header structure */
void free_supported(struct supported_body **sb)
{
	if (sb && *sb) {
		pkg_free(*sb);
		*sb = 0;
	}
}

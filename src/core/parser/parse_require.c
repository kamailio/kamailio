/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief Require parser
 * \ingroup parser
 */

#include "../mem/mem.h"
#include "parse_require.h"

/*!
 * Parse all Require headers
 */
int parse_require( struct sip_msg *msg)
{
	unsigned int require;
	struct hdr_field  *hdr;
	struct option_tag_body *rb;

	/* maybe the header is already parsed! */
	if (msg->require && msg->require->parsed)
		return 0;

	/* parse to the end in order to get all SUPPORTED headers */
	if (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->require)
		return -1;

	/* bad luck! :-( - we have to parse them */
	require = 0;
	for( hdr=msg->require ; hdr ; hdr=next_sibling_hdr(hdr)) {
		if (hdr->parsed) {
			require |= ((struct option_tag_body*)hdr->parsed)->option_tags;
			continue;
		}

		rb = (struct option_tag_body*)pkg_malloc(sizeof(struct option_tag_body));
		if (rb == 0) {
			LM_ERR("out of pkg_memory\n");
			return -1;
		}

		parse_option_tag_body(&(hdr->body), &(rb->option_tags));
		rb->hfree = hf_free_option_tag;
		rb->option_tags_all = 0;
		hdr->parsed = (void*)rb;
		require |= rb->option_tags;
	}

	((struct option_tag_body*)msg->require->parsed)->option_tags_all = 
		require;
	return 0;
}

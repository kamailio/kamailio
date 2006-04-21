/*
 * $Id$
 *
 * Copyright (C) 2005 iptelorg GmbH
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

#include "http.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../parser/parse_via.h"
#include "../../data_lump.h"
#include "../../ip_addr.h"
#include "../../msg_translator.h"
#include "../../ut.h"
#include <string.h>


/*
 * Create a fake topmost Via header field because HTTP clients
 * do not generate Vias
 */
static int insert_fake_via(struct sip_msg* msg, char* via, int via_len)
{
	struct via_body* vb = 0;

	via_cnt++;
	vb = pkg_malloc(sizeof(struct via_body));
	if (vb == 0){
	        ERR("insert_fake_via: Out of memory\n");
		goto error;
	}

	msg->h_via1 = pkg_malloc(sizeof(struct hdr_field));
	if (!msg->h_via1) {
		ERR("No memory left\n");
		goto error;
	}
	memset(msg->h_via1, 0, sizeof(struct hdr_field));
	memset(vb, 0, sizeof(struct via_body));

	     /* FIXME: The code below would break if the VIA prefix
	      * gets changed in config.h
	      */
	msg->h_via1->name.s = via;
	msg->h_via1->name.len = 3;
	msg->h_via1->body.s = via + 5;
	msg->h_via1->body.len = via_len - 5 - CRLF_LEN;
	msg->h_via1->type = HDR_VIA_T;
	msg->h_via1->parsed = vb;

	/* This field is used by the msg translator to add a new
	 * via when forwarding the request. It must point to an existing
	 * header field because otherwise call to anchor_lump, which does
	 * hdr.s - buf, would crash
	 */
	vb->hdr.s = msg->headers->name.s;
	vb->hdr.len = 0;
	
	msg->via1 = vb;
	
	     /* We have to replace the zero terminating character right behind
	      * CRLF because otherwise the parser will return an error.
	      * It expects that there is either a next header field or another
	      * CRLF delimiter
	      */
	via[via_len] = 'a';
	parse_via(via + 5, via + via_len + 1, vb);
	if (vb->error == PARSE_ERROR){
	        ERR("Bad via\n");
		goto error;
	}

	if (msg->last_header == 0) {
		msg->headers = msg->h_via1;
		msg->last_header = msg->h_via1;
	} else {
		msg->last_header->next = msg->h_via1;
		msg->last_header = msg->h_via1;
	}

	return 0;

 error:
	if (vb) {
		free_via_list(vb);
		pkg_free(vb);
	}

	if (msg->h_via1) pkg_free(msg->h_via1);
	return -1;
}


static int insert_via_lump(struct sip_msg* msg, char* via, int via_len)
{
	struct lump* anchor;
	
	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, HDR_VIA_T);
	if (anchor == 0) {
		ERR("Unable to create anchor\n");
		return -1;
	}

	if (insert_new_lump_after(anchor, via, via_len, HDR_VIA_T) == 0) {
		ERR("Unable to insert via lump\n");
		return -1;
	}

	return 0;	
}


int create_via(struct sip_msg* msg, char* s1, char* s2)
{
	char* via;
	unsigned int via_len;
	str ip, port;
	struct hostport hp;
	struct dest_info dst;

	ip.s = ip_addr2a(&msg->rcv.src_ip);
	ip.len = strlen(ip.s);

	port.s = int2str(msg->rcv.src_port, &port.len);

	hp.host = &ip;
	hp.port = &port;

	init_dst_from_rcv(&dst, &msg->rcv);
	via = via_builder(&via_len, &dst, 0, 0, &hp);
	if (!via) {
		ERR("Unable to build Via header field\n");
		return -1;
	}

	if (insert_fake_via(msg, via, via_len) < 0) {
		pkg_free(via);
		return -1;
	}

	if (insert_via_lump(msg, via, via_len - CRLF_LEN) < 0) {
		pkg_free(via);
		return -1;
	}

	return 1;
}

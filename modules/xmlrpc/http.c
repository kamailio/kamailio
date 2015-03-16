/*
 * Copyright (C) 2005 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/** @addtogroup xmlrpc
 * @{
 */

/** @file
 * XML-RPC requests are carried in the body of HTTP requests, but HTTP
 * requests does not contain some mandatory SIP headers and thus cannot
 * be processed by SER directly. This file contains functions that can
 * turn HTTP requests into SIP request by inserting fake mandatory SIP
 * headers, such as Via. This allows SER to process such HTTP requests
 * and extract the body of the request, which contains the XML-RPC
 * document.
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


/** Insert fake Via header field into SIP message.
 *
 * This function takes a SIP message and a Via header field
 * as text and inserts the Via header field into the SIP
 * message, modifying the data structures within the SIP
 * message to make it look that the Via header field was
 * received with the message.
 *
 * @param msg a pointer to the currently processed SIP message
 * @param via the Via header field text to be inserted
 * @param via_len size of the Via header field being inserted
 * @return 0 on succes, a negative number on error.
 */
static int insert_fake_via(sip_msg_t* msg, char* via, int via_len)
{
	struct via_body* vb = 0;

	via_cnt++;
	vb = pkg_malloc(sizeof(struct via_body));
	if (vb == 0){
	        ERR("insert_fake_via: Out of memory\n");
		goto error;
	}

	msg->h_via1 = pkg_malloc(sizeof(hdr_field_t));
	if (!msg->h_via1) {
		ERR("No memory left\n");
		goto error;
	}
	memset(msg->h_via1, 0, sizeof(hdr_field_t));
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


static int insert_via_lump(sip_msg_t* msg, char* via, int via_len)
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


/** Create a fake Via header field.
 * 
 * This function creates a fake Via header field and inserts
 * the fake header field into the header of the HTTP request.
 * The fake Via header field contains the source IP address
 * and port of the TCP/IP connection.
 */
int create_via(sip_msg_t* msg, char* s1, char* s2)
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

/** @} */

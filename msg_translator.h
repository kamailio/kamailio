/*$Id$
 * 
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
 * 2003-03-01 VOICE_MAIL defs removed (jiri)
 */


#ifndef  _MSG_TRANSLATOR_H
#define _MSG_TRANSLATOR_H

#define MY_HF_SEP ": "
#define MY_HF_SEP_LEN 2

#define BRANCH_SEPARATOR '.'
#define WARNING "Warning: 392 "
#define WARNING_LEN (sizeof(WARNING)-1)
#define WARNING_PHRASE " \"Noisy feedback tells: "
#define WARNING_PHRASE_LEN (sizeof(WARNING_PHRASE)-1)

#include "parser/msg_parser.h"
#include "ip_addr.h"

/* point to some remarkable positions in a SIP message */
struct bookmark {
	str to_tag_val;
};

char * build_req_buf_from_sip_req (	struct sip_msg* msg, 
				unsigned int *returned_len, struct socket_info* send_sock,
				int proto);

char * build_res_buf_from_sip_res(	struct sip_msg* msg,
				unsigned int *returned_len);

char * build_res_buf_from_sip_req(	unsigned int code ,
				char *text ,
				char *new_tag ,
				unsigned int new_tag_len ,
				struct sip_msg* msg,
				unsigned int *returned_len,
				struct bookmark *bmark);
char * build_res_buf_with_body_from_sip_req(	unsigned int code ,
				char *text ,
				char *new_tag ,
				unsigned int new_tag_len ,
				char *body ,
				unsigned int body_len,
				char *content_type,
				unsigned int content_type_len,
				struct sip_msg* msg,
				unsigned int *returned_len,
				struct bookmark *bmark);

char* via_builder( unsigned int *len,
	struct socket_info* send_sock,
	str *branch, str* extra_params, int proto );


int branch_builder( unsigned int hash_index, 
	/* only either parameter useful */
	unsigned int label, char * char_v,
	int branch,
	/* output value: string and actual length */
	char *branch_str, int *len );


#endif

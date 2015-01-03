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
 *
 */

/*!
* \file
* \brief Kamailio core :: Message translations
* \author jiri
* \ingroup core
* Module: \ref core
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

/*#define MAX_CONTENT_LEN_BUF INT2STR_MAX_LEN *//* see ut.h/int2str() */

#define BUILD_NO_LOCAL_VIA		(1<<0)
#define BUILD_NO_VIA1_UPDATE	(1<<1)
#define BUILD_NO_PATH			(1<<2)
#define BUILD_IN_SHM			(1<<7)

#include "parser/msg_parser.h"
#include "ip_addr.h"

/* point to some remarkable positions in a SIP message */
struct bookmark {
	str to_tag_val;
};

/* used by via_builder */
struct hostport {
	str* host;
	str* port;
};


#define set_hostport(hp, msg) \
	do{ \
		if ((msg) && ((struct sip_msg*)(msg))->set_global_address.len) \
			(hp)->host=&(((struct sip_msg*)(msg))->set_global_address); \
		else \
			(hp)->host=&default_global_address; \
		if ((msg) && ((struct sip_msg*)(msg))->set_global_port.len) \
			(hp)->port=&(((struct sip_msg*)(msg))->set_global_port); \
		else \
			(hp)->port=&default_global_port; \
	}while(0)

char * build_req_buf_from_sip_req(struct sip_msg* msg, 
				unsigned int *returned_len, struct dest_info* send_info,
				unsigned int mode);

char * build_res_buf_from_sip_res(struct sip_msg* msg,
				unsigned int *returned_len);

char * generate_res_buf_from_sip_res(struct sip_msg* msg,
				unsigned int *returned_len, unsigned int mode);

char * build_res_buf_from_sip_req(unsigned int code,
				str *text,
				str *new_tag,
				struct sip_msg* msg,
				unsigned int *returned_len,
				struct bookmark *bmark);
/*
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
*/
char* via_builder( unsigned int *len,
	struct dest_info* send_info,
	str *branch, str* extra_params, struct hostport *hp );

/* creates a via header honoring the protocol of the incomming socket
 * msg is an optional parameter */
char* create_via_hf( unsigned int *len,
	struct sip_msg *msg,
	struct dest_info* send_info /* where to send the reply */,
	str* branch);

int branch_builder( unsigned int hash_index, 
	/* only either parameter useful */
	unsigned int label, char * char_v,
	int branch,
	/* output value: string and actual length */
	char *branch_str, int *len );

char* id_builder(struct sip_msg* msg, unsigned int *id_len);

/* check if IP address in Via != source IP address of signaling,
 * or the sender is asking to set the values for rport or received */
int received_test( struct sip_msg *msg );

/* check if IP address in Via != source IP address of signaling */
int received_via_test( struct sip_msg *msg );

/* builds a char* buffer from message headers without body
 * first line is excluded in case of skip_first_line=1
 */
char * build_only_headers( struct sip_msg* msg, int skip_first_line,
				unsigned int *returned_len,
				int *error,
				struct dest_info* send_info);

/* builds a char* buffer from message body
 * error is set -1 if the memory allocation failes
 */
char * build_body( struct sip_msg* msg,
			unsigned int *returned_len,
			int *error,
			struct dest_info* send_info);

/* builds a char* buffer from SIP message including body
 * The function adjusts the Content-Length HF according
 * to body lumps in case of adjust_clen=1.
 */
char * build_all( struct sip_msg* msg, int adjust_clen,
			unsigned int *returned_len,
			int *error,
			struct dest_info* send_info);

/** cfg framework fixup */
void fix_global_req_flags(str* gname, str* name);

int build_sip_msg_from_buf(struct sip_msg *msg, char *buf, int len,
		unsigned int id);

/* returns a copy in private memory of the boundary in a multipart body */
int get_boundary(struct sip_msg* msg, str* boundary);
#endif

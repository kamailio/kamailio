/*$Id$
 * 
 */

#ifndef  _MSG_TRANSLATOR_H
#define _MSG_TRANSLATOR_H

#define MY_HF_SEP ": "
#define MY_HF_SEP_LEN 2

#define BRANCH_SEPARATOR '.'

#include "parser/msg_parser.h"
#include "ip_addr.h"

char * build_req_buf_from_sip_req (	struct sip_msg* msg, 
				unsigned int *returned_len, struct socket_info* send_sock);

char * build_res_buf_from_sip_res(	struct sip_msg* msg,
				unsigned int *returned_len);

char * build_res_buf_from_sip_req(	unsigned int code ,
				char *text ,
				char *new_tag ,
				unsigned int new_tag_len ,
				struct sip_msg* msg,
				unsigned int *returned_len);

char* via_builder( unsigned int *len,
	struct socket_info* send_sock,
	char *branch, int branch_len );

#ifdef _OBSOLETED
char* via_builder( struct sip_msg *msg ,
				unsigned int *len, struct socket_info* send_sock);
#endif

int branch_builder( unsigned int hash_index, 
	/* only either parameter useful */
	unsigned int label, char * char_v,
	int branch,
	/* output value: string and actual length */
	char *branch_str, int *len );


#endif

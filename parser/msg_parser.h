/*
 * $Id$
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
 * History
 * -------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-28 removed scratchpad (jiri)
 */


#ifndef msg_parser_h
#define msg_parser_h

#include "../comp_defs.h"
#include "../str.h"
#include "../data_lump.h"
#include "../flags.h"
#include "../ip_addr.h"
#include "../md5utils.h"
#include "../config.h"
#include "parse_def.h"
#include "parse_cseq.h"
#include "parse_to.h"
#include "parse_via.h"
#include "parse_fline.h"
#include "hf.h"


/* convenience short-cut macros */
#define REQ_LINE(_msg) ((_msg)->first_line.u.request)
#define REQ_METHOD first_line.u.request.method_value
#define REPLY_STATUS first_line.u.reply.statuscode
#define REPLY_CLASS(_reply) ((_reply)->REPLY_STATUS/100)

/* number methods as power of two to allow bitmap matching */
enum request_method { METHOD_INVITE=1, METHOD_CANCEL=2, METHOD_ACK=4, 
	METHOD_BYE=8, METHOD_OTHER=16 };


#define IFISMETHOD(methodname,firstchar)                                  \
if (  (*tmp==(firstchar) || *tmp==((firstchar) | 32)) &&                  \
        strncasecmp( tmp+1, #methodname +1, methodname##_LEN-1)==0 &&     \
        *(tmp+methodname##_LEN)==' ') {                                   \
                fl->type=SIP_REQUEST;                                     \
                fl->u.request.method.len=methodname##_LEN;                \
                fl->u.request.method_value=METHOD_##methodname;           \
                tmp=buffer+methodname##_LEN;                              \
}



struct sip_uri {
	str user;     /* Username */
	str passwd;   /* Password */
	str host;     /* Host name */
	str port;     /* Port number */
	str params;   /* Parameters */
	str headers;  
	unsigned short port_no;
};



struct sip_msg {
	unsigned int id;               /* message id, unique/process*/
	struct msg_start first_line;   /* Message first line */
	struct via_body* via1;         /* The first via */
	struct via_body* via2;         /* The second via */
	struct hdr_field* headers;     /* All the parsed headers*/
	struct hdr_field* last_header; /* Pointer to the last parsed header*/
	int parsed_flag;               /* Already parsed header field types */

	     /* Via, To, CSeq, Call-Id, From, end of header*/
	     /* pointers to the first occurances of these headers;
		  * everything is also saved in 'headers'
		  * (WARNING: do not deallocate them twice!)*/

	struct hdr_field* h_via1;
	struct hdr_field* h_via2;
	struct hdr_field* callid;
	struct hdr_field* to;
	struct hdr_field* cseq;
	struct hdr_field* from;
	struct hdr_field* contact;
	struct hdr_field* maxforwards;
	struct hdr_field* route;
	struct hdr_field* record_route;
	struct hdr_field* content_type;
	struct hdr_field* content_length;
	struct hdr_field* authorization;
	struct hdr_field* expires;
	struct hdr_field* proxy_auth;
	struct hdr_field* www_auth;
	struct hdr_field* supported;
	struct hdr_field* require;
	struct hdr_field* proxy_require;
	struct hdr_field* unsupported;
	struct hdr_field* allow;
	struct hdr_field* event;

	char* eoh;        /* pointer to the end of header (if found) or null */
	char* unparsed;   /* here we stopped parsing*/
	
	struct receive_info rcv; /* source & dest ip, ports, proto a.s.o*/

	char* buf;        /* scratch pad, holds a modfied message,
					   *  via, etc. point into it */
	unsigned int len; /* message len (orig) */

	     /* modifications */
	
	str new_uri; /* changed first line uri*/

	str dst_uri; /* Destination URI, must be forwarded to this URI if len != 0 */

	int parsed_uri_ok; /* 1 if parsed_uri is valid, 0 if not */
	struct sip_uri parsed_uri; /* speed-up > keep here the parsed uri*/
	
	struct lump* add_rm;         /* used for all the forwarded requests */
	struct lump* repl_add_rm;    /* used for all the forwarded replies */
	struct lump_rpl *reply_lump; /* only for localy generated replies !!!*/

	/* str add_to_branch; 
	   whatever whoever want to append to branch comes here 
	*/
	char add_to_branch_s[MAX_BRANCH_PARAM_LEN];
	int add_to_branch_len;
	
	     /* index to TM hash table; stored in core to avoid unnecessary calcs */
	unsigned int  hash_index;
	
	     /* allows to set various flags on the message; may be used for 
	      *	simple inter-module communication or remembering processing state
	      * reached 
	      */
	flag_t flags;	
};

/* pointer to a fakes message which was never received ;
   (when this message is "relayed", it is generated out
    of the original request)
*/
#define FAKED_REPLY     ((struct sip_msg *) -1)

extern int via_cnt;

int parse_msg(char* buf, unsigned int len, struct sip_msg* msg);

int parse_headers(struct sip_msg* msg, int flags, int next);

void free_sip_msg(struct sip_msg* msg);

/* make sure all HFs needed for transaction identification have been
   parsed; return 0 if those HFs can't be found
 */

int check_transaction_quadruple( struct sip_msg* msg );

/* calculate characteristic value of a message -- this value
   is used to identify a transaction during the process of
   reply matching
 */
inline static int char_msg_val( struct sip_msg *msg, char *cv )
{
	str src[8];

	if (!check_transaction_quadruple(msg)) {
		LOG(L_ERR, "ERROR: can't calculate char_value due "
			"to a parsing error\n");
		memset( cv, '0', MD5_LEN );
		return 0;
	}

	src[0]= msg->from->body;
	src[1]= msg->to->body;
	src[2]= msg->callid->body;
	src[3]= msg->first_line.u.request.uri;
	src[4]= get_cseq( msg )->number;
	
	/* topmost Via is part of transaction key as well ! */
	src[5]= msg->via1->host;
	src[6]= msg->via1->port_str;
	if (msg->via1->branch) {
		src[7]= msg->via1->branch->value;
		MDStringArray ( cv, src, 8 );
	} else {
		MDStringArray( cv, src, 7 );
	}
	return 1;
}


/* returns a pointer to the begining of the msg's body
 */
inline static char* get_body(struct sip_msg *msg)
{
	int offset;
	int len;

	if ( parse_headers(msg,HDR_EOH, 0)==-1 )
		return 0;

	if (msg->unparsed){
		len=(int)(msg->unparsed-msg->buf);
	}else return 0;
	if ((len+2<=msg->len) && (strncmp(CRLF,msg->unparsed,CRLF_LEN)==0) )
		offset = CRLF_LEN;
	else if ( (len+1<=msg->len) &&
				(*(msg->unparsed)=='\n' || *(msg->unparsed)=='\r' ) )
		offset = 1;
	else
		return 0;

	return msg->unparsed + offset;
}

#endif

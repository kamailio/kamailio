/*
 * $Id$
 */

#ifndef msg_parser_h
#define msg_parser_h

#include "../str.h"
#include "../data_lump.h"
#include "../flags.h"
#include "../ip_addr.h"
#include "parse_def.h"
#include "parse_cseq.h"
#include "parse_to.h"
#include "parse_via.h"
#include "parse_uri.h"
#include "parse_fline.h"
#include "hf.h"


/* Maximum length of values appended to Via-branch parameter */
#ifdef USE_SYNONIM
#define MAX_BRANCH_PARAM_LEN  22
#else
#define MAX_BRANCH_PARAM_LEN  48
#endif


/* convenience short-cut macros */
#define REQ_LINE(_msg) ((_msg)->first_line.u.request)
#define REQ_METHOD first_line.u.request.method_value
#define REPLY_STATUS first_line.u.reply.statuscode
#define REPLY_CLASS(_reply) ((_reply)->REPLY_STATUS/100)

enum { METHOD_OTHER, METHOD_INVITE, METHOD_CANCEL, METHOD_ACK, METHOD_BYE };

#define IFISMETHOD(methodname,firstchar)                                  \
if (  (*tmp==(firstchar) || *tmp==((firstchar) | 32)) &&                  \
        strncasecmp( tmp+1, #methodname +1, methodname##_LEN-1)==0 &&     \
        *(tmp+methodname##_LEN)==' ') {                                   \
                fl->type=SIP_REQUEST;                                     \
                fl->u.request.method.len=methodname##_LEN;                \
                fl->u.request.method_value=METHOD_##methodname;           \
                tmp=buffer+methodname##_LEN;                              \
}


struct sip_msg {
	unsigned int id;               /* message id, unique/process*/
	struct msg_start first_line;   /* Message first line */
	struct via_body* via1;         /* The first via */
	struct via_body* via2;         /* The second via */
	struct hdr_field* headers;     /* All the parsed headers*/
	struct hdr_field* last_header; /* Pointer to the last parsed header*/
	int parsed_flag;               /* Already parsed header field types */

	     /* Via, To, CSeq, Call-Id, From, end of header*/
	     /* first occurance of it; subsequent occurances saved in 'headers' */

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

	char* eoh;        /* pointer to the end of header (if found) or null */
	char* unparsed;   /* here we stopped parsing*/

	struct ip_addr src_ip;
	struct ip_addr dst_ip;
	
	char* orig;       /* original message copy */
	char* buf;        /* scratch pad, holds a modfied message,
			   *  via, etc. point into it 
			   */
	unsigned int len; /* message len (orig) */

	     /* modifications */
	
	str new_uri; /* changed first line uri*/
	
	struct lump* add_rm;         /* used for all the forwarded messages */
	struct lump* repl_add_rm;    /* only for localy generated replies !!!*/
	struct lump_rpl *reply_lump;
	
	     /* str add_to_branch */ 
	     /* whatever whoever want to append to branch comes here */
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


int parse_msg(char* buf, unsigned int len, struct sip_msg* msg);

int parse_headers(struct sip_msg* msg, int flags);

void free_sip_msg(struct sip_msg* msg);

/* make sure all HFs needed for transaction identification have been
   parsed; return 0 if those HFs can't be found
 */

#define check_transaction_quadruple(msg ) \
	(parse_headers((msg), HDR_FROM|HDR_TO|HDR_CALLID|HDR_CSEQ)!=-1 && \
	(msg)->from && (msg)->to && (msg)->callid && (msg)->cseq)

/* restored to the original double-check and put macro params
   in parenthesses  -jiri */
/* re-reverted to the shorter version -andrei 
#define check_transaction_quadruple(msg ) \
   ( ((msg)->from || (parse_headers( (msg), HDR_FROM)!=-1 && (msg)->from)) && 	\
   ((msg)->to|| (parse_headers( (msg), HDR_TO)!=-1 && (msg)->to)) &&		\
   ((msg)->callid|| (parse_headers( (msg), HDR_CALLID)!=-1 && (msg)->callid)) &&\
   ((msg)->cseq|| (parse_headers( (msg), HDR_CSEQ)!=-1 && (msg)->cseq)) && \
   ((msg)->via1|| (parse_headers( (msg), HDR_VIA)!=-1 && (msg)->via1)) ) 
*/
	
#endif

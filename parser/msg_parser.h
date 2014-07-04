/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History
 * -------
 *  2003-01-28  removed scratchpad (jiri)
 *  2003-02-28  scratchpad compatibility abandoned (jiri)
 *  2003-03-06  enum_request_method changed to begin with 1;
 *               0 reserved for invalid values; (jiri)
 *  2003-03-31  removed sip_msg->repl_add_rm (andrei)
 *  2003-04-01  2 macros added: GET_NEXT_HOP and GET_RURI (janakj)
 *  2003-04-04  structure for parsed inbound uri added (jiri)
 *  2003-04-11  updated the  sip_uri structure (lots of fields added) (andrei)
 *  2003-04-12  added msg_flags to sip_msg (andrei)
 *  2003-11-02  added diversion header field to sip_msg (jh)
 *  2004-11-08  added force_send_socket (andrei)
 *  2005-02-25  uri types added (sip, sips & tel)  (andrei)
 *  2006-04-20  uri comp member (only if USE_COMP is defined) (andrei)
 *  2006-11-10  check_transaction_quadruple inlined (andrei)
 *  2007-01-26  added date, identity, identity_info header fields
 *		to sip_msg (gergo)
 *  2007-03-14  added SIP_MSG_START(msg) macro
 */

/*! \file 
 * \brief Parser :: ???
 *
 * \ingroup parser
 */


#ifndef msg_parser_h
#define msg_parser_h


#include "../comp_defs.h"
#include "../str.h"
#include "../lump_struct.h"
#include "../flags.h"
#include "../ip_addr.h"
#include "../md5utils.h"
#include "../config.h"
#include "parse_def.h"
#include "parse_cseq.h"
#include "parse_via.h"
#include "parse_fline.h"
#include "parse_retry_after.h"
#include "hf.h"
#include "../error.h"


/*! \name convenience short-cut macros */
/*@{ */
#define REQ_LINE(_msg) ((_msg)->first_line.u.request)
#define REQ_METHOD first_line.u.request.method_value
#define REPLY_STATUS first_line.u.reply.statuscode
#define REPLY_CLASS(_reply) ((_reply)->REPLY_STATUS/100)
/*@} */

/*! \brief start of "actual" sip msg (start of first line) */
#define SIP_MSG_START(m)	((m)->first_line.u.request.method.s)

/*! \brief number methods as power of two to allow bitmap matching */
typedef enum request_method {
	METHOD_UNDEF=0,           /*!< 0 - --- */
	METHOD_INVITE=1,          /*!< 1 - 2^0 */
	METHOD_CANCEL=2,          /*!< 2 - 2^1 */
	METHOD_ACK=4,             /*!< 4 - 2^2 */
	METHOD_BYE=8,             /*!< 8 - 2^3 */
	METHOD_INFO=16,           /*!< 16 - 2^4 */
	METHOD_REGISTER=32,       /*!< 32 - 2^5 */
	METHOD_SUBSCRIBE=64,      /*!< 64 - 2^6 */
	METHOD_NOTIFY=128,        /*!< 128 - 2^7 */
	METHOD_MESSAGE=256,       /*!< 256 - 2^8 */
	METHOD_OPTIONS=512,       /*!< 512 - 2^9 */
	METHOD_PRACK=1024,        /*!< 1024 - 2^10 */
	METHOD_UPDATE=2048,       /*!< 2048 - 2^11 */
	METHOD_REFER=4096,        /*!< 4096 - 2^12 */
	METHOD_PUBLISH=8192,      /*!< 8192 - 2^13 */
	METHOD_OTHER=16384        /*!< 16384 - 2^14 */
} request_method_t;

#define FL_FORCE_RPORT  (1 << 0)  /*!< force rport */
#define FL_FORCE_ACTIVE (1 << 1)  /*!< force active SDP */
#define FL_SDP_IP_AFS   (1 << 2)  /*!< SDP IP rewritten */
#define FL_SDP_PORT_AFS (1 << 3)  /*!< SDP port rewritten */
#define FL_SHM_CLONE    (1 << 4)  /*!< msg cloned in SHM as a single chunk */
#define FL_TIMEOUT      (1 << 5)  /*!< message belongs to an "expired" branch
									 (for failure route use) */
#define FL_REPLIED      (1 << 6)  /*!< message branch received at least one reply
									 (for failure route use) */
#define FL_HASH_INDEX   (1 << 7)  /*!< msg->hash_index contains a valid value (tm use)*/

#define FL_MTU_TCP_FB   (1 << 8)
#define FL_MTU_TLS_FB   (1 << 9)
#define FL_MTU_SCTP_FB  (1 << 10)
#define FL_ADD_LOCAL_RPORT  (1 << 11) /*!< add 'rport' to local via hdr */
#define FL_SDP_BODY     (1 << 12)  /*!< msg has SDP in body */
#define FL_USE_UAC_FROM      (1<<13)  /* take FROM hdr from UAC instead of UAS*/
#define FL_USE_UAC_TO        (1<<14)  /* take TO hdr from UAC instead of UAS */
#define FL_TM_RPL_MATCHED    (1<<15)  /* tm matched reply already */
#define FL_RPL_SUSPENDED     (1<<16)  /* for async reply processing */
#define FL_BODY_MULTIPART    (1<<17)  /* body modified is multipart */
#define FL_RR_ADDED          (1<<18)  /* Record-Route header was added */

/* WARNING: Value (1 << 28) is temporarily reserved for use in kamailio call_control
 * module (flag  FL_USE_CALL_CONTROL )! */

/* WARNING: Value (1 << 29) is temporarily reserved for use in kamailio acc
 * module (flag FL_REQ_UPSTREAM)! */

/* WARNING: Value (1 << 30) is temporarily reserved for use in kamailio
 * media proxy module (flag FL_USE_MEDIA_PROXY)! */

/* WARNING: Value (1 << 31) is temporarily reserved for use in kamailio
 * nat_traversal module (flag FL_DO_KEEPALIVE)! */

#define FL_MTU_FB_MASK  (FL_MTU_TCP_FB|FL_MTU_TLS_FB|FL_MTU_SCTP_FB)


#define IFISMETHOD(methodname,firstchar)                                  \
if (  (*tmp==(firstchar) || *tmp==((firstchar) | 32)) &&                  \
        strncasecmp( tmp+1, #methodname +1, methodname##_LEN-1)==0 &&     \
        *(tmp+methodname##_LEN)==' ') {                                   \
                fl->type=SIP_REQUEST;                                     \
                fl->u.request.method.len=methodname##_LEN;                \
                fl->u.request.method_value=METHOD_##methodname;           \
                tmp=buffer+methodname##_LEN;                              \
}

#define IS_HTTP(req)                                                \
    ((req)->first_line.u.request.version.len >= HTTP_VERSION_LEN && \
    !strncasecmp((req)->first_line.u.request.version.s,             \
		HTTP_VERSION, HTTP_VERSION_LEN))

#define IS_SIP(req)                                                \
    ((req)->first_line.u.request.version.len >= SIP_VERSION_LEN && \
    !strncasecmp((req)->first_line.u.request.version.s,             \
		SIP_VERSION, SIP_VERSION_LEN))

#define IS_HTTP_REPLY(rpl)                                                \
    ((rpl)->first_line.u.reply.version.len >= HTTP_VERSION_LEN && \
    !strncasecmp((rpl)->first_line.u.reply.version.s,             \
		HTTP_VERSION, HTTP_VERSION_LEN))

#define IS_SIP_REPLY(rpl)                                                \
    ((rpl)->first_line.u.reply.version.len >= SIP_VERSION_LEN && \
    !strncasecmp((rpl)->first_line.u.reply.version.s,             \
		SIP_VERSION, SIP_VERSION_LEN))

/*! \brief
 * Return a URI to which the message should be really sent (not what should
 * be in the Request URI. The following fields are tried in this order:
 * 1) dst_uri
 * 2) new_uri
 * 3) first_line.u.request.uri
 */
#define GET_NEXT_HOP(m) \
(((m)->dst_uri.s && (m)->dst_uri.len) ? (&(m)->dst_uri) : \
(((m)->new_uri.s && (m)->new_uri.len) ? (&(m)->new_uri) : (&(m)->first_line.u.request.uri)))


/*! \brief
 * Return the Reqeust URI of a message.
 * The following fields are tried in this order:
 * 1) new_uri
 * 2) first_line.u.request.uri
 */
#define GET_RURI(m) \
(((m)->new_uri.s && (m)->new_uri.len) ? (&(m)->new_uri) : (&(m)->first_line.u.request.uri))


enum _uri_type{ERROR_URI_T=0, SIP_URI_T, SIPS_URI_T, TEL_URI_T, TELS_URI_T, URN_URI_T};
typedef enum _uri_type uri_type;
enum _uri_flags{
	URI_USER_NORMALIZE=1,
	URI_SIP_USER_PHONE=2
}; /* bit fields */
typedef enum _uri_flags uri_flags;

/*! \brief The SIP uri object */
struct sip_uri {
	str user;     /*!< Username */
	str passwd;   /*!< Password */
	str host;     /*!< Host name */
	str port;     /*!< Port number */
	str params;   /*!< Parameters */
	str sip_params; /*!< Parameters of the sip: URI.
			  * (If a tel: URI is embedded in a sip: URI, then
			  * params points to the parameters of the tel: URI,
			  * and sip_params to the parameters of the sip: URI. 
			  */
	str headers;
	unsigned short port_no;
	unsigned short proto; /*!< from transport */
	uri_type type; /*!< uri scheme */
	uri_flags flags;
	/*!< parameters */
	str transport;
	str ttl;
	str user_param;
	str maddr;
	str method;
	str lr;
	str r2; /*!< ser specific rr parameter */
	str gr;
	str transport_val; /*!< transport value */
	str ttl_val;	 /*!< TTL value */
	str user_param_val; /*!< User= param value */
	str maddr_val; /*!< Maddr= param value */
	str method_val; /*!< Method value */
	str lr_val; /*!< lr value placeholder for lr=on a.s.o*/
	str r2_val;
	str gr_val;
#ifdef USE_COMP
	unsigned short comp;
#endif
};

typedef struct sip_uri sip_uri_t;

struct msg_body;

typedef void (*free_msg_body_f)(struct msg_body** ptr);

typedef enum msg_body_type {
	MSG_BODY_UNKNOWN = 0,
	MSG_BODY_SDP
} msg_body_type_t;

/*! \brief This structure represents a generic SIP message body, regardless of the
 * body type.
 * 
 * Body parsers are supposed to cast this structure to some other
 * body-type specific structure, but the body type specific structure must
 * retain msg_body_type variable and a pointer to the free function as the 
 * first two variables within the structure.
 */
typedef struct msg_body {
	msg_body_type_t type;
	free_msg_body_f free;
} msg_body_t;


/* pre-declaration, to include sys/time.h in .c */
struct timeval;

/* structure for cached decoded flow for outbound */
typedef struct ocd_flow {
		int decoded;
		struct receive_info rcv;
} ocd_flow_t;

/* structure holding fields that don't have to be cloned in shm
 * - its content is memset'ed to in shm clone
 * - add to msg_ldata_reset() if a field uses dynamic memory */
typedef struct msg_ldata {
	ocd_flow_t flow;
} msg_ldata_t;

/*! \brief The SIP message */
typedef struct sip_msg {
	unsigned int id;               /*!< message id, unique/process*/
	int pid;                       /*!< process id */
	struct timeval tval;           /*!< time value associated to message */
	snd_flags_t fwd_send_flags;    /*!< send flags for forwarding */
	snd_flags_t rpl_send_flags;    /*!< send flags for replies */
	struct msg_start first_line;   /*!< Message first line */
	struct via_body* via1;         /*!< The first via */
	struct via_body* via2;         /*!< The second via */
	struct hdr_field* headers;     /*!< All the parsed headers*/
	struct hdr_field* last_header; /*!< Pointer to the last parsed header*/
	hdr_flags_t parsed_flag;    /*!< Already parsed header field types */

	     /* Via, To, CSeq, Call-Id, From, end of header*/
	     /* pointers to the first occurrences of these headers;
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
	struct hdr_field* supported;
	struct hdr_field* require;
	struct hdr_field* proxy_require;
	struct hdr_field* unsupported;
	struct hdr_field* allow;
	struct hdr_field* event;
	struct hdr_field* accept;
	struct hdr_field* accept_language;
	struct hdr_field* organization;
	struct hdr_field* priority;
	struct hdr_field* subject;
	struct hdr_field* user_agent;
	struct hdr_field* server;
	struct hdr_field* content_disposition;
	struct hdr_field* diversion;
	struct hdr_field* rpid;
	struct hdr_field* refer_to;
	struct hdr_field* session_expires;
	struct hdr_field* min_se;
	struct hdr_field* sipifmatch;
	struct hdr_field* subscription_state;
	struct hdr_field* date;
	struct hdr_field* identity;
	struct hdr_field* identity_info;
	struct hdr_field* pai;
	struct hdr_field* ppi;
	struct hdr_field* path;
	struct hdr_field* privacy;

	struct msg_body* body;

	char* eoh;        /*!< pointer to the end of header (if found) or null */
	char* unparsed;   /*!< here we stopped parsing*/

	struct receive_info rcv; /*!< source & dest ip, ports, proto a.s.o*/

	char* buf;        /*!< scratch pad, holds a modified message,
					   *  via, etc. point into it */
	unsigned int len; /*!< message len (orig) */

	     /* modifications */

	str new_uri; /*!< changed first line uri, when you change this
	                don't forget to set parsed_uri_ok to 0*/

	str dst_uri; /*!< Destination URI, must be forwarded to this URI if len != 0 */

	/* current uri */
	int parsed_uri_ok; /*!< 1 if parsed_uri is valid, 0 if not, set if to 0
	                      if you modify the uri (e.g change new_uri)*/
	struct sip_uri parsed_uri; /*!< speed-up > keep here the parsed uri*/
	int parsed_orig_ruri_ok; /*!< 1 if parsed_orig_uri is valid, 0 if not, set if to 0
                              if you modify the uri (e.g change new_uri)*/
	struct sip_uri parsed_orig_ruri; /*!< speed-up > keep here the parsed orig uri*/

	struct lump* add_rm;       /*!< used for all the forwarded requests/replies */
	struct lump* body_lumps;     /*!< Lumps that update Content-Length */
	struct lump_rpl *reply_lump; /*!< only for localy generated replies !!!*/

	/*! \brief str add_to_branch;
	   whatever whoever want to append to Via branch comes here */
	char add_to_branch_s[MAX_BRANCH_PARAM_LEN];
	int add_to_branch_len;

	unsigned int  hash_index; /*!< index to TM hash table; stored in core to avoid unnecessary calculations */
	unsigned int msg_flags; /*!< internal flags used by core */
	flag_t flags; /*!< config flags */
	str set_global_address;
	str set_global_port;
	struct socket_info* force_send_socket; /*!< force sending on this socket */
	str path_vec;
	str instance;
	unsigned int reg_id;
	str ruid;
	str location_ua;

	/* structure with fields that are needed for local processing
	 * - not cloned to shm, reset to 0 in the clone */
	msg_ldata_t ldv;

	/* IMPORTANT: when adding new fields in this structure (sip_msg_t),
	 * be sure it is freed in free_sip_msg() and it is cloned or reset
	 * to shm structure for transaction - see sip_msg_clone.c. In tm
	 * module, take care of these fields for faked environemt used for
	 * runing failure handlers - see modules/tm/t_reply.c */
} sip_msg_t;

/*! \brief pointer to a fakes message which was never received ;
   (when this message is "relayed", it is generated out
    of the original request)
*/
#define FAKED_REPLY     ((struct sip_msg *) -1)

extern int via_cnt;
/** global  request flags.
 *  msg->msg_flags should be OR'ed with it before
 * a flag value is checked, e.g.:
 * if ((msg->msg_flags|global_req_flags) & FL_XXX) ...
 */
extern unsigned int global_req_flags;


int parse_msg(char* const buf, const unsigned int len, struct sip_msg* const msg);

int parse_headers(struct sip_msg* const msg, const hdr_flags_t flags, const int next);

char* get_hdr_field(char* const buf, char* const end, struct hdr_field* const hdr);

void free_sip_msg(struct sip_msg* const msg);

/*! \brief make sure all HFs needed for transaction identification have been
   parsed; return 0 if those HFs can't be found
*/
inline static int check_transaction_quadruple(struct sip_msg* const msg)
{
	if ( parse_headers(msg, HDR_FROM_F|HDR_TO_F|HDR_CALLID_F|HDR_CSEQ_F,0)!=-1
		&& msg->from && msg->to && msg->callid && msg->cseq ) {
		return 1;
	} else {
		ser_error=E_BAD_TUPEL;
		return 0;
	}
}



/*! \brief returns a pointer to the begining of the msg's body
 */
inline static char* get_body(struct sip_msg* const msg)
{
	int offset;
	unsigned int len;

	if ( parse_headers(msg, HDR_EOH_F, 0)==-1 )
		return 0;

	if (msg->unparsed){
		len=(unsigned int)(msg->unparsed-msg->buf);
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

/*! \brief If the new_uri is set, then reset it */
void reset_new_uri(struct sip_msg* const msg);

/*! \brief
 * Make a private copy of the string and assign it to dst_uri
 */
int set_dst_uri(struct sip_msg* const msg, const str* const uri);

/*! \brief If the dst_uri is set to an URI then reset it */
void reset_dst_uri(struct sip_msg* const msg);

hdr_field_t* get_hdr(const sip_msg_t* const msg, const enum _hdr_types_t ht);
hdr_field_t* next_sibling_hdr(const hdr_field_t* const hf);
/** not used yet */
hdr_field_t* get_hdr_by_name(const sip_msg_t* const msg, const char* const name, const int name_len);
hdr_field_t* next_sibling_hdr_by_name(const hdr_field_t* const hf);

int set_path_vector(struct sip_msg* msg, str* path);

void reset_path_vector(struct sip_msg* const msg);

int set_instance(struct sip_msg* msg, str* instance);

void reset_instance(struct sip_msg* const msg);

int set_ruid(struct sip_msg* msg, str* ruid);

void reset_ruid(struct sip_msg* const msg);

int set_ua(struct sip_msg* msg, str *location_ua);

void reset_ua(struct sip_msg* const msg);

/** force a specific send socket for forwarding a request.
 * @param msg - sip msg.
 * @param fsocket - forced socket, pointer to struct socket_info, can be 0 (in
 *                  which case it's equivalent to reset_force_socket()).
 */
#define set_force_socket(msg, fsocket) \
	do { \
		(msg)->force_send_socket=(fsocket); \
		if ((msg)->force_send_socket) \
			(msg)->fwd_send_flags.f |= SND_F_FORCE_SOCKET; \
		else \
			(msg)->fwd_send_flags.f &= ~SND_F_FORCE_SOCKET; \
	} while (0)

/** reset a previously forced send socket. */
#define reset_force_socket(msg) set_force_socket(msg, 0)

/**
 * struct to identify a msg context
 * - the pair of pid and message-id
 */
typedef struct msg_ctx_id {
	int pid;
	int msgid;
} msg_ctx_id_t;

/**
 * set msg context id
 * - return: -1 on error; 0 - on set 
 */
int msg_ctx_id_set(const sip_msg_t* const msg, msg_ctx_id_t* const mid);

/**
 * check msg context id
 * - return: -1 on error; 0 - on no match; 1 - on match
 */
int msg_ctx_id_match(const sip_msg_t* const msg, const msg_ctx_id_t* const mid);

/**
 * set msg time value
 */
int msg_set_time(sip_msg_t* const msg);

/**
 * reset content of msg->ldv (msg_ldata_t structure)
 */
void msg_ldata_reset(sip_msg_t*);

#endif

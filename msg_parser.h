/*
 * $Id$
 */

#ifndef msg_parser_h
#define msg_parser_h

#include "str.h"
#include "data_lump.h"

#define SIP_REQUEST 1
#define SIP_REPLY   2
#define SIP_INVALID 0



/*header types and flags*/
#define HDR_EOH           -1
#define HDR_ERROR          0
#define HDR_VIA            1
#define HDR_VIA1           1
#define HDR_VIA2           2  /*only used as flag*/
#define HDR_TO             4
#define HDR_FROM           8
#define HDR_CSEQ          16
#define HDR_CALLID        32
#define HDR_CONTACT       64
#define HDR_MAXFORWARDS  128
#define HDR_ROUTE        256
#define HDR_OTHER       65536 /*unknown header type*/

/* maximum length of values appended to Via-branch parameter */
#define	MAX_BRANCH_PARAM_LEN	32

/* via param types
 * WARNING: keep in sync w/ FIN_*, GEN_PARAM and PARAM_ERROR from via_parse.c*/
enum{
		PARAM_HIDDEN=230, PARAM_TTL, PARAM_BRANCH, PARAM_MADDR, PARAM_RECEIVED,
		GEN_PARAM,
		PARAM_ERROR
};

/* casting macro for accessing CSEQ body */
#define get_cseq( p_msg)    ((struct cseq_body*)(p_msg)->cseq->parsed)



#define INVITE_LEN	6
#define ACK_LEN		3
#define CANCEL_LEN	6
#define BYE_LEN		3
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


#define VIA_PARSE_OK	1
#define VIA_PARSE_ERROR -1
#define PARSE_ERROR -1
#define PARSE_OK 1

#define SIP_VERSION	"SIP/2.0"
#define SIP_VERSION_LEN 7


struct msg_start{
	int type;
	union {
		struct {
			str method;
			str uri;
			str version;
			int method_value;
		}request;
		struct {
			str version;
			str status;
			str reason;
			unsigned int /* statusclass,*/ statuscode;
		}reply;
	}u;
};

struct hdr_field{   /* format: name':' body */
	int type;
	str name;
	str body;
	void* parsed;
	struct hdr_field* next;
};



struct via_param{
	int type;
	str name;
	str value;
	int size; /* total size*/
	struct via_param* next;
};

struct via_body{  /* format: name/version/transport host:port;params comment */
	int error;
	str hdr;   /* contains "Via" or "v" */
	str name;
	str version;
	str transport;
	str host;
	int port;
	str port_str;
	str params;
	str comment;
	int bsize;    /* body size, not including hdr */
	struct via_param* param_lst; /* list of parameters*/
	struct via_param* last_param; /*last via parameter, internal use*/
	/* shortcuts to "important" params*/
	struct via_param* branch;
	
	struct via_body* next; /* pointer to next via body string if
							  compact via or null */
};





struct cseq_body{
	int error;
	str number;
	str method;
};



struct sip_msg{
	unsigned int id; /* message id, unique/process*/
	struct msg_start first_line;
	struct via_body* via1;
	struct via_body* via2;
	struct hdr_field* headers; /* all the parsed headers*/
	struct hdr_field* last_header; /* pointer to the last parsed header*/
	int parsed_flag;
	/* via, to, cseq, call-id, from, end of header*/
	/* first occurance of it; subsequent occurances saved in 'headers' */
	struct hdr_field* h_via1;
	struct hdr_field* h_via2;
	struct hdr_field* callid;
	struct hdr_field* to;
	struct hdr_field* cseq;
	struct hdr_field* from;
	struct hdr_field* contact;
	struct hdr_field* route;    /* janakj, was missing here */
	char* eoh; /* pointer to the end of header (if found) or null */

	char* unparsed; /* here we stopped parsing*/

	unsigned int src_ip;
	unsigned int dst_ip;
	char* orig; /* original message copy */
	char* buf;  /* scratch pad, holds a modfied message,
				   via, etc. point into it */
				   
	unsigned int len; /* message len (orig) */

	/* modifications */
	str new_uri; /* changed first line uri*/

	struct lump* add_rm;      /* used for all the forwarded messages */
	struct lump* repl_add_rm; /* only for localy generated replies !!!*/

	/* str add_to_branch; */ /* whatever whoever want to append to branch comes here */
	char add_to_branch_s[MAX_BRANCH_PARAM_LEN];
	int add_to_branch_len;

	
};


struct sip_uri{
	str user;
	str passwd;
	str host;
	str port;
	str params;
	str headers;
};


char* parse_fline(char* buffer, char* end, struct msg_start* fl);
char* parse_first_line(char* buffer, unsigned int len, struct msg_start * fl);
char* parse_hostport(char* buf, str* host, short int* port);

int parse_msg(char* buf, unsigned int len, struct sip_msg* msg);
int parse_uri(char *buf, int len, struct sip_uri* uri);
int parse_headers(struct sip_msg* msg, int flags);

void free_uri(struct sip_uri* u);


char* parse_hname(char* buf, char* end, struct hdr_field* hdr);
char* parse_via(char* buffer, char* end, struct via_body *vb);
char* parse_cseq(char* buffer, char* end, struct cseq_body *cb);

void free_via_param_list(struct via_param *vp);
void free_via_list(struct via_body *vb);
void clean_hdr_field(struct hdr_field* hf);
void free_hdr_field_lst(struct hdr_field* hf);
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

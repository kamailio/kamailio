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
			short method_value;
		}request;
		struct {
			str version;
			str status;
			str reason;
			unsigned short statusclass, statuscode;
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
	struct hdr_field* h_via1;
	struct hdr_field* h_via2;
	struct hdr_field* callid;
	struct hdr_field* to;
	struct hdr_field* cseq;
	struct hdr_field* from;
	struct hdr_field* contact;
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
	
};


struct sip_uri{
	str user;
	str passwd;
	str host;
	str port;
	str params;
	str headers;
};



char* parse_first_line(char* buffer, unsigned int len, struct msg_start * fl);
#ifdef OLD_PARSER 
char* get_hdr_field(char *buffer, unsigned int len, struct hdr_field*  hdr_f);
int field_name(char *s, int len);
#endif
char* parse_hostport(char* buf, str* host, short int* port);

#ifdef OLD_PARSER 
char* parse_via_body(char* buffer,unsigned int len, struct via_body * vb);
#endif
int parse_msg(char* buf, unsigned int len, struct sip_msg* msg);
int parse_uri(char *buf, int len, struct sip_uri* uri);
int parse_headers(struct sip_msg* msg, int flags);

void free_uri(struct sip_uri* u);


#ifndef OLD_PARSER
char* parse_hname(char* buf, char* end, struct hdr_field* hdr);
char* parse_via(char* buffer, char* end, struct via_body *vb);
char* parse_cseq(char* buffer, char* end, struct cseq_body *cb);
#endif

void free_via_list(struct via_body *vb);
void clean_hdr_field(struct hdr_field* hf);
void free_hdr_field_lst(struct hdr_field* hf);


#endif

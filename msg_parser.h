/*
 * $Id$
 */

#ifndef msg_parser_h
#define msg_parser_h


#define SIP_REQUEST 1
#define SIP_REPLY   2
#define SIP_INVALID 0


#define HDR_ERROR 0
/* end of header */
#define HDR_EOH   -1
#define HDR_OTHER 1
#define HDR_VIA   2
#define HDR_TO    3

#define VIA_PARSE_OK	1
#define VIA_PARSE_ERROR -1

#define SIP_VERSION	"SIP/2.0"


struct msg_start{
	int type;
	union {
		struct {
			char* method;
			char* uri;
			char* version;
		}request;
		struct {
			char* version;
			char* status;
			char* reason;
		}reply;
	}u;
};

struct hdr_field{   /* format: name':' body */
	int type;
	char* name;
	char* body;
};

struct via_body{  /* format: name/version/transport host:port;params comment */
	int error;
	char *hdr;   /* contains "Via" or "v" */
	char* name;
	char* version;
	char* transport;
	char* host;
	int port;
	char* params;
	char* comment;
	int size;    /* full size, including hdr */
	char* next; /* pointer to next via body string if compact via or null */
};

struct sip_msg{
	struct msg_start first_line;
	struct via_body via1;
	struct via_body via2;
	unsigned int src_ip;
	unsigned int dst_ip;
};



char* parse_first_line(char* buffer, unsigned int len, struct msg_start * fl);
char* get_hdr_field(char *buffer, unsigned int len, struct hdr_field*  hdr_f);
int field_name(char *s);
char* parse_hostport(char* buf, char** host, short int* port);
char* parse_via_body(char* buffer,unsigned int len, struct via_body * vb);
int parse_msg(char* buf, unsigned int len, struct sip_msg* msg);



#endif

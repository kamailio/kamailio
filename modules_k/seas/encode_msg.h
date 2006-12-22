/* $Id$ */

#include "../../parser/hf.h"
#include "../../parser/msg_parser.h"

#define MAX_ERROR 32
#define MAX_HEADERS 32
#define MAX_ENCODED_MSG 1500
#define MAX_MESSAGE_LEN 1500

#define TYPE_IDX 0
#define MSG_START_IDX 	(TYPE_IDX+2)
#define MSG_LEN_IDX 	(MSG_START_IDX+2)
#define CONTENT_IDX 	(MSG_LEN_IDX+2)
#define METHOD_CODE_IDX (CONTENT_IDX+2)
#define URI_REASON_IDX 	(METHOD_CODE_IDX+2)
#define VERSION_IDX 	(URI_REASON_IDX+2)
#define REQUEST_URI_IDX (VERSION_IDX+2)

#define GET_PAY_SIZE( A ) (ntohs(((short*)( A ))[1]) + ntohs(((short*)( A ))[2]))
char get_header_code(struct hdr_field *f);
int encode_msg(struct sip_msg *msg,char *payload,int len);
int print_encoded_msg(int fd,char *code,char *prefix);
int dump_msg_test(char *code,int fd,char header,char segregationLevel);
extern unsigned int theSignal;

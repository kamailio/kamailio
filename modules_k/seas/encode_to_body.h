/* $Id$ */

#include "../../str.h"
#include "../../parser/msg_parser.h"
#define MAX_XHDR_LEN 255
#define HAS_DISPLAY_F	0x01
#define HAS_TAG_F	0x02
#define HAS_OTHERPAR_F	0x04
int encode_to_body(char *hdrstart,int hdrlen,struct to_body *body,unsigned char *where);
int print_encoded_to_body(int fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix);
int dump_to_body_test(char *hdr,int hdrlen,unsigned char* payload,int paylen,int fd,char segregationLevel);

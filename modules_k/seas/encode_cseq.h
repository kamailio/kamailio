/* $Id$ */

#include "../../str.h"
#include "../../parser/msg_parser.h"
int encode_cseq(char *hdrstart,int hdrlen,struct cseq_body *body,unsigned char *where);
int print_encoded_cseq(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);

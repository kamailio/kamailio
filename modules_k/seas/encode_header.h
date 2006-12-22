/* $Id$ */

#include "../../parser/hf.h"
#include "../../str.h"
int print_encoded_header(int fd,char *msg,int len,unsigned char *payload,int paylen,char type,char *prefix);
int encode_header(struct sip_msg *msg,struct hdr_field *hdr,unsigned char *payload,int paylen);
int dump_headers_test(char *msg,int msglen,unsigned char *payload,int len,char type,int fd,char segregationLevel);
int dump_standard_hdr_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,int fd);

/* $Id$ */

int encode_expires(char *hdrstart,int hdrlen,exp_body_t *body,unsigned char *where);
int print_encoded_expires(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);

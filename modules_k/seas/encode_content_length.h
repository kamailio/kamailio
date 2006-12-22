/* $Id$ */

int encode_contentlength(char *hdr,int hdrlen,long int len,char *where);
int print_encoded_contentlength(int fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix);

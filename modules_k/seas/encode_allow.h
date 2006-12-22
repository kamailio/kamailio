/* $Id$ */

int encode_allow(char *hdrstart,int hdrlen,unsigned int *bodi,char *where);
int print_encoded_allow(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);

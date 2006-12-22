/* $Id$ */

int encode_content_disposition(char *hdrstart,int hdrlen,struct disposition *body,unsigned char *where);
int print_encoded_content_disposition(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);

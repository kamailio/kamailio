/* $Id$ */

int encode_content_type(char *hdrstart,int hdrlen,unsigned int bodi,char *where);
int encode_accept(char *hdrstart,int hdrlen,unsigned int *bodi,char *where);
int encode_mime_type(char *hdrstart,int hdrlen,unsigned int bodi,char *where);
int print_encoded_mime_type(int fd,char *hdr,int hdrlen,unsigned int* payload,int paylen,char *prefix);
int print_encoded_content_type(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);
int print_encoded_accept(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);

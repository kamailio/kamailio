/* $Id$ */

int encode_via_body(char *hdr,int hdrlen,struct via_body *via_parsed,unsigned char *where);
int encode_via(char *hdrstart,int hdrlen,struct via_body *body,unsigned char *where);
int print_encoded_via_body(int fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix);
int print_encoded_via(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);
int dump_via_body_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,int fd,char segregationLevel);

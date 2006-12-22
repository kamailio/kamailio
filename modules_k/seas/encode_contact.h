/* $Id$ */

#define STAR_F 0x01
param_t *reverseParameters(param_t *param);
int encode_contact_body(char *hdr,int hdrlen,contact_body_t *contact_parsed,unsigned char *where);
int encode_contact(char *hdr,int hdrlen,contact_t *mycontact,unsigned char *where);
int print_encoded_contact_body(int fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix);
int print_encoded_contact(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);
int dump_contact_test(char *hdr,int hdrlen,unsigned char* payload,int paylen,int fd,char segregationLevel,char *prefix);
int dump_contact_body_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,int fd,char segregationLevel,char *prefix);

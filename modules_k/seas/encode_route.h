/* $Id$ */

#define STAR_F 0x01

int encode_route_body(char *hdr,int hdrlen,rr_t *route_parsed,unsigned char *where);
int encode_route(char *hdrstart,int hdrlen,rr_t *body,unsigned char *where);
/* TESTING FUNCTIONS */
int print_encoded_route_body(int fd,char *hdr,int hdrlen,unsigned char *payload,int paylen,char *prefix);
int print_encoded_route(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);
int dump_route_body_test(char *hdr,int hdrlen,unsigned char *payload,int paylen,int fd,char segregationLevel,char *prefix);
int dump_route_test(char *hdr,int hdrlen,unsigned char* payload,int paylen,int fd,char segregationLevel,char *prefix);

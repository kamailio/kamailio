/* $Id$ */

int encode_digest(char *hdrstart,int hdrlen,dig_cred_t *digest,unsigned char *where);
int print_encoded_digest(int fd,char *hdr,int hdrlen,unsigned char* payload,int paylen,char *prefix);
int dump_digest_test(char *hdr,int hdrlen,unsigned char* payload,int paylen,int fd,char segregationLevel);

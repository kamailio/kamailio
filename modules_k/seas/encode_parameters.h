/* $Id$ */

#include "../../parser/parse_param.h"  /*for param_t def*/
int encode_parameters(unsigned char *where,void *pars,char *hdrstart,void *_body,char to);
param_t *reverseParameters(param_t *p);
int print_encoded_parameters(int fd,unsigned char *payload,char *hdr,int paylen,char *prefix);

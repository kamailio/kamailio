/*
 * $Id$
 */

#ifndef PARSE_URI_H
#define PARSE_URI_H

/*
 * SIP URI parser
 */


#include "../str.h"
#include "../parser/msg_parser.h"



/* buf= pointer to begining of uri (sip:x@foo.bar:5060;a=b?h=i)
 * len= len of uri
 * returns: fills uri & returns <0 on error or 0 if ok 
 */
int parse_uri(char *buf, int len, struct sip_uri* uri);
int parse_sip_msg_uri(struct sip_msg* msg);

void free_uri(struct sip_uri* u);

#endif /* PARSE_URI_H */

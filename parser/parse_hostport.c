/*
 * $Id$
 */

#include "parse_hostport.h"
#include <string.h>    /* strlen */
#include "../dprint.h"
#include "../ut.h"     /* str2s */

char* parse_hostport(char* buf, str* host, short int* port)
{
	char *tmp;
	int err;

	host->s=buf;
	for(tmp=buf;(*tmp)&&(*tmp!=':');tmp++);
	host->len=tmp-buf;
	if (*tmp==0) {
		*port=0;
	} else {
		*tmp=0;
		*port=str2s((unsigned char*)(tmp+1), strlen(tmp+1), &err);
		if (err ){
			LOG(L_INFO, 
			    "ERROR: hostport: trailing chars in port number: %s\n",
			    tmp+1);
			     /* report error? */
		}
	}

	return host->s;
}

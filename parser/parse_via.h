/*
 * $Id$
 */

#ifndef PARSE_VIA_H
#define PARSE_VIA_H

#include "../str.h"

/* via param types
 * WARNING: keep in sync w/ FIN_*, GEN_PARAM and PARAM_ERROR from via_parse.c
 */
enum {
	PARAM_HIDDEN=230, PARAM_TTL, PARAM_BRANCH, 
	PARAM_MADDR, PARAM_RECEIVED, GEN_PARAM,
	PARAM_ERROR
};


struct via_param {
	int type;               /* Type of the parameter */
	str name;               /* Name of the parameter */
	str value;              /* Value of the parameter */
	int size;               /* total size*/
	struct via_param* next; /* Next parameter in the list */
};


/* Format: name/version/transport host:port;params comment */
struct via_body { 
	int error;
	str hdr;   /* Contains "Via" or "v" */
	str name;
	str version;   
	str transport;
	str host;
	int port;
	str port_str;
	str params;
	str comment;
	int bsize;                    /* body size, not including hdr */
	struct via_param* param_lst;  /* list of parameters*/
	struct via_param* last_param; /*last via parameter, internal use*/

	     /* shortcuts to "important" params*/
	struct via_param* branch;
	struct via_param* received;
	
	struct via_body* next; /* pointer to next via body string if
				  compact via or null */
};


/*
 * Main Via header field parser
 */
char* parse_via(char* buffer, char* end, struct via_body *vb);


/*
 * Free allocated memory
 */
void free_via_list(struct via_body *vb);


#endif /* PARSE_VIA_H */

/*
 * $Id$
 */

#ifndef PARSE_TO
#define PARSE_TO

#include "../str.h"

struct to_param{
	int type;              /* Type of parameter */
	str name;              /* Name of parameter */
	str value;             /* Parameter value */
	struct to_param* next; /* Next parameter in the list */
};


struct to_body{
	int error;                    /* Error code */
	str body;                     /* The whole header field body */
	str uri;                      /* URI */
	str tag_value;                /* Value of tag */
	struct to_param *param_lst;   /* Linked list of parameters */
	struct to_param *last_param;  /* Last parameter in the list */
};


/* casting macro for accessing To body */
#define get_to( p_msg)      ((struct to_body*)(p_msg)->to->parsed)


/*
 * To header field parser
 */
char* parse_to(char* buffer, char *end, struct to_body *to_b);


void free_to(struct to_body* tb);


#endif

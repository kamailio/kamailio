/*
 * $Id$
 */

#ifndef PARSE_CSEQ
#define PARSE_CSEQ

#include "../str.h"


struct cseq_body{
	int error;  /* Error code */
	str number; /* CSeq number */
	str method; /* Associated method */
};


/* casting macro for accessing CSEQ body */
#define get_cseq(p_msg) ((struct cseq_body*)(p_msg)->cseq->parsed)


/*
 * Parse CSeq header field
 */
char* parse_cseq(char *buf, char* end, struct cseq_body* cb);


/*
 * Free all associated memory
 */
void free_cseq(struct cseq_body* cb);


#endif

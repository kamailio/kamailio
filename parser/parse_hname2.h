/*
 * $Id$
 */

#ifndef PARSE_HNAME2_H
#define PARSE_HNAME2_H

#include "hf.h"


/*
 * Fast 32-bit header field name parser
 */
char* parse_hname2(char* begin, char* end, struct hdr_field* hdr);


/*
 * Initialize hash table
 */
void init_hfname_parser(void);


#endif /* PARSE_HNAME2_H */

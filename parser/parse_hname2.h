/*
 * $Id$
 */

#ifndef PARSE_HNAME2_H
#define PARSE_HNAME2_H

#include "hf.h"


/*
 * Yet another parse_hname - Ultra Fast version :-)
 */
char* parse_hname2(char* begin, char* end, struct hdr_field* hdr);


/*
 * Initialize hash table
 */
void init_htable(void);


#endif

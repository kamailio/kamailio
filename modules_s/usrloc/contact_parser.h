/* 
 * $Id$ 
 */

#ifndef __CONTACT_PARSER_H__
#define __CONTACT_PARSER_H__

#include <time.h>
#include "location.h"

int parse_contact_hdr(char* _b, location_t* _loc, int _expires, int* _star,
		      const char* _callid, int _cseq);

#endif

/*
 * $Id$
 */

#ifndef PARSE_FROM
#define PARSE_FROM

#include "../str.h"
#include "parse_to.h"
/*
 * To header field parser
 */
char* parse_from_header(char* buffer, char *end, struct to_body *from_b);

#endif

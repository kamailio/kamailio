/*
 * $Id: 
 * Common function needed by authorize
 * and challenge related functions
 */

#ifndef UTILS_H
#define UTILS_H

#include "../../parser/msg_parser.h"
#include "../../str.h"


/*
 * Cut username part of a URL
 */
int auth_get_username(str* _s);

/*
 * remove unwanted chars of the body
 */
char* cleanbody(str body); 


#endif /* UTILS_H */

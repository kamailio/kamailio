/*
 * $Id: 
 * Common function needed by authorize
 * and challenge related functions
 */

#ifndef UTILS_H
#define UTILS_H
#include "../../str.h"
#include "../../parser/msg_parser.h"

/*
 * remove unwanted chars of the body
 */
char* cleanbody(str body); 


#endif /* UTILS_H */

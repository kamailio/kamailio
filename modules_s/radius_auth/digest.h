/*
 * Include file for Radius digest.
 */

#include "../../str.h"
#include "../../parser/digest/digest_parser.h"

int radius_authorize(dig_cred_t* cred, str* method); 
int radius_authorize_freeradius(dig_cred_t* cred, str* method); 


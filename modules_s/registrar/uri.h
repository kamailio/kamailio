/*
 * $Id$
 *
 * URI related functions
 */

#ifndef URI_H
#define URI_H

#include "../../str.h"


/*
 * This function skips name part
 * uri parsed by parse_contact must be used
 * (the uri must not contain any leading or
 *  trailing part and if angle bracket were
 *  used, right angle bracket must be the
 *  last character in the string)
 *
 * _s will be modified so it should be a tmp
 * copy
 */
void get_raw_uri(str* _s);


#endif /* URI_H */

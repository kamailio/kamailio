/*
 * $Id$
 *
 * Common stuff
 */

#ifndef COMMON_H
#define COMMON_H

#include "../../str.h"


/*
 * Find a character occurence that is not quoted
 */
char* ul_fnq(str* _s, char _c);


/*
 * Extract username part from URI
 */
int ul_get_user(str* _s);


/*
 * Copy str structure, doesn't copy
 * the whole string !
 */
static inline void str_copy(str* _d, str* _s)
{
	_d->s = _s->s;
	_d->len = _s->len;
}


#endif /* COMMON_H */

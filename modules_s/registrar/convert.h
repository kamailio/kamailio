/*
 * $Id$
 *
 * Convert functions
 */

#ifndef CONVERT_H
#define CONVERT_H

#include "../../str.h"


/*
 * ASCII to integer
 */
static inline int atoi(str* _s, int* _r)
{
	int i;
	
	*_r = 0;
	for(i = 0; i < _s->len; i++) {
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			*_r *= 10;
			*_r += _s->s[i] - '0';
		} else {
			return -1;
		}
	}
	
	return 0;
}


/*
 * ASCII to float
 */
static inline int atof(str* _s, float* _r)
{
	int i, dot = 0;
	float order = 0.1;

	*_r = 0;
	for(i = 0; i < _s->len; i++) {
		if (_s->s[i] == '.') {
			if (dot) return -1;
			dot = 1;
			continue;
		}
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			if (dot) {
				*_r += (_s->s[i] - '0') * order;
				order /= 10;
			} else {
				*_r *= 10;
				*_r += _s->s[i] - '0';
			}
		} else {
			return -2;
		}
	}
	return 0;
}


#endif /* CONVERT_H */

/*
 * $Id$
 *
 * Convert functions
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

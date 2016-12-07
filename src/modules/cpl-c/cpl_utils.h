/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * History:
 * -------
 * 2003-06-24: file created (bogdan)

 */

#ifndef _CPL_UTILS_H
#define _CPL_UTILS_H

#include <ctype.h>
#include "../../str.h"


/* looks for s2 into s1 */
static inline char *strcasestr_str(str *s1, str *s2)
{
	int i,j;
	for(i=0;i<s1->len-s2->len;i++) {
		for(j=0;j<s2->len;j++) {
			if ( !((s1->s[i+j]==s2->s[j]) ||
			( isalpha((int)s1->s[i+j]) && ((s1->s[i+j])^(s2->s[j]))==0x20 )) )
				break;
		}
		if (j==s2->len)
			return s1->s+i;
	}
	return 0;
}



#endif





/* utils.c v 0.2 2003/1/14
 *
 * Utility function to un-escape a URI user
 *
 * Copyright (C) 2003 Juha Heinanen
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


#include "utils.h"
#include "../../dprint.h"

/* Convert a hex char to decimal */
inline int hex_to_dec(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return 0;
}


/* Un-escape URI user */
void un_escape(str *user, str *new_user ) 
{
	int i, j, value;

	new_user->len = 0;
	j = 0;

	for (i = 0; i < user->len; i++) {
		if (user->s[i] == '%') {
			if (i + 2 >= user->len) return;
			value = hex_to_dec(user->s[i + 1]) * 16 + hex_to_dec(user->s[i + 2]);
			if (value < 32 || value > 126) return;
			new_user->s[j] = value;
			i = i + 2;
		} else {
			new_user->s[j] = user->s[i];
		}
		j++;
	}
	new_user->len = j;
}

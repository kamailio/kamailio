/*
 * Presence Agent, common functions
 *
 * $Id$
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


#ifndef COMMON_H
#define COMMON_H

#include "../../str.h"


char* find_not_quoted(str* _s, char _c);


/*
 * Extract username part from a URI
 */
int extract_username(str* _s);


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


#endif /* COMMON_H */

/* $Id: utils.c
 *
 * Set of utils to extract the user-name from the FROM field
 * borrowed from the auth module.
 * @author Stelios Sidiroglou-Douskos <ssi@fokus.gmd.de>
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

#include "utils.h"
#include "../../ut.h"

/*
 * This method simply cleans off the trailing character of the string body.
 * params: str body
 * returns: the new char* or NULL on failure
 */
char * cleanbody(str body) 
{	
	char* tmp;
	/*
	 * This works because when the structure is created it is memset to 0
	 */
	if (body.s == NULL)
		return NULL;
		
	tmp = &body.s[0];
	tmp[body.len] = '\0';

	return tmp;
}


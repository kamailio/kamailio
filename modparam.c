/*
 * $Id$
 *
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


#include "modparam.h"
#include "dprint.h"
#include <string.h>


int set_mod_param(char* _mod, char* _name, modparam_t _type, void* _val)
{
	void* ptr;
	
	if (!_mod) {
		LOG(L_ERR, "set_mod_param(): Invalid _mod parameter value\n");
		return -1;
	}

	if (!_name) {
		LOG(L_ERR, "set_mod_param(): Invalid _name parameter value\n");
		return -2;
	}

	ptr = find_param_export(_mod, _name, _type);
	if (!ptr) {
		LOG(L_ERR, "set_mod_param(): Parameter not found\n");
		return -3;
	}

	switch(_type) {
	case STR_PARAM:
		*((char**)ptr) = strdup((char*)_val);
		break;

	case INT_PARAM:
		*((int*)ptr) = (int)(long)_val;
		break;
	}

	return 0;
}

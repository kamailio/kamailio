/* enum_mod.c v 0.2 2002/12/27
 *
 * Enum module
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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


#include "enum_mod.h"
#include <stdio.h>
#include "../../sr_module.h"
#include "enum.h"
/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Module parameter variables
 */

struct module_exports exports = {
	"enum", 
	(char*[]) {"enum_query", "is_from_user_e164"},
	(cmd_function[]) {enum_query, is_from_user_e164},
	(int[]) {0, 0},
	(fixup_function[]) {0, 0},
	2, /* number of functions*/
	(char*[]){},
	(modparam_t[]){},
	(void*[]){},
	0,
	
	mod_init, /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	printf("enum module - initializing\n");
	return 0;
}

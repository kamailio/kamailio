/*
 * Copyright (C) 2009 Sippy Software, Inc., http://www.sippysoft.com
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
*/

// Python includes
#include <Python.h>

// router includes
#include "../../core/action.h"
#include "../../core/dprint.h"
#include "../../core/route_struct.h"
#include "../../core/str.h"
#include "../../core/sr_module.h"

// local includes
#include "mod_Router.h"
#include "mod_Core.h"
#include "mod_Ranks.h"
#include "mod_Logger.h"

#include "apy_kemi.h"


int ap_init_modules(void)
{
	init_mod_Router();
	/* Python 3:
	 * this will be done in the Router module initialization
	init_mod_Core();
	init_mod_Ranks();
	init_mod_Logger();
	*/
	if(sr_apy_init_ksr()<0) return -1;

	return 0;
}


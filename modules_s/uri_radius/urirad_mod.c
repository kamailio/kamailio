/* 
 * $Id$ 
 *
 * URI checks using Radius
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
 *
 * History:
 * --------
 * 2003-03-15 - created by janakj
 */

#include <radiusclient.h>
#include "../../dprint.h"
#include "../../sr_module.h"
#include "urirad_mod.h"
#include "checks.h"


static int mod_init(void); /* Module initialization function */


/*
 * Module parameter variables
 */
char* radius_config = "/usr/local/etc/radiusclient/radiusclient.conf";


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"radius_does_uri_exist", radius_does_uri_exist, 0, 0},
	{0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"radius_config", STR_PARAM, &radius_config},
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"uri_radius", 
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	0,          /* oncancel function */
	0           /* child initialization function */
};


static int mod_init(void)
{
	DBG("uri_radius - initializing\n");
	
	if (rc_read_config(radius_config) != 0) {
		LOG(L_ERR, "uri_radius: Error opening configuration file \n");
		return -1;
	}
    
	if (rc_read_dictionary(rc_conf_str("dictionary")) != 0) {
		LOG(L_ERR, "uri_radius: Error opening dictionary file \n");
		return -2;
	}

	return 0;
}

/*
 * This file is part of kamailio, a free SIP server.
 *
 * kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef TEXTOPSX_API_H_
#define TEXTOPSX_API_H_
#include "../../str.h"
#include "../../sr_module.h"


/*
 * Struct with the textopsx api.
 */
typedef struct textopsx_binds {
	cmd_function msg_apply_changes;
} textopsx_api_t;

typedef int (*bind_textopsx_f)(textopsx_api_t*);

/*
 * Function to be called direclty from other modules to load
 * the textops API.
 */
inline static int load_textopsx_api(textopsx_api_t *tob){
	bind_textopsx_f bind_textopsx_exports;
	if(!(bind_textopsx_exports=(bind_textopsx_f)find_export("bind_textopsx", 1, 0))){
		LM_ERR("Failed to import bind_textopsx\n");
		return -1;
	}
	return bind_textopsx_exports(tob);
}

#endif /*TEXTX_OPS_API_H_*/

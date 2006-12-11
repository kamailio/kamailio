/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2006-11-30  first version (lavinia)
 */


#include "../../str.h"
#include "../../dprint.h"
#include "../../sr_module.h"
#include "../../mi/mi.h"
#include "../../mem/mem.h"
#include "xr_writer.h"
#include "xr_parser.h"
#include "mi_xmlrpc.h"
#include "xr_server.h"
#include <xmlrpc_abyss.h>


xmlrpc_value*  default_method	(xmlrpc_env* 	env, 
								char* 			host,
								char* 			methodName,
								xmlrpc_value* 	paramArray,
								void* 			serverInfo){

	struct mi_root* mi_cmd = NULL;
	struct mi_root* mi_rpl = NULL;
	struct mi_cmd* 	f;
	char* response = 0;

	DBG("DEBUG: mi_xmlrpc: default_method: starting up.....\n");

	f = lookup_mi_cmd(methodName, strlen(methodName));
	
	if ( f == 0 ) {
		LOG(L_ERR, "ERROR: mi_xmlrpc: default_method: Command %s is not "
			"available!\n", methodName);
		xmlrpc_env_set_fault_formatted(env, XMLRPC_NO_SUCH_METHOD_ERROR, 
			"Requested command (%s) is not available!", methodName);
		goto error;
	}

	DBG("DEBUG: mi_xmlrpc: default_method: Done looking the mi command.\n");

	if (f->flags&MI_NO_INPUT_FLAG) {
		mi_cmd = 0;
	} else {
		mi_cmd = xr_parse_tree(env, paramArray);
		if ( mi_cmd == NULL ){
			LOG(L_ERR,"ERROR: mi_xmlrpc: default_method: error parsing"
				" MI tree\n");
			if ( !env->fault_occurred )
				xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR,
					"The xmlrpc request could not be parsed into a MI tree!");
			goto error;
		}
	}

	DBG("DEBUG: mi_xmlrpc: default_method: Done parsing the mi tree.\n");

	if ( ( mi_rpl = run_mi_cmd(f, mi_cmd) ) == 0 ){
		LOG(L_ERR, "ERROR: mi_xmlrpc: default_method: Command (%s) processing "
			"failed.\n", methodName);
		xmlrpc_env_set_fault_formatted(env, XMLRPC_INTERNAL_ERROR, 
			"Command (%s) processing failed.\n", methodName);
		goto error;
	}

	DBG("DEBUG: mi_xmlrpc: default_method: Done running the mi command.\n");

	if ( rpl_opt == 1 ) {
		
		if ( xr_build_response_array( env, mi_rpl ) != 0 ){
			LOG(L_ERR, "ERROR: mi_xmlrpc: default_method: Failed to parse "
				"the xmlrpc response from the mi tree.\n");
			if ( !env->fault_occurred )
				xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR, 
					"Failed to parse the xmlrpc response from the mi tree.");
			goto error;
		}
		DBG("DEBUG:mi_xmlrpc:default_method: Done building response array.\n");

		return xr_response;
	} else {
		if ( (response = xr_build_response( env, mi_rpl )) == 0 ){
			LOG(L_ERR, "ERROR: mi_xmlrpc: default_method: Failed to parse "
				"the xmlrpc response from the mi tree.\n");
			if ( !env->fault_occurred )
				xmlrpc_env_set_fault_formatted(env, XMLRPC_INTERNAL_ERROR,
					"Failed to parse the xmlrpc response from the mi tree.");
			goto error;
		}
		DBG("DEBUG: mi_xmlrpc: default_method: Done building response.\n");

		return xmlrpc_build_value(env, "s", response);
	}

error:
	if ( mi_rpl ) free_mi_tree( mi_rpl );
	if ( mi_cmd ) free_mi_tree( mi_cmd );
	return NULL;
}


int set_default_method ( xmlrpc_env * env )
{
	xmlrpc_registry * registry;
	
	registry = xmlrpc_server_abyss_registry();
	xmlrpc_registry_set_default_method(env, registry, &default_method, NULL);

	if ( env->fault_occurred ) {
		LOG(L_ERR, "ERROR: mi_xmlrpc: set_default_method: Failed to add "
			"default method: %s\n", env->fault_string);
		return -1;
	}

	return 0;
}

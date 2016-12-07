/* sp-ul_db module
 *
 * Copyright (C) 2007 1&1 Internet AG
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
 */

#include "ul_db_api.h"
#include "../../sr_module.h"

int bind_ul_db(ul_db_api_t* api)
{
	if (!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	
	api->insert = (ul_db_insert_t) ul_db_insert;
	if(api->insert == 0){
		LM_ERR("can't bind ul_db_insert\n");
		return -1;
	}

	api->update = (ul_db_update_t) ul_db_update;
	if(api->update == 0){
		LM_ERR("can't bind ul_db_update\n");
		return -1;
	}
	
	api->replace = (ul_db_replace_t) ul_db_replace;
	if(api->replace == 0){
		LM_ERR("can't bind ul_db_replace\n");
		return -1;
	}
	
	api->delete = (ul_db_delete_t) ul_db_delete;
	if(api->delete == 0){
		LM_ERR("can't bind ul_db_delete\n");
		return -1;
	}
	
	api->query = (ul_db_query_t) ul_db_query;
	if(api->query == 0){
		LM_ERR("can't bind ul_db_query\n");
		return -1;
	}
	
	api->free_result = (ul_db_free_result_t) ul_db_free_result;
	if(api->free_result == 0){
		LM_ERR("can't bind ul_db_free_result\n");
		return -1;
	}

	return 0;
}

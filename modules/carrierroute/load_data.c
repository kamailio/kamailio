/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/**
 * @file load_data.c
 *
 * @author: Jonas Appel <jonas dot appel at schlund dot de>
 *
 * @date Mi Jan 24 2007
 *
 * Copyright: 2007 1 & 1 Internet AG
 *
 * @brief API to bind a data loading function
 *
 */

#include "load_data.h"
#include "route_db.h"
#include "route_config.h"
#include "carrierroute.h"

/**
 * Binds the loader function pointer api to the matching loader
 * function depending on source
 *
 * @param source the configuration data source, at the moment 
 * it can be db or file
 * @param api pointer to the api where the loader function is
 * bound to
 *
 * @return 0 means everything is ok, -1 means an error
 */
int bind_data_loader(const char * source, route_data_load_func_t * api){
	if(strcmp(source, "db") == 0){
		LM_INFO("use database as configuration source");
		*api = load_route_data;
		mode = SP_ROUTE_MODE_DB;
		if(db_init() < 0){
			return -1;
		}
		return 0;
	}
	if(strcmp(source, "file") == 0){
		LM_INFO("use file as configuration source");
		*api = load_config;
		mode = SP_ROUTE_MODE_FILE;
		return 0;
	}
	LM_NOTICE("could bind configuration source <%s>", source);
	return -1;
}

int data_main_finalize(){
	if(mode == SP_ROUTE_MODE_DB){
		main_db_close();
	}
	return 0;
}

int data_child_init(){
	if(mode == SP_ROUTE_MODE_DB){
		return db_child_init();
	}
	return 0;
}

void data_destroy(){
	if(mode == SP_ROUTE_MODE_DB){
		db_destroy();
	}
	return;
}

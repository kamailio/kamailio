/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file load_data.c
 * \brief API to bind a data loading function.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../globals.h"
#include "load_data.h"
#include "route_db.h"
#include "route_config.h"
#include "carrierroute.h"
#include "db_carrierroute.h"

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
	struct stat fs;
	if(strcmp(source, "db") == 0){
		LM_INFO("use database as configuration source");
		*api = load_route_data;
		mode = SP_ROUTE_MODE_DB;
		if(carrierroute_db_init() < 0){
			return -1;
		}
		// FIXME, move data initialization into child process
		if(carrierroute_db_open() < 0){
			return -1;
		}
		return 0;
	}
	if(strcmp(source, "file") == 0){
		LM_INFO("use file as configuration source");
		*api = load_config;
		mode = SP_ROUTE_MODE_FILE;
		if(stat(config_file, &fs) != 0){
			LM_ERR("can't stat config file\n");
			return -1;
		}
		if(fs.st_mode & S_IWOTH){
			LM_WARN("insecure file permissions, routing data is world writeable");
		}
		if( !( fs.st_mode & S_IWOTH) &&
			!((fs.st_mode & S_IWGRP) && (fs.st_gid == getegid())) &&
			!((fs.st_mode & S_IWUSR) && (fs.st_uid == geteuid())) ) {
				LM_ERR("config file not writable\n");
				return -1;
			}
		return 0;
	}
	LM_ERR("could not bind configuration source <%s>", source);
	return -1;
}

int data_main_finalize(void){
	if(mode == SP_ROUTE_MODE_DB){
		carrierroute_db_close();
	}
	return 0;
}

int data_child_init(void){
	if(mode == SP_ROUTE_MODE_DB){
		return carrierroute_db_open();
	}
	return 0;
}

void data_destroy(void){
	if(mode == SP_ROUTE_MODE_DB){
		carrierroute_db_close();
	}
	return;
}

/*
 * Copyright (C) 2012 VoIP Embedded, Inc.
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
/*!
 * \file
 * \brief XHTTP_PI :: 
 * \ingroup xhttp_pi
 * Module: \ref xhttp_pi
 */

#include <stdio.h>
#include <stdlib.h>

#include "../../lib/srdb1/db.h"
#include "xhttp_pi_fnc.h"



int connect_http_db(ph_framework_t *framework_data, int index)
{
	ph_db_url_t *ph_db_urls = framework_data->ph_db_urls;

	if (ph_db_urls[index].http_db_handle) {
		LM_CRIT("BUG - db connection found already open\n");
		return -1;
	}
	if ((ph_db_urls[index].http_db_handle =
		ph_db_urls[index].http_dbf.init(&ph_db_urls[index].db_url)) == NULL) {
		return -1;
	}
	return 0;
}

int use_table(ph_db_table_t *db_table)
{
	ph_db_url_t *db_url;

	if(db_table==NULL){
		LM_ERR("null db_table handler\n");
		return -1;
	}
	if(db_table->db_url==NULL){
		LM_ERR("null db_url for table [%s]\n", db_table->name.s);
		return -1;
	}
	db_url = db_table->db_url;
	if(db_url->http_db_handle==NULL){
		LM_ERR("null db handle for table [%s]\n", db_table->name.s);
		return -1;
	}
	db_table->db_url->http_dbf.use_table(db_table->db_url->http_db_handle,
						&db_table->name);
	return 0;
}

int init_http_db(ph_framework_t *framework_data, int index)
{
	ph_db_url_t *ph_db_urls = framework_data->ph_db_urls;

	if (db_bind_mod(&ph_db_urls[index].db_url,
		&ph_db_urls[index].http_dbf) < 0) {
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}
	if (connect_http_db(framework_data, index)!=0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	ph_db_urls[index].http_dbf.close(ph_db_urls[index].http_db_handle);
	ph_db_urls[index].http_db_handle = NULL;

	return 0;
}


void destroy_http_db(ph_framework_t *framework_data)
{
	int i;
	ph_db_url_t *ph_db_urls;

	if (framework_data == NULL) return;

	ph_db_urls = framework_data->ph_db_urls;
	/* close the DB connections */
	for(i=0;i<framework_data->ph_db_urls_size;i++){
		if (ph_db_urls[i].http_db_handle) {
			ph_db_urls[i].http_dbf.close(ph_db_urls[i].http_db_handle);
			ph_db_urls[i].http_db_handle = NULL;
		}
	}
}

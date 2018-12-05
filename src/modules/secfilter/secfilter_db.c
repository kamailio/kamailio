/**
 * Copyright (C) 2018 Jose Luis Verdeguer
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


#include "../../lib/srdb1/db.h"
#include "secfilter.h"

int mod_version = 1;

static db_func_t db_funcs;
static db1_con_t *db_handle = NULL;

static int get_type(char *ctype);

static str version_table_name = str_init("version");
static str table_name_col = str_init("table_name");
static str table_version_col = str_init("table_version");


/* Check module version */
int check_version(void)
{
	int version = 0;
	db_key_t db_keys[1];
	db_val_t db_vals[1];
	db_key_t db_cols[1];
	db1_res_t* db_res = NULL;

	/* Connect to DB */
	db_handle = db_funcs.init(&sec_db_url);
	if (db_handle == NULL)
	{
		LM_ERR("Invalid db handle\n");
		return -1;
	}

	/* Prepare the data for the query */
	db_cols[0] = &table_version_col;
	db_keys[0] = &table_name_col;

	db_vals[0].type = DB1_STRING;
	db_vals[0].nul = 0;
	db_vals[0].val.string_val = "secfilter";

	/* Execute query */
	if (db_funcs.use_table(db_handle, &version_table_name) < 0)
	{
		LM_ERR("Unable to use table '%.*s'\n", version_table_name.len, version_table_name.s);
		return -1;
	}
	if (db_funcs.query(db_handle, db_keys, NULL, db_vals, db_cols, 1, 1, NULL, &db_res) < 0)
	{
		LM_ERR("Failed to query database\n");
		db_funcs.close(db_handle);
		return -1;
	}

	if (RES_ROW_N(db_res) == 0)
	{
		LM_ERR("No version value found in database. It must be %d\n", mod_version);
		if (db_res!=NULL && db_funcs.free_result(db_handle, db_res) < 0)
			LM_DBG("Failed to free the result\n");
		db_funcs.close(db_handle);
		return -1;
	}

	/* Get the version value */
	version = RES_ROWS(db_res)[0].values[0].val.int_val;

	if (version != mod_version)
	{
		LM_ERR("Wrong version value. Correct version is %d but found %d\n", mod_version, version);
		if (db_res!=NULL && db_funcs.free_result(db_handle, db_res) < 0)
			LM_DBG("Failed to free the result\n");
		db_funcs.close(db_handle);
		return -1;
	}

	db_funcs.free_result(db_handle, db_res);
	return 0;
}


static int get_type(char *ctype)
{
	if (!strcmp(ctype, "ua"))      return 0;
	if (!strcmp(ctype, "country")) return 1;
	if (!strcmp(ctype, "domain"))  return 2;
	if (!strcmp(ctype, "ip"))      return 3;
	if (!strcmp(ctype, "user"))    return 4;
	
	return -1;
}


/* Load data into arrays */
static int load_db(int action, char *ctype)
{
	db_key_t db_keys[2];
	db_val_t db_vals[2];
	db_key_t db_cols[1];
	db1_res_t* db_res = NULL;
	int rows = 0;
	int i;
	int type;
	
	if (action == 2)
		type = 0;
	else
		type = get_type(ctype);
	
	if (type == -1)
	{
		LM_ERR("Invalid type\n");
		return -1;
	}
	
	/* Connect to DB */
	db_handle = db_funcs.init(&sec_db_url);
	if (db_handle == NULL)
	{
		LM_ERR("Invalid db handle\n");
		return -1;
	}

	/* Prepare the data for the query */
	db_cols[0] = &sec_data_col;
	db_keys[0] = &sec_action_col;
	db_keys[1] = &sec_type_col;

	db_vals[0].type = DB1_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = action;

	db_vals[1].type = DB1_INT;
	db_vals[1].nul = 0;
	db_vals[1].val.int_val = type;

	/* Execute query */
	if (db_funcs.use_table(db_handle, &sec_table_name) < 0)
	{
		LM_ERR("Unable to use table '%.*s'\n", sec_table_name.len, sec_table_name.s);
		return -1;
	}
	if (db_funcs.query(db_handle, db_keys, NULL, db_vals, db_cols, 2, 1, NULL, &db_res) < 0)
	{
		LM_ERR("Failed to query database\n");
		db_funcs.close(db_handle);
		return -1;
	}

	rows = RES_ROW_N(db_res);
	if (rows == 0)
	{
		LM_DBG("No data found in database\n");
		if (db_res!=NULL && db_funcs.free_result(db_handle, db_res) < 0)
			LM_DBG("Failed to free the result\n");
		db_funcs.close(db_handle);
		return 0;
	}

	/* Add values to array */
	for (i=0; i<rows; i++)
	{
		/* Blacklist */
		if (action == 0)
		{
			if (!strcmp(ctype, "ua"))
			{
			        sec_bl_ua_list[*sec_nblUa]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_bl_ua_list[*sec_nblUa]);
		        	*sec_nblUa = *sec_nblUa + 1;
		        }
		        if (!strcmp(ctype, "country"))
			{
			        sec_bl_country_list[*sec_nblCountry]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_bl_country_list[*sec_nblCountry]);
		        	*sec_nblCountry = *sec_nblCountry + 1;
		        }
			if (!strcmp(ctype, "domain"))
			{
			        sec_bl_domain_list[*sec_nblDomain]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_bl_domain_list[*sec_nblDomain]);
		        	*sec_nblDomain = *sec_nblDomain + 1;
		        }
			if (!strcmp(ctype, "user"))
			{
			        sec_bl_user_list[*sec_nblUser]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_bl_user_list[*sec_nblUser]);
		        	*sec_nblUser = *sec_nblUser + 1;
		        }
			if (!strcmp(ctype, "ip"))
			{
			        sec_bl_ip_list[*sec_nblIp]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_bl_ip_list[*sec_nblIp]);
		        	*sec_nblIp = *sec_nblIp + 1;
		        }
		}
		/* Whitelist */
		if (action == 1)
		{
			if (!strcmp(ctype, "ua"))
			{
			        sec_wl_ua_list[*sec_nwlUa]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_wl_ua_list[*sec_nwlUa]);
		        	*sec_nwlUa = *sec_nwlUa + 1;
		        }
			if (!strcmp(ctype, "country"))
			{
			        sec_wl_country_list[*sec_nwlCountry]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_wl_country_list[*sec_nwlCountry]);
		        	*sec_nwlCountry = *sec_nwlCountry + 1;
		        }
			if (!strcmp(ctype, "domain"))
			{
			        sec_wl_domain_list[*sec_nwlDomain]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_wl_domain_list[*sec_nwlDomain]);
		        	*sec_nwlDomain = *sec_nwlDomain + 1;
		        }
			if (!strcmp(ctype, "user"))
			{
			        sec_wl_user_list[*sec_nwlUser]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_wl_user_list[*sec_nwlUser]);
		        	*sec_nwlUser = *sec_nwlUser + 1;
		        }
			if (!strcmp(ctype, "ip"))
			{
			        sec_wl_ip_list[*sec_nwlIp]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
			        uppercase(sec_wl_ip_list[*sec_nwlIp]);
		        	*sec_nwlIp = *sec_nwlIp + 1;
		        }
		}
		/* Destination blacklist*/
		if (action == 2)
		{
		        sec_dst_list[*sec_nDst]=strtok(strdup((char*)RES_ROWS(db_res)[i].values[0].val.string_val), "\n"); 
		        uppercase(sec_dst_list[*sec_nDst]);
		        *sec_nDst = *sec_nDst + 1;
		}
	}

	db_funcs.free_result(db_handle, db_res);
	return 0;
}


/* Insert a value into database */
int insert_db(int action, char *ctype, char *value)
{
	db_key_t db_keys[3];
	db_val_t db_vals[3];
	int ncols = 3;
	int type;
	
	if (action == 2)
		type = 0;
	else
		type = get_type(ctype);
	
	if (type == -1)
	{
		LM_ERR("Invalid type\n");
		return -1;
	}
	
	/* Bind the database module */
	if (db_bind_mod(&sec_db_url, &db_funcs))
	{
		LM_ERR("failed to bind database module\n");
		return -1;
	}
	
	/* Check for SELECT capability */
	if (!DB_CAPABILITY(db_funcs, DB_CAP_INSERT))
	{
		LM_ERR("Database modules does not provide all functions needed here\n");
		return -1;
	}
		
	/* Connect to DB */
	db_handle = db_funcs.init(&sec_db_url);
	if (db_handle == NULL)
	{
		LM_ERR("Invalid db handle\n");
		return -1;
	}

	/* Prepare the data for the insert */
        db_keys[0] = &sec_action_col;
        db_vals[0].type = DB1_INT;
        db_vals[0].nul = 0;
        db_vals[0].val.int_val = action;

        db_keys[1] = &sec_type_col;
        db_vals[1].type = DB1_INT;
        db_vals[1].nul = 0;
        db_vals[1].val.int_val = type;

        db_keys[2] = &sec_data_col;
        db_vals[2].type = DB1_STRING;
        db_vals[2].nul = 0;
        db_vals[2].val.string_val = value;

	/* Execute query */
	if (db_funcs.use_table(db_handle, &sec_table_name) < 0)
	{
		LM_ERR("Unable to use table '%.*s'\n", sec_table_name.len, sec_table_name.s);
		return -1;
	}
	if (db_funcs.insert(db_handle, db_keys, db_vals, ncols) < 0)
	{
		LM_ERR("Failed to insert database\n");
		db_funcs.close(db_handle);
		return -1;
	}

	/* Close connection */
	db_funcs.close(db_handle);

	return 0;
}


/* Init database connection */
int init_db(void)
{
        if (sec_db_url.s == NULL) 
        {
                LM_ERR("Database not configured\n");
                return -1;
	}

	sec_db_url.len = strlen(sec_db_url.s);

        if (db_bind_mod(&sec_db_url, &db_funcs) < 0)
        {
                LM_ERR("Unable to bind to db driver - %.*s\n", sec_db_url.len, sec_db_url.s);
                return -1;
        }
        
        if (check_version() == -1) return -1;
        
        /* Init values */
	*sec_nblUa = 0;
	*sec_nblDomain = 0;
	*sec_nblCountry = 0;
	*sec_nblUser = 0;
	*sec_nblIp = 0;

	*sec_nwlUa = 0;
	*sec_nwlDomain = 0;
	*sec_nwlCountry = 0;
	*sec_nwlUser = 0;
	*sec_nwlIp = 0;

        return 0;
}


/* Load data from database */
void load_data_from_db(void)
{
        /* Blacklist */
        if (load_db(0, "ua") == 0)
        	LM_INFO("User-agent blacklist loaded\n");
        if (load_db(0, "domain") == 0)
        	LM_INFO("Domain blacklist loaded\n");
        if (load_db(0, "country") == 0)
        	LM_INFO("Country blacklist loaded\n");
        if (load_db(0, "user") == 0)
        	LM_INFO("User blacklist loaded\n");
        if (load_db(0, "ip") == 0)
        	LM_INFO("IP address blacklist loaded\n");
	
	/* Whitelist */
        if (load_db(1, "ua") == 0)
        	LM_INFO("User-agent whitelist loaded\n");
        if (load_db(1, "domain") == 0)
        	LM_INFO("Domain whitelist loaded\n");
        if (load_db(1, "country") == 0)
        	LM_INFO("Country whitelist loaded\n");
        if (load_db(1, "user") == 0)
        	LM_INFO("User whitelist loaded\n");
        if (load_db(1, "ip") == 0)
        	LM_INFO("IP address whitelist loaded\n");

        /* Destination blacklist */
        if (load_db(2, "") == 0)
        	LM_INFO("Destinations blacklist loaded\n");
}

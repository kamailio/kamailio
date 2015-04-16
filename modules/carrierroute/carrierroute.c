/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * \file carrierroute.c
 * \brief Contains the functions exported by the module.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

/*!
 * \defgroup carrierroute CARRIERROUTE :: The Kamailio carrierroute Module
 * The module provides routing, balancing and blacklisting capabilities.
 * It reads routing entries from a database source or from a config file
 * at Kamailio startup. It can uses one routing tree (for one carrier),
 * or if needed for every user a different routing tree (unique for each carrier)
 * for number prefix based routing. It supports several routing domains, e.g.
 * for failback routes or different routing rules for VoIP and PSTN targets.
 */

#include "../../sr_module.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../ut.h" /* for user2uid() */
#include "../../rpc_lookup.h" /* for sercmd */
#include "carrierroute.h"
#include "cr_fixup.h"
#include "cr_map.h"
#include "cr_fifo.h"
#include "cr_data.h"
#include "cr_func.h"
#include "db_carrierroute.h"
#include "config.h"
#include <sys/stat.h>

#define AVP_CR_URIS "_cr_uris"
int_str cr_uris_avp; // contains all PSTN destinations

MODULE_VERSION

str carrierroute_db_url = str_init(DEFAULT_RODB_URL);
str subscriber_table = str_init("subscriber");

static str subscriber_username_col = str_init("username");
static str subscriber_domain_col = str_init("domain");
static str cr_preferred_carrier_col = str_init("cr_preferred_carrier");

str * subscriber_columns[SUBSCRIBER_COLUMN_NUM] = {
	&subscriber_username_col,
	&subscriber_domain_col,
	&cr_preferred_carrier_col
};

char * config_source = "file";
char * config_file = CFG_DIR"carrierroute.conf";

str default_tree = str_init("default");
const str CR_EMPTY_PREFIX = str_init("null");

int mode = 0;
int cr_match_mode = 10;
int cr_avoid_failed_dests = 1;

/************* Declaration of Interface Functions **************************/
static int mod_init(void);
static int child_init(int);
static int mi_child_init(void);
static void mod_destroy(void);


/************* Module Exports **********************************************/
static cmd_export_t cmds[]={
	{"cr_user_carrier",  (cmd_function)cr_load_user_carrier,  3, cr_load_user_carrier_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_route",         (cmd_function)cr_route5,              5, cr_route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_route",         (cmd_function)cr_route,              6, cr_route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_nofallback_route",(cmd_function)cr_nofallback_route5,     5, cr_route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_nofallback_route",(cmd_function)cr_nofallback_route,     6, cr_route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_next_domain",   (cmd_function)cr_load_next_domain,   6, cr_load_next_domain_fixup,  0, REQUEST_ROUTE | FAILURE_ROUTE },
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]= {
	carrierroute_DB_URL
	carrierroute_DB_TABLE
	carrierfailureroute_DB_TABLE
	carrier_name_DB_TABLE
	domain_name_DB_TABLE
	carrierroute_DB_COLS
	carrierfailureroute_DB_COLS
	carrier_name_DB_COLS
	domain_name_DB_COLS
	{"subscriber_table",          PARAM_STR, &subscriber_table },
	{"subscriber_user_col",       PARAM_STR, &subscriber_username_col },
	{"subscriber_domain_col",     PARAM_STR, &subscriber_domain_col },
	{"subscriber_carrier_col",    PARAM_STR, &cr_preferred_carrier_col },
	{"config_source",             PARAM_STRING, &config_source },
	{"default_tree",              PARAM_STR, &default_tree },
	{"config_file",               PARAM_STRING, &config_file },
	{"use_domain",                INT_PARAM, &default_carrierroute_cfg.use_domain },
	{"fallback_default",          INT_PARAM, &default_carrierroute_cfg.fallback_default },
	{"fetch_rows",                INT_PARAM, &default_carrierroute_cfg.fetch_rows },
	{"match_mode",                INT_PARAM, &cr_match_mode },
	{"avoid_failed_destinations", INT_PARAM, &cr_avoid_failed_dests },
	{0,0,0}
};

static mi_export_t mi_cmds[] = {
	{ "cr_reload_routes",   reload_fifo,     MI_NO_INPUT_FLAG, 0,  mi_child_init },
	{ "cr_dump_routes",     dump_fifo,       MI_NO_INPUT_FLAG, 0,  0 },
	{ "cr_replace_host",    replace_host,    0,                0,  0 },
	{ "cr_deactivate_host", deactivate_host, 0,                0,  0 },
	{ "cr_activate_host",   activate_host,   0,                0,  0 },
	{ "cr_add_host",        add_host,        0,                0,  0 },
	{ "cr_delete_host",     delete_host,     0,                0,  0 },
	{ 0, 0, 0, 0, 0}
};

static rpc_export_t rpc_methods[];

struct module_exports exports = {
	"carrierroute",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Export parameters */
	0,          /* exported statistics */
	mi_cmds,    /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* Module initialization function */
	0,          /* Response function */
	mod_destroy,/* Destroy function */
	child_init  /* Child initialization function */
};


/************* Interface Functions *****************************************/

/**
 * Initialises the module, i.e. it binds the necessary API functions
 * and registers the fifo commands
 *
 * @return 0 on success, -1 on failure
 */
static int mod_init(void) {
	struct stat fs;
	extern char* user; /*from main.c*/
	int uid, gid;

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if(rpc_register_array(rpc_methods)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if (cr_match_mode != 10 && cr_match_mode != 128) {
		LM_ERR("invalid matching mode %d specific, please use 10 or 128\n", cr_match_mode);
		return -1;
	}

	if (cr_avoid_failed_dests != 0 && cr_avoid_failed_dests != 1) {
		LM_ERR("avoid_failed_dests must be 0 or 1");
		return -1;
	}

	if (strcmp(config_source, "db") == 0) {
		mode = CARRIERROUTE_MODE_DB;

		LM_INFO("use database as configuration source\n");
		if(carrierroute_db_init() < 0){
			return -1;
		}
		// FIXME, move data initialization into child process
		if(carrierroute_db_open() < 0){
			return -1;
		}
	}
	else if (strcmp(config_source, "file") == 0) {
		mode = CARRIERROUTE_MODE_FILE;

		LM_INFO("use file as configuration source\n");
		if(stat(config_file, &fs) != 0){
			LM_ERR("can't stat config file\n");
			return -1;
		}
		if(fs.st_mode & S_IWOTH){
			LM_WARN("insecure file permissions, routing data is world writeable\n");
		}

		if (user){
			if (user2uid(&uid, &gid, user)<0){
				LM_ERR("bad user name/uid number: -u %s\n", user);
				return -1;
			}
		} else {
			uid = geteuid();
			gid = getegid();
		}

		if( !( fs.st_mode & S_IWOTH) &&
			!((fs.st_mode & S_IWGRP) && (fs.st_gid == uid)) &&
			!((fs.st_mode & S_IWUSR) && (fs.st_uid == gid))) {
				LM_ERR("config file %s not writable\n", config_file);
				return -1;
		}
	}
	else {
		LM_ERR("invalid config_source parameter: %s\n", config_source);
		return -1;
	}

	if(cfg_declare("carrierroute", carrierroute_cfg_def, &default_carrierroute_cfg, cfg_sizeof(carrierroute), &carrierroute_cfg)){
		LM_ERR("Fail to declare the configuration\n");
		return -1;
	}

	if (init_route_data() < 0) {
		LM_ERR("could not init route data\n");
		return -1;
	}

	if (reload_route_data() == -1) {
		LM_ERR("could not prepare route data\n");
		return -1;
	}

	if(mode == CARRIERROUTE_MODE_DB){
		carrierroute_db_close();
	}

	cr_uris_avp.s.s = AVP_CR_URIS;
	cr_uris_avp.s.len = sizeof(AVP_CR_URIS) -1;

	return 0;
}


static int child_init(int rank) {
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if(mode == CARRIERROUTE_MODE_DB){
		return carrierroute_db_open();
	}
	return 0;
}


static int mi_child_init(void) {
	if(mode == CARRIERROUTE_MODE_DB){
		return carrierroute_db_open();
	}
	return 0;
}

static void mod_destroy(void) {
	if(mode == CARRIERROUTE_MODE_DB){
		carrierroute_db_close();
	}
	destroy_route_data();
}

static const char *rpc_cr_reload_routes_doc[2] = {
	"Reload routes", 0
};

static void rpc_cr_reload_routes(rpc_t *rpc, void *c) {

	if(mode == CARRIERROUTE_MODE_DB){
		if (carrierroute_dbh==NULL) {
			carrierroute_dbh = carrierroute_dbf.init(&carrierroute_db_url);
			if(carrierroute_dbh==0 ) {
				LM_ERR("cannot initialize database connection\n");
				return;
			}
		}
	}

	if ( (reload_route_data())!=0 ) {
		LM_ERR("failed to load routing data\n");
		return;
	}
}

static rpc_export_t rpc_methods[] = {
	{ "cr.reload_routes",  rpc_cr_reload_routes, rpc_cr_reload_routes_doc, 0},
	{0, 0, 0, 0}
};


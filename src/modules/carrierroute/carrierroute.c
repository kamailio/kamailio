/*
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
 * The module provides routing, balancing and blocklisting capabilities.
 * It reads routing entries from a database source or from a config file
 * at Kamailio startup. It can uses one routing tree (for one carrier),
 * or if needed for every user a different routing tree (unique for each carrier)
 * for number prefix based routing. It supports several routing domains, e.g.
 * for failback routes or different routing rules for VoIP and PSTN targets.
 */

#include "../../core/sr_module.h"
#include "../../core/str.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h" /* for user2uid() */
#include "../../core/kemi.h"
#include "carrierroute.h"
#include "cr_fixup.h"
#include "cr_map.h"
#include "cr_data.h"
#include "cr_func.h"
#include "db_carrierroute.h"
#include "config.h"
#include "cr_db.h"
#include "cr_rpc.h"
#include <sys/stat.h>

#define AVP_CR_URIS "_cr_uris"
int_str cr_uris_avp; // contains all PSTN destinations

MODULE_VERSION

str carrierroute_db_url = str_init(DEFAULT_RODB_URL);
str subscriber_table = str_init("subscriber");

static str subscriber_username_col = str_init("username");
static str subscriber_domain_col = str_init("domain");
static str cr_preferred_carrier_col = str_init("cr_preferred_carrier");
static int cr_load_comments = 1;

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
static void mod_destroy(void);


/************* Module Exports **********************************************/
static cmd_export_t cmds[]={
	{"cr_user_carrier",  (cmd_function)cr_load_user_carrier,  3,
		cr_load_user_carrier_fixup, cr_load_user_carrier_fixup_free,
		REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_route",         (cmd_function)cr_route5,              5,
		cr_route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_route",         (cmd_function)cr_route,              6,
		cr_route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_nofallback_route",(cmd_function)cr_nofallback_route5,     5,
		cr_route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_nofallback_route",(cmd_function)cr_nofallback_route,     6,
		cr_route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_next_domain",   (cmd_function)cr_load_next_domain,   6,
		cr_load_next_domain_fixup,  0, REQUEST_ROUTE | FAILURE_ROUTE },
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
	{"db_load_description", 	  INT_PARAM, &cr_load_comments },
	{"match_mode",                INT_PARAM, &cr_match_mode },
	{"avoid_failed_destinations", INT_PARAM, &cr_avoid_failed_dests },
	{0,0,0}
};



struct module_exports exports = {
	"carrierroute",  /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported functions */
	params,          /* Export parameters */
	0,               /* RPC method exports */
	0,               /* exported pseudo-variables */
	0,               /* Response function */
	mod_init,        /* Module initialization function */
	child_init,      /* Child initialization function */
	mod_destroy      /* Destroy function */
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

	if(rpc_register_array(cr_rpc_methods)!=0) {
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

	if (cr_load_comments != 0 && cr_load_comments != 1) {
		LM_ERR("db_load_comments must be 0 or 1");
		return -1;
	}

	if (strcmp(config_source, "db") == 0) {
		mode = CARRIERROUTE_MODE_DB;

		set_load_comments_params(cr_load_comments);

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
			!((fs.st_mode & S_IWGRP) && (fs.st_gid == gid)) &&
			!((fs.st_mode & S_IWUSR) && (fs.st_uid == uid))) {
				LM_ERR("config file %s not writable or not owned by uid: %d and gid: %d\n",
						config_file, uid, gid);
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


static void mod_destroy(void) {
	if(mode == CARRIERROUTE_MODE_DB){
		carrierroute_db_close();
	}
	destroy_route_data();
}


/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_carrierroute_exports[] = {
	{ str_init("carrierroute"), str_init("cr_user_carrier"),
		SR_KEMIP_INT, ki_cr_load_user_carrier,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_carrierroute_exports);
	return 0;
}

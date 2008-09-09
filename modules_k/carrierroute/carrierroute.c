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
 * for number prefix based routing. It supports several route tree domains, e.g.
 * for failback routes or different routing rules for VoIP and PSTN targets.
 */

#include "../../sr_module.h"
#include "../../str.h"
#include "../../dset.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../prime_hash.h"
#include "carrierroute.h"
#include "load_data.h"
#include "route_fifo.h"
#include "carrier_tree.h"
#include "route_func.h"
#include "db_carrierroute.h"

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
const str SP_EMPTY_PREFIX = str_init("null");

int mode = 0;
int use_domain = 0;

int fallback_default = 1;


/************* Declaration of Interface Functions **************************/
static int mod_init(void);
static int child_init(int);
static int mi_child_init(void);
static void mod_destroy(void);
static int route_fixup(void ** param, int param_no);
static int load_user_carrier_fixup(void ** param, int param_no);
static int load_next_domain_fixup(void ** param, int param_no);


/************* Declaration of Helper Functions *****************************/
static enum hash_source hash_fixup(const char * domain);


/************* Module Exports **********************************************/
static cmd_export_t cmds[]={
	{"cr_user_carrier",          (cmd_function)cr_load_user_carrier,  3, load_user_carrier_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_route",                 (cmd_function)cr_route,              5, route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_route",                 (cmd_function)cr_route,              6, route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_prime_route",           (cmd_function)cr_route,              5, route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_prime_route",           (cmd_function)cr_route,              6, route_fixup,             0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"cr_next_domain",           (cmd_function)cr_load_next_domain,   6, load_next_domain_fixup,  0, REQUEST_ROUTE | FAILURE_ROUTE },
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]= {
	carrierroute_DB_URL
	carrierroute_DB_TABLE
	carrierfailureroute_DB_TABLE
	route_tree_DB_TABLE
	carrierroute_DB_COLS
	carrierfailureroute_DB_COLS
	route_tree_DB_COLS
	{"subscriber_table",           STR_PARAM, &subscriber_table.s },
	{"subscriber_user_col",        STR_PARAM, &subscriber_username_col.s },
	{"subscriber_domain_col",      STR_PARAM, &subscriber_domain_col.s },
	{"subscriber_carrier_col",     STR_PARAM, &cr_preferred_carrier_col.s },
	{"config_source",              STR_PARAM, &config_source },
	{"default_tree",               STR_PARAM, &default_tree },
	{"config_file",                STR_PARAM, &config_file },
	{"use_domain",                 INT_PARAM, &use_domain },
	{"fallback_default",           INT_PARAM, &fallback_default },
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


/************* Helper Functions ********************************************/

/**
 * Fixes the hash source to enum values
 *
 * @param my_hash_source the hash source as string
 *
 * @return the enum value on success, -1 on failure
 */
static enum hash_source hash_fixup(const char * my_hash_source) {
	if (strcasecmp("call_id", my_hash_source) == 0) {
		return shs_call_id;
	} else if (strcasecmp("from_uri", my_hash_source) == 0) {
		return shs_from_uri;
	} else if (strcasecmp("from_user", my_hash_source) == 0) {
		return shs_from_user;
	} else if (strcasecmp("to_uri", my_hash_source) == 0) {
		return shs_to_uri;
	} else if (strcasecmp("to_user", my_hash_source) == 0) {
		return shs_to_user;
	} else {
		return shs_error;
	}
}


/************* Interface Functions *****************************************/

/**
 * Initialises the module, i.e. it binds the necessary API functions
 * and registers the fifo commands
 *
 * @return 0 on success, -1 on failure
 */
static int mod_init(void) {

	subscriber_table.len = strlen(subscriber_table.s);
	subscriber_username_col.len = strlen(subscriber_username_col.s);
	subscriber_domain_col.len = strlen(subscriber_domain_col.s);
	cr_preferred_carrier_col.len = strlen(cr_preferred_carrier_col.s);
	default_tree.len = strlen(default_tree.s);

	carrierroute_db_vars();

	if (init_route_data(config_source) < 0) {
		LM_ERR("could not init route data\n");
		return -1;
	}
	if (prepare_route_tree() == -1) {
		LM_ERR("could not prepare route tree\n");
		return -1;
	}
	if(data_main_finalize() < 0) {
		return -1;
	}
	LM_INFO("module initialized, pid [%d]\n", getpid());
	return 0;
}


/**
 * fixes the module functions' parameters with generic pseudo variable support.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int pv_fixup(void ** param) {
	pv_elem_t *model;
	str s;

	s.s = (char *)(*param);
	s.len = strlen(s.s);
	if (s.len <= 0) return -1;
	/* Check the format */
	if(pv_parse_format(&s, &model)<0) {
		LM_ERR("pv_parse_format failed for '%s'\n", (char *)(*param));
		return -1;
	}
	*param = (void*)model;

	return 0;
}


/**
 * fixes the module functions' parameters if it is a carrier.
 * supports name string, pseudo-variables and AVPs.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int carrier_fixup(void ** param) {
	pv_spec_t avp_spec;
	struct multiparam_t *mp;
	str s;

	mp = (struct multiparam_t *)pkg_malloc(sizeof(struct multiparam_t));
	if (mp == NULL) {
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(mp, 0, sizeof(struct multiparam_t));
	
	s.s = (char *)(*param);
	s.len = strlen(s.s);

	if (s.s[0]!='$') {
		/* This is a name string */
		mp->type=MP_INT;
		
		/* get carrier id */
		if ((mp->u.n = find_tree(s)) < 0) {
			LM_ERR("could not find carrier tree '%s'\n", (char *)(*param));
			pkg_free(mp);
			return -1;
		}
		LM_INFO("carrier tree %s has id %i\n", (char *)*param, mp->u.n);
		
		pkg_free(*param);
		*param = (void *)mp;
	}
	else {
		/* This is a pseudo-variable */
		if (pv_parse_spec(&s, &avp_spec)==0) {
			LM_ERR("pv_parse_spec failed for '%s'\n", (char *)(*param));
			pkg_free(mp);
			return -1;
		}
			if (avp_spec.type==PVT_AVP) {
			/* This is an AVP - could be an id or name */
			mp->type=MP_AVP;
			if(pv_get_avp_name(0, &(avp_spec.pvp), &(mp->u.a.name), &(mp->u.a.flags))!=0) {
				LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
				pkg_free(mp);
				return -1;
			}
		} else {
			mp->type=MP_PVE;
			if(pv_parse_format(&s, &(mp->u.p))<0) {
				LM_ERR("pv_parse_format failed for '%s'\n", (char *)(*param));
				pkg_free(mp);
				return -1;
			}
		}
	}
	*param = (void*)mp;

	return 0;
}


/**
 * fixes the module functions' parameters if it is a domain.
 * supports name string, and AVPs.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int domain_fixup(void ** param) {
	pv_spec_t avp_spec;
	struct multiparam_t *mp;
	str s;

	mp = (struct multiparam_t *)pkg_malloc(sizeof(struct multiparam_t));
	if (mp == NULL) {
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(mp, 0, sizeof(struct multiparam_t));
	
	s.s = (char *)(*param);
	s.len = strlen(s.s);
	
	if (s.s[0]!='$') {
		/* This is a name string */
		mp->type=MP_INT;
		
		/* get domain id */
		if ((mp->u.n = add_domain(&s)) < 0) {
			LM_ERR("could not add domain\n");
			pkg_free(mp);
			return -1;
		}
		pkg_free(*param);
		*param = (void *)mp;
	}
	else {
		/* This is a pseudo-variable */
		if (pv_parse_spec(&s, &avp_spec)==0) {
			LM_ERR("pv_parse_spec failed for '%s'\n", (char *)(*param));
			pkg_free(mp);
			return -1;
		}
		if (avp_spec.type==PVT_AVP) {
			/* This is an AVP - could be an id or name */
			mp->type=MP_AVP;
			if(pv_get_avp_name(0, &(avp_spec.pvp), &(mp->u.a.name), &(mp->u.a.flags))!=0) {
				LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
				pkg_free(mp);
				return -1;
			}
		} else {
			mp->type=MP_PVE;
			if(pv_parse_format(&s, &(mp->u.p))<0) {
				LM_ERR("pv_parse_format failed for '%s'\n", (char *)(*param));
				pkg_free(mp);
				return -1;
			}
		}	
	}
	*param = (void*)mp;

	return 0;
}


/**
 * fixes the module functions' parameters in case of AVP names.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int avp_name_fixup(void ** param) {
	pv_spec_t avp_spec;
	struct multiparam_t *mp;
	str s;

	s.s = (char *)(*param);
	s.len = strlen(s.s);
	if (s.len <= 0) return -1;
	if (pv_parse_spec(&s, &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
		LM_ERR("Malformed or non AVP definition <%s>\n", (char *)(*param));
		return -1;
	}
	
	mp = (struct multiparam_t *)pkg_malloc(sizeof(struct multiparam_t));
	if (mp == NULL) {
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(mp, 0, sizeof(struct multiparam_t));
	
	mp->type=MP_AVP;
	if(pv_get_avp_name(0, &(avp_spec.pvp), &(mp->u.a.name), &(mp->u.a.flags))!=0) {
		LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
		pkg_free(mp);
		return -1;
	}

	*param = (void*)mp;
	
	return 0;
}


/**
 * fixes the module functions' parameters, i.e. it maps
 * the routing domain names to numbers for faster access
 * at runtime
 *
 * @param param the parameter
 * @param param_no the number of the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int route_fixup(void ** param, int param_no) {
	enum hash_source my_hash_source;

	if (param_no == 1) {
		/* carrier */
		if (carrier_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}
	else if (param_no == 2) {
		/* domain */
		if (domain_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}
	else if ((param_no == 3) || (param_no == 4)){
		/* prefix matching */
		/* rewrite user */
		if (pv_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}
	else if (param_no == 5) {
		/* hash source */
		if ((my_hash_source = hash_fixup((char *)*param)) == shs_error) {
			LM_ERR("invalid hash source\n");
			return -1;
		}
		pkg_free(*param);
		*param = (void *)my_hash_source;
	}
	else if (param_no == 6) {
		/* destination avp name */
		if (avp_name_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}

	return 0;
}


/**
 * fixes the module functions' parameters, i.e. it maps
 * the routing domain names to numbers for faster access
 * at runtime
 *
 * @param param the parameter
 * @param param_no the number of the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int load_next_domain_fixup(void ** param, int param_no) {
	if (param_no == 1) {
		/* carrier */
		if (carrier_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}
	else if (param_no == 2) {
		/* domain */
		if (domain_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}
	else if ((param_no == 3) || (param_no == 4) || (param_no == 5)) {
		/* prefix matching */
		/* host */
		/* reply code */
		if (pv_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}
	else if (param_no == 6) {
		/* destination avp name */
		if (avp_name_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}

	return 0;
}


static int load_user_carrier_fixup(void ** param, int param_no) {
	if (mode == SP_ROUTE_MODE_FILE) {
		LM_ERR("command cr_user_rewrite_uri can't be used in file mode\n");
		return -1;
	}

	if ((param_no == 1) || (param_no == 2)) {
		/* user */
		/* domain */
		if (pv_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}
	else if (param_no == 3) {
		/* destination avp name */
		if (avp_name_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}

	return 0;
}


static int child_init(int rank) {
	return data_child_init();
}


static int mi_child_init(void) {
	return data_child_init();
}


static void mod_destroy(void) {
	destroy_route_data();
}

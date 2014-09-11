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
 * \file cr_fixup.c
 * \brief Fixup functions.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include "../../mod_fix.h"
#include "../../mem/mem.h"
#include "cr_fixup.h"
#include "carrierroute.h"
#include "cr_map.h"
#include "cr_domain.h"
#include "prime_hash.h"
#include "cr_data.h"


/**
 * The fixup funcions will use the initial mapping.
 * If the mapping changes afterwards (eg. due to cr_reload_routes),
 * the names used in the routing script will not be mapped
 * to the correct IDs!
 * @param name carrier name
 * @return carrier id
 */
static int carrier_name_2_id(const str *name) {
	int id;
	struct route_data_t * rd;
	
	do {
		rd = get_data();
	} while (rd == NULL);

	id = map_name2id(rd->carrier_map, rd->carrier_num, name);
	
	release_data(rd);

	return id;
}


/**
 * The fixup funcions will use the initial mapping.
 * If the mapping changes afterwards (eg. due to cr_reload_routes),
 * the names used in the routing script will not be mapped
 * to the correct IDs!
 * @param name domain name
 * @return domain id 
 */
static int domain_name_2_id(const str *name) {
	int id;
	struct route_data_t * rd;

	do {
		rd = get_data();
	} while (rd == NULL);
	
	id = map_name2id(rd->domain_map, rd->domain_num, name);
	
	release_data(rd);

	return id;
}


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
	} else if (strcasecmp("rand", my_hash_source) == 0) {
		return shs_rand;
	} else {
		return shs_error;
	}
}


/**
 * Fixes the module functions' parameters if it is a carrier.
 * supports name string and PVs.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int carrier_fixup(void ** param) {
	int id;

	if (fixup_spve_null(param, 1) !=0) {
		LM_ERR("could not fixup parameter");
		return -1;
	}

	if (((gparam_p)(*param))->type == GPARAM_TYPE_STR) {
		/* This is a name string, convert to a int */
		((gparam_p)(*param))->type=GPARAM_TYPE_INT;
		/* get carrier id */
		if ((id = carrier_name_2_id(&((gparam_p)(*param))->v.str)) < 0) {
			LM_ERR("could not find carrier name '%.*s' in map\n", ((gparam_p)(*param))->v.str.len, ((gparam_p)(*param))->v.str.s);
			pkg_free(*param);
			return -1;
		}
		((gparam_p)(*param))->v.i = id;
	}
	return 0;
}


/**
 * Fixes the module functions' parameters if it is a domain.
 * supports name string, and PVs.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int domain_fixup(void ** param) {
	int id;

	if (fixup_spve_null(param, 1) !=0) {
		LM_ERR("could not fixup parameter");
		return -1;
	}

	if (((gparam_p)(*param))->type == GPARAM_TYPE_STR) {
		/* This is a name string, convert to a int */
		((gparam_p)(*param))->type=GPARAM_TYPE_INT;
		/* get domain id */
		if ((id = domain_name_2_id(&(((gparam_p)(*param))->v.str))) < 0) {
			LM_ERR("could not find domain name '%.*s' in map\n", ((gparam_p)(*param))->v.str.len, ((gparam_p)(*param))->v.str.s);
			pkg_free(*param);
			return -1;
		}
		((gparam_p)(*param))->v.i = id;
	}
	return 0;
}


/**
 * Fixes the module functions' parameters in case of AVP names.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int avp_name_fixup(void ** param) {

	if (fixup_spve_null(param, 1) !=0) {
		LM_ERR("could not fixup parameter");
		return -1;
	}
	if (((gparam_p)(*param))->v.pve->spec->type == PVT_AVP &&
			((gparam_p)(*param))->v.pve->spec->pvp.pvn.u.isname.name.s.len == 0 &&
			((gparam_p)(*param))->v.pve->spec->pvp.pvn.u.isname.name.s.s == 0) {
		LM_ERR("malformed or non AVP type definition\n");
		return -1;
	}
	return 0;
}


/**
 * Fixes the module functions' parameters, i.e. it maps
 * the routing domain names to numbers for faster access
 * at runtime
 *
 * @param param the parameter
 * @param param_no the number of the parameter
 *
 * @return 0 on success, -1 on failure
 */
int cr_route_fixup(void ** param, int param_no) {
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
		/* prefix matching, rewrite user */
		if (fixup_spve_null(param, 1) != 0) {
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
int cr_load_next_domain_fixup(void ** param, int param_no) {
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
		/* prefix matching, host, reply code */
		if (fixup_spve_null(param, 1) != 0) {
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


/**
 * Fixes the module functions' parameters.
 *
 * @param param the parameter
 * @param param_no the number of the parameter
 *
 * @return 0 on success, -1 on failure
 */
int cr_load_user_carrier_fixup(void ** param, int param_no) {
	if (mode == CARRIERROUTE_MODE_FILE) {
		LM_ERR("command cr_user_rewrite_uri can't be used in file mode\n");
		return -1;
	}

	if ((param_no == 1) || (param_no == 2)) {
		/* user, domain */
		if (fixup_spve_null(param, 1) != 0) {
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

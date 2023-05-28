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
 * \file cr_fixup.c
 * \brief Fixup functions.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include "../../core/mod_fix.h"
#include "../../core/mem/mem.h"
#include "cr_fixup.h"
#include "carrierroute.h"
#include "cr_map.h"
#include "cr_domain.h"
#include "prime_hash.h"
#include "cr_data.h"


/**
 * The fixup functions will use the initial mapping.
 * If the mapping changes afterwards (eg. due to cr_reload_routes),
 * the names used in the routing script will not be mapped
 * to the correct IDs!
 * @param name carrier name
 * @return carrier id
 */
static int carrier_name_2_id(const str *name)
{
	int id;
	struct route_data_t *rd;

	do {
		rd = get_data();
	} while(rd == NULL);

	id = map_name2id(rd->carrier_map, rd->carrier_num, name);

	release_data(rd);

	return id;
}


/**
 * The fixup functions will use the initial mapping.
 * If the mapping changes afterwards (eg. due to cr_reload_routes),
 * the names used in the routing script will not be mapped
 * to the correct IDs!
 * @param name domain name
 * @return domain id 
 */
static int domain_name_2_id(const str *name)
{
	int id;
	struct route_data_t *rd;

	do {
		rd = get_data();
	} while(rd == NULL);

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
static int hash_fixup(void **param)
{
	enum hash_source my_hash_source = shs_error;
	char *hash_name;

	if(fixup_spve_null(param, 1) != 0) {
		LM_ERR("could not fixup parameter");
		return -1;
	}

	if(((gparam_p)(*param))->type == GPARAM_TYPE_STR) {
		hash_name = ((gparam_p)(*param))->v.str.s;

		if(strcasecmp("call_id", hash_name) == 0) {
			my_hash_source = shs_call_id;
		} else if(strcasecmp("from_uri", hash_name) == 0) {
			my_hash_source = shs_from_uri;
		} else if(strcasecmp("from_user", hash_name) == 0) {
			my_hash_source = shs_from_user;
		} else if(strcasecmp("to_uri", hash_name) == 0) {
			my_hash_source = shs_to_uri;
		} else if(strcasecmp("to_user", hash_name) == 0) {
			my_hash_source = shs_to_user;
		} else if(strcasecmp("rand", hash_name) == 0) {
			my_hash_source = shs_rand;
		} else {
			LM_ERR("invalid hash source\n");
			pkg_free(*param);
			return -1;
		}
	}

	pkg_free(*param);
	*param = (void *)my_hash_source;

	return 0;
}


/**
 * Fixes the module functions' parameters if it is a carrier.
 * supports name string and PVs.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int carrier_fixup(void **param)
{
	int id;

	if(fixup_spve_null(param, 1) != 0) {
		LM_ERR("could not fixup parameter");
		return -1;
	}

	if(((gparam_p)(*param))->type == GPARAM_TYPE_STR) {
		if(str2sint(&(((gparam_p)(*param))->v.str), &id) != 0) {
			/* get carrier id */
			if((id = carrier_name_2_id(&((gparam_p)(*param))->v.str)) < 0) {
				LM_ERR("could not find carrier name '%.*s' in map\n",
						((gparam_p)(*param))->v.str.len,
						((gparam_p)(*param))->v.str.s);
				pkg_free(*param);
				return -1;
			}
		}

		/* This is a name string, convert to an int */
		((gparam_p)(*param))->type = GPARAM_TYPE_INT;
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
static int domain_fixup(void **param)
{
	int id;

	if(fixup_spve_null(param, 1) != 0) {
		LM_ERR("could not fixup parameter");
		return -1;
	}

	if(((gparam_p)(*param))->type == GPARAM_TYPE_STR) {
		if(str2sint(&(((gparam_p)(*param))->v.str), &id) != 0) {
			/* get domain id */
			if((id = domain_name_2_id(&(((gparam_p)(*param))->v.str))) < 0) {
				LM_ERR("could not find domain name '%.*s' in map\n",
						((gparam_p)(*param))->v.str.len,
						((gparam_p)(*param))->v.str.s);
				pkg_free(*param);
				return -1;
			}
		}

		/* This is a name string, convert to an int */
		((gparam_p)(*param))->type = GPARAM_TYPE_INT;
		((gparam_p)(*param))->v.i = id;
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
int cr_route_fixup(void **param, int param_no)
{
	if(param_no == 1) {
		/* carrier */
		if(carrier_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	} else if(param_no == 2) {
		/* domain */
		if(domain_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	} else if((param_no == 3) || (param_no == 4)) {
		/* prefix matching, rewrite user */
		if(fixup_spve_null(param, 1) != 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	} else if(param_no == 5) {
		/* hash source */
		if(hash_fixup(param) != 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	} else if(param_no == 6) {
		/* destination avp name */
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("dst var is not writeble\n");
			return -1;
		}
	}

	return 0;
}


/**
 *
 */
int cr_route_fixup_free(void **param, int param_no)
{
	if((param_no >= 1) && (param_no <= 5)) {
		/* carrier, domain, prefix matching, rewrite user, hash source */
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 6) {
		/* destination var name */
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
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
int cr_load_next_domain_fixup(void **param, int param_no)
{
	if(param_no == 1) {
		/* carrier */
		if(carrier_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	} else if(param_no == 2) {
		/* domain */
		if(domain_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	} else if((param_no == 3) || (param_no == 4) || (param_no == 5)) {
		/* prefix matching, host, reply code */
		if(fixup_spve_null(param, 1) != 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	} else if(param_no == 6) {
		/* destination avp name */
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("dst var is not writeble\n");
			return -1;
		}
	}

	return 0;
}


/**
 *
 */
int cr_load_next_domain_fixup_free(void **param, int param_no)
{
	if((param_no >= 1) && (param_no <= 5)) {
		/* carrier, domain, prefix matching, host, reply code */
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 6) {
		/* destination var name */
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}


/**
 * Fixes the module functions' parameters.
 *
 * @param param the parameter
 * @param param_no the number of the parameter
 *
 * @return 0 on success, -1 on failure
 */
int cr_load_user_carrier_fixup(void **param, int param_no)
{
	if(mode == CARRIERROUTE_MODE_FILE) {
		LM_ERR("command cr_user_rewrite_uri can't be used in file mode\n");
		return -1;
	}

	if((param_no == 1) || (param_no == 2)) {
		/* user, domain */
		if(fixup_spve_null(param, 1) != 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	} else if(param_no == 3) {
		/* destination var name */
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("dst var is not writeble\n");
			return -1;
		}
	}

	return 0;
}


/**
 *
 */
int cr_load_user_carrier_fixup_free(void **param, int param_no)
{
	if((param_no >= 1) && (param_no <= 2)) {
		/* user, domain */
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 3) {
		/* destination var name */
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

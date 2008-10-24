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
	gparam_t *gp;
	str s;

	gp = (gparam_t *)pkg_malloc(sizeof(gparam_t));
	if (gp == NULL) {
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(gp, 0, sizeof(gparam_t));
	
	s.s = (char *)(*param);
	s.len = strlen(s.s);

	if (s.s[0]!='$') {
		/* This is a name string */
		gp->type=GPARAM_TYPE_INT;
		
		/* get carrier id */
		if ((gp->v.ival = find_carrier(s)) < 0) {
			LM_ERR("could not find carrier '%s'\n", (char *)(*param));
			pkg_free(gp);
			return -1;
		}
		LM_INFO("carrier %s has id %i\n", (char *)*param, gp->v.ival);
		
		pkg_free(*param);
		*param = (void *)gp;
	}
	else {
		/* This is a pseudo-variable */
		if (pv_parse_spec(&s, &avp_spec)==0) {
			LM_ERR("pv_parse_spec failed for '%s'\n", (char *)(*param));
			pkg_free(gp);
			return -1;
		}
		if (avp_spec.type==PVT_AVP) {
			/* This is an AVP - could be an id or name */
			gp->type=GPARAM_TYPE_AVP;
			if(pv_get_avp_name(0, &(avp_spec.pvp), &(gp->v.avp.name), &(gp->v.avp.flags))!=0) {
				LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
				pkg_free(gp);
				return -1;
			}
		} else {
			gp->type=GPARAM_TYPE_PVE;
			if(pv_parse_format(&s, &(gp->v.pve))<0) {
				LM_ERR("pv_parse_format failed for '%s'\n", (char *)(*param));
				pkg_free(gp);
				return -1;
			}
		}
	}
	*param = (void*)gp;

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
	gparam_t *gp;
	str s;

	gp = (gparam_t *)pkg_malloc(sizeof(gparam_t));
	if (gp == NULL) {
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(gp, 0, sizeof(gparam_t));
	
	s.s = (char *)(*param);
	s.len = strlen(s.s);
	
	if (s.s[0]!='$') {
		/* This is a name string */
		gp->type=GPARAM_TYPE_INT;
		
		/* get domain id */
		if ((gp->v.ival = add_domain(&s)) < 0) {
			LM_ERR("could not add domain\n");
			pkg_free(gp);
			return -1;
		}
		pkg_free(*param);
		*param = (void *)gp;
	}
	else {
		/* This is a pseudo-variable */
		if (pv_parse_spec(&s, &avp_spec)==0) {
			LM_ERR("pv_parse_spec failed for '%s'\n", (char *)(*param));
			pkg_free(gp);
			return -1;
		}
		if (avp_spec.type==PVT_AVP) {
			/* This is an AVP - could be an id or name */
			gp->type=GPARAM_TYPE_AVP;
			if(pv_get_avp_name(0, &(avp_spec.pvp), &(gp->v.avp.name), &(gp->v.avp.flags))!=0) {
				LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
				pkg_free(gp);
				return -1;
			}
		} else {
			gp->type=GPARAM_TYPE_PVE;
			if(pv_parse_format(&s, &(gp->v.pve))<0) {
				LM_ERR("pv_parse_format failed for '%s'\n", (char *)(*param));
				pkg_free(gp);
				return -1;
			}
		}	
	}
	*param = (void*)gp;

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
	gparam_t *gp;
	str s;

	s.s = (char *)(*param);
	s.len = strlen(s.s);
	if (s.len <= 0) return -1;
	if (pv_parse_spec(&s, &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
		LM_ERR("Malformed or non AVP definition <%s>\n", (char *)(*param));
		return -1;
	}
	
	gp = (gparam_t *)pkg_malloc(sizeof(gparam_t));
	if (gp == NULL) {
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(gp, 0, sizeof(gparam_t));
	
	gp->type=GPARAM_TYPE_AVP;
	if(pv_get_avp_name(0, &(avp_spec.pvp), &(gp->v.avp.name), &(gp->v.avp.flags))!=0) {
		LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
		pkg_free(gp);
		return -1;
	}

	*param = (void*)gp;
	
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


int cr_load_user_carrier_fixup(void ** param, int param_no) {
	if (mode == CARRIERROUTE_MODE_FILE) {
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

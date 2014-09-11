/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#ifndef _IMS_AUTH_API_H_
#define _IMS_AUTH_API_H_

#include "../../sr_module.h"
#include "../../parser/msg_parser.h"

/**
 * return codes to config by auth functions
 */
typedef enum auth_cfg_result {
	AUTH_RESYNC_REQUESTED = -9,	/*!< resync requested from UE via auts param */
	AUTH_USER_MISMATCH = -8,    /*!< Auth user != From/To user */
	AUTH_NONCE_REUSED = -6,     /*!< Returned if nonce is used more than once */
	AUTH_NO_CREDENTIALS = -5,   /*!< Credentials missing */
	AUTH_STALE_NONCE = -4,      /*!< Stale nonce */
	AUTH_USER_UNKNOWN = -3,     /*!< User not found */
	AUTH_INVALID_PASSWORD = -2, /*!< Invalid password */
	AUTH_ERROR = -1,            /*!< Error occurred */
	AUTH_DROP = 0,              /*!< Error, stop config execution */
	AUTH_OK = 1                 /*!< Success */
} auth_cfg_result_t;

typedef int (*digest_authenticate_f)(struct sip_msg* msg, str *realm,
				str *table, hdr_types_t hftype);
/**
 * @brief IMS_AUTH API structure
 */
typedef struct ims_auth_api {
	digest_authenticate_f digest_authenticate;
} ims_auth_api_t;

typedef int (*bind_ims_auth_f)(ims_auth_api_t* api);

/**
 * @brief Load the IMS_AUTH API
 */
static inline int ims_auth_load_api(ims_auth_api_t *api)
{
	bind_ims_auth_f bindauthims;

	bindauthims = (bind_ims_auth_f)find_export("bind_ims_auth", 0, 0);
	if(bindauthims == 0) {
		LM_ERR("cannot find bind_ims_auth\n");
		return -1;
	}
	if (bindauthims(api)==-1)
	{
		LM_ERR("cannot bind authims api\n");
		return -1;
	}
	return 0;
}

#endif /* _IMS_AUTH_API_H_ */

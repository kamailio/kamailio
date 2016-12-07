/*
 * Digest Authentication - Database support
 *
 * Copyright (C) 2001-2003 FhG Fokus
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


#ifndef _AUTH_DB_API_H_
#define _AUTH_DB_API_H_

#include "../../sr_module.h"
#include "../../parser/msg_parser.h"

typedef int (*digest_authenticate_f)(struct sip_msg* msg, str *realm,
				str *table, hdr_types_t hftype, str *method);
/**
 * @brief AUTH_DB API structure
 */
typedef struct auth_db_api {
	digest_authenticate_f digest_authenticate;
} auth_db_api_t;

typedef int (*bind_auth_db_f)(auth_db_api_t* api);

/**
 * @brief Load the SL API
 */
static inline int auth_db_load_api(auth_db_api_t *api)
{
	bind_auth_db_f bindauthdb;

	bindauthdb = (bind_auth_db_f)find_export("bind_auth_db", 0, 0);
	if(bindauthdb == 0) {
		LM_ERR("cannot find bind_auth_db\n");
		return -1;
	}
	if (bindauthdb(api)==-1)
	{
		LM_ERR("cannot bind authdb api\n");
		return -1;
	}
	return 0;
}

#endif /* _AUTH_DB_API_H_ */

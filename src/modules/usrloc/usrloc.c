/*
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
 *
 */

/*! \file
 *  \brief USRLOC - module API exports interfaces
 *  \ingroup usrloc
 *
 * - Module \ref usrloc
 */

#include "usrloc.h"
#include "dlist.h"
#include "urecord.h"
#include "ucontact.h"
#include "udomain.h"
#include "../../sr_module.h"
#include "ul_mod.h"


/*!
 * \brief usrloc module API export bind function
 * \param api usrloc API
 * \return 0 on success, -1 on failure
 */
int bind_usrloc(usrloc_api_t* api)
{
	if (!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	if (init_flag==0) {
		LM_ERR("configuration error - trying to bind to usrloc module"
				" before being initialized\n");
		return -1;
	}

	api->register_udomain   = register_udomain;
	api->get_udomain        = get_udomain;
	api->get_all_ucontacts  = get_all_ucontacts;
	api->insert_urecord     = insert_urecord;
	api->delete_urecord     = delete_urecord;
	api->delete_urecord_by_ruid     = delete_urecord_by_ruid;
	api->get_urecord        = get_urecord;
	api->lock_udomain       = lock_udomain;
	api->unlock_udomain     = unlock_udomain;
	api->release_urecord    = release_urecord;
	api->insert_ucontact    = insert_ucontact;
	api->delete_ucontact    = delete_ucontact;
	api->get_ucontact       = get_ucontact;
	api->update_ucontact    = update_ucontact;
	api->register_ulcb      = register_ulcb;
	api->get_aorhash        = ul_get_aorhash;

	api->get_urecord_by_ruid      = get_urecord_by_ruid;
	api->get_ucontact_by_instance = get_ucontact_by_instance;

	api->set_keepalive_timeout    = ul_set_keepalive_timeout;
	api->refresh_keepalive        = ul_refresh_keepalive;
	api->set_max_partition        = ul_set_max_partition;

	api->use_domain = use_domain;
	api->db_mode    = db_mode;
	api->nat_flag   = nat_bflag;

	return 0;
}

/**
 *
 */
int ul_set_keepalive_timeout(int _to)
{
	int oto;

	oto = ul_keepalive_timeout;
	ul_keepalive_timeout = _to;
	return oto;
}

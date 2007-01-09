/*
 * $Id$
 *
 * Usrloc interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ========
 *
 * 2006-11-28 Added a new function to the usrloc_api, to retrieve the number
 *            of registered users.  (Jeffrey Magder - SOMA Networks)
 */

 
#include "usrloc.h"
#include "../../sr_module.h"
#include "ul_mod.h"

extern unsigned int nat_bflag;
extern unsigned int init_flag;


int bind_usrloc(usrloc_api_t* api)
{
	if (!api) {
		LOG(L_ERR, "ERROR:usrloc:bind_usrloc: invalid parameter value\n");
		return -1;
	}
	if (init_flag==0) {
		LOG(L_ERR, "ERROR:usrloc:bind_usrloc: configuration error - trying "
			"to bind to usrlo module before being initialized\n");
		return -1;
	}

	api->register_udomain = (register_udomain_t)register_udomain;
	api->get_all_ucontacts = (get_all_ucontacts_t)get_all_ucontacts;
	api->insert_urecord = (insert_urecord_t)insert_urecord;
	api->delete_urecord = (delete_urecord_t)delete_urecord;
	api->get_urecord = (get_urecord_t)get_urecord;
	api->lock_udomain = (lock_udomain_t)lock_udomain;
	api->unlock_udomain = (unlock_udomain_t)unlock_udomain;
	api->release_urecord = (release_urecord_t)release_urecord;
	api->insert_ucontact = (insert_ucontact_t)insert_ucontact;
	api->delete_ucontact = (delete_ucontact_t)delete_ucontact;
	api->get_ucontact = (get_ucontact_t)get_ucontact;
	api->update_ucontact = (update_ucontact_t)update_ucontact;
	api->register_watcher = (register_watcher_t)register_watcher;
	api->unregister_watcher = (unregister_watcher_t)unregister_watcher;
	api->register_ulcb = (register_ulcb_t)register_ulcb;

	api->use_domain = use_domain;
	api->db_mode = db_mode;
	api->nat_flag = nat_bflag;

	return 0;
}

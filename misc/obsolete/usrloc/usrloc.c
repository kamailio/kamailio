/*
 * $Id$
 *
 * Usrloc interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

 
#include "usrloc.h"
#include "../../sr_module.h"
#include "ul_mod.h"

int bind_usrloc(usrloc_api_t* api)
{
	if (!api) {
		LOG(L_ERR, "bind_usrloc(): Invalid parameter value\n");
		return -1;
	}

	api->register_udomain = (register_udomain_t)find_export("ul_register_udomain", 1, 0);
	if (api->register_udomain == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind register_udomain\n");
		return -1;
	}

	api->get_all_ucontacts = (get_all_ucontacts_t)find_export("ul_get_all_ucontacts", 1, 0);
	if (api->get_all_ucontacts == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind get_all_ucontacts\n");
		return -1;
	}

	api->insert_urecord = (insert_urecord_t)find_export("ul_insert_urecord", 1, 0);
	if (api->insert_urecord == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind insert_urecord\n");
		return -1;
	}

	api->delete_urecord = (delete_urecord_t)find_export("ul_delete_urecord", 1, 0);
	if (api->delete_urecord == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind delete_urecord\n");
		return -1;
	}

	api->get_urecord = (get_urecord_t)find_export("ul_get_urecord", 1, 0);
	if (api->get_urecord == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind get_urecord\n");
		return -1;
	}

	api->lock_udomain = (lock_udomain_t)find_export("ul_lock_udomain", 1, 0);
	if (api->lock_udomain == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind loc_udomain\n");
		return -1;
	}
	
	api->unlock_udomain = (unlock_udomain_t)find_export("ul_unlock_udomain", 1, 0);
	if (api->unlock_udomain == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind unlock_udomain\n");
		return -1;
	}


	api->release_urecord = (release_urecord_t)find_export("ul_release_urecord", 1, 0);
	if (api->release_urecord == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind release_urecord\n");
		return -1;
	}

	api->insert_ucontact = (insert_ucontact_t)find_export("ul_insert_ucontact", 1, 0);
	if (api->insert_ucontact == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind insert_ucontact\n");
		return -1;
	}

	api->delete_ucontact = (delete_ucontact_t)find_export("ul_delete_ucontact", 1, 0);
	if (api->delete_ucontact == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind delete_ucontact\n");
		return -1;
	}

	api->get_ucontact = (get_ucontact_t)find_export("ul_get_ucontact", 1, 0);
	if (api->get_ucontact == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind get_ucontact\n");
		return -1;
	}

	api->get_ucontact_by_instance = (get_ucontact_by_inst_t)find_export("ul_get_ucontact_by_inst", 1, 0);
	if (api->get_ucontact_by_instance == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind get_ucontact_by_instance\n");
		return -1;
	}

	api->update_ucontact = (update_ucontact_t)find_export("ul_update_ucontact", 1, 0);
	if (api->update_ucontact == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind update_ucontact\n");
		return -1;
	}

	api->register_watcher = (register_watcher_t)
		find_export("ul_register_watcher", 1, 0);
	if (api->register_watcher == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind register_watcher\n");
		return -1;
	}

	api->unregister_watcher = (unregister_watcher_t)
		find_export("ul_unregister_watcher", 1, 0);
	if (api->unregister_watcher == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind unregister_watcher\n");
		return -1;
	}

	api->register_ulcb = (register_ulcb_t)
		find_export("ul_register_ulcb", 1, 0);
	if (api->register_ulcb == 0) {
		LOG(L_ERR, "bind_usrloc(): Can't bind register_ulcb\n");
		return -1;
	}

	return 0;
}

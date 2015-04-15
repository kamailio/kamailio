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

#include "usrloc.h"
#include "dlist.h"
#include "impurecord.h"
#include "ucontact.h"
#include "udomain.h"
#include "subscribe.h"
#include "../../sr_module.h"
#include "ul_mod.h"

/*! nat branch flag */
extern unsigned int nat_bflag;
/*! flag to protect against wrong initialization */
extern unsigned int init_flag;

/*!
 * \brief usrloc module API export bind function
 * \param api usrloc API
 * \return 0 on success, -1 on failure
 */
int bind_usrloc(usrloc_api_t* api) {
	if (!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	if (init_flag == 0) {
		LM_ERR("configuration error - trying to bind to usrloc module"
				" before being initialized\n");
		return -1;
	}

	api->register_udomain = register_udomain;
	api->get_udomain = get_udomain;

	api->insert_impurecord = insert_impurecord;
	api->delete_impurecord = delete_impurecord;
	api->get_impurecord = get_impurecord;
	api->update_impurecord = update_impurecord;

	api->lock_udomain = lock_udomain;
	api->unlock_udomain = unlock_udomain;

	api->lock_contact_slot = lock_contact_slot;
	api->unlock_contact_slot = unlock_contact_slot;
	api->lock_contact_slot_i = lock_contact_slot_i;
	api->unlock_contact_slot_i = unlock_contact_slot_i;	
	api->get_all_ucontacts = get_all_ucontacts;
	api->insert_ucontact = insert_ucontact;
	api->delete_ucontact = delete_ucontact;
	api->get_ucontact = get_ucontact;
	api->release_ucontact = release_ucontact;
	api->update_ucontact = update_ucontact;
	api->expire_ucontact = expire_ucontact;
	api->add_dialog_data_to_contact = add_dialog_data_to_contact;
	api->remove_dialog_data_from_contact = remove_dialog_data_from_contact;
	api->unlink_contact_from_impu = unlink_contact_from_impu;
	api->link_contact_to_impu = link_contact_to_impu;
	api->get_subscriber = get_subscriber;
	api->add_subscriber = add_subscriber;
	api->external_delete_subscriber = external_delete_subscriber;
	api->update_subscriber = update_subscriber;

	api->get_impus_from_subscription_as_string = get_impus_from_subscription_as_string;
	
	api->get_presentity_from_subscriber_dialog = get_presentity_from_subscriber_dialog;
        
	api->register_ulcb = register_ulcb;

	//api->update_user_profile = update_user_profile;
	api->nat_flag = nat_bflag;

	return 0;
}

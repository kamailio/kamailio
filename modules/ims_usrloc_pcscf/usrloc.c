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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include "usrloc.h"
#include "dlist.h"
#include "pcontact.h"
#include "udomain.h"
#include "../../sr_module.h"
#include "ul_mod.h"

extern unsigned int init_flag;

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
	api->lock_udomain = lock_udomain;
	api->unlock_udomain = unlock_udomain;

	api->insert_pcontact = insert_pcontact;
	api->delete_pcontact = delete_pcontact;
	api->get_pcontact = get_pcontact;
	api->get_pcontact_by_src = get_pcontact_by_src;
	api->assert_identity = assert_identity;

	api->update_pcontact = update_pcontact;
	api->update_rx_regsession = update_rx_regsession;

	api->get_all_ucontacts = get_all_ucontacts;

	api->register_ulcb = register_ulcb;

	return 0;
}

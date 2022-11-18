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
#include "pcontact.h"
#include "udomain.h"
#include "../../core/sr_module.h"
#include "ims_usrloc_pcscf_mod.h"
#include "../../core/parser/parse_uri.h"

extern int ims_ulp_init_flag;

struct ul_callback *cbp_registrar = 0;
struct ul_callback *cbp_qos = 0;


int bind_usrloc(usrloc_api_t* api) {
	if (!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	if (ims_ulp_init_flag == 0) {
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
        api->unreg_pending_contacts_cb = unreg_pending_contacts_cb;
	api->get_pcontact = get_pcontact;
	api->assert_identity = assert_identity;
	api->update_pcontact = update_pcontact;
	api->update_rx_regsession = update_rx_regsession;
	api->get_all_ucontacts = get_all_ucontacts;
	api->update_security = update_security;
	api->update_temp_security = update_temp_security;
	api->register_ulcb = register_ulcb;
	api->get_number_of_contacts = get_number_of_contacts;
	api->is_ulcb_registered = is_ulcb_registered;
	api->register_ulcb_method = register_ulcb_method;

	return 0;
}

#define ALIAS        "alias="
#define ALIAS_LEN (sizeof(ALIAS) - 1)

int get_alias_host_from_contact(str *contact_uri_params, str *alias_host) {
    char *rest, *sep;
    unsigned int rest_len;
    
    rest = contact_uri_params->s;
    rest_len = contact_uri_params->len;
    if (rest_len == 0) {
        LM_DBG("no params\n");
        return -1;
    }

    /*Get full alias parameter*/
    while (rest_len >= ALIAS_LEN) {
        if (strncmp(rest, ALIAS, ALIAS_LEN) == 0) break;
        sep = memchr(rest, 59 /* ; */, rest_len);
        if (sep == NULL) {
            LM_DBG("no alias param\n");
            return -1;
        } else {
            rest_len = rest_len - (sep - rest + 1);
            rest = sep + 1;
        }
    }

    if (rest_len < ALIAS_LEN) {
        LM_DBG("no alias param\n");
        return -1;
    }

    alias_host->s = rest + ALIAS_LEN;
    alias_host->len = rest_len - ALIAS_LEN;

    /*Get host from alias*/
    rest = memchr(alias_host->s, 126 /* ~ */, alias_host->len);
    if (rest == NULL) {
        LM_ERR("no '~' in alias param value\n");
        return -1;
    }
    alias_host->len = rest - alias_host->s;
    LM_DBG("Alias host to return [%.*s]\n", alias_host->len, alias_host->s);
    return 0;
}


/* return the slot id for inserting contacts in the hash */
unsigned int get_hash_slot(udomain_t* _d, str* via_host, unsigned short via_port, unsigned short via_proto) {
    unsigned int sl;

    sl = get_aor_hash(_d, via_host, via_port, via_proto);
    sl = sl & (_d->size - 1) ;
    LM_DBG("Returning hash slot: [%d]\n", sl);

    return sl;
}

unsigned int get_aor_hash(udomain_t* _d, str* via_host, unsigned short via_port, unsigned short via_proto) {
    unsigned int aorhash;
        
    aorhash = core_hash(via_host, 0, 0);
    LM_DBG("Returning hash: [%u]\n", aorhash);

    return aorhash;
}

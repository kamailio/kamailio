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


#ifndef CXDX_MAR_H
#define CXDX_MAR_H

#include "../../mod_fix.h"

struct sip_msg;
struct parsed_maa;

extern struct cdp_binds cdpb;
extern str cxdx_forced_peer; /**< FQDN of the Diameter peer to send requests to */
extern str cxdx_dest_realm;
extern struct tm_binds tmb;

typedef struct saved_transaction {
	unsigned int tindex;
	unsigned int tlabel;
	unsigned int ticks;
	cfg_action_t *act;
	int is_proxy_auth;
	int is_resync;
    str realm;
} saved_transaction_t;

//linked list of auth_data information for maa structure
struct auth_data_item {
    int item_number;
    struct auth_data_item *next;
    struct auth_data_item *previous;
    str authenticate;
    str authorization;
    str auth_scheme;
    str ck;
    str ik;
    str ip;
    str ha1;
    str line_identifier;
    str response_auth;
    str digest_realm;

};

/*! list of auth data item entries in the parsed maa structure */
struct auth_data_item_list {
    struct auth_data_item *first; /*!< auth data list */
    struct auth_data_item *last; /*!< optimisation, end of the auth data list */
};

void free_saved_transaction_data(saved_transaction_t* data);
int create_return_code(int result);

/**
 * Create and send a Multimedia-Authentication-Request and returns the parsed Answer structure.
 * This function retrieves authentication vectors from the HSS.
 * @param msg - the SIP message to send for
 * @parma public_identity - the public identity of the user
 * @param private_identity - the private identity of the user
 * @param count - how many authentication vectors to ask for
 * @param algorithm - for which algorithm
 * @param authorization - the authorization value
 * @param server_name - local name of the S-CSCF to save on the HSS
 * @param realm - Realm of the user
 * @returns the parsed maa struct
 */
int cxdx_send_mar(struct sip_msg *msg, str public_identity, str private_identity,
					unsigned int count,str algorithm,str authorization,str server_name, saved_transaction_t* transaction_data);

void async_cdp_callback(int is_timeout, void *param, AAAMessage *maa, long elapsed_msecs);

#endif


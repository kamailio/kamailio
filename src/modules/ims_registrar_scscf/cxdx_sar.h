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

#ifndef CXDX_SAR_H
#define CXDX_SAR_H


extern struct cdp_binds cdpb;
extern str cxdx_forced_peer; /**< FQDN of the Diameter peer to send requests to */
extern str cxdx_dest_realm;

typedef struct saved_transaction {
	unsigned int tindex;
	unsigned int tlabel;
	unsigned int ticks;
	cfg_action_t *act;
        int expires; //used to see if this is a dereg as then we don't need to touch usrloc! > 0 if not dereg - 0 id de-reg
        int require_user_data;
        int sar_assignment_type;
        str public_identity;
        udomain_t* domain;
        contact_for_header_t* contact_header;//used to send the 200 OK with contacts after async callback for dereg where we don't rebuild the contacts
} saved_transaction_t;

/**
 * Create and send a Server-Assignment-Request and returns the Answer received for it.
 * This function performs the Server Assignment operation.
 * @param msg - the SIP message to send for
 * @parma public_identity - the public identity of the user
 * @param server_name - local name of the S-CSCF to save on the HSS
 * @param realm - Realm of the user
 * @param assignment_type - type of the assignment
 * @param data_available - if the data is already available
 * @returns the SAA
 */
int cxdx_send_sar(struct sip_msg *msg, str public_identity, str private_identity,
					str server_name,int assignment_type, int data_available, saved_transaction_t* transaction_data);

void free_saved_transaction_data(saved_transaction_t* data);
int create_return_code(int result);
 

#endif


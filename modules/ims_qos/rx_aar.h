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
 *
 *
 * History:
 * --------
 *  2011-02-02  initial version (jason.penton)
 */

#ifndef RX_AAR_H
#define RX_AAR_H

#include "../../mod_fix.h"
#include "../../locking.h"

struct cdp_binds cdpb;
cdp_avp_bind_t *cdp_avp;

/*this is the parcel to pass for CDP async for AAR*/
typedef struct saved_transaction {
	gen_lock_t *lock;
	unsigned int ignore_replies;
	unsigned int answers_not_received;
	unsigned int failed;	//will start at 0 - if 1 fails we can set the flag up (1)
	unsigned int tindex;
	unsigned int tlabel;
	unsigned int ticks;
	cfg_action_t *act;
	udomain_t* domain;
        str callid;
        str ftag;
        str ttag;
	unsigned int aar_update;
} saved_transaction_t;

typedef struct saved_transaction_local {
	int is_rereg;
	str contact;
	str auth_session_id;
	saved_transaction_t* global_data;
} saved_transaction_local_t;

/* the destination realm*/
extern str rx_dest_realm;
extern str rx_forced_peer;
extern int rx_auth_expiry;


/* AAR */
struct AAAMessage;
struct sip_msg;
struct rx_authdata;

void free_saved_transaction_data(saved_transaction_local_t* data);
void free_saved_transaction_global_data(saved_transaction_t* data);

//AAAMessage *rx_send_aar(struct sip_msg *req, struct sip_msg *res, AAASession* auth, str *callid, str *ftag, str *ttag, char *direction, rx_authsessiondata_t **rx_authdata);
int rx_send_aar(struct sip_msg *req, struct sip_msg *res, AAASession* auth, char *direction, saved_transaction_t* saved_t_data);

//send AAR to remove video after failed AAR update that added video
int rx_send_aar_update_no_video(AAASession* auth);


//TODOD remove - no longer user AOR parm
//int rx_send_aar_register(struct sip_msg *msg, AAASession* auth, str *ip_address, uint16_t *ip_version, str *aor, saved_transaction_local_t* saved_t_data);
int rx_send_aar_register(struct sip_msg *msg, AAASession* auth, saved_transaction_local_t* saved_t_data);

int rx_process_aaa(AAAMessage *aaa, unsigned int * rc);
enum dialog_direction get_dialog_direction(char *direction);

void async_aar_reg_callback(int is_timeout, void *param, AAAMessage *aaa, long elapsed_msecs);

void async_aar_callback(int is_timeout, void *param, AAAMessage *aaa, long elapsed_msecs);

#endif


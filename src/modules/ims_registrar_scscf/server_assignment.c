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

#include "server_assignment.h"
#include "reg_mod.h"
#include "../../lib/ims/ims_getters.h"
#include "../cdp/diameter_ims_code_avp.h"
#include "cxdx_sar.h"

extern str scscf_name_str;
/* Does the Server Assignment procedures, assigning this S-CSCF to the user.
 * Covered cases:
 * AVP_IMS_SAR_NO_ASSIGNMENT							= 0			
 * AVP_IMS_SAR_REGISTRATION								= 1,		YES,HERE
 * AVP_IMS_SAR_RE_REGISTRATION							= 2,
 * AVP_IMS_SAR_UNREGISTERED_USER						= 3,		in S_assign_server_unreg
 * AVP_IMS_SAR_TIMEOUT_DEREGISTRATION					= 4,
 * AVP_IMS_SAR_USER_DEREGISTRATION						= 5,		YES,HERE
 * AVP_IMS_SAR_TIMEOUT_DEREGISTRATION_STORE_SERVER_NAME = 6,
 * AVP_IMS_SAR_USER_DEREGISTRATION_STORE_SERVER_NAME	= 7,		YES,HERE
 * AVP_IMS_SAR_ADMINISTRATIVE_DEREGISTRATION			= 8,
 * AVP_IMS_SAR_AUTHENTICATION_FAILURE					= 9,
 * AVP_IMS_SAR_AUTHENTICATION_TIMEOUT					= 10,
 * AVP_IMS_SAR_DEREGISTRATION_TOO_MUCH_DATA
 * 
 * @param msg - the SIP REGISTER message (that is authorized)
 * @param str2 - not used
 * @returns true if ok, false if not, break on error
 */
int scscf_assign_server(struct sip_msg *msg,
		str public_identity, str private_identity, int assignment_type,
		int data_available, saved_transaction_t* transaction_data) {


        int result = -1;        

	if (assignment_type != AVP_IMS_SAR_REGISTRATION
			&& assignment_type != AVP_IMS_SAR_RE_REGISTRATION
			&& assignment_type != AVP_IMS_SAR_USER_DEREGISTRATION
			&& assignment_type != AVP_IMS_SAR_USER_DEREGISTRATION_STORE_SERVER_NAME
			&& assignment_type != AVP_IMS_SAR_UNREGISTERED_USER) {
		LM_DBG("Invalid SAR assignment type\n");
                return result;
	}

        result = cxdx_send_sar(msg, public_identity, private_identity,
			scscf_name_str, assignment_type, data_available, transaction_data);
        
        return result;
}

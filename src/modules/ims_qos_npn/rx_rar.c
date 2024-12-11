/*
 * $Id$
 *
 * Copyright (C) 2024 Neat Path Networks GmbH, alberto@neatpath.net
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 *  2024-06-19  initial version (Alberto Diez)
 */

#include "../cdp_avp/cdp_avp_mod.h"

#include "../../core/fmsg.h"

#include "../../modules/ims_usrloc_pcscf/usrloc.h"

#include "rx_authdata.h"
#include "ims_qos_mod.h"

#include "rx_rar.h"
#include "rx_avp.h"
#include "ims_qos_stats.h"

extern struct cdp_binds cdpb;
extern cdp_avp_bind_t *cdp_avp;

#define ACCESS_NETWORK_INFO_REPORT 12


/**
 * This function is called when receiving a RAR, it checks the Session-Id to
 * find the status of the registration / call of the UE, parses the relevant information
 * of the event being reported, creates the Response and calls the corresponding event-route in the config
 * file
 * Events supported are Access-Network-Information
 * TODO: LOSS_OF_BEARER, RECOVERY_OF_BEARER
 * @param request - the AAAMessage with the RAR
 * @returns an RAA to be sent to the PCRF by the cdp stack
*/
AAAMessage *rx_process_rar(AAAMessage *request)
{
	AAAMessage *raa = 0;
	AAASession *session = 0;
	AAA_AVP *avp = NULL;
	int32_t action = 0;
	rx_authsessiondata_t *p_session_data = 0;
	str visited_net = {0};
	str pani_content = {0};
	str access_network_charging_info = {0};
	char x[4];
	str identifier = {0};

	if(!request)
		return 0;
	raa = cdpb.AAACreateResponse(request);
	if(!raa)
		return 0;
	if(request->sessionId) {
		//This one locks the session
		session = cdpb.AAAGetAuthSession(request->sessionId->data);
		if(!session)
			goto unknown_session;
	} else {
		goto unknown_session;
	}

	if(!rx_avp_process_3gpp_access_network_charging_identifier(
			   request, &access_network_charging_info)) {
		LM_ERR("Error processing Access Network Charging Identifier\n");
		goto error;
	}


	//Here the session is locked
	p_session_data = (rx_authsessiondata_t *)session->u.auth.generic_data;
	if(!p_session_data)
		goto unknown_session;
	for(avp = request->avpList.head; avp; avp = avp->next) {
		switch(avp->code) {
			case AVP_IMS_Specific_Action:
				// check the type of specific Action is an enum (Integer32)
				cdp_avp->data.get_Integer32(avp, &action);
				if(action == ACCESS_NETWORK_INFO_REPORT) {
					//And then process them differently if its a signaling path status or a call
					rx_avp_process_3gpp_user_location_information(
							request, &pani_content);
					rx_avp_process_3gpp_sgsn_mcc_mnc(request, &visited_net);
					//these two functions have reserved memory

					if(p_session_data->subscribed_to_signaling_path_status) {
						identifier = p_session_data->registration_aor;
					} else {
						identifier = p_session_data->identifier;
					}
					create_avps_for_dialog_event(&p_session_data->callid,
							&p_session_data->ftag, &p_session_data->ttag,
							&p_session_data->direction);
					create_complex_return_code(2001, visited_net, pani_content,
							access_network_charging_info,
							request->sessionId->data);
					qos_run_route(NULL, &identifier, "qos:rar_access_network");
				}
				break;
			default:
				break;
		}
	}
	//TODO check if there is a transaction stopped, and in that case then continue it
	//if (p_session_data->)
	goto success;
error:
	if(session)
		cdpb.AAASessionsUnlock(session->hash);
	set_4bytes(x, 5012); // UNABLE_TO_COMPLY
	if(pani_content.s)
		pkg_free(pani_content.s);
	if(visited_net.s)
		pkg_free(visited_net.s);
	if(access_network_charging_info.s)
		pkg_free(access_network_charging_info.s);
	goto send;
unknown_session:
	if(pani_content.s)
		pkg_free(pani_content.s);
	if(visited_net.s)
		pkg_free(visited_net.s);
	if(access_network_charging_info.s)
		pkg_free(access_network_charging_info.s);
	set_4bytes(x, 5002); // UNKNOWN_SESSION_ID
	goto send;
success:
	set_4bytes(x, 2001); // SUCCESS
	cdpb.AAASessionsUnlock(session->hash);
	if(pani_content.s)
		pkg_free(pani_content.s);
	if(visited_net.s)
		pkg_free(visited_net.s);
	if(access_network_charging_info.s)
		pkg_free(access_network_charging_info.s);
send:
	rx_add_avp(raa, x, 4, AVP_Result_Code, AAA_AVP_FLAG_MANDATORY, 0,
			AVP_DUPLICATE_DATA, __FUNCTION__);
	return raa;
}

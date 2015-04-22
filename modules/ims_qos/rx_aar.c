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

#include "../../mem/shm_mem.h"
#include "../../parser/sdp/sdp.h"
#include "../cdp_avp/mod_export.h"

#include "../../modules/dialog_ng/dlg_load.h"
#include "../../modules/tm/tm_load.h"
#include "../ims_usrloc_pcscf/usrloc.h"
#include "rx_authdata.h"

#include "rx_aar.h"
#include "rx_avp.h"

#include "../../lib/ims/ims_getters.h"

#include "mod.h"

#include "../../lib/ims/useful_defs.h"
#include "ims_qos_stats.h"


#define macro_name(_rc)	#_rc

//extern struct tm_binds tmb;
usrloc_api_t ul;

extern struct ims_qos_counters_h ims_qos_cnts_h;

extern int authorize_video_flow;

str IMS_Serv_AVP_val = {"IMS Services", 12};
str IMS_Em_Serv_AVP_val = {"Emergency IMS Call", 18};
str IMS_Reg_AVP_val = {"IMS Registration", 16};

static void free_dialog_data(void *data) {
    str *rx_session_id = (str*) data;
    if (rx_session_id) {
        if (rx_session_id->s) {
            shm_free(rx_session_id->s);
            rx_session_id->s = 0;
        }
        shm_free(rx_session_id);
        rx_session_id = 0;
    }

}

void async_aar_callback(int is_timeout, void *param, AAAMessage *aaa, long elapsed_msecs) {
    struct cell *t = 0;
    unsigned int cdp_result;
    int result = CSCF_RETURN_ERROR;
    
    LM_DBG("Received AAR callback\n");
    saved_transaction_t* data = (saved_transaction_t*) param;

    LM_DBG("received AAA answer");

    if (tmb.t_lookup_ident(&t, data->tindex, data->tlabel) < 0) {
        LM_ERR("t_continue: transaction not found\n");
        goto error;
    } else {
        LM_DBG("t_continue: transaction found\n");
    }
    //we have T, lets restore our state (esp. for AVPs)
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to);

    if (is_timeout != 0) {
        LM_ERR("Error timeout when sending AAR message via CDP\n");
        counter_inc(ims_qos_cnts_h.media_aar_timeouts);
        goto error;
    }
    if (!aaa) {
        LM_ERR("Error sending message via CDP\n");
        goto error;
    }

    counter_inc(ims_qos_cnts_h.media_aars);
    counter_add(ims_qos_cnts_h.media_aar_response_time, elapsed_msecs);
    counter_inc(ims_qos_cnts_h.media_aar_replies_received);

    /* Process the response to AAR, retrieving result code and associated Rx session ID */
    if (rx_process_aaa(aaa, &cdp_result) < 0) {
        LM_ERR("Failed to process AAA from PCRF\n"); //puri.host.len, puri.host.s);
        goto error;
    }

    if (cdp_result >= 2000 && cdp_result < 3000) {
        LM_DBG("Success, received code: [%i] from PCRF for AAR request\n", cdp_result);
	counter_inc(ims_qos_cnts_h.successful_media_aars);
	
	LM_DBG("Auth session ID [%.*s]", aaa->sessionId->data.len, aaa->sessionId->data.s);

	if(!data->aar_update) {
	    LM_DBG("This is an AAA response to an initial AAR");
	    counter_inc(ims_qos_cnts_h.active_media_rx_sessions);
	    
	    str * passed_rx_session_id = shm_malloc(sizeof (struct _str));
	    passed_rx_session_id->s = 0;
	    passed_rx_session_id->len = 0;
	    STR_SHM_DUP(*passed_rx_session_id, aaa->sessionId->data, "cb_passed_rx_session_id");
	    LM_DBG("passed rx session id [%.*s]", passed_rx_session_id->len, passed_rx_session_id->s);

	    dlgb.register_dlgcb_nodlg(&data->callid, &data->ftag, &data->ttag, DLGCB_TERMINATED | DLGCB_DESTROY | DLGCB_EXPIRED | DLGCB_RESPONSE_WITHIN | DLGCB_CONFIRMED | DLGCB_FAILED, callback_dialog, (void*) (passed_rx_session_id), free_dialog_data);
	} 
        result = CSCF_RETURN_TRUE;
    } else {
        LM_DBG("Received negative reply from PCRF for AAR Request\n");
	counter_inc(ims_qos_cnts_h.failed_media_aars);
        //we don't free rx_authdata_p here - it is free-ed when the CDP session expires
        goto error; // if its not a success then that means i want to reject this call!
    }

    //set success response code AVP
    create_return_code(result);
    goto done;

out_of_memory:
    error :
            //set failure response code
            create_return_code(result);

done:
    if (t) tmb.unref_cell(t);
    //free memory
    if (aaa)
        cdpb.AAAFreeMessage(&aaa);

    tmb.t_continue(data->tindex, data->tlabel, data->act);
    free_saved_transaction_global_data(data);
}

void async_aar_reg_callback(int is_timeout, void *param, AAAMessage *aaa, long elapsed_msecs) {
    struct cell *t = 0;
    pcontact_t* pcontact;
    unsigned int cdp_result;
    struct pcontact_info ci;
    udomain_t* domain_t;
    int finalReply = 0;
    AAASession *auth = 0;
    rx_authsessiondata_t* p_session_data = 0;
    int result = CSCF_RETURN_ERROR;

    LM_DBG("Received AAR callback\n");
    saved_transaction_local_t* local_data = (saved_transaction_local_t*) param;
    saved_transaction_t* data = local_data->global_data;
    domain_t = data->domain;

    int is_rereg = local_data->is_rereg;

    //before we do anything else, lets decrement the reference counter on replies
    lock_get(data->lock);
    data->answers_not_received--;
    if (data->answers_not_received <= 0) {
        finalReply = 1;
    }
    if (data->ignore_replies) { //there was obv. a subsequent error AFTER we had sent one/more AAR's - so we can ignore these replies and just free memory
        free_saved_transaction_data(local_data);
        if (finalReply) {
            free_saved_transaction_global_data(data);
        }
        return;
    }
    lock_release(data->lock);

    LM_DBG("received answer and we are waiting for [%d] answers so far failures flag is [%d]\n", data->answers_not_received, data->failed);

    if (tmb.t_lookup_ident(&t, data->tindex, data->tlabel) < 0) {
        LM_ERR("t_continue: transaction not found\n");
        goto error;
    }
    //we have T, lets restore our state (esp. for AVPs)
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to);

    if (is_timeout != 0) {
        LM_ERR("Error timeout when sending AAR message via CDP\n");
        counter_inc(ims_qos_cnts_h.registration_aar_timeouts);
        goto error;
    }
    if (!aaa) {
        LM_ERR("Error sending message via CDP\n");
        goto error;
    }

    counter_inc(ims_qos_cnts_h.registration_aars);
    counter_add(ims_qos_cnts_h.registration_aar_response_time, elapsed_msecs);
    counter_inc(ims_qos_cnts_h.registration_aar_replies_received);

    /* Process the response to AAR, retrieving result code and associated Rx session ID */
    if (rx_process_aaa(aaa, &cdp_result) < 0) {
        LM_ERR("Failed to process AAA from PCRF\n"); //puri.host.len, puri.host.s);
        goto error;
    }

    if (cdp_result >= 2000 && cdp_result < 3000) {
        counter_inc(ims_qos_cnts_h.successful_registration_aars);
	if (is_rereg) {
            LM_DBG("this is a re-registration, therefore we don't need to do anything except know that the the subscription was successful\n");
            result = CSCF_RETURN_TRUE;
            create_return_code(result);
            goto done;
        }
	//need to set Rx auth data to say this session has been successfully opened
	//This is used elsewhere to prevent acting on termination events when the session has not been opened
	//getting auth session
	auth = cdpb.AAAGetAuthSession(aaa->sessionId->data);
	if (!auth) {
	    LM_DBG("Could not get Auth Session for session id: [%.*s]\n", aaa->sessionId->data.len, aaa->sessionId->data.s);
	    goto error;
	}
	//getting session data
	p_session_data = (rx_authsessiondata_t*) auth->u.auth.generic_data;
	if (!p_session_data) {
	    LM_DBG("Could not get session data on Auth Session for session id: [%.*s]\n", aaa->sessionId->data.len, aaa->sessionId->data.s);
	    if (auth) cdpb.AAASessionsUnlock(auth->hash);
	    goto error;
	}
	p_session_data->session_has_been_opened = 1;
	counter_inc(ims_qos_cnts_h.active_registration_rx_sessions);
	
	if (auth) cdpb.AAASessionsUnlock(auth->hash);
	
	
        LM_DBG("Success, received code: [%i] from PCRF for AAR request (contact: [%.*s]), (auth session id: %.*s)\n",
                cdp_result, local_data->contact.len, local_data->contact.s,
                local_data->auth_session_id.len, local_data->auth_session_id.s);
        LM_DBG("Registering for Usrloc callbacks on DELETE\n");

        ul.lock_udomain(domain_t, &local_data->contact, &p_session_data->ip, p_session_data->recv_port);
        if (ul.get_pcontact(domain_t, &local_data->contact, &p_session_data->ip, p_session_data->recv_port, &pcontact) != 0) {
            LM_ERR("Shouldn't get here, can find contact....\n");
            ul.unlock_udomain(domain_t, &local_data->contact, &p_session_data->ip, p_session_data->recv_port);
            goto error;
        }

        //at this point we have the contact
        /*set the contact state to say we have succesfully done ARR for register and that we dont need to do it again
         * for the duration of the registration.
         * */
        if (ul.update_rx_regsession(domain_t, &local_data->auth_session_id, pcontact) != 0) {
            LM_ERR("unable to update pcontact......\n");
            ul.unlock_udomain(domain_t, &local_data->contact, &p_session_data->ip, p_session_data->recv_port);
            goto error;
        }
        memset(&ci, 0, sizeof (struct pcontact_info));
        ci.reg_state = PCONTACT_REG_PENDING_AAR;
        ci.num_service_routes = 0;
        ci.num_public_ids = 0;
        LM_DBG("impu: [%.*s] updating status to PCONTACT_REG_PENDING\n", pcontact->aor.len, pcontact->aor.s);
        ul.update_pcontact(domain_t, &ci, pcontact);
        //register for callbacks on contact
        ul.register_ulcb(pcontact, PCSCF_CONTACT_DELETE | PCSCF_CONTACT_EXPIRE,
                callback_pcscf_contact_cb, NULL);
        ul.unlock_udomain(domain_t, &local_data->contact, &p_session_data->ip, p_session_data->recv_port);
        result = CSCF_RETURN_TRUE;
    } else {
        LM_DBG("Received negative reply from PCRF for AAR Request\n");
	counter_inc(ims_qos_cnts_h.failed_registration_aars);
        result = CSCF_RETURN_FALSE;
        goto error;
    }

    //set success response code AVP
    create_return_code(result);
    goto done;

error:
    //set failure response code
    create_return_code(result);

done:
    if (t) tmb.unref_cell(t);
    //free memory
    if (aaa)
        cdpb.AAAFreeMessage(&aaa);

    if (finalReply) {
        tmb.t_continue(data->tindex, data->tlabel, data->act);
        free_saved_transaction_global_data(data);
    }
    free_saved_transaction_data(local_data);
}

/* handle an AAA response to an AAR for resource reservation for a successful registration or initiated/updated dialog
 * @param aaa - the diameter reply
 * @return -  1 if result code found and processing ok, -1 if error
 */
int rx_process_aaa(AAAMessage *aaa, unsigned int * rc) {
    int ret = 1;

    ret = rx_get_result_code(aaa, rc);

    if (ret == 0) {
        LM_DBG("AAA message without result code\n");
        return ret;
    }

    return ret;
}

/** Helper function for adding media component AVPs - uses previously stored flow descriptions not SDP from messages*/
int add_media_components_using_current_flow_description(AAAMessage* aar, rx_authsessiondata_t *p_session_data) {
    
    flow_description_t *flow_description;
    int add_flow = 1;
    
    flow_description = p_session_data->first_current_flow_description;
    if(!flow_description) {
	return -1;
    }
    while (flow_description) {
	
	if(!authorize_video_flow) {
	    if (strncmp(flow_description->media.s, "video", 5) == 0) {
		add_flow = 0;
	    }
	}
	
	if(add_flow) {
	    rx_add_media_component_description_avp(aar, flow_description->stream_num,
		&flow_description->media, &flow_description->req_sdp_ip_addr,
		&flow_description->req_sdp_port, &flow_description->rpl_sdp_ip_addr,
		&flow_description->rpl_sdp_port, &flow_description->rpl_sdp_transport,
		&flow_description->req_sdp_raw_stream,
		&flow_description->rpl_sdp_raw_stream, flow_description->direction);
	}
	
	flow_description = flow_description->next;
	add_flow = 1;
    }
    return 0;
}



/** Helper function for adding media component AVPs for each SDP stream*/
int add_media_components(AAAMessage* aar, struct sip_msg *req,
        struct sip_msg *rpl, enum dialog_direction direction, AAASession* auth) {
    int sdp_session_num;
    int sdp_stream_num;
    sdp_session_cell_t* req_sdp_session, *rpl_sdp_session;
    sdp_stream_cell_t* req_sdp_stream, *rpl_sdp_stream;
    int add_flow = 1;

    if (!req || !rpl) {
        return CSCF_RETURN_FALSE;
    }

    if (parse_sdp(req) < 0) {
        LM_ERR("Unable to parse req SDP\n");
        return CSCF_RETURN_FALSE;
    }

    if (parse_sdp(rpl) < 0) {
        LM_ERR("Unable to parse res SDP\n");
        return CSCF_RETURN_FALSE;
    }

    sdp_session_num = 0;

    //Loop through req sessions and streams and get corresponding rpl sessions and streams and populate avps
    for (;;) {
        //we only cater for one session at the moment: TDOD: extend
        if (sdp_session_num > 0) {
            break;
        }

        req_sdp_session = get_sdp_session(req, sdp_session_num);
        rpl_sdp_session = get_sdp_session(rpl, sdp_session_num);
        if (!req_sdp_session || !rpl_sdp_session) {
            if (!req_sdp_session)
                LM_ERR("Missing SDP session information from req\n");

            if (!rpl_sdp_session)
                LM_ERR("Missing SDP session information from rpl\n");
            break;
        }

        sdp_stream_num = 0;
        for (;;) {
            req_sdp_stream = get_sdp_stream(req, sdp_session_num,
                    sdp_stream_num);
            rpl_sdp_stream = get_sdp_stream(rpl, sdp_session_num,
                    sdp_stream_num);
            if (!req_sdp_stream || !rpl_sdp_stream) {
                //LM_ERR("Missing SDP stream information\n");
                break;
            }
            //is this a stream to add to AAR.
            if (req_sdp_stream->is_rtp) {

		//check if the src or dst port is 0 and if so then don't add to rx
		int intportA = atoi(req_sdp_stream->port.s);
		int intportB = atoi(rpl_sdp_stream->port.s);
		if(intportA != 0 && intportB != 0){
			if(!authorize_video_flow) {
			    if (strncmp(req_sdp_stream->media.s, "video", 5) == 0) {
				add_flow = 0;
			    }
			}
		    
			if(add_flow) {
			//add this to auth session data
			    add_flow_description((rx_authsessiondata_t*) auth->u.auth.generic_data, sdp_stream_num + 1,
				    &req_sdp_stream->media, &req_sdp_session->ip_addr,
				    &req_sdp_stream->port, &rpl_sdp_session->ip_addr,
				    &rpl_sdp_stream->port, &rpl_sdp_stream->transport,
				    &req_sdp_stream->raw_stream,
				    &rpl_sdp_stream->raw_stream, direction, 0 /*This is a new mcd, we are not setting it as active*/);

			    rx_add_media_component_description_avp(aar, sdp_stream_num + 1,
				    &req_sdp_stream->media, &req_sdp_session->ip_addr,
				    &req_sdp_stream->port, &rpl_sdp_session->ip_addr,
				    &rpl_sdp_stream->port, &rpl_sdp_stream->transport,
				    &req_sdp_stream->raw_stream,
				    &rpl_sdp_stream->raw_stream, direction);	
			}
			add_flow = 1;
		}
            }
            sdp_stream_num++;
        }
        sdp_session_num++;
    }

    free_sdp((sdp_info_t**) (void*) &req->body);
    free_sdp((sdp_info_t**) (void*) &rpl->body);

    return 0;
}

/**
 * Sends the Authorization Authentication Request - specifically this is an asynchronous AAR sent if another update adding video has failed so we need to remove video
 */

int rx_send_aar_update_no_video(AAASession* auth) {

    AAAMessage* aar = 0;
    
    str identifier;
    int identifier_type;


    AAA_AVP* avp = 0;
    char x[4];
    int ret = 0;

    str ip;
    uint16_t ip_version;

    //we get ip and identifier for the auth session data 
    rx_authsessiondata_t* p_session_data = 0;
    p_session_data = (rx_authsessiondata_t*) auth->u.auth.generic_data;
    identifier = p_session_data->identifier;
    identifier_type = p_session_data->identifier_type;
    ip = p_session_data->ip;
    ip_version = p_session_data->ip_version;
    
    aar = cdpb.AAACreateRequest(IMS_Rx, IMS_AAR, Flag_Proxyable, auth);

    LM_DBG("Sending AAR update to remove a video bearer\n");
    show_callsessiondata(p_session_data);

    if (!aar)
        goto error;

    /*Adding AVPs*/

    LM_DBG("Adding auth app id AVP...\n");
    /* Add Auth-Application-Id AVP */
    if (!rx_add_auth_application_id_avp(aar, IMS_Rx))
        goto error;
    if (!rx_add_vendor_specific_application_id_group(aar, IMS_vendor_id_3GPP,
            IMS_Rx))
        goto error;

    LM_DBG("Adding dest realm if not there already...\n");
    /* Add Destination-Realm AVP, if not already there */
    avp = cdpb.AAAFindMatchingAVP(aar, aar->avpList.head, AVP_Destination_Realm,
            0, AAA_FORWARD_SEARCH);
    if (!avp) {
        str realm = rx_dest_realm;
        if (realm.len && !rx_add_destination_realm_avp(aar, realm))
            goto error;
    }

    LM_DBG("Adding AF App identifier...\n");
    /* Add AF-Application-Identifier AVP */
    str af_id = {0, 0};
    af_id = IMS_Serv_AVP_val;
    if (!rx_add_avp(aar, af_id.s, af_id.len, AVP_IMS_AF_Application_Identifier,
            AAA_AVP_FLAG_MANDATORY, IMS_vendor_id_3GPP, AVP_DUPLICATE_DATA,
            __FUNCTION__))
        goto error;

    LM_DBG("Adding service info status...\n");
    /* Add Service-Info-Status AVP, if prelimiary
     * by default(when absent): final status is considered*/
    
    set_4bytes(x,
	    AVP_EPC_Service_Info_Status_Preliminary_Service_Information);
    if (!rx_add_avp(aar, x, 4, AVP_IMS_Service_Info_Status,
	    AAA_AVP_FLAG_MANDATORY, IMS_vendor_id_3GPP, AVP_DUPLICATE_DATA,
	    __FUNCTION__))
	goto error;

    /* Add Auth lifetime AVP */LM_DBG("auth_lifetime %u\n", rx_auth_expiry); //TODO check why this is 0 all the time
    if (rx_auth_expiry) {
        set_4bytes(x, rx_auth_expiry);
        if (!rx_add_avp(aar, x, 4, AVP_Authorization_Lifetime,
                AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__))
            goto error;
    }

    LM_DBG("Adding subscription id...\n");

    rx_add_subscription_id_avp(aar, identifier, identifier_type);


    LM_DBG("Adding reservation priority...\n");
    /* Add Reservation Priority AVP*/
    set_4bytes(x, 0);
    if (!rx_add_avp(aar, x, 4, AVP_ETSI_Reservation_Priority,
            AAA_AVP_FLAG_VENDOR_SPECIFIC, IMS_vendor_id_ETSI,
            AVP_DUPLICATE_DATA, __FUNCTION__))
        goto error;

    LM_DBG("Adding media component...\n");
    //Note we add this AVP first as it gets the IP address which we need to create the auth session
    //Could and maybe should have a separate method that retrieves the IP from SDP - TODO

    /*---------- 2. Create and add Media-Component-Description AVP ----------*/

    /*
     *  See 3GPP TS29214
     *
     *  <Media-Component-Description> = {Media-Component-Number}
     * 								 	[Media-Sub-Component]
     * 								 	[AF-Application-Identifier]
     * 								 	[Media-Type]
     * 								 	[Max-Requested-Bandwidth-UL]
     * 									[Max-Requested-Bandwidth-DL]
     * 									[Flow-Status]
     * 									[Reservation-Priority] (Not used yet)
     * 								 	[RS-Bandwidth]
     * 									[RR-Bandwidth]
     * 									*[Codec-Data]
     */

    add_media_components_using_current_flow_description(aar, p_session_data);

    LM_DBG("Adding framed ip address [%.*s]\n", ip.len, ip.s);
    /* Add Framed IP address AVP*/
    if (!rx_add_framed_ip_avp(&aar->avpList, ip, ip_version)) {
        LM_ERR("Unable to add framed IP AVP\n");
        goto error;
    }
    LM_DBG("Unlocking AAA session...\n");

    if (auth)
        cdpb.AAASessionsUnlock(auth->hash);

    LM_DBG("sending AAR to PCRF\n");
    if (rx_forced_peer.len)
        ret = cdpb.AAASendMessageToPeer(aar, &rx_forced_peer,
            NULL, NULL);
    else
        ret = cdpb.AAASendMessage(aar, NULL,
            NULL);

    return ret;

error:
    LM_ERR("unexpected error\n");
    if (aar)
        cdpb.AAAFreeMessage(&aar);
    if (auth) {
        cdpb.AAASessionsUnlock(auth->hash);
        cdpb.AAADropAuthSession(auth);
        auth = 0;
    }
    return ret;
}



/**
 * Sends the Authorization Authentication Request.
 * @param req - SIP request
 * @param res - SIP response
 * @param direction - 0/o/orig for originating side, 1/t/term for terminating side
 * @param rx_auth_data - the returned rx auth data
 * @returns AAA message or NULL on error
 */

int rx_send_aar(struct sip_msg *req, struct sip_msg *res,
        AAASession* auth, char* direction, saved_transaction_t* saved_t_data) {

    AAAMessage* aar = 0;
    
    str identifier;
    int identifier_type;


    AAA_AVP* avp = 0;
    char x[4];
    int ret = 0;

    str ip;
    uint16_t ip_version;

    //we get ip and identifier for the auth session data 
    rx_authsessiondata_t* p_session_data = 0;
    p_session_data = (rx_authsessiondata_t*) auth->u.auth.generic_data;
    identifier = p_session_data->identifier;
    identifier_type = p_session_data->identifier_type;
    ip = p_session_data->ip;
    ip_version = p_session_data->ip_version;
    
    /* find direction for AAR (orig/term) */
    //need this to add the media component details
    enum dialog_direction dlg_direction = get_dialog_direction(direction);
    if (dlg_direction == DLG_MOBILE_UNKNOWN) {
        LM_DBG("Asked to send AAR for unknown direction.....Aborting...\n");
        goto error;
    }

    aar = cdpb.AAACreateRequest(IMS_Rx, IMS_AAR, Flag_Proxyable, auth);

    LM_DBG("Created aar request...\n");

    if (!aar)
        goto error;

    /*Adding AVPs*/

    LM_DBG("Adding auth app id AVP...\n");
    /* Add Auth-Application-Id AVP */
    if (!rx_add_auth_application_id_avp(aar, IMS_Rx))
        goto error;
    if (!rx_add_vendor_specific_application_id_group(aar, IMS_vendor_id_3GPP,
            IMS_Rx))
        goto error;

    LM_DBG("Adding dest realm if not there already...\n");
    /* Add Destination-Realm AVP, if not already there */
    avp = cdpb.AAAFindMatchingAVP(aar, aar->avpList.head, AVP_Destination_Realm,
            0, AAA_FORWARD_SEARCH);
    if (!avp) {
        str realm = rx_dest_realm;
        if (realm.len && !rx_add_destination_realm_avp(aar, realm))
            goto error;
    }

    LM_DBG("Adding AF App identifier...\n");
    /* Add AF-Application-Identifier AVP */
    str af_id = {0, 0};
    af_id = IMS_Serv_AVP_val;
    if (!rx_add_avp(aar, af_id.s, af_id.len, AVP_IMS_AF_Application_Identifier,
            AAA_AVP_FLAG_MANDATORY, IMS_vendor_id_3GPP, AVP_DUPLICATE_DATA,
            __FUNCTION__))
        goto error;

    LM_DBG("Adding service info status...\n");
    /* Add Service-Info-Status AVP, if prelimiary
     * by default(when absent): final status is considered*/
    if (!res) {
        set_4bytes(x,
                AVP_EPC_Service_Info_Status_Preliminary_Service_Information);
        if (!rx_add_avp(aar, x, 4, AVP_IMS_Service_Info_Status,
                AAA_AVP_FLAG_MANDATORY, IMS_vendor_id_3GPP, AVP_DUPLICATE_DATA,
                __FUNCTION__))
            goto error;
    }

    /* Add Auth lifetime AVP */LM_DBG("auth_lifetime %u\n", rx_auth_expiry); //TODO check why this is 0 all the time
    if (rx_auth_expiry) {
        set_4bytes(x, rx_auth_expiry);
        if (!rx_add_avp(aar, x, 4, AVP_Authorization_Lifetime,
                AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__))
            goto error;
    }

    LM_DBG("Adding subscription id...\n");

    rx_add_subscription_id_avp(aar, identifier, identifier_type);


    LM_DBG("Adding reservation priority...\n");
    /* Add Reservation Priority AVP*/
    set_4bytes(x, 0);
    if (!rx_add_avp(aar, x, 4, AVP_ETSI_Reservation_Priority,
            AAA_AVP_FLAG_VENDOR_SPECIFIC, IMS_vendor_id_ETSI,
            AVP_DUPLICATE_DATA, __FUNCTION__))
        goto error;

    LM_DBG("Adding media component...\n");
    //Note we add this AVP first as it gets the IP address which we need to create the auth session
    //Could and maybe should have a separate method that retrieves the IP from SDP - TODO

    /*---------- 2. Create and add Media-Component-Description AVP ----------*/

    /*
     *  See 3GPP TS29214
     *
     *  <Media-Component-Description> = {Media-Component-Number}
     * 								 	[Media-Sub-Component]
     * 								 	[AF-Application-Identifier]
     * 								 	[Media-Type]
     * 								 	[Max-Requested-Bandwidth-UL]
     * 									[Max-Requested-Bandwidth-DL]
     * 									[Flow-Status]
     * 									[Reservation-Priority] (Not used yet)
     * 								 	[RS-Bandwidth]
     * 									[RR-Bandwidth]
     * 									*[Codec-Data]
     */

    add_media_components(aar, req, res, dlg_direction, auth);

    LM_DBG("Adding framed ip address [%.*s]\n", ip.len, ip.s);
    /* Add Framed IP address AVP*/
    if (!rx_add_framed_ip_avp(&aar->avpList, ip, ip_version)) {
        LM_ERR("Unable to add framed IP AVP\n");
        goto error;
    }
    
    show_callsessiondata(p_session_data);
    
    LM_DBG("Unlocking AAA session...\n");

    if (auth)
        cdpb.AAASessionsUnlock(auth->hash);

    LM_DBG("sending AAR to PCRF\n");
    if (rx_forced_peer.len)
        ret = cdpb.AAASendMessageToPeer(aar, &rx_forced_peer,
            (void*) async_aar_callback, (void*) saved_t_data);
    else
        ret = cdpb.AAASendMessage(aar, (void*) async_aar_callback,
            (void*) saved_t_data);

    return ret;

error:
    LM_ERR("unexpected error\n");
    if (aar)
        cdpb.AAAFreeMessage(&aar);
    if (auth) {
        cdpb.AAASessionsUnlock(auth->hash);
        cdpb.AAADropAuthSession(auth);
        auth = 0;
    }
    return ret;
}

/**
 * Sends the Authorization Authentication Request for Register messages
 * @param req - SIP Register msg
 * @param rx_auth_data - the returned rx auth data
 * @param ip - ip address extracted from contact to register
 * @param ip_version - AF_INET or AF_INET6
 * @returns int >0 if sent AAR successfully, otherwise 0
 */

int rx_send_aar_register(struct sip_msg *msg, AAASession* auth, saved_transaction_local_t* saved_t_data) {
    AAAMessage* aar = 0;
    int ret = 0;
    AAA_AVP* avp = 0;
    char x[4];
    str identifier;
    
    str ip;
    uint16_t ip_version;

    //we get ip and identifier for the auth session data 
    rx_authsessiondata_t* p_session_data = 0;
    p_session_data = (rx_authsessiondata_t*) auth->u.auth.generic_data;
    identifier = p_session_data->identifier;
    ip = p_session_data->ip;
    ip_version = p_session_data->ip_version;

    LM_DBG("Send AAR register\n");

    aar = cdpb.AAACreateRequest(IMS_Rx, IMS_AAR, Flag_Proxyable, auth);

    if (!aar)
        goto error;

    /*Add AVPs*/

    /* Add Auth-Application-Id AVP */
    if (!rx_add_auth_application_id_avp(aar, IMS_Rx))
        goto error;
    if (!rx_add_vendor_specific_application_id_group(aar, IMS_vendor_id_3GPP,
            IMS_Rx))
        goto error;

    /* Add Destination-Realm AVP, if not already there */
    avp = cdpb.AAAFindMatchingAVP(aar, aar->avpList.head, AVP_Destination_Realm,
            0, AAA_FORWARD_SEARCH);
    if (!avp) {
        str realm = rx_dest_realm;
        if (realm.len && !rx_add_destination_realm_avp(aar, realm))
            goto error;
    }

    /* Add Subscription ID AVP*/
    
    identifier = cscf_get_public_identity(msg);
    
    int identifier_type = AVP_Subscription_Id_Type_SIP_URI; //we only do IMPU now
    rx_add_subscription_id_avp(aar, identifier, identifier_type);

    /* Add media component description avp for register*/
    rx_add_media_component_description_avp_register(aar);

    /* Add Framed IP address AVP*/
    if (!rx_add_framed_ip_avp(&aar->avpList, ip, ip_version)) {
        LM_ERR("Unable to add framed IP AVP\n");
        goto error;
    }

    /* Add Auth lifetime AVP */LM_DBG("auth_lifetime %u\n", rx_auth_expiry); //TODO check why this is 0 all the time
    if (rx_auth_expiry) {
        set_4bytes(x, rx_auth_expiry);
        if (!rx_add_avp(aar, x, 4, AVP_Authorization_Lifetime,
                AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__))
            goto error;
    }

    if (auth)
        cdpb.AAASessionsUnlock(auth->hash);

    LM_DBG("sending AAR to PCRF\n");
    if (rx_forced_peer.len)
        ret = cdpb.AAASendMessageToPeer(aar, &rx_forced_peer,
            (void*) async_aar_reg_callback, (void*) saved_t_data);
    else
        ret = cdpb.AAASendMessage(aar, (void*) async_aar_reg_callback,
            (void*) saved_t_data);

    return ret;

error:
    LM_ERR("unexpected error\n");
    if (aar)
        cdpb.AAAFreeMessage(&aar);
    if (auth) {
        cdpb.AAASessionsUnlock(auth->hash);
        cdpb.AAADropAuthSession(auth);
        auth = 0;
    }
    return ret;
}

enum dialog_direction get_dialog_direction(char *direction) {
    if (!direction) {
        LM_CRIT("Unknown direction NULL");
        return DLG_MOBILE_UNKNOWN;
    }
    switch (direction[0]) {
        case 'o':
        case 'O':
        case '0':
            return DLG_MOBILE_ORIGINATING;
        case 't':
        case 'T':
        case '1':
            return DLG_MOBILE_TERMINATING;
        default:
            LM_CRIT("Unknown direction %s", direction);
            return DLG_MOBILE_UNKNOWN;
    }
}

void free_saved_transaction_global_data(saved_transaction_t* data) {
    if (!data)
        return;
    if (data->callid.s && data->callid.len) {
        shm_free(data->callid.s);
        data->callid.len = 0;
    }
    if (data->ftag.s && data->ftag.len) {
        shm_free(data->ftag.s);
        data->ftag.len = 0;
    }
    if (data->ttag.s && data->ttag.len) {
        shm_free(data->ttag.s);
        data->ttag.len = 0;
    }
    if (data->lock) {
        lock_dealloc(data->lock);
        lock_destroy(data->lock);
    }
    shm_free(data);
}

void free_saved_transaction_data(saved_transaction_local_t* data) {
    if (!data)
        return;
    shm_free(data);
}

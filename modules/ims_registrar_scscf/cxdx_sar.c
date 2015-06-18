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

#include "stats.h"
#include "../cdp/cdp_load.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/dialog_ng/dlg_load.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "api.h"
#include "cxdx_avp.h"

#include "reply.h"
#include "rerrno.h"
#include "reg_mod.h"
#include "cxdx_sar.h"
#include "save.h"
#include "userdata_parser.h"
#include "../../lib/kcore/statistics.h"
#include "../../data_lump_rpl.h"
#include "sip_msg.h"
#include "regtime.h"
#include "../../parser/hf.h"
#include "../../lib/ims/ims_getters.h"

extern struct cdp_binds cdpb;

int create_return_code(int result) {
    int rc;
    int_str avp_val, avp_name;
    avp_name.s.s = "saa_return_code";
    avp_name.s.len = 15;

    //build avp spec for saa_return_code
    avp_val.n = result;

    rc = add_avp(AVP_NAME_STR, avp_name, avp_val);

    if (rc < 0)
        LM_ERR("couldnt create AVP\n");
    else
        LM_INFO("created AVP successfully : [%.*s] - [%d]\n", avp_name.s.len, avp_name.s.s, result);

    return 1;
}

void free_saved_transaction_data(saved_transaction_t* data) {
    if (!data)
        return;

    if (data->public_identity.s && data->public_identity.len) {
        shm_free(data->public_identity.s);
        data->public_identity.len = 0;
    }
    free_contact_buf(data->contact_header);
    shm_free(data);
}

void async_cdp_callback(int is_timeout, void *param, AAAMessage *saa, long elapsed_msecs) {
    struct cell *t = 0;
    int rc = -1, experimental_rc = -1;
    int result = CSCF_RETURN_TRUE;
    saved_transaction_t* data = 0;

    str xml_data = {0, 0}, ccf1 = {0, 0}, ccf2 = {0, 0}, ecf1 = {0, 0}, ecf2 = {0, 0};
    ims_subscription* s = 0;
    rerrno = R_FINE;

    if (!param) {
        LM_DBG("No transaction data this must have been called from usrloc cb impu deleted - just log result code and then exit");
        cxdx_get_result_code(saa, &rc);
        cxdx_get_experimental_result_code(saa, &experimental_rc);

        if (saa) cdpb.AAAFreeMessage(&saa);

        if (!rc && !experimental_rc) {
            LM_ERR("bad SAA result code\n");
            return;
        }
        switch (rc) {
            case -1:
                LM_DBG("Received Diameter error\n");
                return;

            case AAA_UNABLE_TO_COMPLY:
                LM_DBG("Unable to comply\n");
                return;

            case AAA_SUCCESS:
                LM_DBG("received AAA success\n");
                return;

            default:
                LM_ERR("Unknown error\n");
                return;
        }

    } else {
        LM_DBG("There is transaction data this must have been called from save or assign server unreg");
        data = (saved_transaction_t*) param;
        if (tmb.t_lookup_ident(&t, data->tindex, data->tlabel) < 0) {
            LM_ERR("t_continue: transaction not found\n");
            rerrno = R_SAR_FAILED;
            goto error_no_send;
        }

	set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from);
	set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to);
	set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from);
	set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to);
	set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from);
	set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to);

        get_act_time();

        if (is_timeout) {
        	update_stat(stat_sar_timeouts, 1);
            LM_ERR("Transaction timeout - did not get SAA\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }
        if (!saa) {
            LM_ERR("Error sending message via CDP\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }

        update_stat(sar_replies_received, 1);
        update_stat(sar_replies_response_time, elapsed_msecs);

        /* check and see that all the required headers are available and can be parsed */
        if (parse_message_for_register(t->uas.request) < 0) {
            LM_ERR("Unable to parse register message correctly\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }

        cxdx_get_result_code(saa, &rc);
        cxdx_get_experimental_result_code(saa, &experimental_rc);
        cxdx_get_charging_info(saa, &ccf1, &ccf2, &ecf1, &ecf2);

        if (!rc && !experimental_rc) {
            LM_ERR("bad SAA result code\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }

        switch (rc) {
            case -1:
                LM_DBG("Received Diameter error\n");
                rerrno = R_SAR_FAILED;
                goto error;

            case AAA_UNABLE_TO_COMPLY:
                LM_DBG("Unable to comply\n");
                rerrno = R_SAR_FAILED;
                goto error;

            case AAA_SUCCESS:
                LM_DBG("received AAA success\n");
                break;

            default:
                LM_ERR("Unknown error\n");
                rerrno = R_SAR_FAILED;
                goto error;
        }
        //success
        //if this is from a save (not a server assign unreg) and expires is zero we don't update usrloc as this is a dereg and usrloc was updated previously
        if (data->sar_assignment_type != AVP_IMS_SAR_UNREGISTERED_USER && data->expires == 0) {
            LM_DBG("no need to update usrloc - already done for de-reg\n");
            result = CSCF_RETURN_TRUE;
            goto success;
        }

        xml_data = cxdx_get_user_data(saa);
        /*If there is XML user data we must be able to parse it*/
        if (xml_data.s && xml_data.len > 0) {
            LM_DBG("Parsing user data string from SAA\n");
            s = parse_user_data(xml_data);
            if (!s) {
                LM_ERR("Unable to parse user data XML string\n");
                rerrno = R_SAR_FAILED;
                goto error;
            }
            LM_DBG("Successfully parse user data XML\n");
        } else {
            if (data->require_user_data) {
                LM_ERR("We require User data for this assignment/register and none was supplied\n");
                rerrno = R_SAR_FAILED;
                result = CSCF_RETURN_FALSE;
                goto done;
            }
        }

        if (data->sar_assignment_type == AVP_IMS_SAR_REGISTRATION || data->sar_assignment_type == AVP_IMS_SAR_RE_REGISTRATION) {
            if (build_p_associated_uri(s) != 0) {
                LM_ERR("Unable to build p_associated_uri\n");
                rerrno = R_SAR_FAILED;
                goto error;
            }
        }

        //here we update the contacts and also build the new contact header for the 200 OK reply
        if (update_contacts_new(t->uas.request, data->domain, &data->public_identity, data->sar_assignment_type, &s, &ccf1, &ccf2, &ecf1, &ecf2, &data->contact_header) <= 0) {
            LM_ERR("Error processing REGISTER\n");
            rerrno = R_SAR_FAILED;
            goto error;
        }
        
        if (data->contact_header) {
            LM_DBG("Updated contacts: %.*s\n", data->contact_header->data_len, data->contact_header->buf);
        } else {
            LM_DBG("Updated unreg contact\n");
        }
        
    }

success:
    update_stat(accepted_registrations, 1);

done:
    if (data->sar_assignment_type != AVP_IMS_SAR_UNREGISTERED_USER)
        reg_send_reply_transactional(t->uas.request, data->contact_header, t);
    LM_DBG("DBG:SAR Async CDP callback: ... Done resuming transaction\n");

    create_return_code(result);

    //free memory
    if (saa) cdpb.AAAFreeMessage(&saa);
    if (t) {
        del_nonshm_lump_rpl(&t->uas.request->reply_lump);
        tmb.unref_cell(t);
    }
    //free path vector pkg memory
    reset_path_vector(t->uas.request);

    tmb.t_continue(data->tindex, data->tlabel, data->act);
    free_saved_transaction_data(data);
    return;

error:
    if (data->sar_assignment_type != AVP_IMS_SAR_UNREGISTERED_USER)
        reg_send_reply_transactional(t->uas.request, data->contact_header, t);
		
    create_return_code(-2);

error_no_send: //if we don't have the transaction then we can't send a transaction response
    update_stat(rejected_registrations, 1);
    //free memory
    if (saa) cdpb.AAAFreeMessage(&saa);
    if (t) {
        del_nonshm_lump_rpl(&t->uas.request->reply_lump);
        tmb.unref_cell(t);
    }
    tmb.t_continue(data->tindex, data->tlabel, data->act);
    free_saved_transaction_data(data);
    return;
}

/**
 * Create and send a Server-Assignment-Request and returns the Answer received for it.
 * This function performs the Server Assignment operation.
 * @param msg - the SIP message to send for
 * @parma public_identity - the public identity of the user
 * @param server_name - local name of the S-CSCF to save on the HSS
 * @param assignment_type - type of the assignment
 * @param data_available - if the data is already available
 * @returns the SAA
 */
int cxdx_send_sar(struct sip_msg *msg, str public_identity, str private_identity,
        str server_name, int assignment_type, int data_available, saved_transaction_t* transaction_data) {
    AAAMessage *sar = 0;
    AAASession *session = 0;
    unsigned int hash = 0, label = 0;
    struct hdr_field *hdr;

    session = cdpb.AAACreateSession(0);

    sar = cdpb.AAACreateRequest(IMS_Cx, IMS_SAR, Flag_Proxyable, session);
    if (session) {
        cdpb.AAADropSession(session);
        session = 0;
    }
    if (!sar) goto error1;

    if (!cxdx_add_call_id(sar, cscf_get_call_id(msg, &hdr)));
    if (!cxdx_add_destination_realm(sar, cxdx_dest_realm)) goto error1;

    if (!cxdx_add_vendor_specific_appid(sar, IMS_vendor_id_3GPP, IMS_Cx, 0 /*IMS_Cx*/)) goto error1;
    if (!cxdx_add_auth_session_state(sar, 1)) goto error1;

    if (!cxdx_add_public_identity(sar, public_identity)) goto error1;
    if (!cxdx_add_server_name(sar, server_name)) goto error1;
    if (private_identity.len)
        if (!cxdx_add_user_name(sar, private_identity)) goto error1;
    if (!cxdx_add_server_assignment_type(sar, assignment_type)) goto error1;
    if (!cxdx_add_userdata_available(sar, data_available)) goto error1;

    if (msg && tmb.t_get_trans_ident(msg, &hash, &label) < 0) {
        // it's ok cause we can call this async with a message for ul callbacks!
        LM_DBG("SIP message without transaction... must be a ul callback\n");
        //return 0;
    }

    if (cxdx_forced_peer.len)
        cdpb.AAASendMessageToPeer(sar, &cxdx_forced_peer, (void*) async_cdp_callback, (void*) transaction_data);
    else
        cdpb.AAASendMessage(sar, (void*) async_cdp_callback, (void*) transaction_data);

    return 0;

error1: //Only free SAR IFF it has not been passed to CDP
    if (sar) cdpb.AAAFreeMessage(&sar);

    return -1;


}

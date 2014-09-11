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
#include "cxdx_avp.h"
#include "cxdx_uar.h"
#include "registration.h"
#include "mod.h"


//we use pseudo variables to communicate back to config file this takes the result and converys to a return code, publishes it a pseudo variable
int create_uaa_return_code(int result) {
    int rc;
    int_str avp_val, avp_name;
    avp_name.s.s = "uaa_return_code";
    avp_name.s.len = 15;

    //build avp spec for uaa_return_code
    avp_val.n = result;

    rc = add_avp(AVP_NAME_STR, avp_name, avp_val);

    if (rc < 0)
        LM_ERR("couldnt create AVP\n");
    else
        LM_INFO("created AVP successfully : [%.*s]\n", avp_name.s.len, avp_name.s.s);

    return 1;
}

void free_saved_uar_transaction_data(saved_uar_transaction_t* data) {
    if (!data)
        return;

    if (data->callid.s && data->callid.len) {
        shm_free(data->callid.s);
        data->callid.len = 0;
    }
    shm_free(data);
}

void async_cdp_uar_callback(int is_timeout, void *param, AAAMessage *uaa, long elapsed_msecs) {
    struct run_act_ctx ra_ctx;
    str server_name;
    int *m_capab = 0, m_capab_cnt = 0;
    int *o_capab = 0, o_capab_cnt = 0;
    str *p_server_names = 0;
    int p_server_names_cnt = 0;
    int rc = -1, experimental_rc = -1;
    saved_uar_transaction_t* data = (saved_uar_transaction_t*) param;
    struct cell *t = 0;
    int result = CSCF_RETURN_TRUE;
    scscf_entry *list = 0;

    if (tmb.t_lookup_ident(&t, data->tindex, data->tlabel) < 0) {
        LM_ERR("ERROR: t_continue: transaction not found\n");
        //result = CSCF_RETURN_ERROR;//not needed we set by default to error!
        goto error;
    }

    if (is_timeout != 0) {
        LM_ERR("Error timeout when  sending message via CDP\n");
        update_stat(stat_uar_timeouts, 1);
        goto error;
    }

    //update stats on response time
    update_stat(uar_replies_received, 1);
    update_stat(uar_replies_response_time, elapsed_msecs);

    if (!uaa) {
        LM_ERR("Error sending message via CDP\n");
        //result = CSCF_RETURN_ERROR;//not needed we set by default to error!
        goto error;
    }

    server_name = cxdx_get_server_name(uaa);
    if (!server_name.len) {
        cxdx_get_capabilities(uaa, &m_capab, &m_capab_cnt, &o_capab,
                &o_capab_cnt, &p_server_names, &p_server_names_cnt);
    }

    cxdx_get_result_code(uaa, &rc);
    cxdx_get_experimental_result_code(uaa, &experimental_rc);

    if (rc && !experimental_rc) {
        LM_ERR("No result code or experimental result code - responding 480\n");
        cscf_reply_transactional_async(t, t->uas.request, 480, MSG_480_DIAMETER_MISSING_AVP);
        result = CSCF_RETURN_FALSE;
        goto done;
    }

    switch (rc) {
        case -1:
            switch (experimental_rc) {
                case RC_IMS_DIAMETER_ERROR_USER_UNKNOWN:
                    /* Check, if route is set: */
                    if (route_uar_user_unknown_no >= 0) {
                        /* exec routing script */
                        init_run_actions_ctx(&ra_ctx);
                        if (run_actions(&ra_ctx, main_rt.rlist[route_uar_user_unknown_no], t->uas.request) < 0) {
                            LM_DBG("ims_icscf: error while trying script\n");
                        }
                    } else {
                        cscf_reply_transactional_async(t, t->uas.request, 403, MSG_403_USER_UNKNOWN);
                    }
                    LM_ERR("RC_IMS_DIAMETER_ERROR_USER_UNKNOWN\n");
                    result = CSCF_RETURN_FALSE;
                    goto done;
                case RC_IMS_DIAMETER_ERROR_IDENTITIES_DONT_MATCH:
                    LM_ERR("RC_IMS_DIAMETER_ERROR_IDENTITIES_DONT_MATCH returning 403\n");
                    cscf_reply_transactional_async(t, t->uas.request, 403, MSG_403_IDENTITIES_DONT_MATCH);
                    result = CSCF_RETURN_FALSE;
                    goto done;
                case RC_IMS_DIAMETER_ERROR_ROAMING_NOT_ALLOWED:
                    LM_ERR("RC_IMS_DIAMETER_ERROR_ROAMING_NOT_ALLOWED returning 403\n");
                    cscf_reply_transactional_async(t, t->uas.request, 403, MSG_403_ROAMING_NOT_ALLOWED);
                    result = CSCF_RETURN_FALSE;
                    goto done;
                case RC_IMS_DIAMETER_ERROR_IDENTITY_NOT_REGISTERED:
                    LM_ERR("RC_IMS_DIAMETER_ERROR_IDENTITY_NOT_REGISTERED returning 403\n");
                    cscf_reply_transactional_async(t, t->uas.request, 403, MSG_403_IDENTITY_NOT_REGISTERED);
                    result = CSCF_RETURN_FALSE;
                    goto done;
                case RC_IMS_DIAMETER_FIRST_REGISTRATION:
                    goto success;
                case RC_IMS_DIAMETER_SUBSEQUENT_REGISTRATION:
                    goto success;
                case RC_IMS_DIAMETER_SERVER_SELECTION:
                    goto success;

                default:
                    LM_ERR("MSG_403_UNKOWN_EXPERIMENTAL_RC returning 403\n");
                    cscf_reply_transactional_async(t, t->uas.request, 403, MSG_403_UNKOWN_EXPERIMENTAL_RC);
                    result = CSCF_RETURN_FALSE;
                    goto done;
            }
            break;

        case AAA_AUTHORIZATION_REJECTED:
            LM_ERR("AAA_AUTHORIZATION_REJECTED returning 403\n");
            cscf_reply_transactional_async(t, t->uas.request, 403, MSG_403_AUTHORIZATION_REJECTED);
            result = CSCF_RETURN_FALSE;
            goto done;
        case AAA_UNABLE_TO_COMPLY:
            LM_ERR("AAA_UNABLE_TO_COMPLY returning 403\n");
            cscf_reply_transactional_async(t, t->uas.request, 403, MSG_403_UNABLE_TO_COMPLY);
            result = CSCF_RETURN_FALSE;
            goto done;

        case AAA_SUCCESS:
            goto success;

        default:
            LM_ERR("MSG_403_UNKOWN_RC returning 403\n");
            cscf_reply_transactional_async(t, t->uas.request, 403, MSG_403_UNKOWN_RC);
            result = CSCF_RETURN_FALSE;
            goto done;
    }

success:
    LM_DBG("successful UAA response\n");
    list = I_get_capab_ordered(server_name, m_capab, m_capab_cnt, o_capab, o_capab_cnt, p_server_names, p_server_names_cnt, 0);

    if (!list) {
        LM_ERR("Empty capability list returning 600\n");
        cscf_reply_transactional_async(t, t->uas.request, 600, MSG_600_EMPTY_LIST);
        result = CSCF_RETURN_FALSE;
        goto done;
    }

    if (!data->callid.len || !add_scscf_list(data->callid, list)) {
        LM_ERR("Error saving capability list 500\n");
        cscf_reply_transactional_async(t, t->uas.request, 500, MSG_500_ERROR_SAVING_LIST);
        result = CSCF_RETURN_FALSE;
        goto done;
    }

    result = CSCF_RETURN_TRUE;

done:
	//free capabilities if they exist
	if (m_capab) shm_free(m_capab);
	if (o_capab) shm_free(o_capab);
	if (p_server_names) shm_free(p_server_names);

    LM_DBG("DBG:UAR Async CDP callback: ... Done resuming transaction\n");
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to);
    set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from);
    set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to);

    create_uaa_return_code(result);

    if (t)tmb.unref_cell(t);
    //free memory
    if (uaa) cdpb.AAAFreeMessage(&uaa);

    tmb.t_continue(data->tindex, data->tlabel, data->act);
    free_saved_uar_transaction_data(data);
    return;

error:
    if (t)tmb.unref_cell(t);
    //free memory
    if (uaa) cdpb.AAAFreeMessage(&uaa);

    tmb.t_continue(data->tindex, data->tlabel, data->act);
    free_saved_uar_transaction_data(data);


}

/**
 * Sends an UAR and returns the parsed UAA struct.
 * @param msg - the SIP message
 * @param private_identity - the username
 * @param public_identity - the public identity
 * @param visited_network_id - id of the roaming network
 * @param authorization_type - if registration or de-registration
 * @returns the status of sending the async UAR - 0 for success, !0 for failure
 */
int cxdx_send_uar(struct sip_msg *msg, str private_identity, str public_identity, str visited_network_id,
        int authorization_type, int sos_reg, saved_uar_transaction_t* transaction_data) {
    AAAMessage *uar = 0;
    AAASession *session = 0;

    session = cdpb.AAACreateSession(0);

    uar = cdpb.AAACreateRequest(IMS_Cx, IMS_UAR, Flag_Proxyable, session);
    if (session) {
        cdpb.AAADropSession(session);
        session = 0;
    }
    if (!uar) goto error1;

    if (!cxdx_add_destination_realm(uar, cxdx_dest_realm)) goto error1;

    if (!cxdx_add_vendor_specific_appid(uar, IMS_vendor_id_3GPP, IMS_Cx, 0)) goto error1;
    if (!cxdx_add_auth_session_state(uar, 1)) goto error1;

    if (!cxdx_add_user_name(uar, private_identity)) goto error1;
    if (!cxdx_add_public_identity(uar, public_identity)) goto error1;
    if (!cxdx_add_visited_network_id(uar, visited_network_id)) goto error1;
    if (!cxdx_add_UAR_flags(uar, sos_reg)) goto error1;
    if (authorization_type != AVP_IMS_UAR_REGISTRATION)
        if (!cxdx_add_authorization_type(uar, authorization_type)) goto error1;

    if (cxdx_forced_peer.len)
        cdpb.AAASendMessageToPeer(uar, &cxdx_forced_peer, (void*) async_cdp_uar_callback, (void*) transaction_data);
    else
        cdpb.AAASendMessage(uar, (void*) async_cdp_uar_callback, (void*) transaction_data);

    LM_DBG("Successfully sent async diameter\n");

    return 0;

error1:
    //Only free UAR IFF it has not been passed to CDP
    if (uar) cdpb.AAAFreeMessage(&uar);
    LM_ERR("Error occurred trying to send UAR\n");
    return -1;
}



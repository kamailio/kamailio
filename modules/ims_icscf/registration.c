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


#include "registration.h"
#include "../../action.h" /* run_actions */
#include "../../mod_fix.h"
#include "cxdx_uar.h"

extern int route_uar_user_unknown_no;

/**
 * Perform User Authorization Request.
 * creates and send the user authorization query
 * @param msg - the SIP message
 * @returns true if OK, false if not
 */
int I_perform_user_authorization_request(struct sip_msg* msg, char* route, char* str1, char* str2) {
    str private_identity, public_identity, visited_network_id;
    int authorization_type = AVP_IMS_UAR_REGISTRATION;
    int expires = 3600;
    struct hdr_field *hdr;
    str realm={0,0};
    contact_t *c;
    int sos_reg = 0;
    contact_body_t *b = 0;
    str call_id;
    saved_uar_transaction_t* saved_t;
    tm_cell_t *t = 0;
    int intvalue_param;
    cfg_action_t* cfg_action;
    
    str route_name;

    if (fixup_get_ivalue(msg, (gparam_t*) str1, &intvalue_param) != 0) {
        LM_ERR("no int value param passed\n");
        return CSCF_RETURN_ERROR;
    }
    if (fixup_get_svalue(msg, (gparam_t*) route, &route_name) != 0) {
        LM_ERR("no async route block for assign_server_unreg\n");
        return -1;
    }
    
    LM_DBG("Looking for route block [%.*s]\n", route_name.len, route_name.s);
    int ri = route_get(&main_rt, route_name.s);
    if (ri < 0) {
        LM_ERR("unable to find route block [%.*s]\n", route_name.len, route_name.s);
        return -1;
    }
    cfg_action = main_rt.rlist[ri];
    if (cfg_action == NULL) {
        LM_ERR("empty action lists in route block [%.*s]\n", route_name.len, route_name.s);
        return -1;
    }

	/*This should be configurable and not hardwired to RURI domain*/
    //realm = cscf_get_realm_from_ruri(msg);

    //check if we received what we should, we do this even though it should be done in cfg file - double checking!
    if (msg->first_line.type != SIP_REQUEST) {
        LM_ERR("ERR:I_UAR: The message is not a request\n");
        return CSCF_RETURN_ERROR;
    }
    if (msg->first_line.u.request.method.len != 8 ||
            memcmp(msg->first_line.u.request.method.s, "REGISTER", 8) != 0) {
        LM_ERR("ERR:I_UAR: The method is not a REGISTER\n");
        return CSCF_RETURN_ERROR;
    }

    private_identity = cscf_get_private_identity(msg, realm);
    if (!private_identity.len) {
        LM_ERR("ERR:I_UAR: Private Identity not found, responding with 400\n");
        cscf_reply_transactional(msg, 400, MSG_400_NO_PRIVATE);
        return CSCF_RETURN_BREAK;
    }

    public_identity = cscf_get_public_identity(msg);
    if (!public_identity.len) {
        LM_ERR("ERR:I_UAR: Public Identity not found, responding with 400\n");
        cscf_reply_transactional(msg, 400, MSG_400_NO_PUBLIC);
        return CSCF_RETURN_BREAK;

    }

    b = cscf_parse_contacts(msg);

    if (!b || (!b->contacts && !b->star)) {
        LM_DBG("DBG:I_UAR: No contacts found - just fetching bindings\n");
    } else {
        for (c = b->contacts; c; c = c->next) {
            sos_reg = cscf_get_sos_uri_param(c->uri);
            if (sos_reg == -1) {
                //error case
                LM_ERR("ERR:I_UAR: MSG_400_MALFORMED_CONTACT, responding with 400\n");
                cscf_reply_transactional(msg, 400, MSG_400_MALFORMED_CONTACT);
                return CSCF_RETURN_BREAK;
            } else if (sos_reg == -2) {
                LM_ERR("ERR:I_UAR: MSG_500_SERVER_ERROR_OUT_OF_MEMORY, responding with 500\n");
                cscf_reply_transactional(msg, 500, MSG_500_SERVER_ERROR_OUT_OF_MEMORY);
                return CSCF_RETURN_BREAK;
            }
        }
    }

    visited_network_id = cscf_get_visited_network_id(msg, &hdr);
    if (!visited_network_id.len) {
        LM_ERR("ERR:I_UAR: Visited Network Identity not found, responding with 400\n");
        cscf_reply_transactional(msg, 400, MSG_400_NO_VISITED);
        return CSCF_RETURN_BREAK;
    }

    if (atoi(str1)) authorization_type = AVP_IMS_UAR_REGISTRATION_AND_CAPABILITIES;
    else {
        expires = cscf_get_max_expires(msg, 0);
        if (expires == 0) authorization_type = AVP_IMS_UAR_DE_REGISTRATION;
    }

    LM_DBG("SENDING UAR: PI: [%.*s], PU: [%.*s], VNID: [%.*s]\n", private_identity.len, private_identity.s,
            public_identity.len, public_identity.s,
            visited_network_id.len, visited_network_id.s);

    //before we send lets suspend the transaction
    t = tmb.t_gett();
    if (t == NULL || t == T_UNDEFINED) {
        if (tmb.t_newtran(msg) < 0) {
            LM_ERR("cannot create the transaction for UAR async\n");
            cscf_reply_transactional(msg, 480, MSG_480_DIAMETER_ERROR);
            return CSCF_RETURN_BREAK;
        }
        t = tmb.t_gett();
        if (t == NULL || t == T_UNDEFINED) {
            LM_ERR("cannot lookup the transaction\n");
            cscf_reply_transactional(msg, 480, MSG_480_DIAMETER_ERROR);
            return CSCF_RETURN_BREAK;
        }
    }

    saved_t = shm_malloc(sizeof (saved_uar_transaction_t));
    if (!saved_t) {
        LM_ERR("no more memory trying to save transaction state\n");
        return CSCF_RETURN_ERROR;

    }
    memset(saved_t, 0, sizeof (saved_uar_transaction_t));
    saved_t->act = cfg_action;

    call_id = cscf_get_call_id(msg, 0);
    saved_t->callid.s = (char*) shm_malloc(call_id.len + 1);
    if (!saved_t->callid.s) {
    	LM_ERR("no more memory trying to save transaction state : callid\n");
    	shm_free(saved_t);
    	return CSCF_RETURN_ERROR;
    }
    memset(saved_t->callid.s, 0, call_id.len + 1);
    memcpy(saved_t->callid.s, call_id.s, call_id.len);
    saved_t->callid.len = call_id.len;

    LM_DBG("Setting default AVP return code used for async callbacks to default as ERROR \n");
    create_uaa_return_code(CSCF_RETURN_ERROR);
    
    LM_DBG("Suspending SIP TM transaction\n");
    if (tmb.t_suspend(msg, &saved_t->tindex, &saved_t->tlabel) < 0) {
        LM_ERR("failed to suspend the TM processing\n");
        free_saved_uar_transaction_data(saved_t);

        cscf_reply_transactional(msg, 480, MSG_480_DIAMETER_ERROR);
        return CSCF_RETURN_BREAK;
    }

    if (cxdx_send_uar(msg, private_identity, public_identity, visited_network_id, authorization_type, sos_reg, saved_t) != 0) {
        LM_ERR("ERR:I_UAR: Error sending UAR or UAR time-out\n");
        tmb.t_cancel_suspend(saved_t->tindex, saved_t->tlabel);
        free_saved_uar_transaction_data(saved_t);
        cscf_reply_transactional(msg, 480, MSG_480_DIAMETER_ERROR);
        return CSCF_RETURN_BREAK;

    }
    //we use async replies therefore we send break and not true when successful
    return CSCF_RETURN_BREAK;
}




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

#include "location.h"
#include "../../action.h" /* run_actions */
#include "../../mod_fix.h"
#include "cxdx_lir.h"

extern int route_lir_user_unknown_no; 

/**
 * Perform an LIR
 * @param msg - sip message
 * @returns 1 on success or 0 on failure
 */
int I_perform_location_information_request(struct sip_msg* msg, char* route, char* str1, char* str2) {
    str public_identity = {0, 0};
    int orig = 0;
    
    tm_cell_t *t = 0;
    saved_lir_transaction_t* saved_t;
    
    str route_name;
    
     cfg_action_t* cfg_action;

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
    
    LM_DBG("DBG:I_LIR: Starting ...\n");
    /* check if we received what we should */
    if (msg->first_line.type != SIP_REQUEST) {
        LM_ERR("ERR:I_LIR: The message is not a request\n");
        return CSCF_RETURN_BREAK;
    }

    /* check orig uri parameter in topmost Route */
    if (cscf_has_originating(msg, str1, str2) == CSCF_RETURN_TRUE) {
        orig = 1;
        LM_DBG("DBG:I_LIR: orig\n");
    }

    /* extract data from message */
    if (orig) {
        public_identity = cscf_get_asserted_identity(msg, 0);
    } else {
        public_identity = cscf_get_public_identity_from_requri(msg);
    }
    if (!public_identity.len) {
        LM_ERR("ERR:I_LIR: Public Identity not found, responding with 400\n");
        if (orig)
            cscf_reply_transactional(msg, 400, MSG_400_NO_PUBLIC_FROM);
        else
            cscf_reply_transactional(msg, 400, MSG_400_NO_PUBLIC);
        return CSCF_RETURN_BREAK;
    }
    
    
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

    saved_t = shm_malloc(sizeof (saved_lir_transaction_t));
    if (!saved_t) {
        LM_ERR("no more memory trying to save transaction state\n");
        return CSCF_RETURN_ERROR;

    }
    memset(saved_t, 0, sizeof (saved_lir_transaction_t));
    saved_t->act = cfg_action;
    
    saved_t->orig = orig;
    
    LM_DBG("Setting default AVP return code used for async callbacks to default as ERROR \n");
    create_lia_return_code(CSCF_RETURN_ERROR);
    
    LM_DBG("Suspending SIP TM transaction\n");
    if (tmb.t_suspend(msg, &saved_t->tindex, &saved_t->tlabel) < 0) {
        LM_ERR("failed to suspend the TM processing\n");
        free_saved_lir_transaction_data(saved_t);

        cscf_reply_transactional(msg, 480, MSG_480_DIAMETER_ERROR);
        return CSCF_RETURN_BREAK;
    }
    
    if (cxdx_send_lir(msg, public_identity, saved_t) != 0) {
        LM_ERR("ERR:I_LIR: Error sending LIR or LIR time-out\n");
        tmb.t_cancel_suspend(saved_t->tindex, saved_t->tlabel);
        free_saved_lir_transaction_data(saved_t);
        cscf_reply_transactional(msg, 480, MSG_480_DIAMETER_ERROR);
        
        if (public_identity.s && !orig)
        shm_free(public_identity.s); // shm_malloc in cscf_get_public_identity_from_requri		
        return CSCF_RETURN_BREAK;

    }
    if (public_identity.s && !orig)
        shm_free(public_identity.s); // shm_malloc in cscf_get_public_identity_from_requri		
    
    //we use async replies therefore we send break and not true when successful
    return CSCF_RETURN_BREAK;
}

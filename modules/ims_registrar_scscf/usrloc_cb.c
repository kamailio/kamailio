/*
 * $Id$
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
#include "registrar_notify.h"
#include "usrloc_cb.h"

extern str scscf_name_str;

void ul_impu_inserted(impurecord_t* r, ucontact_t* c, int type, void* param) {

    LM_DBG("Received notification of UL IMPU insert for IMPU <%.*s>", r->public_identity.len, r->public_identity.s);

    LM_DBG("Registering for callbacks on this IMPU for contact insert, update, delete or expire to send notifications if there are any subscriptions");
    ul.register_ulcb(r, 0, UL_IMPU_NEW_CONTACT, ul_contact_changed, 0); //this allows us to receive cbs on new contact for IMPU
    ul.register_ulcb(r, 0, UL_IMPU_UPDATE_CONTACT | UL_IMPU_EXPIRE_CONTACT | UL_IMPU_DELETE_CONTACT, ul_contact_changed, 0);

    LM_DBG("Selectively asking for expire or no contact delete callbacks only on the anchor of the implicit set so that we only send one SAR per implicit set");
    if (r->is_primary) {
        //TODO only do this if a flag in the IMPU record identifies this as the implicit set anchor
        if (ul.register_ulcb(r, 0, UL_IMPU_REG_NC_DELETE | UL_IMPU_UNREG_EXPIRED, ul_impu_removed, 0) < 0) {
            LM_ERR("can not register callback for no contacts delete or IMPI expire\n");
            return;
        }
    }
}

void ul_impu_removed(impurecord_t* r, ucontact_t* c, int type, void* param) {
    int assignment_type = AVP_IMS_SAR_USER_DEREGISTRATION;
    int data_available = AVP_IMS_SAR_USER_DATA_NOT_AVAILABLE;

    //we only send SAR if the REGISTRATION state is (NOT) IMPU_NOT_REGISTERED and if send_sar_on_delete is set
    //send_sar_on_delete is set by default - only unset if impu is deleted due to explicit dereg
    LM_DBG("Received notification of UL IMPU removed for IMPU <%.*s>", r->public_identity.len, r->public_identity.s);

    if (r->reg_state != IMPU_NOT_REGISTERED && r->send_sar_on_delete) {
        LM_DBG("Sending SAR to DeRegister [%.*s] (pvt: <%.*s>)\n",
                r->public_identity.len, r->public_identity.s,
                r->s->private_identity.len, r->s->private_identity.s);
        LM_DBG("Sending SAR\n");
        cxdx_send_sar(NULL, r->public_identity, r->s->private_identity, scscf_name_str, assignment_type, data_available, 0);
    }
}

void ul_contact_changed(impurecord_t* r, ucontact_t* c, int type, void* param) {

    LM_DBG("Received notification of type %d on contact Address <%.*s>", type, c->c.len, c->c.s);
    
    if(!r->shead){
        LM_DBG("There are no subscriptions for this IMPU therefore breaking out now as nothing to do");
        return;
    }
    
    if (type == UL_IMPU_DELETE_CONTACT) {
        LM_DBG("Received notification of UL CONTACT DELETE");
        event_reg(0, r, c, IMS_REGISTRAR_CONTACT_UNREGISTERED, 0, 0);
    } else if (type == UL_IMPU_EXPIRE_CONTACT) {
        LM_DBG("Received notification of UL CONTACT EXPIRE");
        event_reg(0, r, c, IMS_REGISTRAR_CONTACT_EXPIRED, 0, 0);
    } else if (type == UL_IMPU_UPDATE_CONTACT) {
        LM_DBG("Received notification of UL CONTACT UPDATE");
        event_reg(0, r, c, IMS_REGISTRAR_CONTACT_REFRESHED, 0, 0);
    } else if (type == UL_IMPU_NEW_CONTACT) {
        LM_DBG("Received notification of UL IMPU CONTACT INSERT");
        event_reg(0, r, c, IMS_REGISTRAR_CONTACT_REGISTERED, 0, 0);
    } else {
        LM_DBG("This type of callback not supported here");
    }
}

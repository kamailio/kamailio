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



#include "../cdp_avp/mod_export.h"
#include "../../modules/dialog_ng/dlg_load.h"
#include "../ims_usrloc_pcscf/usrloc.h"
#include "rx_authdata.h"
#include "mod.h"
#include "rx_avp.h"
#include "rx_str.h"

#include "../../lib/ims/ims_getters.h"

extern str IMS_Serv_AVP_val;

int rx_send_str(str *rx_session_id) {

    
    LM_DBG("Sending STR\n");

    AAASession *auth = 0;
    AAAMessage *str = NULL;

    AAA_AVP * avp = NULL;

    if (!rx_session_id || !rx_session_id->s || !rx_session_id->len) {
        LM_ERR("Dialog has no Rx session associated\n");
        return CSCF_RETURN_FALSE;
    }else
    {
        LM_DBG("Rx session id exists\n");
    }

    
    LM_DBG("About to try get Auth session\n");
    auth = cdpb.AAAGetAuthSession(*rx_session_id);
    if (!auth) {
        LM_DBG("Could not get Auth Session for session id: [%.*s] - this is fine as this might have been started by already sending an STR\n", rx_session_id->len, rx_session_id->s);
        return CSCF_RETURN_FALSE;
    }else{
        LM_DBG("Retrieved Auth Session for session id: [%.*s]\n", rx_session_id->len, rx_session_id->s);
    }
    
    LM_DBG("Got auth session\n");
    
    LM_DBG("dest host [%.*s]\n", auth->dest_host.len, auth->dest_host.s);
    LM_DBG("dest realm [%.*s]\n", auth->dest_realm.len, auth->dest_realm.s);
    LM_DBG("id [%.*s]\n", auth->id.len, auth->id.s);
    
    //create STR

    char x[4];

    LM_DBG("Checking auth state\n");

    if (auth->u.auth.state == AUTH_ST_DISCON) {
        // If we are in DISCON is because an STR was already sent
        // so just wait for STA or for Grace Timout to happen
        LM_DBG("Hmmm, auth session already in disconnected state\n");
        goto error;
    }

    LM_DBG("Creating STR\n");
    str = cdpb.AAACreateRequest(IMS_Rx, IMS_STR, Flag_Proxyable, auth);

    if (!str) {
        LM_ERR("Unable to create STR request\n");
        goto error;
    }

    LM_DBG("Adding Auth app id\n");
    /* Add Auth-Application-Id AVP */
    if (!rx_add_auth_application_id_avp(str, IMS_Rx)) goto error;
    if (!rx_add_vendor_specific_application_id_group(str, IMS_vendor_id_3GPP, IMS_Rx)) goto error;


    LM_DBG("Adding destination realm\n");
    /* Add Destination-Realm AVP, if not already there */
    avp = cdpb.AAAFindMatchingAVP(str, str->avpList.head, AVP_Destination_Realm, 0, AAA_FORWARD_SEARCH);
    if (!avp) {
        if (rx_dest_realm.len && !rx_add_destination_realm_avp(str, rx_dest_realm)) goto error;
    }

    LM_DBG("Adding AF app id\n");
    /* Add AF-Application-Identifier AVP */
    if (!rx_add_avp(str, IMS_Serv_AVP_val.s, IMS_Serv_AVP_val.len,
            AVP_IMS_AF_Application_Identifier,
            AAA_AVP_FLAG_MANDATORY, IMS_vendor_id_3GPP,
            AVP_DUPLICATE_DATA, __FUNCTION__)) goto error;


    LM_DBG("Adding Termination cause\n");
    /* Termination-Cause */
    set_4bytes(x, 1);
    if (!rx_add_avp(str, x, 4, AVP_Termination_Cause,
            AAA_AVP_FLAG_MANDATORY, 0,
            AVP_DUPLICATE_DATA, __FUNCTION__)) goto error;

    
    LM_DBG("Unlocking AAA session...\n");
    cdpb.AAASessionsUnlock(auth->hash);
    
    LM_DBG("sending STR to PCRF\n");
    if (rx_forced_peer.len)
        cdpb.AAASendMessageToPeer(str, &rx_forced_peer , NULL, NULL);
    else
        cdpb.AAASendMessage(str, NULL, NULL);

    LM_DBG("Successfully sent Rx STR for session: [%.*s]\n", rx_session_id->len, rx_session_id->s);

    return CSCF_RETURN_TRUE;

error:
    LM_DBG("Error sending Rx STR for session: [%.*s]\n", rx_session_id->len, rx_session_id->s);
    if (str) cdpb.AAAFreeMessage(&str);
    if (auth) {
        cdpb.AAASessionsUnlock(auth->hash);
        cdpb.AAADropAuthSession(auth);
        auth = 0;
    }

    return CSCF_RETURN_FALSE;
}

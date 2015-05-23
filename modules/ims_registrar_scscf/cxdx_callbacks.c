/**
 * Callback functions for RTR/PPR from the HSS
 *
 * Copyright (c) 2012 Carsten Bock, ng-voice GmbH
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
 */

#include "cxdx_callbacks.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "cxdx_avp.h"


extern struct cdp_binds cdpb;
extern usrloc_api_t ul;

extern char *domain;

/*int PPR_RTR_Event(void *parsed_message, int type, void *param) {
	if (type & CXDX_PPR_RECEIVED) {
		LM_ERR("Received a PPR-Request\n");
		return 1;
	}
	if (type & CXDX_RTR_RECEIVED) {
		LM_ERR("Received a RTR-Request\n");
		return 1;
	}
	return 0;
}*/

AAAMessage* cxdx_process_rtr(AAAMessage *rtr) {
    LM_DBG("Processing RTR");
    
    AAAMessage *rta_msg;
    AAA_AVP* avp;
    str public_id;
    impurecord_t* r;
    int i = 0;
    int res = 0;
    udomain_t* udomain;
    
    rta_msg = cdpb.AAACreateResponse(rtr);//session ID?
    if (!rta_msg) return 0;

    avp = cxdx_get_next_public_identity(rtr,0,AVP_IMS_Public_Identity,IMS_vendor_id_3GPP,__FUNCTION__);	
    if(avp==0){
	    LM_WARN("RTR received with only IMPI (username AVP) - currently S-CSCF does not support this kind of RTR\n");
	    return 0;
	    //TODO add support for receiving RTR with IMPI
	    //get all impus related to this impu
	    //get all contacts related to each impu
	    //set the contact expire for each contact to now
    }else{
	    public_id=avp->data;
	    LM_DBG("RTR received with IMPU [%.*s] in public identity AVP - this is supported\n", public_id.len, public_id.s);

	    //TODO this should be a configurable module param
	    if (ul.register_udomain(domain, &udomain) < 0) {
		LM_ERR("Unable to register usrloc domain....aborting\n");
		return 0;
	    }
	    
	    ul.lock_udomain(udomain, &public_id);
            res = ul.get_impurecord(udomain, &public_id, &r);
            if (res != 0) {
                LM_WARN("Strange, '%.*s' Not found in usrloc\n", public_id.len, public_id.s);
                ul.unlock_udomain(udomain, &public_id);
                //no point in continuing
                return 0;
            }
	    
	    for(i = 0; i < r->num_contacts; i++) {
		LM_DBG("Expiring contact with AOR [%.*s]\n", r->newcontacts[i]->aor.len, r->newcontacts[i]->aor.s);
		ul.expire_ucontact(r, r->newcontacts[i]);
	    }
	    
	    ul.unlock_udomain(udomain, &public_id);
	    
	    while(cdpb.AAAGetNextAVP(avp) && (avp=cxdx_get_next_public_identity(rtr,cdpb.AAAGetNextAVP(avp),AVP_IMS_Public_Identity,IMS_vendor_id_3GPP,__FUNCTION__))!=0){
		    public_id=avp->data;
		    LM_DBG("RTR also has public id [%.*s]\n", public_id.len, public_id.s);
		    ul.lock_udomain(udomain, &public_id);
		    res = ul.get_impurecord(udomain, &public_id, &r);
		    if (res != 0) {
			LM_WARN("Strange, '%.*s' Not found in usrloc\n", public_id.len, public_id.s);
			ul.unlock_udomain(udomain, &public_id);
			//no point in continuing
			return 0;
		    }

		    for(i = 0; i < r->num_contacts; i++) {
			LM_DBG("Expiring contact with AOR [%.*s]\n", r->newcontacts[i]->aor.len, r->newcontacts[i]->aor.s);
			ul.expire_ucontact(r, r->newcontacts[i]);
		    }

		    ul.unlock_udomain(udomain, &public_id);
		}		
    }
    cxdx_add_vendor_specific_appid(rta_msg,IMS_vendor_id_3GPP,IMS_Cx,0 /*IMS_Cx*/);
    
    cxdx_add_auth_session_state(rta_msg,1);		

    /* send an RTA back to the HSS */
    cxdx_add_result_code(rta_msg,DIAMETER_SUCCESS);
    
    return rta_msg;
    
    
}

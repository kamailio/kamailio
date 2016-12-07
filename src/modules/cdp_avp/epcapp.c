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
 * FhG Focus. Thanks for great work! This is an effort to 
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

#include "avp_new.h"
#include "avp_new_base_data_format.h"
#include "avp_add.h"
#include "avp_get.h"
#include "avp_get_base_data_format.h"

extern struct cdp_binds *cdp;


#include "../cdp/cdp_load.h"


#include "epcapp.h"
#include "ccapp.h"


#define CDP_AVP_DEFINITION

	#include "epcapp.h"
	int cdp_avp_add_GG_Enforce_Group(AAA_AVP_LIST * avpList, 
		int32_t type, str id, 
		ip_address ue_ip, ip_address gg_ip, 
		uint32_t interval,
		AVPDataStatus status){

		AAA_AVP_LIST        avp_list = {0,0}, avp_list2 = {0,0};

		if(!cdp_avp_add_UE_Locator(&avp_list, ue_ip))
			goto error;

		if(id.len && id.s){
			if(!cdp_avp_add_Subscription_Id_Group(&avp_list,
					type,
					id,
					AVP_DUPLICATE_DATA))
				goto error;
		}

		if(!cdp_avp_add_UE_Locator_Id_Group(&avp_list2, 
				&avp_list, AVP_FREE_DATA))
			goto error;

		if(!cdp_avp_add_GG_IP(&avp_list2, gg_ip))
			goto error;
	
		if(!cdp_avp_add_GG_Enforce(avpList, &avp_list2,AVP_FREE_DATA)){
			LOG(L_ERR, "could not find the GG_Enforce AVP\n");
			goto error;
		}
		return 1;
	error:
		LOG(L_ERR, "error while adding the GG change AVPs\n");
		return 0;
	}

#undef CDP_AVP_DEFINITION





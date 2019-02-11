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

#include "avp_get.h"

extern struct cdp_binds *cdp;

/**
 * Iteratively find an AVP matching in a list
 *  Should be called subsequently with the old result, unless it was null (which would cause a restart of the search).
 * @param msg
 * @param avp_code
 * @param avp_vendor_id
 * @param start_avp - avp where to resume. A null value will trigger a restart of the search.
 * @return the AAA_AVP* or NULL if not found (anymore)
 */
inline AAA_AVP* cdp_avp_get_next_from_list(AAA_AVP_LIST list,int avp_code,int avp_vendor_id,AAA_AVP *start_avp)
{
	AAA_AVP *avp;
	if (!start_avp)	start_avp = list.head;
	else start_avp = start_avp->next;
	LOG(L_DBG,"Looking for AVP with code %d vendor id %d startin at avp %p\n",
			avp_code,avp_vendor_id,start_avp);
	
	if (!start_avp){
		LOG(L_DBG,"Failed finding AVP with Code %d and VendorId %d - Empty list or at end of list\n",avp_code,avp_vendor_id);
		return 0;
	}
	avp = cdp->AAAFindMatchingAVPList(list,start_avp,avp_code,avp_vendor_id,AAA_FORWARD_SEARCH);
	if (avp==0){
		LOG(L_DBG,"Failed finding AVP with Code %d and VendorId %d - at end of list\n",avp_code,avp_vendor_id);
		return 0;
	}

	return avp;
}

/**
 * Iteratively find an AVP matching in a message
 * @param msg
 * @param avp_code
 * @param avp_vendor_id
 * @param start_avp
 * @return the AAA_AVP* or NULL if not found (anymore)
 */
inline AAA_AVP* cdp_avp_get_next_from_msg(AAAMessage *msg,int avp_code,int avp_vendor_id,AAA_AVP *start_avp)
{
	return cdp_avp_get_next_from_list(msg->avpList,avp_code,avp_vendor_id,start_avp);
}

/**
 * Find the first matching AVP in a list and return it 
 * @param list
 * @param avp_code
 * @param avp_vendor_id
 * @return the AAA_AVP* or null if not found
 */
inline AAA_AVP* cdp_avp_get_from_list(AAA_AVP_LIST list,int avp_code,int avp_vendor_id)
{
	return cdp_avp_get_next_from_list(list,avp_code,avp_vendor_id,0);
}

/**
 * Find the first AVP matching in the message and return it
 * @param msg
 * @param avp_code
 * @param avp_vendor_id
 * @return the AAA_AVP* or null if not found
 */
inline AAA_AVP* cdp_avp_get_from_msg(AAAMessage *msg,int avp_code,int avp_vendor_id)
{
	if (!msg){
		LOG(L_ERR,"Failed finding AVP with Code %d and VendorId %d in NULL message!\n",avp_code,avp_vendor_id);
		return 0;
	}
	return cdp_avp_get_from_list(msg->avpList,avp_code,avp_vendor_id);
}

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

#ifndef __AVP_ADD_H
#define __AVP_ADD_H

#ifndef CDP_AVP_REFERENCE

	#include "../cdp/cdp_load.h"

	int cdp_avp_add_new_to_list(AAA_AVP_LIST *list,int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	typedef int (*cdp_avp_add_new_to_list_f)(AAA_AVP_LIST *list,int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);

	int cdp_avp_add_new_to_msg(AAAMessage *msg,int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	typedef int (*cdp_avp_add_new_to_msg_f)(AAAMessage *msg,int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);

	int cdp_avp_add_to_list(AAA_AVP_LIST *list,AAA_AVP *avp);
	typedef int (*cdp_avp_add_to_list_f)(AAA_AVP_LIST *list,AAA_AVP *avp);

	int cdp_avp_add_to_msg(AAAMessage *msg,AAA_AVP *avp);
	typedef int (*cdp_avp_add_to_msg_f)(AAAMessage *msg,AAA_AVP *avp);

#else
	
	int basic.add_new_to_list(AAA_AVP_LIST *list,int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);

	int basic.add_new_to_msg(AAAMessage *msg,int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);

	int basic.add_to_list(AAA_AVP_LIST *list,AAA_AVP *avp);

	int basic.add_to_msg(AAAMessage *msg,AAA_AVP *avp);
	
#endif


#endif /* __AVP_NEW_H */

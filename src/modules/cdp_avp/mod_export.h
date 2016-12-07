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

#ifndef __CDP_AVP_EXPORT_H
#define __CDP_AVP_EXPORT_H

#ifndef M_NAME
	#define M_NAME "cdp_avp"
#endif

#include "../cdp/cdp_load.h"


#include "avp_new.h"
#include "avp_new_base_data_format.h"

#include "avp_add.h"

#include "avp_get.h"
#include "avp_get_base_data_format.h"

#include "base.h"
#include "nasapp.h"
#include "ccapp.h"
#include "imsapp.h"
#include "epcapp.h"

#include "../../sr_module.h"



typedef struct {
	
	cdp_avp_new_f 					new;
	
	cdp_avp_add_new_to_list_f		add_new_to_list;
	cdp_avp_add_new_to_msg_f 		add_new_to_msg;
	cdp_avp_add_to_list_f			add_to_list;
	cdp_avp_add_to_msg_f			add_to_msg;
	
	cdp_avp_get_next_from_list_f	get_next_from_list;
	cdp_avp_get_next_from_msg_f		get_next_from_msg;
	cdp_avp_get_from_list_f			get_from_list;
	cdp_avp_get_from_msg_f			get_from_msg;

} cdp_avp_bind_basic_t ;

typedef struct {

	cdp_avp_new_OctetString_f 		new_OctetString;
	cdp_avp_new_Integer32_f			new_Integer32;
	cdp_avp_new_Integer64_f			new_Integer64;
	cdp_avp_new_Unsigned32_f		new_Unsigned32;
	cdp_avp_new_Unsigned64_f		new_Unsigned64;
	cdp_avp_new_Float32_f			new_Float32;
	cdp_avp_new_Float64_f			new_Float64;
	cdp_avp_new_Grouped_f			new_Grouped;
	
	cdp_avp_new_Address_f			new_Address;
	cdp_avp_new_Time_f				new_Time;
	cdp_avp_new_UTF8String_f		new_UTF8String;
	cdp_avp_new_DiameterIdentity_f	new_DiameterIdentity;
	cdp_avp_new_DiameterURI_f		new_DiameterURI;
	cdp_avp_new_Enumerated_f		new_Enumerated;
	cdp_avp_new_IPFilterRule_f		new_IPFilterRule;
	cdp_avp_new_QoSFilterRule_f		new_QoSFilterRule;
	
	
	cdp_avp_get_OctetString_f		get_OctetString;
	cdp_avp_get_Integer32_f			get_Integer32;
	cdp_avp_get_Integer64_f			get_Integer64;
	cdp_avp_get_Unsigned32_f		get_Unsigned32;
	cdp_avp_get_Unsigned64_f		get_Unsigned64;
	cdp_avp_get_Float32_f			get_Float32;
	cdp_avp_get_Float64_f			get_Float64;
	cdp_avp_get_Grouped_f			get_Grouped;
	cdp_avp_free_Grouped_f			free_Grouped;
	
	cdp_avp_get_Address_f			get_Address;
	cdp_avp_get_Time_f				get_Time;
	cdp_avp_get_UTF8String_f		get_UTF8String;
	cdp_avp_get_DiameterIdentity_f	get_DiameterIndentity;
	cdp_avp_get_DiameterURI_f		get_DiameterURI;
	cdp_avp_get_Enumerated_f		get_Enumerated;	
	cdp_avp_get_IPFilterRule_f		get_IPFilterRule;
	cdp_avp_get_QoSFilterRule_f		get_QoSFilterRule;
	
} cdp_avp_bind_base_data_format_t;


typedef struct {
	
	#define CDP_AVP_EXPORT			
		#include "base.h"		
	#undef	CDP_AVP_EXPORT
	
} cdp_avp_bind_base_avp_t;

typedef struct {
		
	#define CDP_AVP_EXPORT			
		#include "ccapp.h"		
	#undef	CDP_AVP_EXPORT

} cdp_avp_bind_ccapp_avp_t;


typedef struct {

	
	#define CDP_AVP_EXPORT			
		#include "nasapp.h"		
	#undef	CDP_AVP_EXPORT
	
} cdp_avp_bind_nasapp_avp_t;

typedef struct {
	
	#define CDP_AVP_EXPORT			
		#include "imsapp.h"		
	#undef	CDP_AVP_EXPORT

} cdp_avp_bind_imsapp_avp_t;

typedef struct {
		
	#define CDP_AVP_EXPORT			
		#include "epcapp.h"		
	#undef	CDP_AVP_EXPORT
	
} cdp_avp_bind_epcapp_avp_t;

typedef struct {
	struct cdp_binds				*cdp;
	cdp_avp_bind_basic_t 			basic;
	cdp_avp_bind_base_data_format_t data;
	cdp_avp_bind_base_avp_t			base;
	cdp_avp_bind_ccapp_avp_t		ccapp;
	cdp_avp_bind_nasapp_avp_t 		nasapp;
	cdp_avp_bind_imsapp_avp_t 		imsapp;
	cdp_avp_bind_epcapp_avp_t 		epcapp;
} cdp_avp_bind_t;


typedef cdp_avp_bind_t* (*cdp_avp_get_bind_f)(void);

static inline cdp_avp_bind_t * load_cdp_avp()
{
        cdp_avp_get_bind_f load_cdp_avp;
	
	/* import the TM auto-loading function */
	load_cdp_avp = (cdp_avp_get_bind_f)find_export("cdp_avp_get_bind", NO_SCRIPT, 0);
	
	if (load_cdp_avp == NULL) {
		LOG(L_WARN, "Cannot import load_cdp function from CDP module\n");
		return 0;
	}
	
	return (cdp_avp_bind_t *)load_cdp_avp();
}


#endif /* __CDP_AVP_EXPORT_H */

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

/*
 * TS 29.229 - Cx
 *
 * http://www.3gpp.org/ftp/Specs/html-info/29229.htm
 * 
 * + ETSI, CableLabs Cx additions
 *
 *
 * TS 29.329 - Sh
 *
 * http://www.3gpp.org/ftp/Specs/html-info/29329.htm
 *
 */

#include "macros.h"


#undef CDP_AVP_MODULE
#define CDP_AVP_MODULE imsapp

#if !defined(CDP_AVP_DECLARATION) && !defined(CDP_AVP_EXPORT) && !defined(CDP_AVP_INIT) && !defined(CDP_AVP_REFERENCE)
	#ifndef _CDP_AVP_IMSAPP_H_1
	#define _CDP_AVP_IMSAPP_H_1

		#include "../cdp/cdp_load.h"

	#else

		/* undo the macros definition if this was re-included */
		#define CDP_AVP_EMPTY_MACROS
			#include "macros.h"
		#undef CDP_AVP_EMPTY_MACROS

	#endif
#endif //_CDP_AVP_NASAPP_H_1	

/*
 * The list of AVPs must be declared in the following format:
 * 
 * 		cdp_avp_add(<avp_name>.<vendor_id>,<flags>,<avp_type>,<data_type>)
 * 		or
 * 		cdp_avp_add_ptr(<avp_name>.<vendor_id>,<flags>,<avp_type>,<data_type>)
 * 
 * 		cdp_avp_get(<avp_name>.<vendor_id>,<avp_type>,<data_type>)
 * 
 * or, to add both add and get at once:
 * 
 * 		cdp_avp(<avp_name>.<vendor_id>,<flags>,<avp_type>,<data_type>)
 * 		or 
 * 		cdp_avp_ptr(<avp_name>.<vendor_id>,<flags>,<avp_type>,<data_type>)
 * 
 * The add macros ending in _ptr will generate function with the extra AVPDataStatus data_do parameter
 * 
 * Parameters:
 *  - avp_name - a value of AVP_<avp_name> must resolve to the AVP code
 *  - vendor_id - an int value
 *  - flags	- AVP Flags to add to the AVP
 *  - avp_type - an avp type for which a function was defined a
 * 				int cdp_avp_get_<avp_type>(AAA_AVP *avp,<data_type> *data)
 * 		Some valid suggestions (and the data_type):		
 *  
 *  			OctetString 	- str
 *  			Integer32		- int32_t
 *  			Integer64 		- int64_t
 *  			Unsigned32 		- uint32_t
 *  			Unsigned64 		- uint64_t
 *  			Float32 		- float
 *  			Float64 		- double
 *  			Grouped 		- AAA_AVP_LIST
 *  
 *  			Address 		- ip_address
 *  			Time 			- time_t
 *  			UTF8String 		- str
 *  			DiameterIdentity- str
 *  			DiameterURI		- str
 *  			Enumerated		- int32_t
 *  			IPFilterRule	- str
 *  			QoSFilterRule	- str
 *  - data_type - the respective data type for the avp_type defined above
 *  
 *  The functions generated will return 1 on success or 0 on error or not found
 *  The prototype of the function will be:
 *  
 *  	int cdp_avp_get_<avp_name_group>(AAA_AVP_LIST list,<data_type> *data,AAA_AVP **avp_ptr)
 * 
 * 
 *  
 *  For Grouped AVPs with 2 or 3 known inside AVPs, you can define a shortcut function which will find the group and
 *  also extract the 2 or 3 AVPs. 
 *  Do not define both 2 and 3 for the same type!
 * 
 * 
 *		cdp_avp_add2(<avp_name_group>.<vendor_id_group>,<flags_group>,<avp_name_1>,<data_type_1>,<avp_name_2>,<data_type_2>)
 * 		cdp_avp_get2(<avp_name_group>.<vendor_id_group>,<avp_name_1>,<data_type_1>,<avp_name_2>,<data_type_2>)
 *  	
 *		cdp_avp_get3(<avp_name_group>.<vendor_id_group>,<flags_group>,<avp_name_1>,<data_type_1>,<avp_name_2>,<data_type_2>,<avp_name_3>,<data_type_3>)
 *  	cdp_avp_get3(<avp_name_group>.<vendor_id_group>,<avp_name_1>,<data_type_1>,<avp_name_2>,<data_type_2>,<avp_name_3>,<data_type_3>)
 * 
 * 	 or, to add both add and get at once:
 * 
 *		cdp_avp2(<avp_name_group>.<vendor_id_group>,<flags_group>,<avp_name_1>,<data_type_1>,<avp_name_2>,<data_type_2>)
 * 		cdp_avp3(<avp_name_group>.<vendor_id_group>,<flags_group>,<avp_name_1>,<data_type_1>,<avp_name_2>,<data_type_2>)
 *  
 *  avp_name_group - a value of AVP_<avp_name_group> must resolve to the AVP code of the group
 *  
 *  vendor_id_group - an int value
 *  
 *  avp_name_N	- the name of the Nth parameter. 
 *  	Previously, a cdp_avp_get(<avp_name_N>,<vendor_id_N>,<avp_type_N>,<data_type_N>) must be defined!
 *  
 *  data_type_N	- the respective data type for avp_type_N (same as <data_type_N) 
 *  
 *  The functions generated will return the number of found AVPs inside on success or 0 on error or not found
 *  The prototype of the function will be:
 *  
 *  	int cdp_avp_get_<avp_name_group>_Group(AAA_AVP_LIST list,<data_type_1> *avp_name_1,<data_type_2> *avp_name_2[,<data_type_3> *avp_name_3],AAA_AVP **avp_ptr)
 *  
 *  Note - generally, all data of type str will need to be defined with ..._ptr
 *  Note - Groups must be defined with:
 *  	 cdp_avp_add_ptr(...) and data_type AAA_AVP_LIST*
 *  	 cdp_avp_get(...) and data_type AAA_AVP_LIST 	
 */
#undef CDP_AVP_NAME
#define CDP_AVP_NAME(avp_name) AVP_IMS_##avp_name

cdp_avp_ptr		(Visited_Network_Identifier,	IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp_ptr		(Public_Identity,				IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		UTF8String,		str)	

cdp_avp_ptr		(Server_Name,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		UTF8String,		str)	

cdp_avp_add_ptr	(Server_Capabilities,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(Server_Capabilities,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST)	

cdp_avp			(Mandatory_Capability,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Unsigned32,		uint32_t)	

cdp_avp			(Optional_Capability,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Unsigned32,		uint32_t)	

cdp_avp_ptr		(User_Data_Cx,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp			(SIP_Number_Auth_Items,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Unsigned32,		uint32_t)	

cdp_avp_ptr		(SIP_Authentication_Scheme,		IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		UTF8String,		str)	

cdp_avp_ptr		(SIP_Authenticate,				IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp_ptr		(SIP_Authorization,				IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp_ptr		(SIP_Authentication_Context,	IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp_add_ptr	(SIP_Auth_Data_Item,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(SIP_Auth_Data_Item,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST)	

cdp_avp			(SIP_Item_Number,				IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Unsigned32,		uint32_t)

cdp_avp			(Server_Assignment_Type,		IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp_add_ptr	(Deregistration_Reason,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST*)	
cdp_avp_get 	(Deregistration_Reason,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST)	

cdp_avp			(Reason_Code,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp_ptr		(Reason_Info,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		UTF8String,		str)	

cdp_avp_add_ptr	(Charging_Information,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(Charging_Information,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST)	

cdp_avp_ptr		(Primary_Event_Charging_Function_Name,IMS_vendor_id_3GPP,AAA_AVP_FLAG_MANDATORY,DiameterURI,	str)	

cdp_avp_ptr		(Secondary_Event_Charging_Function_Name,IMS_vendor_id_3GPP,AAA_AVP_FLAG_MANDATORY,DiameterURI,	str)	

cdp_avp_ptr		(Primary_Charging_Collection_Function_Name,IMS_vendor_id_3GPP,AAA_AVP_FLAG_MANDATORY,DiameterURI,str)	

cdp_avp_ptr		(Secondary_Charging_Collection_Function_Name,IMS_vendor_id_3GPP,AAA_AVP_FLAG_MANDATORY,DiameterURI,	str)	

cdp_avp			(User_Authorization_Type,		IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp			(User_Data_Already_Available,	IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp_ptr		(Confidentiality_Key,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp_ptr		(Integrity_Key,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp_add_ptr	(Supported_Features,			IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST*)	
cdp_avp_add3	(Supported_Features,			IMS_vendor_id_3GPP,	0,	Vendor_Id,	uint32_t,	Feature_List_ID,	uint32_t, Feature_List, uint32_t)	
cdp_avp_get		(Supported_Features,			IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST)	
cdp_avp_get3	(Supported_Features,			IMS_vendor_id_3GPP,	0,	Vendor_Id,	uint32_t,	Feature_List_ID,	uint32_t, Feature_List, uint32_t)	

cdp_avp			(Feature_List_ID,				IMS_vendor_id_3GPP,	0,							Unsigned32,		uint32_t)

cdp_avp			(Feature_List,					IMS_vendor_id_3GPP,	0,							Unsigned32,		uint32_t)

cdp_avp_add_ptr	(Supported_Applications,		IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(Supported_Applications,		IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST)	

cdp_avp_add_ptr	(Associated_Identities,			IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(Associated_Identities,			IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST)	

cdp_avp			(Originating_Request,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp_ptr		(Wildcarded_PSI,				IMS_vendor_id_3GPP,	0,							UTF8String,		str)	

cdp_avp_add_ptr	(SIP_Digest_Authenticate,		IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(SIP_Digest_Authenticate,		IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST)	

cdp_avp_ptr		(Wildcarded_IMPU,				IMS_vendor_id_3GPP,	0,							UTF8String,		str)	

cdp_avp			(UAR_Flags,						IMS_vendor_id_3GPP,	0,							Unsigned32,		uint32_t)

cdp_avp			(Loose_Route_Indication,		IMS_vendor_id_3GPP,	0,							Enumerated,		int32_t)	

cdp_avp_add_ptr	(SCSCF_Restoration_Info,		IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(SCSCF_Restoration_Info,		IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST)	

cdp_avp_ptr		(Path,							IMS_vendor_id_3GPP,	0,							OctetString,	str)	

cdp_avp_ptr		(Contact,						IMS_vendor_id_3GPP,	0,							OctetString,	str)	

cdp_avp_add_ptr	(Subscription_Info,				IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(Subscription_Info,				IMS_vendor_id_3GPP,	0,							Grouped,		AAA_AVP_LIST)	

cdp_avp_ptr		(From_SIP_Header,				IMS_vendor_id_3GPP,	0,							OctetString,	str)	

cdp_avp_ptr		(To_SIP_Header,					IMS_vendor_id_3GPP,	0,							OctetString,	str)	

cdp_avp_ptr		(Record_Route,					IMS_vendor_id_3GPP,	0,							OctetString,	str)	

cdp_avp_add_ptr	(Associated_Registered_Identities,IMS_vendor_id_3GPP,0,							Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(Associated_Registered_Identities,IMS_vendor_id_3GPP,0,							Grouped,		AAA_AVP_LIST)	

cdp_avp			(Multiple_Registration_Indication,IMS_vendor_id_3GPP,0,							Enumerated,		int32_t)	

cdp_avp_add_ptr	(Restoration_Info,				IMS_vendor_id_3GPP,0,							Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(Restoration_Info,				IMS_vendor_id_3GPP,0,							Grouped,		AAA_AVP_LIST)	

cdp_avp_ptr		(Access_Network_Information,            IMS_vendor_id_3GPP,	0,		UTF8String,		str)        


/*
 * ETSI something, that probably does not exist anymore
 * 
 */
#undef CDP_AVP_NAME
#define CDP_AVP_NAME(avp_name) AVP_##avp_name

cdp_avp_ptr		(ETSI_Line_Identifier,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_add_ptr	(ETSI_SIP_Authenticate,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(ETSI_SIP_Authenticate,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST)	

cdp_avp_add_ptr	(ETSI_SIP_Authorization,		IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(ETSI_SIP_Authorization,		IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST)	

cdp_avp_add_ptr	(ETSI_SIP_Authentication_Info,	IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(ETSI_SIP_Authentication_Info,	IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST)	

cdp_avp_ptr		(ETSI_Digest_Realm,				IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Nonce,				IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Domain,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Opaque,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Stale,				IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Algorithm,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_QoP,				IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_HA1,				IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Auth_Param,		IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Username,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_URI,				IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Response,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_CNonce,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Nonce_Count,		IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Method,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Entity_Body_Hash,	IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Nextnonce,			IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)

cdp_avp_ptr		(ETSI_Digest_Response_Auth,		IMS_vendor_id_ETSI,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)


/*
 * CableLabs 29.229
 * 
 */
#undef CDP_AVP_NAME
#define CDP_AVP_NAME(avp_name) AVP_##avp_name

cdp_avp_add_ptr	(CableLabs_SIP_Digest_Authenticate,IMS_vendor_id_CableLabs,	AAA_AVP_FLAG_MANDATORY,	Grouped,	AAA_AVP_LIST*)	
cdp_avp_get		(CableLabs_SIP_Digest_Authenticate,IMS_vendor_id_CableLabs,	AAA_AVP_FLAG_MANDATORY,	Grouped,	AAA_AVP_LIST)	

cdp_avp_ptr		(CableLabs_Digest_Realm,		IMS_vendor_id_CableLabs,	AAA_AVP_FLAG_MANDATORY,	OctetString,str)

cdp_avp_ptr		(CableLabs_Digest_Domain,		IMS_vendor_id_CableLabs,	AAA_AVP_FLAG_MANDATORY,	OctetString,str)

cdp_avp_ptr		(CableLabs_Digest_Algorithm,	IMS_vendor_id_CableLabs,	AAA_AVP_FLAG_MANDATORY,	OctetString,str)

cdp_avp_ptr		(CableLabs_Digest_QoP,			IMS_vendor_id_CableLabs,	AAA_AVP_FLAG_MANDATORY,	OctetString,str)

cdp_avp_ptr		(CableLabs_Digest_HA1,			IMS_vendor_id_CableLabs,	AAA_AVP_FLAG_MANDATORY,	OctetString,str)

cdp_avp_ptr		(CableLabs_Digest_Auth_Param,	IMS_vendor_id_CableLabs,	AAA_AVP_FLAG_MANDATORY,	OctetString,str)

/*
 * TS 29.329
 *
 */
#undef CDP_AVP_NAME
#define CDP_AVP_NAME(avp_name) AVP_IMS_##avp_name

cdp_avp_add_ptr	(User_Identity,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST*)	
cdp_avp_get		(User_Identity,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Grouped,		AAA_AVP_LIST)	

cdp_avp_ptr		(MSISDN,						IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp_ptr		(User_Data_Sh,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp			(Data_Reference,				IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp_ptr		(Service_Indication,			IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	

cdp_avp			(Subs_Req_Type,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp			(Requested_Domain,				IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp			(Current_Location,				IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp			(Identity_Set,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Enumerated,		int32_t)	

cdp_avp			(Expiry_Time,					IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		Time,			time_t)	

cdp_avp			(Send_Data_Indication,			IMS_vendor_id_3GPP,	0,							Enumerated,		int32_t)	

cdp_avp_ptr		(DSAI_Tag,						IMS_vendor_id_3GPP,	AAA_AVP_FLAG_MANDATORY,		OctetString,	str)	




#undef CDP_AVP_NAME
#define CDP_AVP_NAME(avp_name) AVP_##avp_name





/*
 * From here-on you can define/export/init/declare functions which can not be generate with the macros
 */

#if defined(CDP_AVP_DEFINITION)

	/*
	 * Put here your supplementary definitions. Typically:
	 * 
	 * int <function1>(param1)
	 * {
	 *   code1
	 * }
	 * 
	 * 
	 */


#elif defined(CDP_AVP_EXPORT)

	/*
	 * Put here your supplimentary exports in the format: 
	 * 	<function_type1> <nice_function_name1>; 
	 *  <function_type2> <nice_function_name1>;
	 *  ...
	 *  
	 */


#elif defined(CDP_AVP_INIT)

	/*
	 * Put here your supplementary inits in the format: 
	 * 	<function1>,
	 *  <function2>,
	 *  ...
	 * 
	 * Make sure you keep the same order as in export!
	 * 
	 */


#elif defined(CDP_AVP_REFERENCE)
	/*
	 * Put here what you want to get in the reference. Typically:
	 * <function1>
	 * <function2>
	 * ... 
	 * 
	 */

	
#elif defined(CDP_AVP_EMPTY_MACROS)
	
	/* this should be left blank */
	
#else

	/*
	 * Put here your definitions according to the declarations, exports, init, etc above. Typically:
	 * 
	 * int <function1(params1);>
	 * typedef int <*function_type1>(params1);
	 * 
	 * int <function2(param2);>
	 * typedef int <*function_type2>(params2);
	 * 
	 * ...
	 *  
	 */

	
	#ifndef _CDP_AVP_IMSAPP_H_2
	#define _CDP_AVP_IMSAPP_H_2




	#endif //_CDP_AVP_IMSAPP_H_2
	
#endif



#define CDP_AVP_UNDEF_MACROS
	#include "macros.h"
#undef CDP_AVP_UNDEF_MACROS
	




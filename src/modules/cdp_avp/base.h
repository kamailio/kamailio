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
 * RFC 3588 Base AVPs
 * 
 * http://tools.ietf.org/html/rfc3588
 * 
 */

#include "macros.h"

#undef CDP_AVP_MODULE
#define CDP_AVP_MODULE base

#if !defined(CDP_AVP_DECLARATION) && !defined(CDP_AVP_EXPORT) && !defined(CDP_AVP_INIT) && !defined(CDP_AVP_REFERENCE)
	#ifndef _CDP_AVP_BASE_H_1
	#define _CDP_AVP_BASE_H_1

		#include "../cdp/cdp_load.h"

	#else

		/* undo the macros definition if this was re-included */
		#define CDP_AVP_EMPTY_MACROS
			#include "macros.h"
		#undef CDP_AVP_EMPTY_MACROS

	#endif
#endif //_CDP_AVP_BASE_H_1	

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

cdp_avp_add		(Vendor_Id,						0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)
cdp_avp_get		(Vendor_Id,						0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp			(Firmware_Revision,				0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp			(Host_IP_Address,				0,	AAA_AVP_FLAG_MANDATORY,	Address,			ip_address)

cdp_avp			(Supported_Vendor_Id,			0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp_ptr		(Product_Name,					0,	AAA_AVP_FLAG_MANDATORY,	UTF8String,			str)

cdp_avp			(Disconnect_Cause,				0,	AAA_AVP_FLAG_MANDATORY,	Enumerated,			int32_t)

cdp_avp_ptr		(Origin_Host,					0,	AAA_AVP_FLAG_MANDATORY,	DiameterIdentity,	str)

cdp_avp_ptr		(Origin_Realm,					0,	AAA_AVP_FLAG_MANDATORY,	DiameterIdentity,	str)

cdp_avp_ptr		(Destination_Host,				0,	AAA_AVP_FLAG_MANDATORY,	DiameterIdentity,	str)

cdp_avp_ptr		(Destination_Realm,				0,	AAA_AVP_FLAG_MANDATORY,	DiameterIdentity,	str)

cdp_avp_ptr		(Route_Record,					0,	AAA_AVP_FLAG_MANDATORY,	DiameterIdentity,	str)

cdp_avp_ptr		(Proxy_Host,					0,	AAA_AVP_FLAG_MANDATORY,	DiameterIdentity,	str)

cdp_avp_ptr		(Proxy_State,					0,	AAA_AVP_FLAG_MANDATORY,	OctetString,		str)

cdp_avp_add_ptr	(Proxy_Info,					0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST*)
cdp_avp_get		(Proxy_Info,					0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST)

cdp_avp			(Auth_Application_Id,			0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp			(Acct_Application_Id,			0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp			(Inband_Security_Id,			0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp_add_ptr	(Vendor_Specific_Application_Id,0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST*)
cdp_avp_get		(Vendor_Specific_Application_Id,0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST)
cdp_avp_get3	(Vendor_Specific_Application_Id,0,	AAA_AVP_FLAG_MANDATORY,	Vendor_Id, uint32_t, Auth_Application_Id,	 	uint32_t, Acct_Application_Id,	uint32_t)

cdp_avp_ptr		(Redirect_Host,					0,	AAA_AVP_FLAG_MANDATORY,	DiameterIdentity,	str)

cdp_avp			(Redirect_Host_Usage,			0,	AAA_AVP_FLAG_MANDATORY,	Enumerated,			int32_t)

cdp_avp			(Redirect_Max_Cache_Time,		0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp_ptr		(E2E_Sequence,					0,	AAA_AVP_FLAG_MANDATORY,	OctetString,		str)

cdp_avp			(Result_Code,					0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp_ptr		(Error_Message,					0,	AAA_AVP_FLAG_MANDATORY,	UTF8String,			str)

cdp_avp_ptr		(Error_Reporting_Host,			0,	AAA_AVP_FLAG_MANDATORY,	DiameterIdentity,	str)

cdp_avp_add_ptr	(Failed_AVP,					0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST*)
cdp_avp_get		(Failed_AVP,					0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST)

cdp_avp			(Experimental_Result_Code,		0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp_add_ptr	(Experimental_Result,			0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST*)
cdp_avp_get		(Experimental_Result,			0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST)
cdp_avp2		(Experimental_Result,			0,	AAA_AVP_FLAG_MANDATORY,	Vendor_Id, uint32_t, Experimental_Result_Code,	uint32_t)

cdp_avp			(Auth_Request_Type,				0,	AAA_AVP_FLAG_MANDATORY,	Enumerated,			int32_t)

cdp_avp_ptr		(Session_Id,					0,	AAA_AVP_FLAG_MANDATORY,	UTF8String,			str)

cdp_avp			(Authorization_Lifetime,		0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp			(Auth_Grace_Period,				0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp			(Auth_Session_State,			0,	AAA_AVP_FLAG_MANDATORY,	Enumerated,			int32_t)

cdp_avp			(Re_Auth_Request_Type,			0,	AAA_AVP_FLAG_MANDATORY,	Enumerated,			int32_t)

cdp_avp			(Session_Timeout,				0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp_ptr		(User_Name,						0,	AAA_AVP_FLAG_MANDATORY,	UTF8String,			str)

cdp_avp			(Termination_Cause,				0,	AAA_AVP_FLAG_MANDATORY,	Enumerated,			int32_t)

cdp_avp			(Origin_State_Id,				0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp			(Session_Binding,				0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp			(Session_Server_Failover,		0,	AAA_AVP_FLAG_MANDATORY,	Enumerated,			int32_t)

cdp_avp			(Multi_Round_Time_Out,			0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp_ptr		(Class,							0,	AAA_AVP_FLAG_MANDATORY,	OctetString,		str)

cdp_avp			(Event_Timestamp,				0,	AAA_AVP_FLAG_MANDATORY,	Time,				time_t)

cdp_avp			(Accounting_Record_Type,		0,	AAA_AVP_FLAG_MANDATORY,	Enumerated,			int32_t)

cdp_avp			(Acct_Interim_Interval,			0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp			(Accounting_Record_Number,		0,	AAA_AVP_FLAG_MANDATORY,	Unsigned32,			uint32_t)

cdp_avp_ptr		(Acct_Session_Id,				0,	AAA_AVP_FLAG_MANDATORY,	OctetString,		str)

cdp_avp_ptr		(Acct_Multi_Session_Id,			0,	AAA_AVP_FLAG_MANDATORY,	UTF8String,			str)

cdp_avp			(Accounting_Sub_Session_Id,		0,	AAA_AVP_FLAG_MANDATORY,	Unsigned64,			uint64_t)

cdp_avp			(Accounting_Realtime_Required,	0,	AAA_AVP_FLAG_MANDATORY,	Enumerated,			int32_t)

cdp_avp_add_ptr	(MIP6_Agent_Info,				0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST*)
cdp_avp_get		(MIP6_Agent_Info,				0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST)

cdp_avp			(MIP_Home_Agent_Address,		0,	AAA_AVP_FLAG_MANDATORY,	Address,			ip_address)

cdp_avp_add_ptr	(MIP_Home_Agent_Host,			0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST*)
cdp_avp_get		(MIP_Home_Agent_Host,			0,	AAA_AVP_FLAG_MANDATORY,	Grouped,			AAA_AVP_LIST)

cdp_avp_ptr		(MIP6_Home_Link_Prefix,			0,	AAA_AVP_FLAG_MANDATORY,	OctetString,		str)

cdp_avp			(MIP6_Feature_Vector,			0,	AAA_AVP_FLAG_MANDATORY,	Unsigned64,			uint64_t)

cdp_avp_ptr		(Service_Selection,				0,	AAA_AVP_FLAG_MANDATORY,	UTF8String,			str)



/*
 * From here-on you can define/export/init/declare functions which can not be generate with the macros
 */

#if defined(CDP_AVP_DEFINITION)

	/*
	 * Put here your supplimentary definitions. Typically:
	 * 
	 * int <function1>(param1)
	 * {
	 *   code1
	 * }
	 * 
	 * 
	 */
	
	int cdp_avp_add_Vendor_Specific_Application_Id_Group(AAA_AVP_LIST *list,uint32_t vendor_id,uint32_t auth_app_id,uint32_t acct_app_id)
	{
		AAA_AVP_LIST list_grp={0,0};   
		if (!cdp_avp_add_Vendor_Id(&list_grp,vendor_id)) goto error;
		if (auth_app_id && !cdp_avp_add_Auth_Application_Id(&list_grp,auth_app_id)) goto error;
		if (acct_app_id && !cdp_avp_add_Acct_Application_Id(&list_grp,acct_app_id)) goto error;
			return cdp_avp_add_to_list(list,
						cdp_avp_new_Grouped(
								AVP_Vendor_Specific_Application_Id,
								AAA_AVP_FLAG_MANDATORY,
								0,
								&list_grp,
								AVP_FREE_DATA));       
	error:
		cdp->AAAFreeAVPList(&list_grp);
		return 0;
	}

	/**
	 * http://tools.ietf.org/html/rfc3588#section-6.11
	 * @param list
	 * @param data
	 * @return
	 */
	int cdp_avp_get_Vendor_Specific_Application_Id_example(AAA_AVP_LIST list,uint32_t *vendor_id,uint32_t *auth_app_id,uint32_t *acct_app_id)
	{
		AAA_AVP_LIST list_grp={0,0};
		AAA_AVP *avp = cdp_avp_get_from_list(list,
				AVP_Vendor_Specific_Application_Id,
				0);
		if (!avp) goto error;
		cdp_avp_get_Grouped(avp,&list_grp);
		if (!cdp_avp_get_Vendor_Id(list_grp,vendor_id,0)) goto error;
		cdp_avp_get_Auth_Application_Id(list_grp,auth_app_id,0);	
		cdp_avp_get_Acct_Application_Id(list_grp,acct_app_id,0);	
		cdp->AAAFreeAVPList(&list_grp);
		return 1;
	error:
		if (vendor_id) *vendor_id = 0;
		if (auth_app_id) *auth_app_id = 0;
		if (acct_app_id) *acct_app_id = 0;
		cdp->AAAFreeAVPList(&list_grp);
		return 0;
	}
	


#elif defined(CDP_AVP_EXPORT)

	/*
	 * Put here your supplimentary exports in the format: 
	 * 	<function_type1> <nice_function_name1>; 
	 *  <function_type2> <nice_function_name1>;
	 *  ...
	 *  
	 */

	cdp_avp_add_Vendor_Specific_Application_Id_Group_f	add_Vendor_Specific_Application_Id_Group;

	cdp_avp_get_Vendor_Specific_Application_Id_example_f	get_Vendor_Specific_Application_Id_example;
	

#elif defined(CDP_AVP_INIT)

	/*
	 * Put here your supplimentary inits in the format: 
	 * 	<function1>,
	 *  <function2>,
	 *  ...
	 * 
	 * Make sure you keep the same order as in export!
	 * 
	 */

	cdp_avp_add_Vendor_Specific_Application_Id_Group,
	
	cdp_avp_get_Vendor_Specific_Application_Id_example,
	


#elif defined(CDP_AVP_REFERENCE)
	/*
	 * Put here what you want to get in the reference. Typically:
	 * <function1>
	 * <function2>
	 * ... 
	 * 
	 */
	int CDP_AVP_MODULE.add_Vendor_Specific_Application_Id_Group(AAA_AVP_LIST *list,uint32_t vendor_id,uint32_t auth_app_id,uint32_t acct_app_id);
	
	int CDP_AVP_MODULE.get_Vendor_Specific_Application_Id_example(AAA_AVP_LIST list,uint32_t *vendor_id,uint32_t *auth_app_id,uint32_t *acct_app_id);
	
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
	
	#ifndef _CDP_AVP_BASE_H_2
	#define _CDP_AVP_BASE_H_2

	int cdp_avp_add_Vendor_Specific_Application_Id_Group(AAA_AVP_LIST *list,uint32_t vendor_id,uint32_t auth_app_id,uint32_t acct_app_id);
	typedef int (*cdp_avp_add_Vendor_Specific_Application_Id_Group_f)(AAA_AVP_LIST *list,uint32_t vendor_id,uint32_t auth_app_id,uint32_t acct_app_id);
	
	int cdp_avp_get_Vendor_Specific_Application_Id_example(AAA_AVP_LIST list,uint32_t *vendor_id,uint32_t *auth_app_id,uint32_t *acct_app_id);
	typedef int (*cdp_avp_get_Vendor_Specific_Application_Id_example_f)(AAA_AVP_LIST list,uint32_t *vendor_id,uint32_t *auth_app_id,uint32_t *acct_app_id);
	

	#endif //_CDP_AVP_BASE_H_2
	
#endif



#define CDP_AVP_UNDEF_MACROS
	#include "macros.h"
#undef CDP_AVP_UNDEF_MACROS
	




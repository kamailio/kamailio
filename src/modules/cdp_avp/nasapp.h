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
 * RFC 4005 Base AVPs
 * 
 * http://tools.ietf.org/html/rfc4005
 * 
 */

#include "macros.h"

#undef CDP_AVP_MODULE
#define CDP_AVP_MODULE nasapp

#if !defined(CDP_AVP_DECLARATION) && !defined(CDP_AVP_EXPORT) && !defined(CDP_AVP_INIT) && !defined(CDP_AVP_REFERENCE)
	#ifndef _CDP_AVP_NASAPP_H_1
	#define _CDP_AVP_NASAPP_H_1

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


cdp_avp			(Accounting_Input_Octets,		0,	AAA_AVP_FLAG_MANDATORY,	Unsigned64,			uint64_t)

cdp_avp			(Accounting_Input_Packets,		0,	AAA_AVP_FLAG_MANDATORY,	Unsigned64,			uint64_t)

cdp_avp			(Accounting_Output_Octets,		0,	AAA_AVP_FLAG_MANDATORY,	Unsigned64,			uint64_t)

cdp_avp			(Accounting_Output_Packets,		0,	AAA_AVP_FLAG_MANDATORY,	Unsigned64,			uint64_t)

cdp_avp_ptr		(Filter_Id,						0,	AAA_AVP_FLAG_MANDATORY,	UTF8String,			str)



cdp_avp_ptr		(Called_Station_Id,				0,	AAA_AVP_FLAG_MANDATORY,	UTF8String,			str)





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
	int cdp_avp_add_Framed_IP_Address(AAA_AVP_LIST *list,ip_address ip)
	{
		if (ip.ai_family!=AF_INET) {
			LOG(L_ERR,"Trying to build from non IPv4 address!\n");
			return 0;
		}
		char x[4];
		str s={x,4};
		//*((uint32_t*)x) = htonl(ip.ip.v4.s_addr);
		memcpy(x,&(ip.ip.v4.s_addr),sizeof(uint32_t));
		return cdp_avp_add_to_list(list,
				cdp_avp_new(
						AVP_Framed_IP_Address,
						AAA_AVP_FLAG_MANDATORY,
						0,
						s,
						AVP_DUPLICATE_DATA));
	}
	
	int cdp_avp_get_Framed_IP_Address(AAA_AVP_LIST list,ip_address *ip,AAA_AVP **avp_ptr)
	{
		if (!ip) return 0;
		AAA_AVP *avp = cdp_avp_get_next_from_list(list,
				AVP_Framed_IP_Address,
				0,
				avp_ptr?*avp_ptr:0);
		if (avp_ptr) *avp_ptr = avp;
		if (!avp) {
			bzero(ip,sizeof(ip_address));			
			return 0;											
		}			
		if (avp->data.len<4){
			LOG(L_ERR,"Error decoding Framed IP Address from AVP data of length %d < 4",avp->data.len);
			bzero(ip,sizeof(ip_address));
			return 0;
		}
		ip->ai_family = AF_INET;
		//ip->ip.v4.s_addr = ntohl(*((uint32_t*)avp->data.s));
		ip->ip.v4.s_addr = (*((uint32_t*)avp->data.s));
		return 1;
	}
	
	/**
	 * http://tools.ietf.org/html/rfc4005#section-6.11.6
	 * http://tools.ietf.org/html/rfc3162#section-2.3
	 * @param list
	 * @param data
	 * @return
	 */
	int cdp_avp_add_Framed_IPv6_Prefix(AAA_AVP_LIST *list,ip_address_prefix ip)
	{
		uint8_t buffer[18];
		str data={(char*)buffer,18};
		if (ip.addr.ai_family!=AF_INET6) {
			LOG(L_ERR,"Trying to build from non IPv6 address!\n");
			return 0;
		}
		buffer[0]=0;
		buffer[1]=ip.prefix;
		memcpy(buffer+2,ip.addr.ip.v6.s6_addr,16);
		return cdp_avp_add_to_list(list,
				cdp_avp_new_OctetString(
						AVP_Framed_IPv6_Prefix,
						AAA_AVP_FLAG_MANDATORY,
						0,
						data,
						AVP_DUPLICATE_DATA));
	}

	int cdp_avp_get_Framed_IPv6_Prefix(AAA_AVP_LIST list,ip_address_prefix *ip,AAA_AVP **avp_ptr)
	{
		if (!ip) return 0;
		AAA_AVP *avp = cdp_avp_get_next_from_list(list,
				AVP_Framed_IPv6_Prefix,
				0,
				avp_ptr?*avp_ptr:0);
		if (avp_ptr) *avp_ptr = avp;
		if (!avp) {
			bzero(ip,sizeof(ip_address_prefix));			
			return 0;											
		}	
		if (avp->data.len<18) {
			LOG(L_ERR,"Error decoding Framed-IPv6-Prefix from data len < 18 bytes!\n");
			bzero(ip,sizeof(ip_address_prefix));
			return 0;
		}
		ip->addr.ai_family = AF_INET6;
		ip->prefix = avp->data.s[1];
		memcpy(ip->addr.ip.v6.s6_addr,avp->data.s+2,16);	
		return 1;
	}	

#elif defined(CDP_AVP_EXPORT)

	/*
	 * Put here your supplimentary exports in the format: 
	 * 	<function_type1> <nice_function_name1>; 
	 *  <function_type2> <nice_function_name1>;
	 *  ...
	 *  
	 */
	cdp_avp_add_Framed_IP_Address_f		add_Framed_IP_Address;
	cdp_avp_get_Framed_IP_Address_f		get_Framed_IP_Address;
	
	cdp_avp_add_Framed_IPv6_Prefix_f	add_Framed_IPv6_Prefix;
	cdp_avp_get_Framed_IPv6_Prefix_f	get_Framed_IPv6_Prefix;	
	

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
	cdp_avp_add_Framed_IP_Address,
	cdp_avp_get_Framed_IP_Address,

	cdp_avp_add_Framed_IPv6_Prefix,
	cdp_avp_get_Framed_IPv6_Prefix,	

#elif defined(CDP_AVP_REFERENCE)
	/*
	 * Put here what you want to get in the reference. Typically:
	 * <function1>
	 * <function2>
	 * ... 
	 * 
	 */
int CDP_AVP_MODULE.add_Framed_IP_Address(AAA_AVP_LIST *list,ip_address ip);
int CDP_AVP_MODULE.get_Framed_IP_Address(AAA_AVP_LIST list,ip_address *ip,AAA_AVP **avp_ptr);
	
int CDP_AVP_MODULE.add_Framed_IPv6_Prefix(AAA_AVP_LIST *list,ip_address_prefix ip);
int CDP_AVP_MODULE.get_Framed_IPv6_Prefix(AAA_AVP_LIST list,ip_address_prefix *ip,AAA_AVP **avp_ptr);
	
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

	
	#ifndef _CDP_AVP_NASAPP_H_2
	#define _CDP_AVP_NASAPP_H_2

		int cdp_avp_add_Framed_IP_Address(AAA_AVP_LIST *list,ip_address ip);
		typedef int (*cdp_avp_add_Framed_IP_Address_f)(AAA_AVP_LIST *list,ip_address ip);

		int cdp_avp_get_Framed_IP_Address(AAA_AVP_LIST list,ip_address *ip,AAA_AVP **avp_ptr);
		typedef int (*cdp_avp_get_Framed_IP_Address_f)(AAA_AVP_LIST list,ip_address *ip,AAA_AVP **avp_ptr);
	
		int cdp_avp_add_Framed_IPv6_Prefix(AAA_AVP_LIST *list,ip_address_prefix ip);
		typedef int (*cdp_avp_add_Framed_IPv6_Prefix_f)(AAA_AVP_LIST *list,ip_address_prefix ip);

		int cdp_avp_get_Framed_IPv6_Prefix(AAA_AVP_LIST list,ip_address_prefix *ip,AAA_AVP **avp_ptr);
		typedef int (*cdp_avp_get_Framed_IPv6_Prefix_f)(AAA_AVP_LIST list,ip_address_prefix *ip,AAA_AVP **avp_ptr);


	#endif //_CDP_AVP_BASE_H_2
	
#endif



#define CDP_AVP_UNDEF_MACROS
	#include "macros.h"
#undef CDP_AVP_UNDEF_MACROS
	




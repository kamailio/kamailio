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

#ifndef __AVP_NEW_BASE_DATA_FORMAT_H
#define __AVP_NEW_BASE_DATA_FORMAT_H

#ifndef CDP_AVP_REFERENCE

	#include "../cdp/cdp_load.h"

		#include <inttypes.h>
		#include <netinet/ip6.h>

		typedef struct {
			uint16_t ai_family;
			union{
				struct in_addr v4;
				struct in6_addr v6;
			} ip;
		} ip_address;
	
		typedef struct {
			uint8_t prefix;
			ip_address addr;	
		} ip_address_prefix;
		

	#define EPOCH_UNIX_TO_EPOCH_NTP 2208988800u // according to http://www.cis.udel.edu/~mills/y2k.html
	
	
	/* 
	 * RFC 3588 Basic AVP Data Types
	 * 
	 * http://tools.ietf.org/html/rfc3588#section-4.2
	 * 
	 */
	
	AAA_AVP* cdp_avp_new_OctetString(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	typedef AAA_AVP* (*cdp_avp_new_OctetString_f)(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* cdp_avp_new_Integer32(int avp_code,int avp_flags,int avp_vendorid,int32_t data);
	typedef AAA_AVP* (*cdp_avp_new_Integer32_f)(int avp_code,int avp_flags,int avp_vendorid,int32_t data);
	
	
	AAA_AVP* cdp_avp_new_Integer64(int avp_code,int avp_flags,int avp_vendorid,int64_t data);
	typedef AAA_AVP* (*cdp_avp_new_Integer64_f)(int avp_code,int avp_flags,int avp_vendorid,int64_t data);
	
	
	AAA_AVP* cdp_avp_new_Unsigned32(int avp_code,int avp_flags,int avp_vendorid,uint32_t data);
	typedef AAA_AVP* (*cdp_avp_new_Unsigned32_f)(int avp_code,int avp_flags,int avp_vendorid,uint32_t data);
	
	
	AAA_AVP* cdp_avp_new_Unsigned64(int avp_code,int avp_flags,int avp_vendorid,uint64_t data);
	typedef AAA_AVP* (*cdp_avp_new_Unsigned64_f)(int avp_code,int avp_flags,int avp_vendorid,uint64_t data);
	
	
	AAA_AVP* cdp_avp_new_Float32(int avp_code,int avp_flags,int avp_vendorid,float data);
	typedef AAA_AVP* (*cdp_avp_new_Float32_f)(int avp_code,int avp_flags,int avp_vendorid,float data);
	
	
	AAA_AVP* cdp_avp_new_Float64(int avp_code,int avp_flags,int avp_vendorid,double data);
	typedef AAA_AVP* (*cdp_avp_new_Float64_f)(int avp_code,int avp_flags,int avp_vendorid,double data);
	
	
	AAA_AVP* cdp_avp_new_Grouped(int avp_code,int avp_flags,int avp_vendorid,AAA_AVP_LIST *list,AVPDataStatus data_do);
	typedef AAA_AVP* (*cdp_avp_new_Grouped_f)(int avp_code,int avp_flags,int avp_vendorid,AAA_AVP_LIST *list,AVPDataStatus data_do);
	
	
	/*
	 * RFC 3588 Derived AVP Data Formats
	 * 
	 * http://tools.ietf.org/html/rfc3588#section-4.3
	 * 
	 */
	
	AAA_AVP* cdp_avp_new_Address(int avp_code,int avp_flags,int avp_vendorid,ip_address data);
	typedef AAA_AVP* (*cdp_avp_new_Address_f)(int avp_code,int avp_flags,int avp_vendorid,ip_address data);
	
	AAA_AVP* cdp_avp_new_Time(int avp_code,int avp_flags,int avp_vendorid,time_t data);
	typedef AAA_AVP* (*cdp_avp_new_Time_f)(int avp_code,int avp_flags,int avp_vendorid,time_t data);
	
	AAA_AVP* cdp_avp_new_UTF8String(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	typedef AAA_AVP* (*cdp_avp_new_UTF8String_f)(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* cdp_avp_new_DiameterIdentity(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	typedef AAA_AVP* (*cdp_avp_new_DiameterIdentity_f)(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* cdp_avp_new_DiameterURI(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	typedef AAA_AVP* (*cdp_avp_new_DiameterURI_f)(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* cdp_avp_new_Enumerated(int avp_code,int avp_flags,int avp_vendorid,int32_t data);
	typedef AAA_AVP* (*cdp_avp_new_Enumerated_f)(int avp_code,int avp_flags,int avp_vendorid,int32_t data);
	
	AAA_AVP* cdp_avp_new_IPFilterRule(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	typedef AAA_AVP* (*cdp_avp_new_IPFilterRule_f)(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* cdp_avp_new_QoSFilterRule(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	typedef AAA_AVP* (*cdp_avp_new_QoSFilterRule_f)(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	

#else
		
	AAA_AVP* basic.new_OctetString(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* basic.new_Integer32(int avp_code,int avp_flags,int avp_vendorid,int32_t data);
	
	AAA_AVP* basic.new_Integer64(int avp_code,int avp_flags,int avp_vendorid,int64_t data);
	
	AAA_AVP* basic.new_Unsigned32(int avp_code,int avp_flags,int avp_vendorid,uint32_t data);
	
	AAA_AVP* basic.new_Unsigned64(int avp_code,int avp_flags,int avp_vendorid,uint64_t data);
	
	AAA_AVP* basic.new_Float32(int avp_code,int avp_flags,int avp_vendorid,float data);
	
	AAA_AVP* basic.new_Float64(int avp_code,int avp_flags,int avp_vendorid,double data);
	
	AAA_AVP* basic.new_Grouped(int avp_code,int avp_flags,int avp_vendorid,AAA_AVP_LIST *list,AVPDataStatus data_do);
		
	AAA_AVP* basic.new_Address(int avp_code,int avp_flags,int avp_vendorid,ip_address data);
	
	AAA_AVP* basic.new_Time(int avp_code,int avp_flags,int avp_vendorid,time_t data);
	
	AAA_AVP* basic.new_UTF8String(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* basic.new_DiameterIdentity(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* basic.new_DiameterURI(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* basic.new_Enumerated(int avp_code,int avp_flags,int avp_vendorid,int32_t data);
	
	AAA_AVP* basic.new_IPFilterRule(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
	
	AAA_AVP* basic.new_QoSFilterRule(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do);
#endif


#endif /* __AVP_NEW_DATA_FORMAT_H */

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

#ifndef __AVP_GET_BASE_DATA_FORMAT_H
#define __AVP_GET_BASE_DATA_FORMAT_H

	#include "../cdp/cdp_load.h"
	#include "avp_new_base_data_format.h"

	#define EPOCH_UNIX_TO_EPOCH_NTP 2208988800u // according to http://www.cis.udel.edu/~mills/y2k.html
	
	/* 
	 * RFC 3588 Basic AVP Data Types
	 * 
	 * http://tools.ietf.org/html/rfc3588#section-4.2
	 * 
	 */
	
	int cdp_avp_get_OctetString(AAA_AVP *avp,str *data);
	typedef int (*cdp_avp_get_OctetString_f)(AAA_AVP *avp,str *data);
	
	
	int cdp_avp_get_Integer32(AAA_AVP *avp,int32_t *data);
	typedef int (*cdp_avp_get_Integer32_f)(AAA_AVP *avp,int32_t *data);
	
	
	int cdp_avp_get_Integer64(AAA_AVP *avp,int64_t *data);
	typedef int (*cdp_avp_get_Integer64_f)(AAA_AVP *avp,int64_t *data);
	
	
	int cdp_avp_get_Unsigned32(AAA_AVP *avp,uint32_t *data);
	typedef int (*cdp_avp_get_Unsigned32_f)(AAA_AVP *avp,uint32_t *data);
	
	
	int cdp_avp_get_Unsigned64(AAA_AVP *avp,uint64_t *data);
	typedef int (*cdp_avp_get_Unsigned64_f)(AAA_AVP *avp,uint64_t *data);
	
	
	int cdp_avp_get_Float32(AAA_AVP *avp,float *data);
	typedef int (*cdp_avp_get_Float32_f)(AAA_AVP *avp,float *data);
	
	
	int cdp_avp_get_Float64(AAA_AVP *avp,double *data);
	typedef int (*cdp_avp_get_Float64_f)(AAA_AVP *avp,double *data);
	
	
	int cdp_avp_get_Grouped(AAA_AVP *avp,AAA_AVP_LIST *data);
	typedef int (*cdp_avp_get_Grouped_f)(AAA_AVP *avp,AAA_AVP_LIST *data);
	
	
	void cdp_avp_free_Grouped(AAA_AVP_LIST *list);
	typedef void (*cdp_avp_free_Grouped_f)(AAA_AVP_LIST *list);
	
	/*
	 * RFC 3588 Derived AVP Data Formats
	 * 
	 * http://tools.ietf.org/html/rfc3588#section-4.3
	 * 
	 */
	
	int cdp_avp_get_Address(AAA_AVP *avp,ip_address *data);
	typedef int (*cdp_avp_get_Address_f)(AAA_AVP *avp,ip_address *data);
	
	int cdp_avp_get_Time(AAA_AVP *avp,time_t *data);
	typedef int (*cdp_avp_get_Time_f)(AAA_AVP *avp,time_t *data);
	
	int cdp_avp_get_UTF8String(AAA_AVP *avp,str *data);
	typedef int (*cdp_avp_get_UTF8String_f)(AAA_AVP *avp,str *data);
	
	int cdp_avp_get_DiameterIdentity(AAA_AVP *avp,str *data);
	typedef int (*cdp_avp_get_DiameterIdentity_f)(AAA_AVP *avp,str *data);
	
	int cdp_avp_get_DiameterURI(AAA_AVP *avp,str *data);
	typedef int (*cdp_avp_get_DiameterURI_f)(AAA_AVP *avp,str *data);
	
	int cdp_avp_get_Enumerated(AAA_AVP *avp,int32_t *data);
	typedef int (*cdp_avp_get_Enumerated_f)(AAA_AVP *avp,int32_t *data);
	
	int cdp_avp_get_IPFilterRule(AAA_AVP *avp,str *data);
	typedef int (*cdp_avp_get_IPFilterRule_f)(AAA_AVP *avp,str *data);
	
	int cdp_avp_get_QoSFilterRule(AAA_AVP *avp,str *data);
	typedef int (*cdp_avp_get_QoSFilterRule_f)(AAA_AVP *avp,str *data);
	

#endif /* __AVP_GET_DATA_FORMAT_H */

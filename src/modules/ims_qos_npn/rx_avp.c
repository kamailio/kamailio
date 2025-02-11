/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * Copyright (C) 2024 Neat Path Networks GmbH, alberto@neatpath.net
 * Copyright (C) 2024 Neat Path Networks GmbH, dragos@neatpath.net
 *
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fraunhofer FOKUS Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 *
 * NB: A lot of this code was originally part of OpenIMSCore,
 * FhG Fokus.
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 *
 *
 * History:
 * --------
 *  2011-02-02  initial version (jason.penton)
 *  2024-07-15  improved support for 3GPP AVPs (Alberto Diez, Dragos Vingarzan)
 */


#include <arpa/inet.h>
#include "../cdp_avp/cdp_avp_mod.h"
#include "../ims_usrloc_pcscf/usrloc.h"
#include "rx_authdata.h"
#include "rx_avp.h"
#include "ims_qos_mod.h"
#include "../../core/parser/sdp/sdp_helpr_funcs.h"
#include <regex.h>

#include "../../lib/ims/ims_getters.h"

/**< Structure with pointers to cdp funcs, global variable defined in mod.c  */
extern struct cdp_binds cdpb;
extern cdp_avp_bind_t *cdp_avp;

extern str regex_sdp_ip_prefix_to_maintain_in_fd;

extern int include_rtcp_fd;

extern int omit_flow_ports;
extern int rs_default_bandwidth;
extern int rr_default_bandwidth;

static const int prefix_length_ipv6 = 128;

/**
 * Create and add an AVP to a Diameter message.
 * @param m - Diameter message to add to
 * @param d - the payload data
 * @param len - length of the payload data
 * @param avp_code - the code of the AVP
 * @param flags - flags for the AVP
 * @param vendorid - the value of the vendor id or 0 if none
 * @param data_do - what to do with the data when done
 * @param func - the name of the calling function, for debugging purposes
 * @returns 1 on success or 0 on failure
 */
inline int rx_add_avp(AAAMessage *m, char *d, int len, int avp_code, int flags,
		int vendorid, int data_do, const char *func)
{
	AAA_AVP *avp;
	if(vendorid != 0)
		flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
	avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
	if(!avp) {
		LM_ERR("Rx: :%s: Failed creating avp\n", func);
		return 0;
	}
	if(cdpb.AAAAddAVPToMessage(m, avp, m->avpList.tail) != AAA_ERR_SUCCESS) {
		LM_ERR(":%s: Failed adding avp to message\n", func);
		cdpb.AAAFreeAVP(&avp);
		return 0;
	}
	return CSCF_RETURN_TRUE;
}

/**
 * Create and add an AVP to a list of AVPs.
 * @param list - the AVP list to add to
 * @param d - the payload data
 * @param len - length of the payload data
 * @param avp_code - the code of the AVP
 * @param flags - flags for the AVP
 * @param vendorid - the value of the vendor id or 0 if none
 * @param data_do - what to do with the data when done
 * @param func - the name of the calling function, for debugging purposes
 * @returns 1 on success or 0 on failure
 */
static inline int rx_add_avp_list(AAA_AVP_LIST *list, char *d, int len,
		int avp_code, int flags, int vendorid, int data_do, const char *func)
{
	AAA_AVP *avp;
	if(vendorid != 0)
		flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
	avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
	if(!avp) {
		LM_ERR(":%s: Failed creating avp\n", func);
		return 0;
	}
	if(list->tail) {
		avp->prev = list->tail;
		avp->next = 0;
		list->tail->next = avp;
		list->tail = avp;
	} else {
		list->head = avp;
		list->tail = avp;
		avp->next = 0;
		avp->prev = 0;
	}

	return CSCF_RETURN_TRUE;
}

/**
 * Returns the value of a certain AVP from a Diameter message.
 * @param m - Diameter message to look into
 * @param avp_code - the code to search for
 * @param vendorid - the value of the vendor id to look for or 0 if none
 * @param func - the name of the calling function, for debugging purposes
 * @returns the str with the payload on success or an empty string on failure
 */
static inline str rx_get_avp(
		AAAMessage *msg, int avp_code, int vendor_id, const char *func)
{
	AAA_AVP *avp;
	str r = {0, 0};

	avp = cdpb.AAAFindMatchingAVP(msg, 0, AVP_Result_Code, 0, 0);
	if(avp == 0) {
		//LOG(L_INFO,"INFO:"M_NAME":%s: Failed finding avp\n",func);
		return r;
	} else
		return avp->data;
}

/*creates an AVP for the framed-ip info:
 * 	if ipv4: AVP_Framed_IP_Address,
 * 	otherwise: AVP_Framed_IPv6_Prefix
 * 	using inet_pton to convert the IP addresses
 * 	from human-readable strings to their bynary representation
 * 	see http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
 * 	http://beej.us/guide/bgnet/output/html/multipage/sockaddr_inman.html
 */

static unsigned int ip_buflen = 0;
static char *ip_buf = 0;

int rx_add_framed_ip_avp(AAA_AVP_LIST *list, str ip, uint16_t version)
{
	ip_address_prefix ip_adr;
	int ret = 0;

	if(ip.len <= 0)
		return 0;
	if(version == AF_INET) {
		if(ip.len > INET_ADDRSTRLEN)
			goto error;
	} else {
		if(ip.len > INET6_ADDRSTRLEN)
			goto error;
	}
	int len = ip.len + 1;
	if(!ip_buf || ip_buflen < len) {
		if(ip_buf)
			pkg_free(ip_buf);
		ip_buf = (char *)pkg_malloc(len);
		if(!ip_buf) {
			LM_ERR("rx_add_framed_ip_avp: out of memory \
					    when allocating %i bytes in pkg\n",
					len);
			goto error;
		}
		ip_buflen = len;
	}
	if(ip.s[0] == '[' && ip.s[ip.len - 1] == ']') {
		memcpy(ip_buf, ip.s + 1, ip.len - 2);
		ip_buf[ip.len - 2] = '\0';
	} else {
		memcpy(ip_buf, ip.s, ip.len);
		ip_buf[ip.len] = '\0';
	}

	ip_adr.addr.ai_family = version;

	if(version == AF_INET) {

		if(inet_pton(AF_INET, ip_buf, &(ip_adr.addr.ip.v4.s_addr)) != 1)
			goto error;
		ret = cdp_avp->nasapp.add_Framed_IP_Address(list, ip_adr.addr);
	} else {

		if(inet_pton(AF_INET6, ip_buf, &(ip_adr.addr.ip.v6.s6_addr)) != 1)
			goto error;
		ip_adr.prefix = prefix_length_ipv6;
		ret = cdp_avp->nasapp.add_Framed_IPv6_Prefix(list, ip_adr);
	}

	//TODO: should free ip_buf in module shutdown....

error:
	return ret;
}

/**
 * Creates and adds a Vendor-Specifig-Application-ID AVP.
 * @param msg - the Diameter message to add to.
 * @param vendor_id - the value of the vendor_id,
 * @param auth_id - the authorization application id
 * @param acct_id - the accounting application id
 * @returns 1 on success or 0 on error
 */
inline static int rx_add_vendor_specific_appid_avp(AAAMessage *msg,
		unsigned int vendor_id, unsigned int auth_id, unsigned int acct_id)
{
	AAA_AVP_LIST list;
	str group;
	char x[4];

	list.head = 0;
	list.tail = 0;

	set_4bytes(x, vendor_id);
	rx_add_avp_list(&list, x, 4, AVP_Vendor_Id, AAA_AVP_FLAG_MANDATORY, 0,
			AVP_DUPLICATE_DATA, __FUNCTION__);

	if(auth_id) {
		set_4bytes(x, auth_id);
		rx_add_avp_list(&list, x, 4, AVP_Auth_Application_Id,
				AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
	}
	if(acct_id) {
		set_4bytes(x, acct_id);
		rx_add_avp_list(&list, x, 4, AVP_Acct_Application_Id,
				AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
	}

	group = cdpb.AAAGroupAVPS(list);

	cdpb.AAAFreeAVPList(&list);

	return rx_add_avp(msg, group.s, group.len,
			AVP_Vendor_Specific_Application_Id, AAA_AVP_FLAG_MANDATORY, 0,
			AVP_FREE_DATA, __FUNCTION__);
}

/**
 * Creates and adds a Destination-Realm AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int rx_add_destination_realm_avp(AAAMessage *msg, str data)
{
	return rx_add_avp(msg, data.s, data.len, AVP_Destination_Realm,
			AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
}

/**
 * Creates and adds an Acct-Application-Id AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @return CSCF_RETURN_TRUE on success or 0 on error
 */
inline int rx_add_auth_application_id_avp(AAAMessage *msg, unsigned int data)
{
	char x[4];
	set_4bytes(x, data);

	return rx_add_avp(msg, x, 4, AVP_Auth_Application_Id,
			AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
}

/*
 * Creates and adds a Subscription_Id AVP
 * @param msg - the Diameter message to add to.
 * @param r - the sip_message to extract the data from.
 * @param tag - originating (0) terminating (1)
 * @return CSCF_RETURN_TRUE on success or 0 on error
 *
 */

int rx_add_subscription_id_avp(
		AAAMessage *msg, str identifier, int identifier_type)
{

	AAA_AVP_LIST list;
	AAA_AVP *type, *data;
	str subscription_id_avp;
	char x[4];
	list.head = 0;
	list.tail = 0;

	set_4bytes(x, identifier_type);

	type = cdpb.AAACreateAVP(AVP_IMS_Subscription_Id_Type,
			AAA_AVP_FLAG_MANDATORY, 0, x, 4, AVP_DUPLICATE_DATA);

	data = cdpb.AAACreateAVP(AVP_IMS_Subscription_Id_Data,
			AAA_AVP_FLAG_MANDATORY, 0, identifier.s, identifier.len,
			AVP_DUPLICATE_DATA);

	cdpb.AAAAddAVPToList(&list, type);
	cdpb.AAAAddAVPToList(&list, data);

	subscription_id_avp = cdpb.AAAGroupAVPS(list);

	cdpb.AAAFreeAVPList(&list);

	return rx_add_avp(msg, subscription_id_avp.s, subscription_id_avp.len,
			AVP_IMS_Subscription_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA,
			__FUNCTION__);
}

inline static unsigned int sdp_b_value(str *payload, char *subtype)
{
	char *line;
	unsigned int i;
	str s;
	line = find_sdp_line(payload->s, payload->s + payload->len, 'b');
	while(line != NULL) {
		// b=AS:
		if((line[2] == subtype[0]) && (line[3] == subtype[1])) {
			LM_DBG("SDP-Line: %.*s\n", 5, line);
			line += 5;
			i = 0;
			while((line[i] != '\r') && (line[i] != '\n')
					&& ((line + i) <= (payload->s + payload->len))) {
				i++;
			}
			s.s = line;
			s.len = i;
			LM_DBG("value: %.*s\n", s.len, s.s);
			if(str2int(&s, &i) == 0)
				return i;
			else
				return 0;
		}
		line = find_next_sdp_line(line, payload->s + payload->len, 'b', NULL);
	}
	return 0;
}

inline int rx_add_media_component_description_avp(AAAMessage *msg, int number,
		str *media_description, str *ipA, str *portA, str *ipB, str *portB,
		str *transport, str *req_raw_payload, str *rpl_raw_payload,
		enum dialog_direction dlg_direction, int flow_usage_type)
{
	str data;
	AAA_AVP_LIST list;
	AAA_AVP *media_component_number, *media_type;
	AAA_AVP *codec_data1, *codec_data2;
	AAA_AVP *media_sub_component[PCC_Media_Sub_Components];
	AAA_AVP *flow_status;
	AAA_AVP *dl_bw, *ul_bw, *rs_bw, *rr_bw;

	int media_sub_component_number = 0;
	unsigned int bandwidth = 0;

	int type;
	char x[4];

	list.head = 0;
	list.tail = 0;

	/*media-component-number*/
	set_4bytes(x, number);
	media_component_number = cdpb.AAACreateAVP(AVP_IMS_Media_Component_Number,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);

	if(media_component_number != NULL) {
		cdpb.AAAAddAVPToList(&list, media_component_number);
	} else {
		LM_ERR("Unable to create media_component_number AVP");
		return 0;
	}

	/*media-sub-component*/
	if(dlg_direction != DLG_MOBILE_ORIGINATING
			&& dlg_direction != DLG_MOBILE_REGISTER) {
		media_sub_component[media_sub_component_number] =
				rx_create_media_subcomponent_avp(number, transport, ipA, portA,
						ipB, portB, flow_usage_type);
	} else {
		media_sub_component[media_sub_component_number] =
				rx_create_media_subcomponent_avp(number, transport, ipB, portB,
						ipA, portA, flow_usage_type);
	}
	if(media_sub_component[media_sub_component_number])
		cdpb.AAAAddAVPToList(
				&list, media_sub_component[media_sub_component_number]);


	/*media type*/
	if(strncmp(media_description->s, "audio", 5) == 0) {
		type = AVP_IMS_Media_Type_Audio;
	} else if(strncmp(media_description->s, "video", 5) == 0) {
		type = AVP_IMS_Media_Type_Video;
	} else if(strncmp(media_description->s, "data", 4) == 0) {
		type = AVP_IMS_Media_Type_Data;
	} else if(strncmp(media_description->s, "application", 11) == 0) {
		type = AVP_IMS_Media_Type_Application;
	} else if(strncmp(media_description->s, "control", 7) == 0) {
		type = AVP_IMS_Media_Type_Control;
	} else if(strncmp(media_description->s, "text", 4) == 0) {
		type = AVP_IMS_Media_Type_Text;
	} else if(strncmp(media_description->s, "message", 7) == 0) {
		type = AVP_IMS_Media_Type_Message;
	} else {
		type = AVP_IMS_Media_Type_Other;
	}


	set_4bytes(x, type);
	media_type = cdpb.AAACreateAVP(AVP_IMS_Media_Type,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
	cdpb.AAAAddAVPToList(&list, media_type);

	/*RR and RS*/
	if((type == AVP_IMS_Media_Type_Audio)
			|| (type == AVP_IMS_Media_Type_Video)) {
		// Get bandwidth from SDP:
		bandwidth = sdp_b_value(req_raw_payload, "AS");
		LM_DBG("Request: got bandwidth %i from b=AS-Line\n", bandwidth);
		// Set default values:
		if((type == AVP_IMS_Media_Type_Audio) && (bandwidth <= 0))
			bandwidth = audio_default_bandwidth;
		if((type == AVP_IMS_Media_Type_Video) && (bandwidth <= 0))
			bandwidth = video_default_bandwidth;

		// According to 3GPP TS 29.213, Rel. 9+, this value is * 1000:
		bandwidth *= 1000;

		// Add AVP
		set_4bytes(x, bandwidth);
		ul_bw = cdpb.AAACreateAVP(AVP_EPC_Max_Requested_Bandwidth_UL,
				AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
				IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, ul_bw);

		// Get bandwidth from SDP:
		bandwidth = sdp_b_value(rpl_raw_payload, "AS");
		LM_DBG("Answer: got bandwidth %i from b=AS-Line\n", bandwidth);
		// Set default values:
		if((type == AVP_IMS_Media_Type_Audio) && (bandwidth <= 0))
			bandwidth = audio_default_bandwidth;
		if((type == AVP_IMS_Media_Type_Video) && (bandwidth <= 0))
			bandwidth = video_default_bandwidth;

		// According to 3GPP TS 29.213, Rel. 9+, this value is * 1000:
		bandwidth *= 1000;

		// Add AVP
		set_4bytes(x, bandwidth);
		dl_bw = cdpb.AAACreateAVP(AVP_EPC_Max_Requested_Bandwidth_DL,
				AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
				IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, dl_bw);

		// Get A=RS-bandwidth from SDP-Reply:
		bandwidth = sdp_b_value(rpl_raw_payload, "RS");

		if(bandwidth == 0) {
			bandwidth = rs_default_bandwidth;
		}

		LM_DBG("Answer: Got bandwidth %i from b=RS-Line\n", bandwidth);
		if(bandwidth > 0) {
			// Add AVP
			set_4bytes(x, bandwidth);
			rs_bw = cdpb.AAACreateAVP(AVP_EPC_RS_Bandwidth,
					AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
					IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
			cdpb.AAAAddAVPToList(&list, rs_bw);
		}
		// Get A=RS-bandwidth from SDP-Reply:
		bandwidth = sdp_b_value(rpl_raw_payload, "RR");

		if(bandwidth == 0) {
			bandwidth = rr_default_bandwidth;
		}

		LM_DBG("Answer: Got bandwidth %i from b=RR-Line\n", bandwidth);
		if(bandwidth > 0) {
			// Add AVP
			set_4bytes(x, bandwidth);
			rr_bw = cdpb.AAACreateAVP(AVP_EPC_RR_Bandwidth,
					AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
					IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
			cdpb.AAAAddAVPToList(&list, rr_bw);
		}
	}

	/*codec-data*/

	if(dlg_direction == DLG_MOBILE_ORIGINATING) {
		//0 means uplink offer
		codec_data1 = rx_create_codec_data_avp(req_raw_payload, number, 0);
		cdpb.AAAAddAVPToList(&list, codec_data1);
		//3 means downlink answer
		codec_data2 = rx_create_codec_data_avp(rpl_raw_payload, number, 3);
		cdpb.AAAAddAVPToList(&list, codec_data2);
	} else {
		//2 means downlink offer
		codec_data1 = rx_create_codec_data_avp(req_raw_payload, number, 2);
		cdpb.AAAAddAVPToList(&list, codec_data1);
		//1 means uplink answer
		codec_data2 = rx_create_codec_data_avp(rpl_raw_payload, number, 1);
		cdpb.AAAAddAVPToList(&list, codec_data2);
	}


	set_4bytes(x, AVP_IMS_Flow_Status_Enabled);
	flow_status = cdpb.AAACreateAVP(AVP_IMS_Flow_Status,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
	cdpb.AAAAddAVPToList(&list, flow_status);

	/*now group them in one big AVP and free them*/
	data = cdpb.AAAGroupAVPS(list);
	cdpb.AAAFreeAVPList(&list);


	return rx_add_avp(msg, data.s, data.len,
			AVP_IMS_Media_Component_Description,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, AVP_FREE_DATA, __FUNCTION__);
}


//just for registration to signalling path - much cut down MCD AVP
//See 3GPP TS 29.214 section 4.4.5

inline int rx_add_media_component_description_avp_register(AAAMessage *msg)
{
	str data;
	AAA_AVP_LIST list;
	AAA_AVP *media_component_number;

	char x[4];

	list.head = 0;
	list.tail = 0;

	/*media-component-number*/
	set_4bytes(x, 0);
	media_component_number = cdpb.AAACreateAVP(AVP_IMS_Media_Component_Number,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);

	if(media_component_number != NULL) {
		cdpb.AAAAddAVPToList(&list, media_component_number);
	} else {
		LM_ERR("Unable to create media_component_number AVP");
		return 0;
	}

	/*media-sub-component*/
	cdpb.AAAAddAVPToList(&list, rx_create_media_subcomponent_avp_register());

	/*now group them in one big AVP and free them*/
	data = cdpb.AAAGroupAVPS(list);
	cdpb.AAAFreeAVPList(&list);
	return rx_add_avp(msg, data.s, data.len,
			AVP_IMS_Media_Component_Description,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, AVP_FREE_DATA, __FUNCTION__);
}


/**
 * Creates a media-sub-component AVP
 *
 * TODO - fix this ... or just delete it and do it again! It adds 2x Flow-Description for example, as a bug!
 * I don't think that more than 1 can be in one Media Subcomponent.
 *
 * @param number - the flow number
 * @param proto - the protocol of the IPFilterRule
 * @param ipA - ip of the INVITE  (if creating rule for UE that sent INVITE)
 * @param portA - port of the INVITE (if creating rule for UE that sent INVITE)
 * @param ipB - ip of 200 OK (if creating rule for UE that sent INVITE)
 * @param portB - port of 200 OK (if creating rule for UE that sent INVITE)
 * @param options - any options to append to the IPFilterRules
 * @param attributes - indication of attributes
 * 						0 no attributes , 1 sendonly , 2 recvonly , 3 RTCP flows, 4 AF signaling flows
 * @param bwUL - bandwidth uplink
 * @param bwDL - bandiwdth downlink
 */

static str permit_out = {"permit out ", 11};
static str permit_in = {"permit in ", 10};
static str from_s = {" from ", 6};
static str to_s = {" to ", 4};
//removed final %s - this is options which Rx 29.214 says will not be used for flow-description AVP
static char *permit_out_with_ports = "permit out %s from %.*s %u to %.*s %u";
static char *permit_out_without_ports = "permit out %s from %.*s to %.*s";
static char *permit_out_with_any_as_dst = "permit out %s from %.*s %u to any";
//static char * permit_out_with_any_as_src = "permit out %s from any to %.*s %u";
//static char * permit_out_with_ports = "permit out %s from %.*s %u to %.*s %u %s";
static char *permit_in_with_ports = "permit in %s from %.*s %u to %.*s %u";
static char *permit_in_without_ports = "permit in %s from %.*s to %.*s";
static char *permit_in_with_any_as_src = "permit in %s from any to %.*s %u";
//static char * permit_in_with_any_as_dst = "permit in %s from %.*s %u to any";
//static char * permit_in_with_ports = "permit in %s from %.*s %u to %.*s %u %s";

static unsigned int flowdata_buflen = 0;
static str flowdata_buf = {0, 0};

#define MAX_MATCH 20

/*! \brief Match pattern against string and store result in pmatch */
int reg_match(char *pattern, char *string, regmatch_t *pmatch)
{
	regex_t preg;

	if(regcomp(&preg, pattern, REG_EXTENDED | REG_NEWLINE)) {
		return -1;
	}
	if(preg.re_nsub > MAX_MATCH) {
		regfree(&preg);
		return -2;
	}
	if(regexec(&preg, string, MAX_MATCH, pmatch, 0)) {
		regfree(&preg);
		return -3;
	}
	regfree(&preg);
	return 0;
}

AAA_AVP *rx_create_media_subcomponent_avp(int number, str *proto, str *ipA,
		str *portA, str *ipB, str *portB, int flow_usage_type)
{
	str data;

	int len, len2;
	int int_port_rctp_a = 0, int_port_rctp_b = 0;
	str port_rtcp_a = STR_NULL, port_rtcp_b = STR_NULL;
	AAA_AVP *flow_description1 = 0, *flow_description2 = 0,
			*flow_description3 = 0, *flow_description4 = 0, *flow_number = 0;
	AAA_AVP *flow_description5 = 0, *flow_description6 = 0,
			*flow_description7 = 0, *flow_description8 = 0;
	AAA_AVP *flow_usage = 0;

	AAA_AVP_LIST list;
	list.tail = 0;
	list.head = 0;
	char x[4];
	char *proto_nr = 0;
	if(proto->len == 2 && strncasecmp(proto->s, "IP", proto->len) == 0) {
		proto_nr = "ip";
	} else if(proto->len == 3
			  && strncasecmp(proto->s, "UDP", proto->len) == 0) {
		proto_nr = "17";
	} else if(proto->len == 3
			  && strncasecmp(proto->s, "TCP", proto->len) == 0) {
		proto_nr = "6";
	} else if(proto->len == 7
			  && strncasecmp(proto->s, "RTP/AVP", proto->len) == 0) {
		proto_nr = "17"; /* for now we just use UDP for all RTP */
	} else if(proto->len == 8
			  && strncasecmp(proto->s, "RTP/SAVP", proto->len) == 0) {
		proto_nr = "17"; /* for now we just use UDP for all RTP */
	} else if(proto->len == 8
			  && strncasecmp(proto->s, "RTP/AVPF", proto->len) == 0) {
		proto_nr = "17"; /* for now we just use UDP for all RTP */
	} else {
		LOG(L_ERR, "Not yet implemented for protocol %.*s\n", proto->len,
				proto->s);
		return 0;
	}
	int proto_len = strlen(proto_nr);

	int intportA = atoi(portA->s);
	int intportB = atoi(portB->s);

	set_4bytes(x, number);
	flow_number = cdpb.AAACreateAVP(AVP_IMS_Flow_Number,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
	cdpb.AAAAddAVPToList(&list, flow_number);

	/*IMS Flow descriptions*/
	/*first flow is the receive flow*/

	if(omit_flow_ports) {
		len = (permit_out.len + from_s.len + to_s.len + ipB->len + ipA->len + 4
					  + proto_len + 1 /*nul terminator*/)
			  * sizeof(char);
	} else {
		len = (permit_out.len + from_s.len + to_s.len + ipB->len + ipA->len + 4
					  + proto_len + portA->len + portB->len
					  + 1 /*nul terminator*/)
			  * sizeof(char);
	}

	if(!flowdata_buf.s || flowdata_buflen < len) {
		if(flowdata_buf.s)
			pkg_free(flowdata_buf.s);
		flowdata_buf.s = (char *)pkg_malloc(len);
		if(!flowdata_buf.s) {
			LM_ERR("PCC_create_media_component: out of memory \
                                                        when allocating %i bytes in pkg\n",
					len);
			return NULL;
		}
		flowdata_buflen = len;
	}

	if(omit_flow_ports) {
		flowdata_buf.len =
				snprintf(flowdata_buf.s, len, permit_out_without_ports,
						proto_nr, ipA->len, ipA->s, ipB->len, ipB->s);
	} else {
		flowdata_buf.len =
				snprintf(flowdata_buf.s, len, permit_out_with_ports, proto_nr,
						ipA->len, ipA->s, intportA, ipB->len, ipB->s, intportB);
	}

	flowdata_buf.len = strlen(flowdata_buf.s);
	flow_description1 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
			AVP_DUPLICATE_DATA);
	cdpb.AAAAddAVPToList(&list, flow_description1);

	/*second flow*/
	if(omit_flow_ports) {
		len2 = (permit_in.len + from_s.len + to_s.len + ipB->len + ipA->len + 4
					   + proto_len + 1 /*nul terminator*/)
			   * sizeof(char);
	} else {
		len2 = (permit_in.len + from_s.len + to_s.len + ipB->len + ipA->len + 4
					   + proto_len + portA->len + portB->len
					   + 1 /*nul terminator*/)
			   * sizeof(char);
	}

	if(!flowdata_buf.s || len < len2) {
		len = len2;
		if(flowdata_buf.s)
			pkg_free(flowdata_buf.s);
		flowdata_buf.s = (char *)pkg_malloc(len);
		if(!flowdata_buf.s) {
			LM_ERR("PCC_create_media_component: out of memory \
                                                                when allocating %i bytes in pkg\n",
					len);
			return NULL;
		}
		flowdata_buflen = len;
	}

	if(omit_flow_ports) {
		flowdata_buf.len =
				snprintf(flowdata_buf.s, len, permit_in_without_ports, proto_nr,
						ipB->len, ipB->s, ipA->len, ipA->s);
	} else {
		flowdata_buf.len =
				snprintf(flowdata_buf.s, len, permit_in_with_ports, proto_nr,
						ipB->len, ipB->s, intportB, ipA->len, ipA->s, intportA);
	}

	flowdata_buf.len = strlen(flowdata_buf.s);
	flow_description2 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
			AVP_DUPLICATE_DATA);
	cdpb.AAAAddAVPToList(&list, flow_description2);

	if(include_rtcp_fd) {
		LM_DBG("Need to add RTCP FD description - RTCP ports are by default "
			   "next odd port number up from RTP ports\n");
		int_port_rctp_a = intportA + 1;
		if(int_port_rctp_a % 2 == 0) {
			int_port_rctp_a++;
		}
		int_port_rctp_b = intportB + 1;
		if(int_port_rctp_b % 2 == 0) {
			int_port_rctp_b++;
		}
		char c_port_rtcp_a[10];
		port_rtcp_a.len = snprintf(c_port_rtcp_a, 10, "%d", int_port_rctp_a);
		port_rtcp_a.s = c_port_rtcp_a;
		char c_port_rtcp_b[10];
		port_rtcp_b.len = snprintf(c_port_rtcp_b, 10, "%d", int_port_rctp_b);
		port_rtcp_b.s = c_port_rtcp_b;
		LM_DBG("RTCP A Port [%.*s] RCTP B Port [%.*s]\n", port_rtcp_a.len,
				port_rtcp_a.s, port_rtcp_b.len, port_rtcp_b.s);

		/*3rd (optional RTCP) flow*/
		len2 = (permit_out.len + from_s.len + to_s.len + ipB->len + ipA->len + 4
					   + proto_len + port_rtcp_a.len + port_rtcp_b.len
					   + 1 /*nul terminator*/)
			   * sizeof(char);

		if(!flowdata_buf.s || len < len2) {
			len = len2;
			if(flowdata_buf.s)
				pkg_free(flowdata_buf.s);
			flowdata_buf.s = (char *)pkg_malloc(len);
			if(!flowdata_buf.s) {
				LM_ERR("PCC_create_media_component: out of memory \
																when allocating %i bytes in pkg\n",
						len);
				return NULL;
			}
			flowdata_buflen = len;
		}

		flowdata_buf.len = snprintf(flowdata_buf.s, len, permit_out_with_ports,
				proto_nr, ipA->len, ipA->s, int_port_rctp_a, ipB->len, ipB->s,
				int_port_rctp_b);

		flowdata_buf.len = strlen(flowdata_buf.s);
		flow_description3 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
				AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
				IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
				AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, flow_description3);

		/*4th (optional RTCP) flow*/
		len2 = (permit_in.len + from_s.len + to_s.len + ipB->len + ipA->len + 4
					   + proto_len + port_rtcp_a.len + port_rtcp_b.len
					   + 1 /*nul terminator*/)
			   * sizeof(char);
		if(!flowdata_buf.s || len < len2) {
			len = len2;
			if(flowdata_buf.s)
				pkg_free(flowdata_buf.s);
			flowdata_buf.s = (char *)pkg_malloc(len);
			if(!flowdata_buf.s) {
				LM_ERR("PCC_create_media_component: out of memory \
																		when allocating %i bytes in pkg\n",
						len);
				return NULL;
			}
			flowdata_buflen = len;
		}

		flowdata_buf.len = snprintf(flowdata_buf.s, len, permit_in_with_ports,
				proto_nr, ipB->len, ipB->s, int_port_rctp_b, ipA->len, ipA->s,
				int_port_rctp_a);

		flowdata_buf.len = strlen(flowdata_buf.s);
		flow_description4 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
				AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
				IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
				AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, flow_description4);
	}

	int useAnyForIpA = 0;
	int useAnyForIpB = 0;

	if(regex_sdp_ip_prefix_to_maintain_in_fd.len > 0
			&& regex_sdp_ip_prefix_to_maintain_in_fd.s) {
		LM_DBG("regex_sdp_ip_prefix_to_maintain_in_fd is set to: [%.*s] "
			   "therefore we check if we need to replace non matching IPs with "
			   "any\n",
				regex_sdp_ip_prefix_to_maintain_in_fd.len,
				regex_sdp_ip_prefix_to_maintain_in_fd.s);
		regmatch_t pmatch[MAX_MATCH];
		if(reg_match(regex_sdp_ip_prefix_to_maintain_in_fd.s, ipA->s,
				   &(pmatch[0]))) {
			LM_DBG("ipA [%.*s] does not match so will use any instead of ipA",
					ipA->len, ipA->s);
			useAnyForIpA = 1;
		} else {
			LM_DBG("ipA [%.*s] matches regex so will not use any", ipA->len,
					ipA->s);
			useAnyForIpA = 0;
		}
		if(reg_match(regex_sdp_ip_prefix_to_maintain_in_fd.s, ipB->s,
				   &(pmatch[0]))) {
			LM_DBG("ipB [%.*s] does not match so will use any instead of ipB",
					ipB->len, ipB->s);
			useAnyForIpB = 1;
		} else {
			LM_DBG("ipB [%.*s] matches regex so will not use any", ipB->len,
					ipB->s);
			useAnyForIpB = 0;
		}
	}

	if(useAnyForIpA) {
		/*5th (optional replace IP A with ANY) flow*/
		len2 = (permit_out.len + from_s.len + to_s.len
					   + 3 /*for 'any'*/ + ipB->len + 4 + proto_len + portB->len
					   + 1 /*nul terminator*/)
			   * sizeof(char);
		if(!flowdata_buf.s || len < len2) {
			len = len2;
			if(flowdata_buf.s)
				pkg_free(flowdata_buf.s);
			flowdata_buf.s = (char *)pkg_malloc(len);
			if(!flowdata_buf.s) {
				LM_ERR("PCC_create_media_component: out of memory \
																when allocating %i bytes in pkg\n",
						len);
				return NULL;
			}
			flowdata_buflen = len;
		}
		flowdata_buf.len =
				snprintf(flowdata_buf.s, len, permit_out_with_any_as_dst,
						proto_nr, ipB->len, ipB->s, intportB);
		flowdata_buf.len = strlen(flowdata_buf.s);
		flow_description5 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
				AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
				IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
				AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, flow_description5);

		if(include_rtcp_fd) {
			/*7th (optional RTCP replace IP A with ANY) flow*/
			len2 = (permit_out.len + from_s.len + to_s.len
						   + 3 /*for 'any'*/ + ipB->len + 4 + proto_len
						   + port_rtcp_b.len + 1 /*nul terminator*/)
				   * sizeof(char);
			if(!flowdata_buf.s || len < len2) {
				len = len2;
				if(flowdata_buf.s)
					pkg_free(flowdata_buf.s);
				flowdata_buf.s = (char *)pkg_malloc(len);
				if(!flowdata_buf.s) {
					LM_ERR("PCC_create_media_component: out of memory \
																		when allocating %i bytes in pkg\n",
							len);
					return NULL;
				}
				flowdata_buflen = len;
			}
			flowdata_buf.len =
					snprintf(flowdata_buf.s, len, permit_out_with_any_as_dst,
							proto_nr, ipB->len, ipB->s, int_port_rctp_b);
			flowdata_buf.len = strlen(flowdata_buf.s);
			flow_description7 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
					AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
					IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
					AVP_DUPLICATE_DATA);
			cdpb.AAAAddAVPToList(&list, flow_description7);
		}


		/*6th (optional replace IP A with ANY) flow*/
		len2 = (permit_in.len + from_s.len + to_s.len
					   + 3 /*for 'any'*/ + ipB->len + 4 + proto_len + portB->len
					   + 1 /*nul terminator*/)
			   * sizeof(char);
		if(!flowdata_buf.s || len < len2) {
			len = len2;
			if(flowdata_buf.s)
				pkg_free(flowdata_buf.s);
			flowdata_buf.s = (char *)pkg_malloc(len);
			if(!flowdata_buf.s) {
				LM_ERR("PCC_create_media_component: out of memory \
																when allocating %i bytes in pkg\n",
						len);
				return NULL;
			}
			flowdata_buflen = len;
		}
		flowdata_buf.len =
				snprintf(flowdata_buf.s, len, permit_in_with_any_as_src,
						proto_nr, ipB->len, ipB->s, intportB);
		flowdata_buf.len = strlen(flowdata_buf.s);
		flow_description6 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
				AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
				IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
				AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, flow_description6);

		if(include_rtcp_fd) {
			/*8th (optional RTCP replace IP A with ANY) flow*/
			len2 = (permit_in.len + from_s.len + to_s.len
						   + 3 /*for 'any'*/ + ipB->len + 4 + proto_len
						   + port_rtcp_b.len + 1 /*nul terminator*/)
				   * sizeof(char);
			if(!flowdata_buf.s || len < len2) {
				len = len2;
				if(flowdata_buf.s)
					pkg_free(flowdata_buf.s);
				flowdata_buf.s = (char *)pkg_malloc(len);
				if(!flowdata_buf.s) {
					LM_ERR("PCC_create_media_component: out of memory \
																		when allocating %i bytes in pkg\n",
							len);
					return NULL;
				}
				flowdata_buflen = len;
			}
			flowdata_buf.len =
					snprintf(flowdata_buf.s, len, permit_in_with_any_as_src,
							proto_nr, ipB->len, ipB->s, int_port_rctp_b);
			flowdata_buf.len = strlen(flowdata_buf.s);
			flow_description8 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
					AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
					IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
					AVP_DUPLICATE_DATA);
			cdpb.AAAAddAVPToList(&list, flow_description8);
		}


	} else if(useAnyForIpB) {
		/*5th (optional replace IP B with ANY) flow*/
		len2 = (permit_out.len + from_s.len + to_s.len
					   + 3 /*for 'any'*/ + ipA->len + 4 + proto_len + portA->len
					   + 1 /*nul terminator*/)
			   * sizeof(char);
		if(!flowdata_buf.s || len < len2) {
			len = len2;
			if(flowdata_buf.s)
				pkg_free(flowdata_buf.s);
			flowdata_buf.s = (char *)pkg_malloc(len);
			if(!flowdata_buf.s) {
				LM_ERR("PCC_create_media_component: out of memory \
																when allocating %i bytes in pkg\n",
						len);
				return NULL;
			}
			flowdata_buflen = len;
		}
		flowdata_buf.len =
				snprintf(flowdata_buf.s, len, permit_out_with_any_as_dst,
						proto_nr, ipA->len, ipA->s, intportA);
		flow_description5 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
				AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
				IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
				AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, flow_description5);

		if(include_rtcp_fd) {
			/*7th (optional RTCP replace IP B with ANY) flow*/
			len2 = (permit_out.len + from_s.len + to_s.len
						   + 3 /*for 'any'*/ + ipA->len + 4 + proto_len
						   + port_rtcp_a.len + 1 /*nul terminator*/)
				   * sizeof(char);
			if(!flowdata_buf.s || len < len2) {
				len = len2;
				if(flowdata_buf.s)
					pkg_free(flowdata_buf.s);
				flowdata_buf.s = (char *)pkg_malloc(len);
				if(!flowdata_buf.s) {
					LM_ERR("PCC_create_media_component: out of memory \
																		when allocating %i bytes in pkg\n",
							len);
					return NULL;
				}
				flowdata_buflen = len;
			}
			flowdata_buf.len =
					snprintf(flowdata_buf.s, len, permit_out_with_any_as_dst,
							proto_nr, ipA->len, ipA->s, int_port_rctp_a);
			flow_description7 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
					AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
					IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
					AVP_DUPLICATE_DATA);
			cdpb.AAAAddAVPToList(&list, flow_description7);
		}

		/*6th (optional replace IP B with ANY) flow*/
		len2 = (permit_in.len + from_s.len + to_s.len
					   + 3 /*for 'any'*/ + ipA->len + 4 + proto_len + portA->len
					   + 1 /*nul terminator*/)
			   * sizeof(char);
		if(!flowdata_buf.s || len < len2) {
			len = len2;
			if(flowdata_buf.s)
				pkg_free(flowdata_buf.s);
			flowdata_buf.s = (char *)pkg_malloc(len);
			if(!flowdata_buf.s) {
				LM_ERR("PCC_create_media_component: out of memory \
																when allocating %i bytes in pkg\n",
						len);
				return NULL;
			}
			flowdata_buflen = len;
		}
		flowdata_buf.len =
				snprintf(flowdata_buf.s, len, permit_in_with_any_as_src,
						proto_nr, ipA->len, ipA->s, intportA);
		flow_description6 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
				AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
				IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
				AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, flow_description6);

		if(include_rtcp_fd) {
			/*8th (optional RTCP replace IP B with ANY) flow*/
			len2 = (permit_in.len + from_s.len + to_s.len
						   + 3 /*for 'any'*/ + ipA->len + 4 + proto_len
						   + port_rtcp_a.len + 1 /*nul terminator*/)
				   * sizeof(char);
			if(!flowdata_buf.s || len < len2) {
				len = len2;
				if(flowdata_buf.s)
					pkg_free(flowdata_buf.s);
				flowdata_buf.s = (char *)pkg_malloc(len);
				if(!flowdata_buf.s) {
					LM_ERR("PCC_create_media_component: out of memory \
																		when allocating %i bytes in pkg\n",
							len);
					return NULL;
				}
				flowdata_buflen = len;
			}
			flowdata_buf.len =
					snprintf(flowdata_buf.s, len, permit_in_with_any_as_src,
							proto_nr, ipA->len, ipA->s, int_port_rctp_a);
			flow_description8 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
					AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
					IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
					AVP_DUPLICATE_DATA);
			cdpb.AAAAddAVPToList(&list, flow_description8);
		}
	}

	set_4bytes(x, flow_usage_type);
	flow_usage = cdpb.AAACreateAVP(AVP_IMS_Flow_Usage,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
	cdpb.AAAAddAVPToList(&list, flow_usage);

	/*group all AVPS into one big.. and then free the small ones*/

	data = cdpb.AAAGroupAVPS(list);
	cdpb.AAAFreeAVPList(&list);

	//TODO: should free the buffer for the flows in module shutdown....

	return (cdpb.AAACreateAVP(AVP_IMS_Media_Sub_Component,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, data.s, data.len, AVP_FREE_DATA));
}

//just for registration to signalling status much cut down MSC AVP
//see 3GPP TS 29.214 4.4.5

AAA_AVP *rx_create_media_subcomponent_avp_register()
{

	char x[4];

	AAA_AVP *flow_usage = 0;
	AAA_AVP *flow_number = 0;

	str data;
	AAA_AVP_LIST list;
	list.tail = 0;
	list.head = 0;

	//always set to zero for subscription to signalling status
	set_4bytes(x, 0);

	flow_number = cdpb.AAACreateAVP(AVP_IMS_Flow_Number,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
	cdpb.AAAAddAVPToList(&list, flow_number);

	set_4bytes(x, AVP_EPC_Flow_Usage_AF_Signaling);

	flow_usage = cdpb.AAACreateAVP(AVP_IMS_Flow_Usage,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, x, 4, AVP_DUPLICATE_DATA);
	cdpb.AAAAddAVPToList(&list, flow_usage);

	/*group all AVPS into one big.. and then free the small ones*/

	data = cdpb.AAAGroupAVPS(list);

	cdpb.AAAFreeAVPList(&list);

	return (cdpb.AAACreateAVP(AVP_IMS_Media_Sub_Component,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, data.s, data.len, AVP_FREE_DATA));
}

/*
 * Creates a Codec-Data AVP as defined in TS29214 (Rx interface)
 * @param sdp - sdp body of message
 * @param number - the number of the m= line being used
 * @param direction - 0 means uplink offer 1 means uplink answer ,
 * 	2 means downlink offer , 3 downlink answer
 * returns NULL on failure or the pointer to the AAA_AVP on success
 * (this AVP should be freed!)
 */

AAA_AVP *rx_create_codec_data_avp(
		str *raw_sdp_stream, int number, int direction)
{
	str data;
	int l = 0;
	AAA_AVP *result;
	data.len = 0;

	switch(direction) {
		case 0:
			data.len = 13;
			break;
		case 1:
			data.len = 14;
			break;
		case 2:
			data.len = 15;
			break;
		case 3:
			data.len = 16;
			break;
		default:
			break;
	}
	data.len += raw_sdp_stream->len + 1; // 0 Terminated.
	LM_DBG("data.len is calculated %i, sdp-stream has a len of %i\n", data.len,
			raw_sdp_stream->len);
	data.s = (char *)pkg_malloc(data.len);
	memset(data.s, 0, data.len);

	switch(direction) {
		case 0:
			memcpy(data.s, "uplink\noffer\n", 13);
			l = 13;
			break;
		case 1:
			memcpy(data.s, "uplink\nanswer\n", 14);
			l = 14;
			break;
		case 2:
			memcpy(data.s, "downlink\noffer\n", 15);
			l = 15;
			break;
		case 3:
			memcpy(data.s, "downlink\nanswer\n", 16);
			l = 16;
			break;
		default:
			break;
	}
	// LM_DBG("data.s = \"%.*s\"\n", l, data.s);
	memcpy(data.s + l, raw_sdp_stream->s, raw_sdp_stream->len);
	LM_DBG("data.s = \"%.*s\"\n", data.len, data.s);

	result = cdpb.AAACreateAVP(AVP_IMS_Codec_Data,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, data.s, data.len, AVP_DUPLICATE_DATA);

	// Free the buffer:
	pkg_free(data.s);

	return result;
}

/**
 * Creates and adds a Vendor Specific Application ID Group AVP.
 * @param msg - the Diameter message to add to.
 * @param vendor_id - the value for the vendor id AVP
 * @param auth_app_id - the value of the authentication application AVP
 * @returns 1 on success or 0 on error
 */
int rx_add_vendor_specific_application_id_group(
		AAAMessage *msg, uint32_t vendor_id, uint32_t auth_app_id)
{
	return cdp_avp->base.add_Vendor_Specific_Application_Id_Group(
			&(msg->avpList), vendor_id, auth_app_id, 0);
}

/**
 * Returns the Result-Code AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
unsigned int rx_get_abort_cause(AAAMessage *msg)
{
	AAA_AVP *avp = 0;
	unsigned int code = 0;
	//getting abort cause
	avp = cdpb.AAAFindMatchingAVP(msg, msg->avpList.head, AVP_IMS_Abort_Cause,
			IMS_vendor_id_3GPP, AAA_FORWARD_SEARCH);
	if(avp) {
		code = get_4bytes(avp->data.s);
	}
	return code;
}

/**
 * Returns the Result-Code AVP from a Diameter message.
 * or the Experimental-Result-Code if there is no Result-Code , because .. who cares
 * @param msg - the Diameter message
 * @returns 1 if result code found or 0 if error
 */
inline int rx_get_result_code(AAAMessage *msg, unsigned int *data)
{

	AAA_AVP *avp;
	AAA_AVP_LIST list;
	list.head = 0;
	list.tail = 0;
	*data = 0;
	int ret = 0;

	for(avp = msg->avpList.tail; avp; avp = avp->prev) {
		//LOG(L_INFO,"pcc_get_result_code: looping with avp code %i\n",avp->code);
		if(avp->code == AVP_Result_Code) {
			*data = get_4bytes(avp->data.s);
			ret = 1;

		} else if(avp->code == AVP_Experimental_Result) {
			list = cdpb.AAAUngroupAVPS(avp->data);
			for(avp = list.head; avp; avp = avp->next) {
				//LOG(L_CRIT,"in the loop with avp code %i\n",avp->code);
				if(avp->code == AVP_IMS_Experimental_Result_Code) {
					*data = get_4bytes(avp->data.s);
					cdpb.AAAFreeAVPList(&list);
					ret = 1;
					break;
				}
			}
			cdpb.AAAFreeAVPList(&list);
			break; // this has to be here because i have changed the avp!!!
		}
	}
	return ret;
}

/**
 * Creates and adds an Specific-Action AVP
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @return CSCF_RETURN_TRUE on success or 0 on error
 */
inline int rx_add_specific_action_avp(AAAMessage *msg, unsigned int data)
{
	char x[4];
	set_4bytes(x, data);

	return rx_add_avp(msg, x, 4, AVP_IMS_Specific_Action,
			AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, AVP_DUPLICATE_DATA, __FUNCTION__);
}


/**
 * Decode the 3GPP-SGSN-MCC-MNC which has a specific format
 * @param dst - pointer to string where to store result in PKG memory that must be freed after use
 * @param src - pointer to string where to extract the content of the Diameter data buffer without type and length
 * @returns 1 on success and 0 on failure
 */
/* Format of the 3GPP-SGSN-MCC-MNC
     *		Bits
            Octets		8	7	6	5	4	3	2	1
            1		3GPP type = 18
            2		3GPP Length= n
            3		MCC digit1 (UTF-8 encoded character)
            4		MCC digit2 (UTF-8 encoded character)
            5		MCC digit3 (UTF-8 encoded character)
            6		MNC digit1 (UTF-8 encoded character)
            7		MNC digit2 (UTF-8 encoded character)
            8		MNC digit3 if present (UTF-8 encoded character)
    *
    */
int rx_mcc_mnc_to_sip_visited(str *dst, str src)
{
	uint16_t mnc, mcc = 0;

	if(!dst || src.len == 0) {
		return 0;
	}
	mcc = (100 * (src.s[0] - '0') + (10 * (src.s[1] - '0'))
			+ ((src.s[2] - '0')));
	if(mcc >= 999) {
		LOG(L_ERR, "Invalid MCC value\n");
		return 0;
	}
	if(src.len == 5) {
		//extract an MNC of 2 digits
		mnc = ((src.s[4] - '0') + (10 * (src.s[3] - '0')));

	} else if(src.len == 6) {
		// extract an MNC of 3 digits
		mnc = ((src.s[5] - '0') + (10 * (src.s[4] - '0'))
				+ (100 * (src.s[3] - '0')));
	} else {
		LOG(L_ERR, "Invalid 3GPP-SGSN-MCC-MNC length [%d]\n", src.len);
		return 0;
	}
//Now allocate memory in process to store it in the format of P-Visited-Network-Id
#define VISITED_ID_MAX_LENGTH 64
	dst->s = pkg_malloc(sizeof(char) * VISITED_ID_MAX_LENGTH);
	dst->len = snprintf(dst->s, VISITED_ID_MAX_LENGTH,
			"ims.mnc%03d.mcc%03d.3gppnetwork.org", mnc, mcc);
	return 1;
}

int rx_avp_process_3gpp_sgsn_mcc_mnc(AAAMessage *aaa, str *dst)
{
	AAA_AVP *avp;
	if(!aaa)
		return 0;
	for(avp = aaa->avpList.head; avp; avp = avp->next) {
		if(avp->code == AVP_EPC_3GPP_SGSN_MCC_MNC
				&& (avp->flags & AAA_AVP_FLAG_VENDOR_SPECIFIC)
				&& avp->vendorId == IMS_vendor_id_3GPP) {
			break;
		}
	}
	if(!avp)
		return 0;
	/* Sparing this because its UTF8String
	if (!cdp_avp_get_UTF8String(avp, data)) {
		return 0;
	} */
	if(avp->data.len) {
		return rx_mcc_mnc_to_sip_visited(dst, avp->data);
	}
	return 0;
}

/**
 * According to the standard this AVP only comes in the RAR
 * The result includes the cell-id P-Access-Network-Info: 3GPP-E-UTRAN-FDD;utran-cell-id-3gpp=214076FCC4497716;network-provided
 * Tokens are  3GPP-GERAN, 3GPP-UTRAN-FDD, 3GPP-E-UTRAN-FDD, 3GPP-NR-FDD or 3GPP-NR-U-FDD (also TDD versions for everything)
 * for 2G   cgi-3gpp parameter MCC MNC LAC (4 hex dig) CI    (whole thing as ASCII)
 * for 3G utran-cell-id-3gpp with MCC MNC LAC (4 hex dig) CGI (7 hex digits)  (whole thing as ASCII)
 * for 4G utran-cell-id-3gpp with MCC MNC TAC (4 hex dig in EPC 6 hex dig in 5GC)  and ECI (7 hex digits)  (whole thing as ASCII)
 * for 5G utran-cell-id-3gpp with MCC MNC TAC (6 hex digit) NCI (9 hex digits) (whole thing as ascii)
 *
		Bits
	Octets	8	7	6	5	4	3	2	1
	1	Type = 86 (decimal)
	2 to 3	Length = n
	4	Spare	Instance
	5	Extended Macro eNodeB ID	Macro eNodeB ID	LAI	ECGI	TAI	RAI	SAI	CGI
	a to a+6	CGI
	b to b+6	SAI
	c to c+6	RAI
	d to d+4	TAI
	 e to e+6	ECGI
	f to f+4	LAI
	g to g+5	Macro eNodeB ID
	g to g+5	Extended Macro eNodeB ID
	h to (n+4)	These octet(s) is/are present only if explicitly specified

		Bits
	Octets	8	7	6	5	4	3	2	1
	d	MCC digit 2	MCC digit 1
	d+1	MNC digit 3	MCC digit 3
	d+2	MNC digit 2	MNC digit 1
	d+3 to d+4	Tracking Area Code (TAC)

		Bits
	Octets	8	7	6	5	4	3	2	1
	e	MCC digit 2	MCC digit 1
	e+1	MNC digit 3	MCC digit 3
	e+2	MNC digit 2	MNC digit 1
	e+3	Spare	ECI
	e+4 to e+6	ECI (E-UTRAN Cell Identifier)

		Bits
	Octets	8	7	6	5	4	3	2	1
	a	MCC digit 2	MCC digit 1
	a+1	MNC digit 3	MCC digit 3
	a+2	MNC digit 2	MNC digit 1
	a+3 to a+4	Location Area Code (LAC)
	a+5 to a+6	Cell Identity (CI)

BOTH are full hex
 */

int rx_avp_extract_mcc_mnc(str src, int *mcc, int *mnc, int *mnc_digits)
{
	if(src.len < 3 || !src.s || !mcc || !mnc)
		return 0;
	*mcc = (src.s[0] & 0x0F) * 100 + ((src.s[0] & 0xF0) >> 4) * 10
		   + (src.s[1] & 0x0F);
	if(((src.s[1] & 0xF0) >> 4) == 0x0F) {
		//ignore MNC digit 3 means its 0 at the front
		*mnc = (src.s[2] & 0x0F) * 10 + ((src.s[2] & 0xF0) >> 4);
		if(mnc_digits)
			*mnc_digits = 2;
	} else {
		*mnc = (src.s[2] & 0x0F) * 100 + ((src.s[2] & 0xF0) >> 4) * 10
			   + ((src.s[1] & 0xF0) >> 4);
		if(mnc_digits)
			*mnc_digits = 3;
	}
	return 1;
}


char unknown[64];

char *rx_avp_get_access_class(int32_t ip_can_type, int32_t rat_type)
{
	// 3GPP TS 29.212
	switch(rat_type) {
		case 0:
			return "3GPP-WLAN";
		case 1:
			return "VIRTUAL";
		case 2:
			return "TRUSTED-N3GA";
		case 3:
			return "WIRELINE";
		case 4:
			return "WIRELINE-CABLE";
		case 5:
			return "WIRELINE-BBF";
		case 1000:
			return "3GPP-UTRAN";
		case 1001:
			return "3GPP-GERAN";
		case 1002:
			return "3GPP-GAN";
		case 1003:
			return "3GPP-HSPA";
		case 1004:
			return "3GPP-E-UTRAN";
		case 1005:
			return "3GPP-E-UTRAN-NB-IoT";
		case 1006:
			return "3GPP-NR";
		case 1007:
			return "3GPP-E-UTRAN-LTE-M";
		case 1008:
			return "3GPP-NR-U";
		case 1011:
			return "3GPP-E-UTRAN-LEO";
		case 1012:
			return "3GPP-E-UTRAN-MEO";
		case 1013:
			return "3GPP-E-UTRAN-GEO";
		case 1014:
			return "3GPP-E-UTRAN-OTHERSAT";
		case 1021:
			return "3GPP-E-UTRAN-NB-IoT-LEO";
		case 1022:
			return "3GPP-E-UTRAN-NB-IoT-MEO";
		case 1023:
			return "3GPP-E-UTRAN-NB-IoT-GEO";
		case 1024:
			return "3GPP-E-UTRAN-NB-IoT-OTHERSAT";
		case 1031:
			return "3GPP-E-UTRAN-LTE-M-LEO";
		case 1032:
			return "3GPP-E-UTRAN-LTE-M-MEO";
		case 1033:
			return "3GPP-E-UTRAN-LTE-M-GEO";
		case 1034:
			return "3GPP-E-UTRAN-LTE-M-OTHERSAT";
		case 1035:
			return "3GPP-NR-LEO";
		case 1036:
			return "3GPP-NR-MEO";
		case 1037:
			return "3GPP-NR-GEO";
		case 1038:
			return "3GPP-NR-OTHERSAT";
		case 1039:
			return "3GPP-NR-REDCAP";
		case 1040:
			return "3GPP-NR-EREDCAP";
		case 2000:
			return "3GPP2-1X";
		case 2001:
			return "3GPP2-1X-HRPD";
		case 2002:
			return "3GPP2-UMB";
		case 2003:
			return "3GPP2-EHRPD";
	}

	switch(ip_can_type) {
		case -1:
		default:
			snprintf(unknown, 64, "UNKNOWN-IP-CAN-Type-%d-RAT-Type-%d",
					ip_can_type, rat_type);
			return unknown;
		case 1:
			return "DOCSIS";
		case 2:
			return "xDSL";
		case 3:
			return "WiMAX";
		case 4:
			return "3GPP2";
		case 5:
			return "3GPP-EPS";
		case 6:
			return "Non-3GPP-EPS";
		case 7:
			return "FBA";
		case 8:
			return "3GPP-5GS";
		case 9:
			return "Non-3GPP-5GS";
	}
}

#define MAX_PANI_LEN 128

/**
 * This function allocates memory for dst in pkg memory, needs to be freed by the caller
 */
int rx_avp_process_3gpp_user_location_information(AAAMessage *rar, str *dst)
{
	int32_t ip_can_type = -1;
	char *c_access_class = 0;
	int32_t rat_type = -1;

	str data = {0};
	char *p = 0;
	uint16_t length = 0;
	str cgi = {0};
	str sai = {0};
	str rai = {0};
	str tai = {0};
	str ecgi = {0};
	str enodebid = {0};
	str eenodebid = {0};
	str ncgi = {0};

	int mnc = 0, mcc = 0, mnc_digits = 0;
	int mnc2 = 0, mcc2 = 0, mnc_digits2 = 0;
	uint16_t tac = 0;
	uint32_t eci = 0;
	uint32_t macro_enodebid = 0;
	int is_long = 0; // whether the macro_enodebid needs 5 or 6 hex digits
	uint64_t nrci = 0;

	if(!rar || !dst)
		return 0;


	dst->s = pkg_malloc(MAX_PANI_LEN);
	if(!dst->s) {
		LOG(L_ERR, "Could not allocate memory for P-Visited-Network-Id\n");
		return 0;
	}

	// close to, but not really the access-class or access-type
	cdp_avp->epcapp.get_IP_CAN_Type(rar->avpList, &ip_can_type, 0);
	cdp_avp->epcapp.get_RAT_Type(rar->avpList, &rat_type, 0);
	c_access_class = rx_avp_get_access_class(ip_can_type, rat_type);

	if(!cdp_avp->epcapp.get_3GPP_User_Location_Info(rar->avpList, &data, 0)
			|| !data.len) {
		// Fallback to IP-CAN-Type and RAT-Type
		dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
				"%s;network-provided", c_access_class);
		return 1;
	}

	uint8_t type = data.s[0];

	// that's the payload length independent of what it says - first byte is flags
	length = data.len - 1;
	LOG(L_INFO, "Got a 3GPP-User-Location-Info AVP type %d and %d bytes\n",
			type, length);
	p = data.s + 1;
	switch(type) {
		case 0:
			// CGI
			if(length >= 7) {
				cgi.s = p;
				cgi.len = 7;
				p += 7;
				length -= 7;
			}
			break;
		case 1:
			// SAI
			if(length >= 7) {
				sai.s = p;
				sai.len = 7;
				p += 7;
				length -= 7;
			}
			break;
		case 2:
			// RAI
			if(length >= 7) {
				rai.s = p;
				rai.len = 7;
				p += 7;
				length -= 7;
			}
			break;
		case 3 ... 127:
			// spare for future use
			break;
		case 128:
			// TAI
			if(length >= 5) {
				tai.s = p;
				tai.len = 5;
				p += 5;
				length -= 5;
			}
			break;
		case 129:
			// ECGI
			if(length >= 7) {
				ecgi.s = p;
				ecgi.len = 7;
				p += 7;
				length -= 7;
			}
			break;
		case 130:
			// TAI and ECGI
			if(length >= 5) {
				tai.s = p;
				tai.len = 5;
				p += 5;
				length -= 5;
			}
			if(length >= 7) {
				ecgi.s = p;
				ecgi.len = 7;
				p += 7;
				length -= 7;
			}
			break;
		case 131:
			// eNodeB-ID
			if(length >= 6) {
				enodebid.s = p;
				enodebid.len = 6;
				p += 6;
				length -= 6;
			}
			break;
		case 132:
			// TAI and eNodeB-ID
			if(length >= 5) {
				tai.s = p;
				tai.len = 5;
				p += 5;
				length -= 5;
			}
			if(length >= 6) {
				enodebid.s = p;
				enodebid.len = 6;
				p += 6;
				length -= 6;
			}
			break;
		case 133:
			// extended EnodeB-ID
			if(length >= 6) {
				eenodebid.s = p;
				eenodebid.len = 6;
				p += 6;
				length -= 6;
			}
			break;
		case 134:
			// TAI and extended EnodeB-ID
			if(length >= 5) {
				tai.s = p;
				tai.len = 5;
				p += 5;
				length -= 5;
			}
			if(length >= 6) {
				eenodebid.s = p;
				eenodebid.len = 6;
				p += 6;
				length -= 6;
			}
			break;
		case 135:
			// NCGI
			if(length >= 9) {
				ncgi.s = p;
				ncgi.len = 9;
				p += 9;
				length -= 9;
			}
			break;
		case 136:
			// TAI and NCGI
			if(length >= 6) {
				tai.s = p;
				tai.len = 3 + 3; // TAC is 3 bytes in NR
				p += 6;
				length -= 6;
			}
			if(length >= 9) {
				ncgi.s = p;
				ncgi.len = 9;
				p += 9;
				length -= 9;
			}
			break;
		case 137 ... 255:
			// spare for future use
			break;
	}
	if(cgi.len) {
		if(!rx_avp_extract_mcc_mnc(tai, &mcc, &mnc, &mnc_digits)) {
			uint32_t data = tai.s[0] << 16 | tai.s[1] << 8 | tai.s[2];
			LOG(L_ERR, "Could not extract PLMN-ID from CGI 0x%06x\n", data);
			return 0;
		}
		uint16_t lac = ntohs(*(uint16_t *)(cgi.s + 3));
		uint16_t ci = ntohs(*(uint16_t *)(cgi.s + 5));
		if(mnc_digits == 2)
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-GERAN;cgi-3gpp=%03u%02u%04x%04x", mcc, mnc, lac, ci);
		else
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-GERAN;cgi-3gpp=%03u%03u%04x%04x", mcc, mnc, lac, ci);
	}
	if(sai.len) {
		if(!rx_avp_extract_mcc_mnc(tai, &mcc, &mnc, &mnc_digits)) {
			uint32_t data = sai.s[0] << 16 | sai.s[1] << 8 | sai.s[2];
			LOG(L_ERR, "Could not extract PLMN-ID from SAI 0x%06x\n", data);
			return 0;
		}
		uint16_t lac = ntohs(*(uint16_t *)(sai.s + 3));
		uint16_t sac = ntohs(*(uint16_t *)(sai.s + 5));
		if(mnc_digits == 2)
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-UTRAN;utran-sai-3gpp=%03u%02u%04x%04x", mcc, mnc, lac,
					sac);
		else
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-UTRAN;utran-sai-3gpp=%03u%03u%04x%04x", mcc, mnc, lac,
					sac);
	}
	if(rai.len) {
		if(!rx_avp_extract_mcc_mnc(tai, &mcc, &mnc, &mnc_digits)) {
			uint32_t data = rai.s[0] << 16 | rai.s[1] << 8 | rai.s[2];
			LOG(L_ERR, "Could not extract PLMN-ID from RAI 0x%06x\n", data);
			return 0;
		}
		uint16_t lac = ntohs(*(uint16_t *)(rai.s + 3));
		uint16_t rac = ntohs(*(uint16_t *)(rai.s + 5));
		if(mnc_digits == 2)
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-UTRAN;utran-cell-id-3gpp=%03u%02u%04x%04x", mcc, mnc,
					lac, rac);
		else
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-UTRAN;utran-cell-id-3gpp=%03u%03u%04x%04x", mcc, mnc,
					lac, rac);
	}
	if(tai.len) {
		if(!rx_avp_extract_mcc_mnc(tai, &mcc, &mnc, &mnc_digits)) {
			LOG(L_ERR, "Could not extract PLMN-ID from TAI [%.*s]\n", tai.len,
					tai.s);
			return 0;
		}
		if(tai.len == 5) {
			tac = ntohs(*(uint16_t *)(tai.s + 3));
		} else if(tai.len == 6) {
			tac = (((uint8_t)tai.s[3]) << 16) + (((uint8_t)tai.s[4]) << 8)
				  + (((uint8_t)tai.s[5]));
		}
	}
	if(ecgi.len) {
		if(!rx_avp_extract_mcc_mnc(ecgi, &mcc2, &mnc2, &mnc_digits2)) {
			uint32_t data = ecgi.s[0] << 16 | ecgi.s[1] << 8 | ecgi.s[2];
			LOG(L_ERR, "Could not extract PLMN-ID from ECGI 0x%06x\n", data);
			return 0;
		}
		eci = ((ecgi.s[3] & 0x0F) << 24) + (ecgi.s[4] << 16) + (ecgi.s[5] << 8)
			  + ecgi.s[6];
		if(mnc_digits2 == 2)
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-E-UTRAN;utran-cell-id-3gpp=%03u%02u", mcc2, mnc2);
		else
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-E-UTRAN;utran-cell-id-3gpp=%03u%03u", mcc2, mnc2);
		switch(tai.len) {
			case 5:
				dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
						"%04x", tac);
				break;
			case 6:
				dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
						"%06x", tac);
		}
		dst->len += snprintf(
				dst->s + dst->len, MAX_PANI_LEN - dst->len, "%05x", eci);
	}
	if(enodebid.len) {
		if(!rx_avp_extract_mcc_mnc(tai, &mcc2, &mnc2, &mnc_digits2)) {
			uint32_t data =
					enodebid.s[0] << 16 | enodebid.s[1] << 8 | enodebid.s[2];
			LOG(L_ERR, "Could not extract PLMN-ID from eNodeBId 0x%06x\n",
					data);
			return 0;
		}
		macro_enodebid = ((((uint8_t)enodebid.s[3]) & 0x0F) << 16)
						 + (((uint8_t)enodebid.s[4]) << 8)
						 + (((uint8_t)enodebid.s[5]));
	}
	if(eenodebid.len) {
		if(!rx_avp_extract_mcc_mnc(tai, &mcc2, &mnc2, &mnc_digits2)) {
			uint32_t data =
					eenodebid.s[0] << 16 | eenodebid.s[1] << 8 | eenodebid.s[2];
			LOG(L_ERR,
					"Could not extract PLMN-ID from Extended-eNodeBId "
					"0x%06x\n",
					data);
			return 0;
		}
		if(((*(uint8_t *)(eenodebid.s + 3)) & 0x80) == 0) {
			// Long Macro eNodeB Id
			is_long = 1;
			macro_enodebid = ((((uint8_t)eenodebid.s[3]) & 0x1F) << 16)
							 + (((uint8_t)eenodebid.s[4]) << 8)
							 + (((uint8_t)eenodebid.s[5]));
		} else {
			// Short Macro eNodeB Id
			macro_enodebid = ((((uint8_t)eenodebid.s[3]) & 0x03) << 16)
							 + (((uint8_t)eenodebid.s[4]) << 8)
							 + (((uint8_t)eenodebid.s[5]));
		}
	}
	if(enodebid.len || eenodebid.len) {
		if(mnc_digits2 == 2)
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-E-UTRAN;utran-cell-id-3gpp=%03u%02u", mcc2, mnc2);
		else
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-E-UTRAN;utran-cell-id-3gpp=%03u%03u", mcc2, mnc2);
		switch(tai.len) {
			case 5:
				dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
						"%04x", tac);
				break;
			case 6:
				dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
						"%06x", tac);
		}
		if(is_long)
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"%06x", macro_enodebid);
		else
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"%05x", macro_enodebid);
	}
	if(ncgi.len) {
		if(!rx_avp_extract_mcc_mnc(tai, &mcc2, &mnc2, &mnc_digits2)) {
			uint32_t data = ncgi.s[0] << 16 | ncgi.s[1] << 8 | ncgi.s[2];
			LOG(L_ERR, "Could not extract PLMN-ID from NCGI 0x%06x\n", data);
			return 0;
		}
		nrci = ((uint64_t)ncgi.s[3] << 40) + ((uint64_t)ncgi.s[4] << 32)
			   + ((uint64_t)ncgi.s[5] << 24) + ((uint64_t)ncgi.s[6] << 16)
			   + ((uint64_t)ncgi.s[7] << 8) + ncgi.s[8];
		if(mnc_digits2 == 2)
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-NR;utran-cell-id-3gpp=%03u%02u", mcc2, mnc2);
		else
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-NR;utran-cell-id-3gpp=%03u%03u", mcc2, mnc2);
		switch(tai.len) {
			case 5:
				dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
						"%04x", tac);
				break;
			case 6:
				dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
						"%06x", tac);
		}
		dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
				"%012" PRIx64, nrci);
	}

	if(tai.len && !ecgi.len && !enodebid.len && !eenodebid.len && !ncgi.len) {
		if(mnc_digits == 2)
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-E-UTRAN;tai-3gpp=%03u%02u", mcc, mnc);
		else
			dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
					"3GPP-E-UTRAN;tai-3gpp=%03u%03u", mcc, mnc);
		switch(tai.len) {
			case 5:
				dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
						"%04x", tac);
				break;
			case 6:
				dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
						"%06x", tac);
		}
	}

	if(!dst->len) {
		// Fallback to IP-CAN-Type and RAT-Type
		dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
				"%s;network-provided", c_access_class);
	} else {
		dst->len += snprintf(dst->s + dst->len, MAX_PANI_LEN - dst->len,
				";network-provided");
	}

	LOG(L_INFO, "P-Access-Network-Info from Diameter is [%.*s]\n", dst->len,
			dst->s);
	if(!dst->len) {
		ims_str_free(*dst, pkg);
	}
	return (dst->len > 1);
}

/**
 * This adds a feature list with its vendor-id and its list id to the provided list
 * @msglist - pointer to the list of AVPs where to add the Supported Features AVP
 */
int rx_add_supported_features(AAA_AVP_LIST *list, uint32_t vendorid,
		uint32_t feature_list_id, uint32_t feature_list)
{
	// AAA_AVP_LIST list = {0};
	// if(!msglist)
	// 	return 0;

	// cdp_avp->base.add_Vendor_Id(&list, vendorid);
	// cdp_avp->imsapp.add_Feature_List_ID(&list, feature_list_id);
	// cdp_avp->imsapp.add_Feature_List(&list, feature_list);

	return cdp_avp->imsapp.add_Supported_Features_Group(
			list, vendorid, feature_list_id, feature_list);
}

int rx_add_required_access_info(AAAMessage *req, uint32_t data)
{
	if(!req)
		return 0;
	char x[4] = {0}; // User-Location and not TimeZone
	set_4bytes(x, data);

	return rx_add_avp(req, x, 4, 536, AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP, AVP_DUPLICATE_DATA, __FUNCTION__);
}


/**
 * Access-Network-Charging-Identifier ::= < AVP Header: 502 >
					  { Access-Network-Charging-Identifier-Value}
					 *[ Flows ]
 *   Access-Network-Charging-Identifier-Value is a OctetString
 * This function allocates PKG memory for dst
 */
int rx_avp_process_3gpp_access_network_charging_identifier(
		AAAMessage *msg, str *dst)
{
	AAA_AVP_LIST list = {0};
	str anci_value = {0};
	ip_address ancaddr = {0};
	int32_t ipcan_type = 0;
	int i = 0;

	if(!msg || !dst)
		return 0;
	if(cdp_avp->epcapp.get_Access_Network_Charging_Identifier(
			   msg->avpList, &list, 0)) {
		cdp_avp->epcapp.get_Access_Network_Charging_Identifier_Value(
				list, &anci_value, 0);
	}

	cdp_avp->epcapp.get_Access_Network_Charging_Address(
			msg->avpList, &ancaddr, 0);

	cdp_avp->epcapp.get_IP_CAN_Type(msg->avpList, &ipcan_type, 0);

	dst->s = 0;
	dst->len = 0;

	switch(ipcan_type) {
		case 0:
		case 5: // EPS
			// pdngw=ancaddr;eps-info="eps-item=1;eps-sig=no;ecid=%x-of-anci_value"
			dst->s = pkg_malloc(7 + 64 + 11 + 9 + 11 + 6 + 2 * anci_value.len
								+ 32 /* 32 just to be safe */);
			if(!dst->s) {
				LOG(L_ERR, "Could not allocate memory for "
						   "Access-Network-Charging-Identifier\n");
				return 0;
			}
			char c_ip[64];
			switch(ancaddr.ai_family) {
				case AF_INET:
					inet_ntop(AF_INET, &ancaddr.ip.v4, c_ip, 64);
					break;
				case AF_INET6:
					inet_ntop(AF_INET6, &ancaddr.ip.v6, c_ip, 64);
					break;
				default:
					c_ip[0] = 0;
			}
			if(c_ip[0] != 0) {
				dst->len += snprintf(dst->s, 6 + 64, "pdngw=%s", c_ip);
			}
			if(anci_value.len > 0) {
				dst->len += snprintf(dst->s + dst->len, 10 + 9 + 11 + 6 + 2,
						"%seps-info=\"eps-item=1;eps-sig=no;ecid=",
						dst->len > 0 ? ";" : "");
				for(i = 0; i < anci_value.len; i++) {
					dst->len += snprintf(dst->s + dst->len, 3, "%02x",
							((uint8_t *)anci_value.s)[i]);
				}
				dst->len += snprintf(dst->s + dst->len, 2, "\"");
			}
			break;
			// TODO implement also for other IPCAN types
	}

	cdp_avp->data.free_Grouped(&list);

	if(dst->s)
		return 1;
	else
		return 0;
}

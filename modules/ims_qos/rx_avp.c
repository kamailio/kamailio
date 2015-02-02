/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
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
 * History:
 * --------
 *  2011-02-02  initial version (jason.penton)
 */


#include <arpa/inet.h>
#include "../cdp_avp/mod_export.h"
#include "../../modules/dialog_ng/dlg_load.h"
#include "../ims_usrloc_pcscf/usrloc.h"
#include "rx_authdata.h"
#include "rx_avp.h"
#include "mod.h"
#include "../../parser/sdp/sdp_helpr_funcs.h"

#include "../../lib/ims/ims_getters.h"

/**< Structure with pointers to cdp funcs, global variable defined in mod.c  */
extern struct cdp_binds cdpb;
extern cdp_avp_bind_t *cdp_avp;

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
inline int rx_add_avp(AAAMessage *m, char *d, int len, int avp_code,
        int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
        LM_ERR("Rx: :%s: Failed creating avp\n", func);
        return 0;
    }
    if (cdpb.AAAAddAVPToMessage(m, avp, m->avpList.tail) != AAA_ERR_SUCCESS) {
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
static inline int rx_add_avp_list(AAA_AVP_LIST *list, char *d, int len, int avp_code,
        int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
        LM_ERR(":%s: Failed creating avp\n", func);
        return 0;
    }
    if (list->tail) {
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
static inline str rx_get_avp(AAAMessage *msg, int avp_code, int vendor_id,
        const char *func) {
    AAA_AVP *avp;
    str r = {0, 0};

    avp = cdpb.AAAFindMatchingAVP(msg, 0, AVP_Result_Code, 0, 0);
    if (avp == 0) {
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
static char* ip_buf = 0;

int rx_add_framed_ip_avp(AAA_AVP_LIST * list, str ip, uint16_t version) {
    ip_address_prefix ip_adr;
    int ret = 0;

    if (ip.len < 0) return 0;
    if (version == AF_INET) {
        if (ip.len > INET_ADDRSTRLEN)
            goto error;
    } else {
        if (ip.len > INET6_ADDRSTRLEN)
            goto error;
    }
    int len = ip.len + 1;
    if (!ip_buf || ip_buflen < len) {
        if (ip_buf)
                pkg_free(ip_buf);
        ip_buf = (char*)pkg_malloc(len);
        if (!ip_buf) {
	    LM_ERR("rx_add_framed_ip_avp: out of memory \
					    when allocating %i bytes in pkg\n", len);
	    goto error;
        }
        ip_buflen = len;
    }
    memcpy(ip_buf, ip.s, ip.len);
    ip_buf[ip.len] = '\0';
    
    ip_adr.addr.ai_family = version;

    if (version == AF_INET) {

        if (inet_pton(AF_INET, ip_buf, &(ip_adr.addr.ip.v4.s_addr)) != 1) goto error;
        ret = cdp_avp->nasapp.add_Framed_IP_Address(list, ip_adr.addr);
    } else {

        if (inet_pton(AF_INET6, ip_buf, &(ip_adr.addr.ip.v6.s6_addr)) != 1) goto error;
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
inline int rx_add_vendor_specific_appid_avp(AAAMessage *msg, unsigned int vendor_id,
        unsigned int auth_id, unsigned int acct_id) {
    AAA_AVP_LIST list;
    str group;
    char x[4];

    list.head = 0;
    list.tail = 0;

    set_4bytes(x, vendor_id);
    rx_add_avp_list(&list,
            x, 4,
            AVP_Vendor_Id,
            AAA_AVP_FLAG_MANDATORY,
            0,
            AVP_DUPLICATE_DATA,
            __FUNCTION__);

    if (auth_id) {
        set_4bytes(x, auth_id);
        rx_add_avp_list(&list,
                x, 4,
                AVP_Auth_Application_Id,
                AAA_AVP_FLAG_MANDATORY,
                0,
                AVP_DUPLICATE_DATA,
                __FUNCTION__);
    }
    if (acct_id) {
        set_4bytes(x, acct_id);
        rx_add_avp_list(&list,
                x, 4,
                AVP_Acct_Application_Id,
                AAA_AVP_FLAG_MANDATORY,
                0,
                AVP_DUPLICATE_DATA,
                __FUNCTION__);
    }

    group = cdpb.AAAGroupAVPS(list);

    cdpb.AAAFreeAVPList(&list);

    return
    rx_add_avp(msg, group.s, group.len,
            AVP_Vendor_Specific_Application_Id,
            AAA_AVP_FLAG_MANDATORY,
            0,
            AVP_FREE_DATA,
            __FUNCTION__);
}

/**
 * Creates and adds a Destination-Realm AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int rx_add_destination_realm_avp(AAAMessage *msg, str data) {
    return
    rx_add_avp(msg, data.s, data.len,
            AVP_Destination_Realm,
            AAA_AVP_FLAG_MANDATORY,
            0,
            AVP_DUPLICATE_DATA,
            __FUNCTION__);
}

/**
 * Creates and adds an Acct-Application-Id AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @return CSCF_RETURN_TRUE on success or 0 on error
 */
inline int rx_add_auth_application_id_avp(AAAMessage *msg, unsigned int data) {
    char x[4];
    set_4bytes(x, data);

    return
    rx_add_avp(msg, x, 4,
            AVP_Auth_Application_Id,
            AAA_AVP_FLAG_MANDATORY,
            0,
            AVP_DUPLICATE_DATA,
            __FUNCTION__);
}

/*
 * Creates and adds a Subscription_Id AVP
 * @param msg - the Diameter message to add to.
 * @param r - the sip_message to extract the data from.
 * @param tag - originating (0) terminating (1)
 * @return CSCF_RETURN_TRUE on success or 0 on error
 * 
 */

int rx_add_subscription_id_avp(AAAMessage *msg, str identifier, int identifier_type) {
    
    AAA_AVP_LIST list;
    AAA_AVP *type, *data;
    str subscription_id_avp;
    char x[4];
    list.head = 0;
    list.tail = 0;

    set_4bytes(x, identifier_type);

    type = cdpb.AAACreateAVP(AVP_IMS_Subscription_Id_Type,
            AAA_AVP_FLAG_MANDATORY,
            0, x, 4,
            AVP_DUPLICATE_DATA);

    data = cdpb.AAACreateAVP(AVP_IMS_Subscription_Id_Data,
            AAA_AVP_FLAG_MANDATORY,
            0, identifier.s, identifier.len,
            AVP_DUPLICATE_DATA);

    cdpb.AAAAddAVPToList(&list, type);
    cdpb.AAAAddAVPToList(&list, data);

    subscription_id_avp = cdpb.AAAGroupAVPS(list);

    cdpb.AAAFreeAVPList(&list);
    
    return rx_add_avp(msg, subscription_id_avp.s, subscription_id_avp.len, AVP_IMS_Subscription_Id,
            AAA_AVP_FLAG_MANDATORY, 0,
            AVP_FREE_DATA,
            __FUNCTION__);
}

inline unsigned int sdp_b_value(str * payload, char * subtype) {
   char * line;
   unsigned int i;
   str s;
   line = find_sdp_line(payload->s, payload->s + payload->len, 'b');
   while (line != NULL) {
      // b=AS:
      if ((line[2] == subtype[0]) && (line[3] == subtype[1])) {
        LM_DBG("SDP-Line: %.*s\n", 5, line);
        line += 5;
        i = 0;
        while ((line[i] != '\r') && (line[i] != '\n') && ((line +i) <= (payload->s + payload->len))) {
          i++;
	}
        s.s = line;
        s.len = i;
        LM_DBG("value: %.*s\n", s.len, s.s);
        if (str2int(&s, &i) == 0) return i;
        else return 0;
      }
      line = find_next_sdp_line(line, payload->s + payload->len, 'b', NULL);
   }
   return 0;
}

inline int rx_add_media_component_description_avp(AAAMessage *msg, int number, str *media_description, str *ipA, str *portA, str *ipB, str *portB, str *transport,
        str *req_raw_payload, str *rpl_raw_payload, enum dialog_direction dlg_direction) {
    str data;
    AAA_AVP_LIST list;
    AAA_AVP *media_component_number, *media_type;
    AAA_AVP *codec_data1, *codec_data2;
    AAA_AVP * media_sub_component[PCC_Media_Sub_Components];
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
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);

    if (media_component_number != NULL) {
        cdpb.AAAAddAVPToList(&list, media_component_number);
    } else {
        LM_ERR("Unable to create media_component_number AVP");
        return 0;
    }

    /*media-sub-component*/
    if (dlg_direction != DLG_MOBILE_ORIGINATING) {
        media_sub_component[media_sub_component_number] = rx_create_media_subcomponent_avp(number, transport->s, ipA, portA, ipB, portB);
        cdpb.AAAAddAVPToList(&list, media_sub_component[media_sub_component_number]);
    } else {
        media_sub_component[media_sub_component_number] = rx_create_media_subcomponent_avp(number, transport->s, ipB, portB, ipA, portA);
        cdpb.AAAAddAVPToList(&list, media_sub_component[media_sub_component_number]);
    }


    /*media type*/
    if (strncmp(media_description->s, "audio", 5) == 0) {
        type = AVP_IMS_Media_Type_Audio;
    } else if (strncmp(media_description->s, "video", 5) == 0) {
        type = AVP_IMS_Media_Type_Video;
    } else if (strncmp(media_description->s, "data", 4) == 0) {
        type = AVP_IMS_Media_Type_Data;
    } else if (strncmp(media_description->s, "application", 11) == 0) {
        type = AVP_IMS_Media_Type_Application;
    } else if (strncmp(media_description->s, "control", 7) == 0) {
        type = AVP_IMS_Media_Type_Control;
    } else if (strncmp(media_description->s, "text", 4) == 0) {
        type = AVP_IMS_Media_Type_Text;
    } else if (strncmp(media_description->s, "message", 7) == 0) {
        type = AVP_IMS_Media_Type_Message;
    } else {
        type = AVP_IMS_Media_Type_Other;
    }


    set_4bytes(x, type);
    media_type = cdpb.AAACreateAVP(AVP_IMS_Media_Type,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);
    cdpb.AAAAddAVPToList(&list, media_type);

    /*RR and RS*/
    if ((type == AVP_IMS_Media_Type_Audio) || (type == AVP_IMS_Media_Type_Video)) {
	// Get bandwidth from SDP:
        bandwidth = sdp_b_value(req_raw_payload, "AS");
        LM_DBG("Request: got bandwidth %i from b=AS-Line\n", bandwidth);
        // Set default values:
        if ((type == AVP_IMS_Media_Type_Audio) && (bandwidth <= 0))
	  bandwidth = audio_default_bandwidth;
        if ((type == AVP_IMS_Media_Type_Video) && (bandwidth <= 0))
	  bandwidth = video_default_bandwidth;
        
        // According to 3GPP TS 29.213, Rel. 9+, this value is * 1000:
        bandwidth *= 1000; 
  
        // Add AVP
        set_4bytes(x,bandwidth);
	ul_bw = cdpb.AAACreateAVP(AVP_EPC_Max_Requested_Bandwidth_UL,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);
    	cdpb.AAAAddAVPToList(&list, ul_bw);

	// Get bandwidth from SDP:
        bandwidth = sdp_b_value(rpl_raw_payload, "AS");
        LM_DBG("Answer: got bandwidth %i from b=AS-Line\n", bandwidth);
        // Set default values:
        if ((type == AVP_IMS_Media_Type_Audio) && (bandwidth <= 0))
	  bandwidth = audio_default_bandwidth;
        if ((type == AVP_IMS_Media_Type_Video) && (bandwidth <= 0))
	  bandwidth = video_default_bandwidth;

        // According to 3GPP TS 29.213, Rel. 9+, this value is * 1000:
        bandwidth *= 1000; 
        
        // Add AVP
        set_4bytes(x,bandwidth);
	dl_bw = cdpb.AAACreateAVP(AVP_EPC_Max_Requested_Bandwidth_DL,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);
    	cdpb.AAAAddAVPToList(&list, dl_bw);

	// Get A=RS-bandwidth from SDP-Reply:
        bandwidth = sdp_b_value(rpl_raw_payload, "RS");
        LM_DBG("Answer: Got bandwidth %i from b=RS-Line\n", bandwidth);
	if (bandwidth >= 0) {
		// Add AVP
		set_4bytes(x,bandwidth);
		rs_bw = cdpb.AAACreateAVP(AVP_EPC_RS_Bandwidth,
		    AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
		    IMS_vendor_id_3GPP, x, 4,
		    AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, rs_bw);
	}
	// Get A=RS-bandwidth from SDP-Reply:
        bandwidth = sdp_b_value(rpl_raw_payload, "RR");
        LM_DBG("Answer: Got bandwidth %i from b=RR-Line\n", bandwidth);
	if (bandwidth >= 0) {
		// Add AVP
		set_4bytes(x,bandwidth);
		rr_bw = cdpb.AAACreateAVP(AVP_EPC_RR_Bandwidth,
		    AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
		    IMS_vendor_id_3GPP, x, 4,
		    AVP_DUPLICATE_DATA);
		cdpb.AAAAddAVPToList(&list, rr_bw);
	}
    }

    /*codec-data*/

    if (dlg_direction == DLG_MOBILE_ORIGINATING) {
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
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);
    cdpb.AAAAddAVPToList(&list, flow_status);

    /*now group them in one big AVP and free them*/
    data = cdpb.AAAGroupAVPS(list);
    cdpb.AAAFreeAVPList(&list);
    
    
    return rx_add_avp(msg, data.s, data.len, AVP_IMS_Media_Component_Description,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP,
            AVP_FREE_DATA,
            __FUNCTION__);
}


//just for registration to signalling path - much cut down MCD AVP
//See 3GPP TS 29.214 section 4.4.5
inline int rx_add_media_component_description_avp_register(AAAMessage *msg) {
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
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);

    if (media_component_number != NULL) {
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
    return rx_add_avp(msg, data.s, data.len, AVP_IMS_Media_Component_Description,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP,
            AVP_FREE_DATA,
            __FUNCTION__);
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
static char * permit_out_with_ports = "permit out %i from %.*s %u to %.*s %u";
//static char * permit_out_with_ports = "permit out %i from %.*s %u to %.*s %u %s";
static char * permit_in_with_ports = "permit in %i from %.*s %u to %.*s %u";
//static char * permit_in_with_ports = "permit in %i from %.*s %u to %.*s %u %s";

static unsigned int flowdata_buflen = 0;
static str flowdata_buf = {0,0};

AAA_AVP *rx_create_media_subcomponent_avp(int number, char* proto,
        str *ipA, str *portA,
        str *ipB, str *portB) {

    str data;
    int len, len2;
//    str flow_data = {0, 0};
//    str flow_data2 = {0, 0};
    AAA_AVP *flow_description1 = 0, *flow_description2 = 0, *flow_number = 0;
    AAA_AVP *flow_usage = 0;

    AAA_AVP_LIST list;
    list.tail = 0;
    list.head = 0;
    char x[4];
    int proto_int = 0, proto_len = 0;

    proto_int = 17;

    int intportA = atoi(portA->s);
    int intportB = atoi(portB->s);

    len = (permit_out.len + from_s.len + to_s.len + ipB->len + ipA->len + 4 +
            proto_len + portA->len + portB->len + 1/*nul terminator*/) * sizeof (char);

    if (!flowdata_buf.s || flowdata_buflen < len) {
        if (flowdata_buf.s)
                pkg_free(flowdata_buf.s);
        flowdata_buf.s = (char*)pkg_malloc(len);
        if (!flowdata_buf.s) {
                        LM_ERR("PCC_create_media_component: out of memory \
                                                        when allocating %i bytes in pkg\n", len);
                        return NULL ;
        }
        flowdata_buflen = len;
    }

    set_4bytes(x, number);
    
    flow_number = cdpb.AAACreateAVP(AVP_IMS_Flow_Number,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);
    cdpb.AAAAddAVPToList(&list, flow_number);
    
    /*IMS Flow descriptions*/
    /*first flow is the receive flow*/
    flowdata_buf.len = snprintf(flowdata_buf.s, len, permit_out_with_ports, proto_int,
            ipA->len, ipA->s, intportA,
            ipB->len, ipB->s, intportB);

    flowdata_buf.len = strlen(flowdata_buf.s);
    flow_description1 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
            AVP_DUPLICATE_DATA);
    cdpb.AAAAddAVPToList(&list, flow_description1);

        /*second flow*/
    len2 = len - (permit_out.len - permit_in.len) * sizeof (char);
        if (!flowdata_buf.s || flowdata_buflen <= len2) {
                if (flowdata_buf.s)
                        pkg_free(flowdata_buf.s);
                flowdata_buf.s = (char*) pkg_malloc(len2);
                if (!flowdata_buf.s) {
                        LM_ERR("PCC_create_media_component: out of memory \
                                                                when allocating %i bytes in pkg\n", len2);
                        return NULL ;
                }
                flowdata_buflen = len2;
        }

    flowdata_buf.len = snprintf(flowdata_buf.s, len2, permit_in_with_ports, proto_int,
            ipB->len, ipB->s, intportB,
            ipA->len, ipA->s, intportA);

    flowdata_buf.len = strlen(flowdata_buf.s);
    flow_description2 = cdpb.AAACreateAVP(AVP_IMS_Flow_Description,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, flowdata_buf.s, flowdata_buf.len,
            AVP_DUPLICATE_DATA);
    cdpb.AAAAddAVPToList(&list, flow_description2);

    set_4bytes(x, AVP_EPC_Flow_Usage_No_Information);
    flow_usage = cdpb.AAACreateAVP(AVP_IMS_Flow_Usage,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);
    cdpb.AAAAddAVPToList(&list, flow_usage);

    /*group all AVPS into one big.. and then free the small ones*/

    data = cdpb.AAAGroupAVPS(list);
    cdpb.AAAFreeAVPList(&list);

    //TODO: should free the buffer for the flows in module shutdown....

    return (cdpb.AAACreateAVP(AVP_IMS_Media_Sub_Component,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, data.s, data.len,
            AVP_FREE_DATA));
}

//just for registration to signalling status much cut down MSC AVP
//see 3GPP TS 29.214 4.4.5

AAA_AVP *rx_create_media_subcomponent_avp_register() {

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
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);
    cdpb.AAAAddAVPToList(&list, flow_number);

    set_4bytes(x, AVP_EPC_Flow_Usage_AF_Signaling);
    
    flow_usage = cdpb.AAACreateAVP(AVP_IMS_Flow_Usage,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, x, 4,
            AVP_DUPLICATE_DATA);
    cdpb.AAAAddAVPToList(&list, flow_usage);

    /*group all AVPS into one big.. and then free the small ones*/

    data = cdpb.AAAGroupAVPS(list);

    cdpb.AAAFreeAVPList(&list);

    return (cdpb.AAACreateAVP(AVP_IMS_Media_Sub_Component,
            AAA_AVP_FLAG_MANDATORY | AAA_AVP_FLAG_VENDOR_SPECIFIC,
            IMS_vendor_id_3GPP, data.s, data.len,
            AVP_FREE_DATA));
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

AAA_AVP* rx_create_codec_data_avp(str *raw_sdp_stream, int number, int direction) {
    str data;
    int l = 0;
    AAA_AVP* result;
    data.len = 0;

    switch (direction) {
        case 0: data.len = 13;
            break;
        case 1: data.len = 14;
            break;
        case 2: data.len = 15;
            break;
        case 3: data.len = 16;
            break;
        default:
            break;
    }
    data.len += raw_sdp_stream->len + 1; // 0 Terminated.
    LM_DBG("data.len is calculated %i, sdp-stream has a len of %i\n", data.len, raw_sdp_stream->len);
    data.s = (char*)pkg_malloc(data.len);
    memset(data.s, 0, data.len);
    
    switch (direction) {
        case 0: memcpy(data.s, "uplink\noffer\n", 13);
		l = 13;
            break;
        case 1: memcpy(data.s, "uplink\nanswer\n", 14);
		l = 14;
            break;
        case 2: memcpy(data.s, "downlink\noffer\n", 15);
		l = 15;
            break;
        case 3: memcpy(data.s, "downlink\nanswer\n", 16);
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
            IMS_vendor_id_3GPP, data.s, data.len,
            AVP_DUPLICATE_DATA);
 
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
int rx_add_vendor_specific_application_id_group(AAAMessage * msg, uint32_t vendor_id, uint32_t auth_app_id) {
    return cdp_avp->base.add_Vendor_Specific_Application_Id_Group(&(msg->avpList), vendor_id, auth_app_id, 0);
}

/**
 * Returns the Result-Code AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
unsigned int rx_get_abort_cause(AAAMessage *msg) {
    AAA_AVP *avp = 0;
    unsigned int code = 0;
    //getting abort cause
    avp = cdpb.AAAFindMatchingAVP(msg, msg->avpList.head, AVP_IMS_Abort_Cause, IMS_vendor_id_3GPP, AAA_FORWARD_SEARCH);
    if (avp) {
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
inline int rx_get_result_code(AAAMessage *msg, unsigned int *data) {

    AAA_AVP *avp;
    AAA_AVP_LIST list;
    list.head = 0;
    list.tail = 0;
    *data = 0;
    int ret = 0;

    for (avp = msg->avpList.tail; avp; avp = avp->prev) {
        //LOG(L_INFO,"pcc_get_result_code: looping with avp code %i\n",avp->code);
        if (avp->code == AVP_Result_Code) {
            *data = get_4bytes(avp->data.s);
            ret = 1;

        } else if (avp->code == AVP_Experimental_Result) {
            list = cdpb.AAAUngroupAVPS(avp->data);
            for (avp = list.head; avp; avp = avp->next) {
                //LOG(L_CRIT,"in the loop with avp code %i\n",avp->code);
                if (avp->code == AVP_IMS_Experimental_Result_Code) {
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



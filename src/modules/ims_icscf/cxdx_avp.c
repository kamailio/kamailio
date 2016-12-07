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
 */

#include "../cdp/cdp_load.h"
#include "../../modules/tm/tm_load.h"
#include "cxdx_avp.h"



static str s_empty = {0, 0};

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
static inline int cxdx_add_avp(AAAMessage *m,char *d,int len,int avp_code,
	int flags,int vendorid,int data_do,const char *func)
{
	AAA_AVP *avp;
	if (vendorid!=0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
	avp = cdpb.AAACreateAVP(avp_code,flags,vendorid,d,len,data_do);
	if (!avp) {
		LM_ERR("%s: Failed creating avp\n",func);
		return 0;
	}
	if (cdpb.AAAAddAVPToMessage(m,avp,m->avpList.tail)!=AAA_ERR_SUCCESS) {
		LM_ERR("%s: Failed adding avp to message\n",func);
		cdpb.AAAFreeAVP(&avp);
		return 0;
	}
	return 1;
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
static inline int cxdx_add_avp_list(AAA_AVP_LIST *list,char *d,int len,int avp_code,
	int flags,int vendorid,int data_do,const char *func)
{
	AAA_AVP *avp;
	if (vendorid!=0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
	avp = cdpb.AAACreateAVP(avp_code,flags,vendorid,d,len,data_do);
	if (!avp) {
		LM_ERR("%s: Failed creating avp\n",func);
		return 0;
	}
	if (list->tail) {
		avp->prev=list->tail;
		avp->next=0;
		list->tail->next = avp;
		list->tail=avp;
	} else {
		list->head = avp;
		list->tail = avp;
		avp->next=0;
		avp->prev=0;
	}

	return 1;
}

/**
 * Returns the value of a certain AVP from a Diameter message.
 * @param m - Diameter message to look into
 * @param avp_code - the code to search for
 * @param vendorid - the value of the vendor id to look for or 0 if none
 * @param func - the name of the calling function, for debugging purposes
 * @returns the str with the payload on success or an empty string on failure
 */
static inline str cxdx_get_avp(AAAMessage *msg,int avp_code,int vendor_id,
							const char *func)
{
	AAA_AVP *avp;
	str r={0,0};

	avp = cdpb.AAAFindMatchingAVP(msg,0,avp_code,vendor_id,0);
	if (avp==0){
		LM_INFO("%s: Failed finding avp\n",func);
		return r;
	}
	else
		return avp->data;
}

/**
 * Creates and adds a Destination-Realm AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_destination_realm(AAAMessage *msg,str data)
{
	return
	cxdx_add_avp(msg,data.s,data.len,
		AVP_Destination_Realm,
		AAA_AVP_FLAG_MANDATORY,
		0,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}


/**
 * Creates and adds a Vendor-Specifig-Application-ID AVP.
 * @param msg - the Diameter message to add to.
 * @param vendor_id - the value of the vendor_id,
 * @param auth_id - the authorization application id
 * @param acct_id - the accounting application id
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_vendor_specific_appid(AAAMessage *msg,unsigned int vendor_id,
	unsigned int auth_id,unsigned int acct_id)
{
	AAA_AVP_LIST list;
	str group;
	char x[4];

	list.head=0;list.tail=0;

	set_4bytes(x,vendor_id);
	cxdx_add_avp_list(&list,
		x,4,
		AVP_Vendor_Id,
		AAA_AVP_FLAG_MANDATORY,
		0,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);

	if (auth_id) {
		set_4bytes(x,auth_id);
		cxdx_add_avp_list(&list,
			x,4,
			AVP_Auth_Application_Id,
			AAA_AVP_FLAG_MANDATORY,
			0,
			AVP_DUPLICATE_DATA,
			__FUNCTION__);
	}
	if (acct_id) {
		set_4bytes(x,acct_id);
		cxdx_add_avp_list(&list,
			x,4,
			AVP_Acct_Application_Id,
			AAA_AVP_FLAG_MANDATORY,
			0,
			AVP_DUPLICATE_DATA,
			__FUNCTION__);
	}

	group = cdpb.AAAGroupAVPS(list);

	cdpb.AAAFreeAVPList(&list);

	return
	cxdx_add_avp(msg,group.s,group.len,
		AVP_Vendor_Specific_Application_Id,
		AAA_AVP_FLAG_MANDATORY,
		0,
		AVP_FREE_DATA,
		__FUNCTION__);
}

/**
 * Creates and adds a Auth-Session-State AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_auth_session_state(AAAMessage *msg,unsigned int data)
{
	char x[4];
	set_4bytes(x,data);
	return
	cxdx_add_avp(msg,x,4,
		AVP_Auth_Session_State,
		AAA_AVP_FLAG_MANDATORY,
		0,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

/**
 * Creates and adds a User-Name AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_user_name(AAAMessage *msg,str data)
{
	return
	cxdx_add_avp(msg,data.s,data.len,
		AVP_User_Name,
		AAA_AVP_FLAG_MANDATORY,
		0,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

/**
 * Creates and adds a Public Identity AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_public_identity(AAAMessage *msg,str data)
{
	return
	cxdx_add_avp(msg,data.s,data.len,
		AVP_IMS_Public_Identity,
		AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
		IMS_vendor_id_3GPP,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

/**
 * Creates and adds a Visited-Network-ID AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_visited_network_id(AAAMessage *msg,str data)
{
	return
	cxdx_add_avp(msg,data.s,data.len,
		AVP_IMS_Visited_Network_Identifier,
		AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
		IMS_vendor_id_3GPP,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

/**
 * Creates and adds a UAR-Flags AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_UAR_flags(AAAMessage *msg, unsigned int sos_reg)
{

	char x[4];
	/* optional AVP*/
	if(!sos_reg)
		return 1;

	set_4bytes(x, AVP_IMS_UAR_Flags_Emergency_Registration);
	return
	cxdx_add_avp(msg,x,4,
		AVP_IMS_UAR_Flags,
		AAA_AVP_FLAG_VENDOR_SPECIFIC,
		IMS_vendor_id_3GPP,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);

}
/**
 * Creates and adds a Authorization-Type AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_authorization_type(AAAMessage *msg,unsigned int data)
{
	char x[4];
	set_4bytes(x,data);
	return
	cxdx_add_avp(msg,x,4,
		AVP_IMS_User_Authorization_Type,
		AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
		IMS_vendor_id_3GPP,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

/**
 * Returns the Result-Code AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline int cxdx_get_result_code(AAAMessage *msg, int *data)
{
	str s;
	s = cxdx_get_avp(msg,
		AVP_Result_Code,
		0,
		__FUNCTION__);
	if (!s.s) return 0;
	*data = get_4bytes(s.s);
	return 1;
}

/**
 * Returns the Experimental-Result-Code AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline int cxdx_get_experimental_result_code(AAAMessage *msg, int *data)
{
	AAA_AVP_LIST list;
	AAA_AVP *avp;
	str grp;
	grp = cxdx_get_avp(msg,
		AVP_IMS_Experimental_Result,
		0,
		__FUNCTION__);
	if (!grp.s) return 0;

	list = cdpb.AAAUngroupAVPS(grp);

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_Experimental_Result_Code,0,0);
	if (!avp||!avp->data.s) {
		cdpb.AAAFreeAVPList(&list);
		return 0;
	}

	*data = get_4bytes(avp->data.s);
	cdpb.AAAFreeAVPList(&list);

	return 1;
}

/**
 * Returns the Server-Name AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline str cxdx_get_server_name(AAAMessage *msg)
{
	return cxdx_get_avp(msg,
		AVP_IMS_Server_Name,
		IMS_vendor_id_3GPP,
		__FUNCTION__);
}

/**
 * Returns the Capabilities from the grouped AVP from a Diameter message.
 * @param msg - the Diameter message
 * @param m - array to be filled with the retrieved mandatory capabilities
 * @param m_cnt - size of the array above to be filled
 * @param o - array to be filled with the retrieved optional capabilities
 * @param o_cnt - size of the array above to be filled
 * @returns 1 on success 0 on fail
 */
inline int cxdx_get_capabilities(AAAMessage *msg,int **m,int *m_cnt,int **o,int *o_cnt,
	str **p,int *p_cnt)
{
	AAA_AVP_LIST list;
	AAA_AVP *avp;
	str grp;
	grp = cxdx_get_avp(msg,
		AVP_IMS_Server_Capabilities,
		IMS_vendor_id_3GPP,
		__FUNCTION__);
	if (!grp.s) return 0;

	list = cdpb.AAAUngroupAVPS(grp);

	avp = list.head;
	*m_cnt=0;
	*o_cnt=0;
	*p_cnt=0;
	while(avp){
		if (avp->code == AVP_IMS_Mandatory_Capability) (*m_cnt)++;
		if (avp->code == AVP_IMS_Optional_Capability) (*o_cnt)++;
		if (avp->code == AVP_IMS_Server_Name) (*p_cnt)++;
		avp = avp->next;
	}
	avp = list.head;
	*m=shm_malloc(sizeof(int)*(*m_cnt));
	if (!*m){
		LM_ERR("cannot allocated %lx bytes of shm.\n",
			sizeof(int)*(*m_cnt));
		goto error;
	}
	*o=shm_malloc(sizeof(int)*(*o_cnt));
	if (!*o){
		LM_ERR("cannot allocated %lx bytes of shm.\n",
			sizeof(int)*(*o_cnt));
		goto error;
	}
	*p=shm_malloc(sizeof(str)*(*p_cnt));
	if (!*p){
		LM_ERR("cannot allocated %lx bytes of shm.\n",
			sizeof(str)*(*p_cnt));
		goto error;
	}

	*m_cnt=0;
	*o_cnt=0;
	*p_cnt=0;
	while(avp){
		if (avp->code == AVP_IMS_Mandatory_Capability)
			(*m)[(*m_cnt)++]=get_4bytes(avp->data.s);
		if (avp->code == AVP_IMS_Optional_Capability)
			(*o)[(*o_cnt)++]=get_4bytes(avp->data.s);
		if (avp->code == AVP_IMS_Server_Name)
			(*p)[(*p_cnt)++]=avp->data;
		avp = avp->next;
	}
	cdpb.AAAFreeAVPList(&list);
	return 1;

error:
	cdpb.AAAFreeAVPList(&list);
	if (*m) shm_free(*m);
	if (*o) shm_free(*o);
	if (*p) shm_free(*p);
	*m_cnt=0;
	*o_cnt=0;
	*p_cnt=0;
	return 0;
}

/**
 * Creates and adds a SIP-Number-Auth-Items AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_sip_number_auth_items(AAAMessage *msg,unsigned int data)
{
	char x[4];
	set_4bytes(x,data);
	return
	cxdx_add_avp(msg,x,4,
		AVP_IMS_SIP_Number_Auth_Items,
		AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
		IMS_vendor_id_3GPP,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

/**
 * Creates and adds a SIP-Auth-Data-Item AVP.
 * @param msg - the Diameter message to add to.
 * @param auth_scheme - the value for the authorization scheme AVP
 * @param auth - the value for the authorization AVP
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_sip_auth_data_item_request(AAAMessage *msg, str auth_scheme, str auth, str username, str realm,str method, str server_name)
{
	AAA_AVP_LIST list;
	str group;
	str etsi_authorization = {0, 0};
	list.head=0;list.tail=0;

	if (auth_scheme.len){
		cxdx_add_avp_list(&list,
			auth_scheme.s,auth_scheme.len,
			AVP_IMS_SIP_Authentication_Scheme,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}
	if (auth.len){
		cxdx_add_avp_list(&list,
			auth.s,auth.len,
			AVP_IMS_SIP_Authorization,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_3GPP,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}

	if (server_name.len)
	{
		etsi_authorization = cxdx_ETSI_sip_authorization(username, realm, s_empty, server_name, s_empty, s_empty, method, s_empty);

		if (etsi_authorization.len){
			cxdx_add_avp_list(&list,
				etsi_authorization.s,etsi_authorization.len,
				AVP_ETSI_SIP_Authorization,
				AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
				IMS_vendor_id_ETSI,
				AVP_FREE_DATA,
				__FUNCTION__);
		}
	}

	if (!list.head) return 1;
	group = cdpb.AAAGroupAVPS(list);

	cdpb.AAAFreeAVPList(&list);

	return
	cxdx_add_avp(msg,group.s,group.len,
		AVP_IMS_SIP_Auth_Data_Item,
		AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
		IMS_vendor_id_3GPP,
		AVP_FREE_DATA,
		__FUNCTION__);
}

/**
 * Creates and adds a Server-Name AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_server_name(AAAMessage *msg,str data)
{
	return
	cxdx_add_avp(msg,data.s,data.len,
		AVP_IMS_Server_Name,
		AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
		IMS_vendor_id_3GPP,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

/**
 * Returns the SIP-Number-Auth-Items AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the number or 0 on error
 */
inline int cxdx_get_sip_number_auth_items(AAAMessage *msg, int *data)
{
	str s;
	s = cxdx_get_avp(msg,
		AVP_IMS_SIP_Number_Auth_Items,
		IMS_vendor_id_3GPP,
		__FUNCTION__);
	if (!s.s) return 0;
	*data = get_4bytes(s.s);
	return 1;
}

/**
 * Returns the Auth-Data-Item from a Diameter answer message.
 * @param msg - the Diameter message
 * @param auth_date - the string to fill with the authorization data
 * @param item_number - the int to fill with the item number
 * @param auth_scheme - the string to fill with the authentication scheme data
 * @param authenticate - the string to fill with the authenticate data
 * @param authorization - the string to fill with the authorization data
 * @param ck - the string to fill with the cipher key
 * @param ik - the string to fill with the integrity key
 * @returns the AVP payload on success or an empty string on error
 */
int cxdx_get_auth_data_item_answer(AAAMessage *msg, AAA_AVP **auth_data,
	int *item_number,str *auth_scheme,str *authenticate,str *authorization,
	str *ck,str *ik,
	str *ip,
	str *ha1, str *response_auth, str *digest_realm,
	str *line_identifier)
{
	AAA_AVP_LIST list;
	AAA_AVP_LIST list2;
	AAA_AVP *avp;
	AAA_AVP *avp2;
	str grp;
	ha1->s = 0; ha1->len = 0;
	*auth_data = cdpb.AAAFindMatchingAVP(msg,*auth_data,AVP_IMS_SIP_Auth_Data_Item,
		IMS_vendor_id_3GPP,0);
	if (!*auth_data) return 0;

	grp = (*auth_data)->data;
	if (!grp.len) return 0;

	list = cdpb.AAAUngroupAVPS(grp);

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_SIP_Item_Number,
		IMS_vendor_id_3GPP,0);
	if (!avp||!avp->data.len==4) *item_number=0;
	else *item_number = get_4bytes(avp->data.s);

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_SIP_Authentication_Scheme,
		IMS_vendor_id_3GPP,0);
	if (!avp||!avp->data.s) {auth_scheme->s=0;auth_scheme->len=0;}
	else *auth_scheme = avp->data;

	/* Early-IMS */
	ip->s=0;ip->len=0;
	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_Framed_IP_Address,0,0);
	if (avp && avp->data.s){
		if (avp->data.len!=4){
			LM_ERR("Invalid length of AVP Framed IP Address (should be 4 for AVP_Framed_IP_Address) >%d.\n",
				avp->data.len);
		}
		ip->len = 4;
		ip->s = avp->data.s;
	} else {
		avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_Framed_IPv6_Prefix,0,0);
		if (avp && avp->data.s){
			if (avp->data.len==0){
				LM_ERR("Invalid length of AVP Framed IPv6 Prefix (should be >0 for AVP_Framed_IPv6_Prefix) >%d.\n",
					avp->data.len);
			}
			ip->len = avp->data.len;
			ip->s = avp->data.s;
		}
	}

	/* Digest */

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_CableLabs_SIP_Digest_Authenticate,IMS_vendor_id_CableLabs,0);
	if (avp  && avp->data.s)
	{
		list2 = cdpb.AAAUngroupAVPS(avp->data);

		avp2 = cdpb.AAAFindMatchingAVPList(list2,0,AVP_CableLabs_Digest_HA1,IMS_vendor_id_CableLabs,0);
		if (!avp2||!avp2->data.s) {
			ha1->s = 0; ha1->len = 0;
			cdpb.AAAFreeAVPList(&list2);
			return 0;
		}
		*ha1 = avp2->data;
		cdpb.AAAFreeAVPList(&list2);
	}


	/* SIP Digest */

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_SIP_Digest_Authenticate,IMS_vendor_id_3GPP,0);
	if (avp  && avp->data.s)
	{
		list2 = cdpb.AAAUngroupAVPS(avp->data);

		avp2 = cdpb.AAAFindMatchingAVPList(list2,0,AVP_IMS_Digest_HA1,0,0);
		if (!avp2||!avp2->data.s) {
			ha1->s = 0; ha1->len = 0;
			cdpb.AAAFreeAVPList(&list2);
			return 0;
		}
		*ha1 = avp2->data;
		cdpb.AAAFreeAVPList(&list2);
	}


	/* AKA, MD5 */
	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_SIP_Authenticate,
		IMS_vendor_id_3GPP,0);
	if (!avp||!avp->data.s) {authenticate->s=0;authenticate->len=0;}
	else *authenticate = avp->data;

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_SIP_Authorization,
		IMS_vendor_id_3GPP,0);
	if (!avp||!avp->data.s) {authorization->s=0;authorization->len=0;}
	else *authorization = avp->data;

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_Confidentiality_Key,
		IMS_vendor_id_3GPP,0);
	if (!avp||!avp->data.s) {ck->s=0;ck->len=0;}
	else *ck = avp->data;

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_Integrity_Key,
		IMS_vendor_id_3GPP,0);
	if (!avp||!avp->data.s) {ik->s=0;ik->len=0;}
	else *ik = avp->data;

	/* ETSI HTTP Digest */

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_ETSI_SIP_Authenticate,IMS_vendor_id_ETSI,0);
	if (avp  && avp->data.s)
	{
		list2 = cdpb.AAAUngroupAVPS(avp->data);

		avp2 = cdpb.AAAFindMatchingAVPList(list2,0,AVP_ETSI_Digest_Realm, IMS_vendor_id_ETSI,0);
		if (!avp2||!avp2->data.s) {
			digest_realm->s=0;digest_realm->len=0;
			cdpb.AAAFreeAVPList(&list2);
			return 0;
		}
		*digest_realm = avp2->data;

		avp2 = cdpb.AAAFindMatchingAVPList(list2,0,AVP_ETSI_Digest_Nonce, IMS_vendor_id_ETSI,0);
		if (!avp2||!avp2->data.s) {
			authenticate->s=0;authenticate->len=0;
			cdpb.AAAFreeAVPList(&list2);
			return 0;
		}
		*authenticate = avp2->data;

		avp2 = cdpb.AAAFindMatchingAVPList(list2,0,AVP_ETSI_Digest_HA1, IMS_vendor_id_ETSI,0);
		if (!avp2||!avp2->data.s) {
			ha1->s = 0; ha1->len = 0;
			cdpb.AAAFreeAVPList(&list2);
			return 0;
		}
		*ha1 = avp2->data;

		cdpb.AAAFreeAVPList(&list2);
	}

	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_ETSI_SIP_Authentication_Info,IMS_vendor_id_ETSI,0);
	if (avp  && avp->data.s)
	{
		list2 = cdpb.AAAUngroupAVPS(avp->data);

		avp2 = cdpb.AAAFindMatchingAVPList(list2,0,AVP_ETSI_Digest_Response_Auth, IMS_vendor_id_ETSI,0);
		if (!avp2||!avp2->data.s) {
			response_auth->s=0;response_auth->len=0;
			cdpb.AAAFreeAVPList(&list2);
			return 0;
		}
		*response_auth = avp2->data;
		cdpb.AAAFreeAVPList(&list2);
	}
	else
	{
		response_auth->s=0;response_auth->len=0;
	}

	/* NASS Bundled */
	avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_ETSI_Line_Identifier, IMS_vendor_id_ETSI,0);
	if (!avp||!avp->data.s) {line_identifier->s=0;line_identifier->len=0;}
	else *line_identifier = avp->data;

	cdpb.AAAFreeAVPList(&list);
	return 1;
}

/**
 * Creates and adds a ETSI_sip_authorization AVP.
 * @param username - UserName
 * @param realm - Realm
 * @param nonce - Nonce
 * @param URI - URI
 * @param response - Response
 * @param algoritm - Algorithm
 * @param method - Method
 * @param hash - Enitity-Body-Hash
 * @returns grouped str on success
 */
str cxdx_ETSI_sip_authorization(str username, str realm, str nonce, str URI, str response, str algorithm, str method, str hash)
{
	AAA_AVP_LIST list;
	str group = {0, 0};
	list.head=0;list.tail=0;

	if (username.len){
		cxdx_add_avp_list(&list,
			username.s,username.len,
			AVP_ETSI_Digest_Username,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_ETSI,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}

	if (realm.len){
		cxdx_add_avp_list(&list,
			realm.s,realm.len,
			AVP_ETSI_Digest_Realm,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_ETSI,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}

	if (nonce.len){
		cxdx_add_avp_list(&list,
			nonce.s,nonce.len,
			AVP_ETSI_Digest_Nonce,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_ETSI,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}

	if (URI.len){
		cxdx_add_avp_list(&list,
			URI.s,URI.len,
			AVP_ETSI_Digest_URI,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_ETSI,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}

	if (response.len){
		cxdx_add_avp_list(&list,
			response.s,response.len,
			AVP_ETSI_Digest_Response,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_ETSI,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}

	if (algorithm.len){
		cxdx_add_avp_list(&list,
			algorithm.s,algorithm.len,
			AVP_ETSI_Digest_Algorithm,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_ETSI,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}

	if (method.len){
		cxdx_add_avp_list(&list,
			method.s,method.len,
			AVP_ETSI_Digest_Method,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_ETSI,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}

	if (hash.len){
		cxdx_add_avp_list(&list,
			hash.s,hash.len,
			AVP_ETSI_Digest_Entity_Body_Hash,
			AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
			IMS_vendor_id_ETSI,
			AVP_DONT_FREE_DATA,
			__FUNCTION__);
	}

	if (!list.head) return group;
	group = cdpb.AAAGroupAVPS(list);

	cdpb.AAAFreeAVPList(&list);

	return group;
}

/**
 * Returns the User-Data from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */

inline str cxdx_get_user_data(AAAMessage *msg)
{
	return cxdx_get_avp(msg,
		AVP_IMS_User_Data_Cx,
		IMS_vendor_id_3GPP,
		__FUNCTION__);
}

/**
 * Returns the Charging-Information from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline int cxdx_get_charging_info(AAAMessage *msg,str *ccf1,str *ccf2,str *ecf1,str *ecf2)
{
	AAA_AVP_LIST list;
	AAA_AVP *avp;
	str grp;
	grp = cxdx_get_avp(msg,
		AVP_IMS_Charging_Information,
		IMS_vendor_id_3GPP,
		__FUNCTION__);
	if (!grp.s) return 0;

	list = cdpb.AAAUngroupAVPS(grp);

	if (ccf1){
		avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_Primary_Charging_Collection_Function_Name,
			IMS_vendor_id_3GPP,0);
		if (avp) *ccf1 = avp->data;
	}
	if (ccf2){
		avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_Secondary_Charging_Collection_Function_Name,
			IMS_vendor_id_3GPP,0);
		if (avp) *ccf2 = avp->data;
	}
	if (ecf1){
		avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_Primary_Event_Charging_Function_Name,
			IMS_vendor_id_3GPP,0);
		if (avp) *ecf1 = avp->data;
	}
	if (ecf2){
		avp = cdpb.AAAFindMatchingAVPList(list,0,AVP_IMS_Secondary_Event_Charging_Function_Name,
			IMS_vendor_id_3GPP,0);
		if (avp) *ecf2 = avp->data;
	}

	cdpb.AAAFreeAVPList(&list);
	return 1;

}

/**
 * Creates and adds a Server-Assignment-Type AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_server_assignment_type(AAAMessage *msg,unsigned int data)
{
	char x[4];
	set_4bytes(x,data);
	return
	cxdx_add_avp(msg,x,4,
		AVP_IMS_Server_Assignment_Type,
		AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
		IMS_vendor_id_3GPP,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

/**
 * Creates and adds Userdata-Available AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_userdata_available(AAAMessage *msg,unsigned int data)
{
	char x[4];
	set_4bytes(x,data);
	return
	cxdx_add_avp(msg,x,4,
		AVP_IMS_User_Data_Already_Available,
		AAA_AVP_FLAG_MANDATORY|AAA_AVP_FLAG_VENDOR_SPECIFIC,
		IMS_vendor_id_3GPP,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

/**
 * Finds out the next Public-Identity AVP from a Diameter message.
 * @param msg - the Diameter message
 * @param pos - position to resume search or NULL if to start from the first AVP
 * @param avp_code - the code of the AVP to look for
 * @param vendor_id - the vendor id of the AVP to look for
 * @param func - the name of the calling function for debugging purposes
 * @returns the AVP payload on success or an empty string on error
 */
inline AAA_AVP* cxdx_get_next_public_identity(AAAMessage *msg,AAA_AVP* pos,int avp_code,int vendor_id,const char *func)
{
	AAA_AVP *avp;

	avp = cdpb.AAAFindMatchingAVP(msg,pos,avp_code,vendor_id,0);
	if (avp==0){
		LM_DBG("INFO:%s: Failed finding avp\n",func);
		return avp;
	}
	else
		return avp;
}

/**
 * Returns the User-Name AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline str cxdx_get_user_name(AAAMessage *msg)
{
	return cxdx_get_avp(msg,
		AVP_User_Name,
		0,
		__FUNCTION__);
}

/**
 * Creates and adds a Result-Code AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_result_code(AAAMessage *msg,unsigned int data)
{
	char x[4];
	set_4bytes(x,data);
	return
	cxdx_add_avp(msg,x,4,
		AVP_Result_Code,
		AAA_AVP_FLAG_MANDATORY,
		0,
		AVP_DUPLICATE_DATA,
		__FUNCTION__);
}

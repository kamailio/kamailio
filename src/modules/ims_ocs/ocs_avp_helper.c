/*
 *
 * Copyright (C) 2015 ng-voice GmbH, Carsten Bock, carsten@ng-voice.com
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
#include "../cdp_avp/cdp_avp_mod.h"
#include "ocs_avp_helper.h"
#include "ims_ocs_mod.h"

/**
 * Returns the value of a certain AVP from a Diameter message.
 * @param m - Diameter message to look into
 * @param avp_code - the code to search for
 * @param vendorid - the value of the vendor id to look for or 0 if none
 * @param func - the name of the calling function, for debugging purposes
 * @returns the str with the payload on success or an empty string on failure
 */
str get_avp(AAAMessage *msg,int avp_code,int vendor_id, const char *func) {
	AAA_AVP *avp;
	str r={0,0};
	avp = cdpb.AAAFindMatchingAVP(msg,0,avp_code,vendor_id,0);
	if (avp==0) {
		LM_INFO("%s: Failed finding avp\n",func);
		return r;
	} else return avp->data;
}

str getSession(AAAMessage *msg) {
	AAA_AVP *avp;
	str r={0,0};
	avp = cdpb.AAAFindMatchingAVP(msg,0,AVP_Session_Id,0,0);
	if (avp==0) {
		LM_INFO("Failed finding avp\n");
		return r;
	} else return avp->data;
}

int getRecordNummber(AAAMessage *msg) {
	AAA_AVP *avp;
	avp = cdpb.AAAFindMatchingAVP(msg,0,AVP_Accounting_Record_Number,0,0);
	if (avp==0) {
		LM_DBG("Failed finding avp\n");
		return 0;
	} else return get_4bytes(avp->data.s);
}

str getSubscriptionId1(AAAMessage *msg, int * type) {
	AAA_AVP *avp, *avp_type, *avp_value;
	str r={0,0};
	avp = cdpb.AAAFindMatchingAVP(msg,0,AVP_Subscription_Id,0,0);
	AAA_AVP_LIST list;
	list = cdp_avp->cdp->AAAUngroupAVPS(avp->data);
	avp_type = cdpb.AAAFindMatchingAVPList(list, list.head, AVP_Subscription_Id_Type, 0, 0);
	avp_value = cdpb.AAAFindMatchingAVPList(list, list.head, AVP_Subscription_Id_Data, 0, 0);
	
	if (avp_type) {
		*type = get_4bytes(avp_type->data.s);
	} else {
		LM_DBG("Failed finding type\n");
		*type = 0;
	}
	if (avp_value==0) {
		LM_DBG("Failed finding value\n");
	} else {
		r = avp_value->data;
	}
	cdpb.AAAFreeAVPList(&list);
	return r;
}

int isOrig(AAAMessage *msg) {
	AAA_AVP *service, *imsinfo, *role;
	AAA_AVP_LIST list, list2;

	int result = 0;
	service = cdpb.AAAFindMatchingAVP(msg,0,AVP_IMS_Service_Information,IMS_vendor_id_3GPP,0);
	if (service) {
		list = cdp_avp->cdp->AAAUngroupAVPS(service->data);
		imsinfo = cdpb.AAAFindMatchingAVPList(list, list.head, AVP_IMS_IMS_Information, IMS_vendor_id_3GPP, 0);
		if (imsinfo) {
			list2 = cdp_avp->cdp->AAAUngroupAVPS(imsinfo->data);
			role = cdpb.AAAFindMatchingAVPList(list2, list2.head, AVP_IMS_Role_Of_Node, IMS_vendor_id_3GPP, 0);
			if (role) {
				result = get_4bytes(role->data.s);
			}
			cdpb.AAAFreeAVPList(&list2);
		} else {
			LM_DBG("Failed finding IMS-Info\n");
		}
		cdpb.AAAFreeAVPList(&list);
	} else {
		LM_DBG("Failed finding Service-Info\n");
	}

	return result;
}

str getCalledParty(AAAMessage *msg) {
	AAA_AVP *service, *imsinfo, *calledparty;
	str r={0,0};
	service = cdpb.AAAFindMatchingAVP(msg,0,AVP_IMS_Service_Information,IMS_vendor_id_3GPP,0);
	if (service) {
		AAA_AVP_LIST list, list2;
		list = cdp_avp->cdp->AAAUngroupAVPS(service->data);
		imsinfo = cdpb.AAAFindMatchingAVPList(list, list.head, AVP_IMS_IMS_Information, IMS_vendor_id_3GPP, 0);
		if (imsinfo) {
			list2 = cdp_avp->cdp->AAAUngroupAVPS(imsinfo->data);
			calledparty = cdpb.AAAFindMatchingAVPList(list2, list2.head, AVP_IMS_Called_Party_Address, IMS_vendor_id_3GPP, 0);
			if (calledparty) {
				r = calledparty->data;
			} else {
				LM_DBG("Failed finding value\n");
			}
			cdpb.AAAFreeAVPList(&list2);
		} else {
			LM_DBG("Failed finding IMS-Info\n");
		}
		cdpb.AAAFreeAVPList(&list);
	} else {
		LM_DBG("Failed finding Service-Info\n");
	}

	return r;
}

str getAccessNetwork(AAAMessage *msg) {
	AAA_AVP *service, *imsinfo, *access;
	str r={0,0};
	service = cdpb.AAAFindMatchingAVP(msg,0,AVP_IMS_Service_Information,IMS_vendor_id_3GPP,0);
	if (service) {
		AAA_AVP_LIST list, list2;
		list = cdp_avp->cdp->AAAUngroupAVPS(service->data);
		imsinfo = cdpb.AAAFindMatchingAVPList(list, list.head, AVP_IMS_IMS_Information, IMS_vendor_id_3GPP, 0);
		if (imsinfo) {
			list2 = cdp_avp->cdp->AAAUngroupAVPS(imsinfo->data);
			access = cdpb.AAAFindMatchingAVPList(list2, list2.head, AVP_IMS_Access_Network_Information, IMS_vendor_id_3GPP, 0);
			if (access) {
				r = access->data;
			} else {
				LM_DBG("Failed finding value\n");
			}
			cdpb.AAAFreeAVPList(&list2);
		} else {
			LM_DBG("Failed finding IMS-Info\n");
		}
		cdpb.AAAFreeAVPList(&list);
	} else {
		LM_DBG("Failed finding Service-Info\n");
	}
	return r;
}

int getUnits(AAAMessage *msg, int * used, int * service, int * group) {
	AAA_AVP *avp, *req_units, *value, *used_units, *service_avp, *rating_group;
	int units = 0;
	*used = 0;
	*service = 0;
	avp = cdpb.AAAFindMatchingAVP(msg,0,AVP_Multiple_Services_Credit_Control,0,0);
	if (avp) {
		AAA_AVP_LIST list, list2;
		list = cdp_avp->cdp->AAAUngroupAVPS(avp->data);
		req_units = cdpb.AAAFindMatchingAVPList(list, list.head, AVP_Requested_Service_Unit, 0, 0);
		if (req_units) {
			list2 = cdp_avp->cdp->AAAUngroupAVPS(req_units->data);
			value = cdpb.AAAFindMatchingAVPList(list2, list2.head, AVP_CC_Time, 0, 0);
			cdpb.AAAFreeAVPList(&list2);
			if (value)
				units = get_4bytes(value->data.s);
			cdpb.AAAFreeAVPList(&list2);
		}
		service_avp = cdpb.AAAFindMatchingAVPList(list, list.head, AVP_Service_Identifier, 0, 0);
		if (service_avp) {
			*service = get_4bytes(service_avp->data.s);
		}
		used_units = cdpb.AAAFindMatchingAVPList(list, list.head, AVP_Used_Service_Unit, 0, 0);
		if (used_units) {
			list2 = cdp_avp->cdp->AAAUngroupAVPS(used_units->data);
			value = cdpb.AAAFindMatchingAVPList(list2, list2.head, AVP_CC_Time, 0, 0);
			if (value)
				*used = get_4bytes(value->data.s);
			cdpb.AAAFreeAVPList(&list2);
		}
		rating_group = cdpb.AAAFindMatchingAVPList(list, list.head, AVP_Rating_Group, 0, 0);
		if (rating_group) {
			*group = get_4bytes(rating_group->data.s);
		}
		cdpb.AAAFreeAVPList(&list);
	}
	if (*service == 0) LM_WARN("Failed to get service-identifier\n");
	return units;
}


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
int ocs_add_avp(AAAMessage *m, char *d, int len, int avp_code, int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
        LM_ERR("%s: Failed creating avp\n", func);
        return 0;
    }
    if (cdpb.AAAAddAVPToMessage(m, avp, m->avpList.tail) != AAA_ERR_SUCCESS) {
        LM_ERR("%s: Failed adding avp to message\n", func);
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
int ocs_add_avp_list(AAA_AVP_LIST *list, char *d, int len, int avp_code,
	int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
	LM_ERR("%s: Failed creating avp\n", func);
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

    return 1;
}

int ocs_build_answer(AAAMessage *ccr, AAAMessage *cca, int result_code, int granted_units, int final_unit) {
	AAA_AVP *avp;
	AAA_AVP_LIST granted_list, mscc_list, final_list;
	char x[4];
	str granted_group, mscc_group, final_group;
	int service, group, used;
	
	if (!ccr) return 0;
   	if (!cca) return 0;

	// Set some basic data: Application-ID, CCR-Type, CCR-Request-Number
	set_4bytes(x, IMS_Ro);
	ocs_add_avp(cca, x, 4, AVP_Acct_Application_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
	
	avp = cdpb.AAAFindMatchingAVP(ccr,0,AVP_IMS_CCR_Type,0,0);
	ocs_add_avp(cca, avp->data.s, avp->data.len, AVP_IMS_CCR_Type, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

	avp = cdpb.AAAFindMatchingAVP(ccr,0,AVP_CC_Request_Number,0,0);
	ocs_add_avp(cca, avp->data.s, avp->data.len,AVP_CC_Request_Number, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
	
	// Result-Code:
	set_4bytes(x, result_code);
	ocs_add_avp(cca, x, 4, AVP_Result_Code, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

	if (result_code == DIAMETER_SUCCESS) {
		granted_list.head = 0;
		granted_list.tail = 0;
		final_list.head = 0;
		final_list.tail = 0;
		mscc_list.head = 0;
		mscc_list.tail = 0;

		getUnits(ccr, &used, &service, &group);
	
		set_4bytes(x, group);
		ocs_add_avp_list(&mscc_list, x, 4, AVP_Rating_Group, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

		set_4bytes(x, service);
		ocs_add_avp_list(&mscc_list, x, 4, AVP_Service_Identifier, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

		if (granted_units > 0) {
			set_4bytes(x, granted_units);
			ocs_add_avp_list(&granted_list, x, 4, AVP_CC_Time, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
			granted_group = cdpb.AAAGroupAVPS(granted_list);
			cdpb.AAAFreeAVPList(&granted_list);
			ocs_add_avp_list(&mscc_list, granted_group.s, granted_group.len, AVP_Granted_Service_Unit, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
		}

		// Result-Code:
		set_4bytes(x, result_code);
		ocs_add_avp_list(&mscc_list, x, 4, AVP_Result_Code, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

		set_4bytes(x, 86400);
		ocs_add_avp_list(&mscc_list, x, 4, AVP_Validity_Time, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);


		if (final_unit > 0) {
			set_4bytes(x, 0);
			ocs_add_avp_list(&final_list, x, 4, AVP_Final_Unit_Action, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
			final_group = cdpb.AAAGroupAVPS(final_list);
			cdpb.AAAFreeAVPList(&final_list);
			ocs_add_avp_list(&mscc_list, final_group.s, final_group.len, AVP_Final_Unit_Indication, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
		}


		mscc_group = cdpb.AAAGroupAVPS(mscc_list);
		cdpb.AAAFreeAVPList(&mscc_list);

		return ocs_add_avp(cca, mscc_group.s, mscc_group.len, AVP_Multiple_Services_Credit_Control, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
	}
	return 1;
}

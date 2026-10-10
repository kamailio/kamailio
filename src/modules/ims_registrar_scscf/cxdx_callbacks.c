/**
 * Callback functions for RTR/PPR from the HSS
 *
 * Copyright (c) 2012 Carsten Bock, ng-voice GmbH
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
 * As a special exception, the copyright holders of the new contributions
 * permit linking those contributions with the OpenSSL library and
 * distributing the resulting combined work. The GNU General Public
 * License applies to all other code.
 */

#include "cxdx_callbacks.h"
#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "cxdx_avp.h"
#include "registrar_notify.h"
#include "userdata_parser.h"
#include "../cdp/diameter_ims_code_result.h"
#include <string.h>

extern struct cdp_binds cdpb;
extern usrloc_api_t ul;
extern char *domain;

#define CX_MAX_IDENTITIES 256
#define CX_MAX_USER_DATA (1024 * 1024)

static AAAMessage *cx_answer(AAAMessage *request, int code, int experimental)
{
	AAAMessage *answer = cdpb.AAACreateResponse(request);
	if(!answer)
		return 0;
	if(!cxdx_add_vendor_specific_appid(answer, IMS_vendor_id_3GPP, IMS_Cx, 0)
			|| !cxdx_add_auth_session_state(answer, 1)
			|| !(experimental ? cxdx_add_experimental_result(answer, code)
							  : cxdx_add_result_code(answer, code))) {
		cdpb.AAAFreeMessage(&answer);
		return 0;
	}
	return answer;
}

static AAA_AVP *cx_avp(AAAMessage *request, int code, int vendor)
{
	return cdpb.AAAFindMatchingAVP(request, 0, code, vendor, 0);
}

static AAAMessage *cx_failed_answer(
		AAAMessage *request, int result, int code, int vendor)
{
	AAAMessage *answer = cx_answer(request, result, 0);
	AAA_AVP *avp = cx_avp(request, code, vendor);
	char zero[4] = {0};
	str value = avp ? avp->data : (str){zero, 0};
	if(!avp && code == AVP_Auth_Session_State)
		value.len = 4;
	if(answer && !cxdx_add_failed_avp(answer, code, vendor, value))
		cdpb.AAAFreeMessage(&answer);
	return answer;
}

static int cx_validate_request(AAAMessage *request, int *failed)
{
	int codes[] = {AVP_Session_Id, AVP_User_Name, AVP_Origin_Host,
			AVP_Origin_Realm, AVP_Destination_Host, AVP_Destination_Realm,
			AVP_Vendor_Specific_Application_Id, AVP_Auth_Session_State};
	AAA_AVP *avp;
	unsigned int i;
	for(i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
		*failed = codes[i];
		avp = cx_avp(request, codes[i], 0);
		if(!avp || !avp->data.len)
			return DIAMETER_MISSING_AVP;
		if(cdpb.AAAGetNextAVP(avp)
				&& cdpb.AAAFindMatchingAVP(
						request, cdpb.AAAGetNextAVP(avp), codes[i], 0, 0))
			return DIAMETER_AVP_OCCURS_TOO_MANY_TIMES;
	}
	*failed = AVP_Auth_Session_State;
	avp = cx_avp(request, AVP_Auth_Session_State, 0);
	if(avp->data.len != 4 || get_4bytes(avp->data.s) != 1)
		return DIAMETER_INVALID_AVP_VALUE;
	{
		AAA_AVP_LIST list;
		int vendors = 0, apps = 0, valid = 1;
		*failed = AVP_Vendor_Specific_Application_Id;
		avp = cx_avp(request, *failed, 0);
		list = cdpb.AAAUngroupAVPS(avp->data);
		for(avp = list.head; avp; avp = avp->next) {
			if(avp->code == AVP_Vendor_Id && avp->vendorId == 0) {
				vendors++;
				if(avp->data.len != 4
						|| get_4bytes(avp->data.s) != IMS_vendor_id_3GPP)
					valid = 0;
			} else if(avp->code == AVP_Auth_Application_Id
					  && avp->vendorId == 0) {
				apps++;
				if(avp->data.len != 4 || get_4bytes(avp->data.s) != IMS_Cx)
					valid = 0;
			}
		}
		cdpb.AAAFreeAVPList(&list);
		if(!valid || vendors != 1 || apps != 1)
			return DIAMETER_INVALID_AVP_VALUE;
	}
	return 0;
}

static AAAMessage *cx_operation_answer(AAAMessage *request, int result)
{
	if(result == -1)
		return cx_answer(request, RC_IMS_DIAMETER_ERROR_USER_UNKNOWN, 1);
	if(result == -3)
		return cx_answer(
				request, RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
	return cx_answer(request,
			result == 0 ? DIAMETER_SUCCESS : DIAMETER_UNABLE_TO_COMPLY, 0);
}

AAAMessage *cxdx_process_ppr(AAAMessage *request)
{
	AAA_AVP *data;
	ims_subscription *profile;
	udomain_t *d;
	str private_id;
	int failed, result = cx_validate_request(request, &failed);
	if(result)
		return cx_failed_answer(request, result, failed, 0);
	/* This receiver implements the sender's User-Data/iFC update subset. */
	if(cx_avp(request, AVP_IMS_Charging_Information, IMS_vendor_id_3GPP)
			|| cx_avp(request, AVP_IMS_SIP_Auth_Data_Item, IMS_vendor_id_3GPP))
		return cx_answer(
				request, RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
	data = cx_avp(request, AVP_IMS_User_Data_Cx, IMS_vendor_id_3GPP);
	if(!data || !data->data.len)
		return cx_failed_answer(request, DIAMETER_MISSING_AVP,
				AVP_IMS_User_Data_Cx, IMS_vendor_id_3GPP);
	if(data->data.len > CX_MAX_USER_DATA)
		return cx_answer(request, RC_IMS_DIAMETER_ERROR_TOO_MUCH_DATA, 1);
	if(cdpb.AAAGetNextAVP(data)
			&& cdpb.AAAFindMatchingAVP(request, cdpb.AAAGetNextAVP(data),
					AVP_IMS_User_Data_Cx, IMS_vendor_id_3GPP, 0))
		return cx_answer(request, DIAMETER_AVP_OCCURS_TOO_MANY_TIMES, 0);
	/* Cx profiles are self-contained XML; never resolve an external DTD. */
	if(memchr(data->data.s, 0, data->data.len))
		return cx_answer(
				request, RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
	{
		int i;
		for(i = 0; i < data->data.len; i++)
			if((i + 9 <= data->data.len
					   && !memcmp(data->data.s + i, "<!DOCTYPE", 9))
					|| (i + 8 <= data->data.len
							&& !memcmp(data->data.s + i, "<!ENTITY", 8)))
				return cx_answer(request,
						RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
	}
	profile = parse_user_data(data->data);
	if(!profile)
		return cx_answer(
				request, RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
	profile->ref_count = 1; /* owned here until the cache takes record refs */
	private_id = cxdx_get_user_name(request);
	if(profile->private_identity.len != private_id.len
			|| memcmp(profile->private_identity.s, private_id.s,
					private_id.len)) {
		ul.unref_subscription(profile);
		return cx_answer(
				request, RC_IMS_DIAMETER_ERROR_IDENTITIES_DONT_MATCH, 1);
	}
	if(ul.register_udomain(domain, &d) < 0) {
		ul.unref_subscription(profile);
		return cx_answer(request, DIAMETER_UNABLE_TO_COMPLY, 0);
	}
	result = ul.cx_replace_profile(d, profile);
	ul.unref_subscription(profile);
	LM_INFO("Cx PPR profile installation result=%d\n", result);
	return cx_operation_answer(request, result);
}

static void cx_notify(impurecord_t *r, ucontact_t *c, int reason)
{
	/* RTR marks the contact deleted before the asynchronous XML builder runs.
	 * Preserve its URI in the notification's copied deregistration list. */
	notify_subscribers(r, c, &c->c, 1,
			reason == 2 ? IMS_REGISTRAR_CONTACT_DEACTIVATED
						: IMS_REGISTRAR_CONTACT_DEREGISTERED);
}

AAAMessage *cxdx_process_rtr(AAAMessage *request)
{
	AAA_AVP *avp, *reason_avp;
	AAA_AVP_LIST reason_list;
	str identities[CX_MAX_IDENTITIES], private_id;
	udomain_t *d;
	int count = 0, reason = -1, reason_count = 0;
	int failed, result = cx_validate_request(request, &failed);
	if(result)
		return cx_failed_answer(request, result, failed, 0);
	if(cx_avp(request, AVP_IMS_Associated_Identities, IMS_vendor_id_3GPP))
		return cx_answer(request, DIAMETER_UNABLE_TO_COMPLY, 0);
	reason_avp =
			cx_avp(request, AVP_IMS_Deregistration_Reason, IMS_vendor_id_3GPP);
	if(!reason_avp)
		return cx_failed_answer(request, DIAMETER_MISSING_AVP,
				AVP_IMS_Deregistration_Reason, IMS_vendor_id_3GPP);
	reason_list = cdpb.AAAUngroupAVPS(reason_avp->data);
	for(avp = reason_list.head; avp; avp = avp->next) {
		if(avp->code == AVP_IMS_Reason_Code
				&& avp->vendorId == IMS_vendor_id_3GPP) {
			reason_count++;
			if(avp->data.len == 4)
				reason = get_4bytes(avp->data.s);
		}
	}
	cdpb.AAAFreeAVPList(&reason_list);
	if(reason_count != 1 || reason < 0 || reason > 3)
		return cx_answer(request, DIAMETER_INVALID_AVP_VALUE, 0);
	avp = cx_avp(request, AVP_IMS_Public_Identity, IMS_vendor_id_3GPP);
	while(avp) {
		if(count == CX_MAX_IDENTITIES)
			return cx_answer(request, DIAMETER_RESOURCES_EXCEEDED, 0);
		if(!avp->data.len)
			return cx_answer(request, DIAMETER_INVALID_AVP_VALUE, 0);
		identities[count++] = avp->data;
		avp = cdpb.AAAGetNextAVP(avp);
		if(avp)
			avp = cdpb.AAAFindMatchingAVP(request, avp, AVP_IMS_Public_Identity,
					IMS_vendor_id_3GPP, 0);
	}
	/* The existing PyHSS sender supplies all identities in the selected IRS. */
	if(!count)
		return cx_answer(request, DIAMETER_UNABLE_TO_COMPLY, 0);
	private_id = cxdx_get_user_name(request);
	if(ul.register_udomain(domain, &d) < 0)
		return cx_answer(request, DIAMETER_UNABLE_TO_COMPLY, 0);
	result = ul.cx_deregister(
			d, &private_id, identities, count, reason, cx_notify);
	LM_INFO("Cx RTR cache cleanup reason=%d public_ids=%d result=%d\n", reason,
			count, result);
	/* Unsupported RTR scope is a base processing error, not a PPR data error. */
	if(result == -3)
		return cx_answer(request, DIAMETER_UNABLE_TO_COMPLY, 0);
	return cx_operation_answer(request, result);
}

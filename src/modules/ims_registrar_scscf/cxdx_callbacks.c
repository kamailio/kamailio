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
 */

#include "cxdx_callbacks.h"
#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "../ims_auth/api.h"
#include "cxdx_avp.h"
#include "registrar_notify.h"
#include "userdata_parser.h"
#include "../cdp/diameter_ims_code_result.h"
#include <string.h>

extern struct cdp_binds cdpb;
extern usrloc_api_t ul;
extern char *domain;

/* Local request/work limits; these are not limits specified by 3GPP. */
#define CX_MAX_IDENTITIES 256
#define CX_MAX_USER_DATA (1024 * 1024)

static AAAMessage *cx_answer(AAAMessage *request, int code, int experimental)
{
	/* TS 29.228 tables 6.1.3.2/6.2.2.2 distinguish Diameter base results
	 * from Cx experimental results; TS 29.229 6.1.10/6.1.12 define the AVPs. */
	AAAMessage *answer = cdpb.AAACreateResponse(request);
	if(!answer) {
		LM_ERR("Cx command %u: failed to create Diameter answer\n",
				(unsigned int)request->commandCode);
		return 0;
	}
	if(!cxdx_add_vendor_specific_appid(answer, IMS_vendor_id_3GPP, IMS_Cx, 0)
			|| !cxdx_add_auth_session_state(answer, 1)
			|| !(experimental ? cxdx_add_experimental_result(answer, code)
							  : cxdx_add_result_code(answer, code))) {
		LM_ERR("Cx command %u: failed to add required answer AVPs\n",
				(unsigned int)request->commandCode);
		cdpb.AAAFreeMessage(&answer);
		return 0;
	}
	/* Construction only: the handler may add AVPs before CDP sends it. */
	LM_INFO("Cx command %u answer prepared: %s=%d endtoend=%u hopbyhop=%u\n",
			(unsigned int)request->commandCode,
			experimental ? "Experimental-Result" : "Result-Code", code,
			(unsigned int)request->endtoendId,
			(unsigned int)request->hopbyhopId);
	return answer;
}

static AAA_AVP *cx_avp(AAAMessage *request, int code, int vendor)
{
	return cdpb.AAAFindMatchingAVP(request, 0, code, vendor, 0);
}

static AAAMessage *cx_failed_answer_avp(
		AAAMessage *request, int result, int code, int vendor, AAA_AVP *avp)
{
	const char *problem = result == DIAMETER_AVP_UNSUPPORTED ? "unsupported"
						  : result == DIAMETER_MISSING_AVP	 ? "missing"
						  : result == DIAMETER_AVP_OCCURS_TOO_MANY_TIMES
								  ? "duplicate"
								  : "invalid";
	AAAMessage *answer = cx_answer(request, result, 0);
	char zero[4] = {0};
	str value;
	if(avp && result == DIAMETER_AVP_OCCURS_TOO_MANY_TIMES) {
		AAA_AVP *duplicate = cdpb.AAAFindMatchingAVP(
				request, cdpb.AAAGetNextAVP(avp), code, vendor, 0);
		if(duplicate)
			avp = duplicate;
	}
	value = avp ? avp->data : (str){zero, 0};
	LM_WARN("Cx command %u rejected: %s AVP %d (vendor=%d)\n",
			(unsigned int)request->commandCode, problem, code, vendor);
	if(!avp && (code == AVP_Auth_Session_State || code == AVP_IMS_Reason_Code))
		value.len = 4;
	if(answer
			&& !cxdx_add_failed_avp(answer, code, vendor, value,
					avp ? avp->flags : AAA_AVP_FLAG_MANDATORY)) {
		LM_ERR("Cx command %u: failed to add Failed-AVP for AVP %d\n",
				(unsigned int)request->commandCode, code);
		cdpb.AAAFreeMessage(&answer);
	}
	return answer;
}

static AAAMessage *cx_failed_answer(
		AAAMessage *request, int result, int code, int vendor)
{
	return cx_failed_answer_avp(
			request, result, code, vendor, cx_avp(request, code, vendor));
}

static int cx_group_valid(str data)
{
	unsigned int pos = 0, len, header;
	while(pos < data.len) {
		if(data.len - pos < 8)
			return 0;
		header = (data.s[pos + 4] & AAA_AVP_FLAG_VENDOR_SPECIFIC) ? 12 : 8;
		len = get_3bytes(data.s + pos + 5);
		if(len < header || len > data.len - pos)
			return 0;
		len = (len + 3) & ~3U;
		if(len > data.len - pos)
			return 0;
		pos += len;
	}
	return 1;
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
		if(i < 6 && memchr(avp->data.s, 0, avp->data.len))
			return DIAMETER_INVALID_AVP_VALUE;
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
		if(!cx_group_valid(avp->data))
			return DIAMETER_INVALID_AVP_VALUE;
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

/* Ignore optional extensions, but never acknowledge an unknown mandatory
 * AVP as if its requested semantics had been applied (RFC 6733 4.1). */
static AAA_AVP *cx_unsupported_mandatory(AAAMessage *request)
{
	AAA_AVP *avp;
	for(avp = request->avpList.head; avp; avp = avp->next) {
		if(!(avp->flags & AAA_AVP_FLAG_MANDATORY))
			continue;
		if(avp->vendorId == 0) {
			switch(avp->code) {
				case AVP_Session_Id:
				case AVP_User_Name:
				case AVP_Origin_Host:
				case AVP_Origin_Realm:
				case AVP_Destination_Host:
				case AVP_Destination_Realm:
				case AVP_Vendor_Specific_Application_Id:
				case AVP_Auth_Session_State:
				case AVP_Origin_State_Id:
				case AVP_Proxy_Info:
				case AVP_Route_Record:
					continue;
			}
		} else if(avp->vendorId == IMS_vendor_id_3GPP) {
			if(request->commandCode == IMS_PPR
					&& (avp->code == AVP_IMS_User_Data_Cx
							|| avp->code == AVP_IMS_Charging_Information
							|| avp->code == AVP_IMS_SIP_Auth_Data_Item
							|| avp->code
									   == AVP_IMS_Allowed_WAF_WWSF_Identities))
				continue;
			if(request->commandCode == IMS_RTR
					&& (avp->code == AVP_IMS_Public_Identity
							|| avp->code == AVP_IMS_Deregistration_Reason
							|| avp->code == AVP_IMS_Associated_Identities
							|| avp->code == AVP_IMS_RTR_Flags))
				continue;
		}
		return avp;
	}
	return NULL;
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


static int cx_duplicate(AAAMessage *request, int code, int vendor)
{
	AAA_AVP *avp = cx_avp(request, code, vendor);
	return avp && avp->next
		   && cdpb.AAAFindMatchingAVP(request, avp->next, code, vendor, 0);
}

static int cx_optional_duplicate(AAAMessage *request)
{
	const int codes[] = {AVP_IMS_User_Data_Cx, AVP_IMS_Charging_Information,
			AVP_IMS_SIP_Auth_Data_Item, AVP_IMS_Deregistration_Reason,
			AVP_IMS_Associated_Identities, AVP_IMS_RTR_Flags,
			AVP_IMS_Allowed_WAF_WWSF_Identities};
	unsigned int i;
	for(i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
		if(cx_duplicate(request, codes[i], IMS_vendor_id_3GPP))
			return codes[i];
	return 0;
}


static void cx_notify(impurecord_t *r, ucontact_t *c, int reason);
static int cx_group_valid(str data);
static int cx_duplicate(AAAMessage *request, int code, int vendor);
static int cx_optional_duplicate(AAAMessage *request);

static int cx_ppr_charging(AAA_AVP *data, cx_charging_t *charging)
{
	AAA_AVP_LIST list;
	AAA_AVP *avp;
	int seen[4] = {0}, i, result = 0;
	const int codes[] = {AVP_IMS_Primary_Charging_Collection_Function_Name,
			AVP_IMS_Secondary_Charging_Collection_Function_Name,
			AVP_IMS_Primary_Event_Charging_Function_Name,
			AVP_IMS_Secondary_Event_Charging_Function_Name};
	str *values[] = {
			&charging->ccf1, &charging->ccf2, &charging->ecf1, &charging->ecf2};
	if(!cx_group_valid(data->data))
		return DIAMETER_INVALID_AVP_VALUE;
	list = cdpb.AAAUngroupAVPS(data->data);
	for(avp = list.head; avp; avp = avp->next) {
		for(i = 0; i < 4; i++)
			if(avp->code == codes[i] && avp->vendorId == IMS_vendor_id_3GPP)
				break;
		if(i == 4) {
			if(avp->flags & AAA_AVP_FLAG_MANDATORY) {
				result = DIAMETER_AVP_UNSUPPORTED;
				break;
			}
			continue;
		}
		if(++seen[i] > 1) {
			result = DIAMETER_AVP_OCCURS_TOO_MANY_TIMES;
			break;
		}
		if(!avp->data.len || memchr(avp->data.s, 0, avp->data.len)) {
			result = DIAMETER_INVALID_AVP_VALUE;
			break;
		}
		*values[i] = avp->data;
	}
	cdpb.AAAFreeAVPList(&list);
	if(!result && !seen[0] && !seen[2])
		result = DIAMETER_INVALID_AVP_VALUE;
	return result;
}

/* Password updates use the SIP Digest/MD5 capability already supported by
 * ims_auth. AKA vectors and unsupported digest algorithms are not accepted. */
static int cx_ppr_digest(AAA_AVP *item, str *ha1)
{
	AAA_AVP_LIST list, digest_list;
	AAA_AVP *avp, *digest = NULL;
	str scheme = str_init("SIP Digest"), md5 = str_init("MD5"),
		auth = str_init("auth");
	int schemes = 0, digests = 0, values = 0, realms = 0, qops = 0,
		algorithms = 0;
	int result = DIAMETER_INVALID_AVP_VALUE, i;
	if(!cx_group_valid(item->data))
		return DIAMETER_INVALID_AVP_VALUE;
	list = cdpb.AAAUngroupAVPS(item->data);
	for(avp = list.head; avp; avp = avp->next) {
		if(avp->vendorId == IMS_vendor_id_3GPP
				&& avp->code == AVP_IMS_SIP_Authentication_Scheme) {
			if(++schemes > 1) {
				result = DIAMETER_AVP_OCCURS_TOO_MANY_TIMES;
				goto done;
			}
			if(str_strcasecmp(&avp->data, &scheme)) {
				result = -1;
				goto done;
			}
		} else if(avp->vendorId == IMS_vendor_id_3GPP
				  && avp->code == AVP_IMS_SIP_Digest_Authenticate) {
			if(++digests > 1) {
				result = DIAMETER_AVP_OCCURS_TOO_MANY_TIMES;
				goto done;
			}
			digest = avp;
		} else if(avp->vendorId == IMS_vendor_id_3GPP
				  && avp->code == AVP_IMS_SIP_Item_Number) {
			if(avp->data.len != 4)
				goto done;
		} else if(avp->flags & AAA_AVP_FLAG_MANDATORY) {
			result = DIAMETER_AVP_UNSUPPORTED;
			goto done;
		}
	}
	if(!digest || !cx_group_valid(digest->data))
		goto done;
	digest_list = cdpb.AAAUngroupAVPS(digest->data);
	result = 0;
	for(avp = digest_list.head; avp; avp = avp->next) {
		if(avp->vendorId == 0 && avp->code == AVP_IMS_Digest_HA1) {
			if(++values > 1) {
				result = DIAMETER_AVP_OCCURS_TOO_MANY_TIMES;
				break;
			}
			if(avp->data.len != 32) {
				result = DIAMETER_INVALID_AVP_VALUE;
				break;
			}
			for(i = 0; i < 32; i++) {
				char c = avp->data.s[i];
				if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
						   || (c >= 'A' && c <= 'F')))
					break;
			}
			if(i != 32) {
				result = DIAMETER_INVALID_AVP_VALUE;
				break;
			}
			*ha1 = avp->data;
		} else if(avp->vendorId == 0 && avp->code == AVP_IMS_Digest_Realm) {
			if(++realms > 1) {
				result = DIAMETER_AVP_OCCURS_TOO_MANY_TIMES;
				break;
			}
			if(!avp->data.len || memchr(avp->data.s, 0, avp->data.len)) {
				result = DIAMETER_INVALID_AVP_VALUE;
				break;
			}
		} else if(avp->vendorId == 0 && avp->code == AVP_IMS_Digest_QoP) {
			if(++qops > 1) {
				result = DIAMETER_AVP_OCCURS_TOO_MANY_TIMES;
				break;
			}
			if(str_strcasecmp(&avp->data, &auth)) {
				result = -1;
				break;
			}
		} else if(avp->vendorId == 0 && avp->code == AVP_IMS_Digest_Algorithm) {
			if(++algorithms > 1) {
				result = DIAMETER_AVP_OCCURS_TOO_MANY_TIMES;
				break;
			}
			if(str_strcasecmp(&avp->data, &md5)) {
				result = -1;
				break;
			}
		} else if(avp->flags & AAA_AVP_FLAG_MANDATORY) {
			result = DIAMETER_AVP_UNSUPPORTED;
			break;
		}
	}
	if(!result && (!values || !realms || !qops))
		result = DIAMETER_INVALID_AVP_VALUE;
	cdpb.AAAFreeAVPList(&digest_list);
done:
	cdpb.AAAFreeAVPList(&list);
	return result;
}

/* TS 29.228 6.2.2.1 defines user-information updates and preservation of
 * installed data on rejection. Clauses 6.5.2.1/6.5.2.3/6.5.2.4 cover the
 * implicit-set profile, charging and SIP Digest updates respectively.
 * The PPR/PPA wire formats are specified in TS 29.229 6.1.11/6.1.12. */
AAAMessage *cxdx_process_ppr(AAAMessage *request)
{
	AAA_AVP *data, *unsupported;
	ims_subscription *profile = NULL;
	udomain_t *d;
	str private_id;
	cx_charging_t charging = {0};
	AAA_AVP *charging_avp, *auth_avp, *waf_avp;
	ims_auth_api_t auth_api = {0};
	void *auth_state = NULL;
	str ha1 = {0, 0};
	int failed, result;
	LM_DBG("Processing Cx PPR endtoend=%u hopbyhop=%u\n",
			(unsigned int)request->endtoendId,
			(unsigned int)request->hopbyhopId);
	result = cx_validate_request(request, &failed);
	if(result)
		return cx_failed_answer(request, result, failed, 0);
	unsupported = cx_unsupported_mandatory(request);
	if(unsupported)
		return cx_failed_answer_avp(request, DIAMETER_AVP_UNSUPPORTED,
				unsupported->code, unsupported->vendorId, unsupported);
	failed = cx_optional_duplicate(request);
	if(failed)
		return cx_failed_answer(request, DIAMETER_AVP_OCCURS_TOO_MANY_TIMES,
				failed, IMS_vendor_id_3GPP);
	waf_avp = cx_avp(
			request, AVP_IMS_Allowed_WAF_WWSF_Identities, IMS_vendor_id_3GPP);
	/* WebRTC is not advertised by this receiver. Optional extension data
	 * is ignored, as required by TS 29.229 table 6.3.1 note 1a. */
	if(waf_avp && (waf_avp->flags & AAA_AVP_FLAG_MANDATORY))
		return cx_failed_answer(request, DIAMETER_AVP_UNSUPPORTED,
				AVP_IMS_Allowed_WAF_WWSF_Identities, IMS_vendor_id_3GPP);
	auth_avp = cx_avp(request, AVP_IMS_SIP_Auth_Data_Item, IMS_vendor_id_3GPP);
	if(auth_avp) {
		result = cx_ppr_digest(auth_avp, &ha1);
		if(result > 0)
			return cx_failed_answer(request, result, AVP_IMS_SIP_Auth_Data_Item,
					IMS_vendor_id_3GPP);
		if(result) {
			LM_WARN("Cx PPR rejected unsupported SIP Digest authentication "
					"data\n");
			return cx_answer(
					request, RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
		}
	}
	charging_avp =
			cx_avp(request, AVP_IMS_Charging_Information, IMS_vendor_id_3GPP);
	if(charging_avp) {
		result = cx_ppr_charging(charging_avp, &charging);
		if(result)
			return cx_failed_answer(request, result,
					AVP_IMS_Charging_Information, IMS_vendor_id_3GPP);
	}
	data = cx_avp(request, AVP_IMS_User_Data_Cx, IMS_vendor_id_3GPP);
	if(!data && (charging_avp || auth_avp || waf_avp))
		goto update;
	if(!data || !data->data.len)
		return cx_failed_answer(request, DIAMETER_MISSING_AVP,
				AVP_IMS_User_Data_Cx, IMS_vendor_id_3GPP);
	if(data->data.len > CX_MAX_USER_DATA) {
		LM_WARN("Cx PPR rejected User-Data exceeding %d bytes\n",
				CX_MAX_USER_DATA);
		return cx_answer(request, RC_IMS_DIAMETER_ERROR_TOO_MUCH_DATA, 1);
	}
	if(cdpb.AAAGetNextAVP(data)
			&& cdpb.AAAFindMatchingAVP(request, cdpb.AAAGetNextAVP(data),
					AVP_IMS_User_Data_Cx, IMS_vendor_id_3GPP, 0)) {
		LM_WARN("Cx PPR rejected duplicate User-Data AVP\n");
		return cx_failed_answer(request, DIAMETER_AVP_OCCURS_TOO_MANY_TIMES,
				AVP_IMS_User_Data_Cx, IMS_vendor_id_3GPP);
	}
	/* Cx profiles are self-contained XML; never resolve an external DTD. */
	if(memchr(data->data.s, 0, data->data.len)) {
		LM_WARN("Cx PPR rejected User-Data containing a NUL byte\n");
		return cx_answer(
				request, RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
	}
	{
		int i;
		for(i = 0; i < data->data.len; i++)
			if((i + 9 <= data->data.len
					   && !memcmp(data->data.s + i, "<!DOCTYPE", 9))
					|| (i + 8 <= data->data.len
							&& !memcmp(data->data.s + i, "<!ENTITY", 8))) {
				LM_WARN("Cx PPR rejected User-Data with DTD or entity "
						"declarations\n");
				return cx_answer(request,
						RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
			}
	}
	profile = parse_user_data(data->data);
	if(!profile) {
		LM_WARN("Cx PPR could not parse User-Data profile\n");
		return cx_answer(
				request, RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
	}
	profile->ref_count = 1; /* owned here until the cache takes record refs */
	private_id = cxdx_get_user_name(request);
	if(profile->private_identity.len != private_id.len
			|| memcmp(profile->private_identity.s, private_id.s,
					private_id.len)) {
		LM_WARN("Cx PPR rejected profile with a mismatched private identity\n");
		ul.unref_subscription(profile);
		return cx_answer(
				request, RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
	}
update:
	private_id = cxdx_get_user_name(request);
	if(ul.register_udomain(domain, &d) < 0) {
		LM_ERR("Cx PPR could not register the usrloc domain\n");
		if(profile)
			ul.unref_subscription(profile);
		return cx_answer(request, DIAMETER_UNABLE_TO_COMPLY, 0);
	}
	if(auth_avp) {
		if(ims_auth_load_api(&auth_api) < 0 || !auth_api.prepare_digest_update
				|| !auth_api.finish_digest_update) {
			LM_WARN("Cx PPR SIP Digest update requires the ims_auth update "
					"API\n");
			if(profile)
				ul.unref_subscription(profile);
			return cx_answer(
					request, RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA, 1);
		}
		result = auth_api.prepare_digest_update(&private_id, &ha1, &auth_state);
		if(result) {
			LM_WARN("Cx PPR could not prepare SIP Digest update: result=%d\n",
					result);
			if(profile)
				ul.unref_subscription(profile);
			return result == -1
						   ? cx_answer(request,
									 RC_IMS_DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA,
									 1)
						   : cx_answer(request, DIAMETER_UNABLE_TO_COMPLY, 0);
		}
	}
	/* Prepare both caches before publication: a rejected combined update
	 * must leave the previous profile, charging and authentication usable. */
	LM_DBG("Cx PPR data validated; updating cached profile and user "
		   "information: user_data=%d charging=%d digest=%d\n",
			profile != NULL, charging_avp != NULL, auth_avp != NULL);
	result = ul.cx_update_profile(d, &private_id, profile,
			charging_avp ? &charging : NULL, cx_notify);
	if(auth_state)
		auth_api.finish_digest_update(auth_state, result == 0);
	if(profile)
		ul.unref_subscription(profile);
	LM_INFO("Cx PPR profile installation result=%d\n", result);
	return cx_operation_answer(request, result);
}

static void cx_notify(impurecord_t *r, ucontact_t *c, int reason)
{
	if(reason == CX_PROFILE_IDENTITY_REMOVED) {
		create_notifications(NULL, r, c, NULL, NULL, NULL, &r->public_identity,
				1, IMS_REGISTRAR_CONTACT_DEREGISTERED, &c->c, 1);
		return;
	}
	if(reason == CX_PROFILE_CHANGED) {
		notify_subscribers(
				r, NULL, NULL, 0, IMS_REGISTRAR_CONTACT_DEREGISTERED);
		return;
	}
	/* NOTIFY workers need a copied URI after RTR releases the cache link. */
	notify_subscribers(r, c, &c->c, 1,
			reason == 2 ? IMS_REGISTRAR_CONTACT_DEACTIVATED
						: IMS_REGISTRAR_CONTACT_DEREGISTERED);
}

/* TS 29.228 6.1.3.1 and 6.5.2.2 define the affected private identities and
 * implicit registration sets, including emergency-registration handling.
 * TS 29.229 6.1.9/6.1.10 define the RTR/RTA command formats. */
AAAMessage *cxdx_process_rtr(AAAMessage *request)
{
	AAA_AVP *avp, *reason_avp, *associated, *unsupported;
	AAA_AVP_LIST list;
	str identities[CX_MAX_IDENTITIES], private_ids[CX_MAX_IDENTITIES];
	cx_dereg_result_t response;
	AAAMessage *answer;
	udomain_t *d;
	int count = 0, private_count = 1, reason = -1, reason_count = 0;
	int failed, result, i;
	unsigned int flags = 0;
	LM_DBG("Processing Cx RTR endtoend=%u hopbyhop=%u\n",
			(unsigned int)request->endtoendId,
			(unsigned int)request->hopbyhopId);
	result = cx_validate_request(request, &failed);
	if(result)
		return cx_failed_answer(request, result, failed, 0);
	unsupported = cx_unsupported_mandatory(request);
	if(unsupported)
		return cx_failed_answer_avp(request, DIAMETER_AVP_UNSUPPORTED,
				unsupported->code, unsupported->vendorId, unsupported);
	failed = cx_optional_duplicate(request);
	if(failed)
		return cx_failed_answer(request, DIAMETER_AVP_OCCURS_TOO_MANY_TIMES,
				failed, IMS_vendor_id_3GPP);
	reason_avp =
			cx_avp(request, AVP_IMS_Deregistration_Reason, IMS_vendor_id_3GPP);
	if(!reason_avp)
		return cx_failed_answer(request, DIAMETER_MISSING_AVP,
				AVP_IMS_Deregistration_Reason, IMS_vendor_id_3GPP);
	if(!cx_group_valid(reason_avp->data))
		return cx_failed_answer(request, DIAMETER_INVALID_AVP_VALUE,
				AVP_IMS_Deregistration_Reason, IMS_vendor_id_3GPP);
	list = cdpb.AAAUngroupAVPS(reason_avp->data);
	for(avp = list.head; avp; avp = avp->next)
		if(avp->code == AVP_IMS_Reason_Code
				&& avp->vendorId == IMS_vendor_id_3GPP) {
			reason_count++;
			if(avp->data.len == 4)
				reason = get_4bytes(avp->data.s);
		}
	cdpb.AAAFreeAVPList(&list);
	if(!reason_count)
		return cx_failed_answer(request, DIAMETER_MISSING_AVP,
				AVP_IMS_Reason_Code, IMS_vendor_id_3GPP);
	if(reason_count != 1 || reason < 0 || reason > 3) {
		LM_WARN("Cx RTR rejected invalid Deregistration-Reason (reason=%d "
				"count=%d)\n",
				reason, reason_count);
		return cx_failed_answer(request,
				reason_count > 1 ? DIAMETER_AVP_OCCURS_TOO_MANY_TIMES
								 : DIAMETER_INVALID_AVP_VALUE,
				AVP_IMS_Deregistration_Reason, IMS_vendor_id_3GPP);
	}
	avp = cx_avp(request, AVP_IMS_RTR_Flags, IMS_vendor_id_3GPP);
	if(avp) {
		if(avp->data.len != 4)
			return cx_failed_answer(request, DIAMETER_INVALID_AVP_VALUE,
					AVP_IMS_RTR_Flags, IMS_vendor_id_3GPP);
		flags = get_4bytes(avp->data.s);
	}
	private_ids[0] = cxdx_get_user_name(request);
	associated =
			cx_avp(request, AVP_IMS_Associated_Identities, IMS_vendor_id_3GPP);
	if(associated) {
		if(!cx_group_valid(associated->data))
			return cx_failed_answer(request, DIAMETER_INVALID_AVP_VALUE,
					AVP_IMS_Associated_Identities, IMS_vendor_id_3GPP);
		list = cdpb.AAAUngroupAVPS(associated->data);
		for(avp = list.head; avp; avp = avp->next) {
			if(avp->code != AVP_User_Name || avp->vendorId != 0)
				continue;
			if(!avp->data.len || memchr(avp->data.s, 0, avp->data.len)
					|| private_count == CX_MAX_IDENTITIES) {
				cdpb.AAAFreeAVPList(&list);
				return cx_failed_answer(request, DIAMETER_INVALID_AVP_VALUE,
						AVP_IMS_Associated_Identities, IMS_vendor_id_3GPP);
			}
			for(i = 0; i < private_count
					   && str_strcmp(&private_ids[i], &avp->data);
					i++) {
			}
			if(i == private_count)
				private_ids[private_count++] = avp->data;
		}
		cdpb.AAAFreeAVPList(&list);
	}
	avp = cx_avp(request, AVP_IMS_Public_Identity, IMS_vendor_id_3GPP);
	while(avp) {
		if(count == CX_MAX_IDENTITIES) {
			LM_WARN("Cx RTR rejected more than %d public identities\n",
					CX_MAX_IDENTITIES);
			return cx_answer(request, DIAMETER_RESOURCES_EXCEEDED, 0);
		}
		if(!avp->data.len || memchr(avp->data.s, 0, avp->data.len))
			return cx_failed_answer_avp(request, DIAMETER_INVALID_AVP_VALUE,
					AVP_IMS_Public_Identity, IMS_vendor_id_3GPP, avp);
		identities[count++] = avp->data;
		avp = avp->next;
		if(avp)
			avp = cdpb.AAAFindMatchingAVP(request, avp, AVP_IMS_Public_Identity,
					IMS_vendor_id_3GPP, 0);
	}
	/* IMPI-only termination excludes NEW_SERVER_ASSIGNED (reason 1),
	 * which requires Public-Identity; see TS 29.228 6.1.3.1. */
	if(!count && reason == 1)
		return cx_failed_answer(request, DIAMETER_MISSING_AVP,
				AVP_IMS_Public_Identity, IMS_vendor_id_3GPP);
	if(!count)
		LM_WARN("Cx RTR received with only IMPI (User-Name AVP); expanding "
				"cached sets\n");
	else
		LM_DBG("Cx RTR received with IMPU [%.*s] in Public-Identity AVP\n",
				identities[0].len, identities[0].s);
	if(ul.register_udomain(domain, &d) < 0) {
		LM_ERR("Cx RTR could not register the usrloc domain\n");
		return cx_answer(request, DIAMETER_UNABLE_TO_COMPLY, 0);
	}
	LM_DBG("Cx RTR validated reason=%d public identities=%d private "
		   "identities=%d flags=0x%x; starting cleanup\n",
			reason, count, private_count, flags);
	result = ul.cx_deregister(d, private_ids, private_count, identities, count,
			reason, flags, cx_notify, &response);
	LM_INFO("Cx RTR cache cleanup reason=%d public_ids=%d result=%d\n", reason,
			count, result);
	LM_DBG("Cx RTR result scope: associated=%d emergency_pairs=%d\n",
			response.associated_count, response.emergency_count);
	answer = cx_answer(request,
			result == 0	  ? DIAMETER_SUCCESS
			: result == 1 ? DIAMETER_LIMITED_SUCCESS
						  : DIAMETER_UNABLE_TO_COMPLY,
			0);
	if(answer && result >= 0) {
		if((response.associated_count
				   && !cxdx_add_associated_identities(answer,
						   response.associated, response.associated_count))
				|| !cxdx_add_emergency_identities(
						answer, response.emergency, response.emergency_count)) {
			LM_ERR("Cx RTR could not encode the termination scope in RTA\n");
			cdpb.AAAFreeMessage(&answer);
		}
	}
	for(i = 0; i < response.emergency_count; i++) {
		pkg_free(response.emergency[i].private_identity.s);
		pkg_free(response.emergency[i].public_identity.s);
	}
	return answer;
}

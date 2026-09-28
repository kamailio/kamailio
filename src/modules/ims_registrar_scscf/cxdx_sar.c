/*
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
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
 */

#include "stats.h"
#include "../cdp/cdp_load.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/ims_dialog/dlg_load.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "api.h"
#include "cxdx_avp.h"

#include "reply.h"
#include "rerrno.h"
#include "ims_registrar_scscf_mod.h"
#include "cxdx_sar.h"
#include "save.h"
#include "userdata_parser.h"
#include "../../core/counters.h"
#include "../../core/data_lump_rpl.h"
#include "sip_msg.h"
#include "path.h"
#include "regtime.h"
#include "../../core/parser/hf.h"
#include "../../core/trim.h"
#include "../../core/ut.h"
#include "../../lib/ims/ims_getters.h"
#include "registrar_notify.h"
#include "pvt_message.h"

extern struct cdp_binds cdpb;
extern unsigned int send_vs_callid_avp;

int create_return_code(int result)
{
	int rc;
	int_str avp_val, avp_name;
	avp_name.s.s = "saa_return_code";
	avp_name.s.len = 15;

	//build avp spec for saa_return_code
	avp_val.n = result;

	rc = add_avp(AVP_NAME_STR, avp_name, avp_val);

	if(rc < 0)
		LM_ERR("couldnt create AVP\n");
	else
		LM_INFO("created AVP successfully : [%.*s] - [%d]\n", avp_name.s.len,
				avp_name.s.s, result);

	return 1;
}

void free_saved_transaction_data(saved_transaction_t *data)
{
	if(!data)
		return;

	if(data->public_identity.s && data->public_identity.len) {
		shm_free(data->public_identity.s);
		data->public_identity.len = 0;
	}
	free_contact_buf(data->contact_header);
	shm_free(data);
}

void async_cdp_callback(
		int is_timeout, void *param, AAAMessage *saa, long elapsed_msecs)
{
	struct cell *t = 0;
	int rc = -1, experimental_rc = -1;
	int result = CSCF_RETURN_TRUE;
	saved_transaction_t *data = 0;
	struct sip_msg *req;

	str xml_data = {0, 0}, ccf1 = {0, 0}, ccf2 = {0, 0}, ecf1 = {0, 0},
		ecf2 = {0, 0};
	restoration_info_t ri = {{0, 0}, {0, 0}, {0, 0}};
	ims_subscription *s = 0;
	rerrno = R_FINE;

	if(!param) {
		LM_DBG("No transaction data this must have been called from usrloc cb "
			   "impu deleted - just log result code and then exit");
		cxdx_get_result_code(saa, &rc);
		cxdx_get_experimental_result_code(saa, &experimental_rc);

		if(saa)
			cdpb.AAAFreeMessage(&saa);

		if(!rc && !experimental_rc) {
			LM_ERR("bad SAA result code\n");
			return;
		}
		switch(rc) {
			case -1:
				LM_DBG("Received Diameter error\n");
				return;

			case AAA_UNABLE_TO_COMPLY:
				LM_DBG("Unable to comply\n");
				return;

			case AAA_SUCCESS:
				LM_DBG("received AAA success\n");
				return;

			default:
				LM_ERR("Unknown error\n");
				return;
		}

	} else {
		LM_DBG("There is transaction data this must have been called from save "
			   "or assign server unreg");
		data = (saved_transaction_t *)param;
		if(tmb.t_lookup_ident(&t, data->tindex, data->tlabel) < 0) {
			LM_ERR("t_continue: transaction not found and t is now pointing to "
				   "%p and will be set to NULL\n",
					t);
			t = NULL;
			rerrno = R_SAR_FAILED;
			goto error_no_send;
		}

		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to);
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to);
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to);

		get_act_time();

		req = get_request_from_tx(t);
		if(!req) {
			LM_ERR("Failed to get SIP Request from Transaction\n");
			goto error_no_send;
		}

		if(is_timeout) {
			update_stat(stat_sar_timeouts, 1);
			LM_ERR("Transaction timeout - did not get SAA\n");
			rerrno = R_SAR_FAILED;
			goto error;
		}
		if(!saa) {
			LM_ERR("Error sending message via CDP\n");
			rerrno = R_SAR_FAILED;
			goto error;
		}

		update_stat(sar_replies_received, 1);
		update_stat(sar_replies_response_time, elapsed_msecs);


		/* check and see that all the required headers are available and can be parsed */
		if(parse_message_for_register(req) < 0) {
			LM_ERR("Unable to parse register message correctly\n");
			rerrno = R_SAR_FAILED;
			goto error;
		}

		LM_DBG("callid for found transaction is [%.*s]\n",
				req->callid->body.len, req->callid->body.s);


		cxdx_get_result_code(saa, &rc);
		cxdx_get_experimental_result_code(saa, &experimental_rc);
		cxdx_get_charging_info(saa, &ccf1, &ccf2, &ecf1, &ecf2);
		/* TS 23.380 4.6.2: present only on the answer to a NO_ASSIGNMENT or
		 * UNREGISTERED_USER SAR, the two assign_server() sends; no point
		 * looking in a REGISTER's SAA. */
		if(data->script_assignment)
			cxdx_get_scscf_restoration_info(
					saa, &ri.path, &ri.contact, &ri.callid);

		if(!rc && !experimental_rc) {
			LM_ERR("bad SAA result code\n");
			rerrno = R_SAR_FAILED;
			goto error;
		}

		/* TS 29.228 6.1.2.1, TS 23.380 4.3.2: an UNREGISTERED_USER SAR for an
		 * identity the HSS holds as registered is answered with the profile,
		 * the restoration information and Experimental-Result
		 * DIAMETER_ERROR_IN_ASSIGNMENT_TYPE. That is the restoration answer,
		 * not a failure: the S-CSCF triggers the registered services. */
		if(rc == -1
				&& experimental_rc == RC_IMS_DIAMETER_ERROR_IN_ASSIGNMENT_TYPE
				&& data->sar_assignment_type == AVP_IMS_SAR_UNREGISTERED_USER
				&& ri.contact.len) {
			LM_DBG("UNREGISTERED_USER SAA with restoration information: "
				   "restoring a registered user\n");
			rc = AAA_SUCCESS;
		}

		switch(rc) {
			case -1:
				LM_DBG("Received Diameter error\n");
				rerrno = R_SAR_FAILED;
				goto error;

			case AAA_UNABLE_TO_COMPLY:
				LM_DBG("Unable to comply\n");
				rerrno = R_SAR_FAILED;
				goto error;

			case AAA_SUCCESS:
				LM_DBG("received AAA success for SAR - SAA\n");
				break;

			default:
				LM_ERR("Unknown error\n");
				rerrno = R_SAR_FAILED;
				goto error;
		}
		//success
		//if this is from a save (not a server assign from the script) and expires is zero we don't update usrloc as this is a dereg and usrloc was updated previously
		if(!data->script_assignment && data->expires == 0) {
			LM_DBG("no need to update usrloc - already done for de-reg\n");
			result = CSCF_RETURN_TRUE;
			goto success;
		}

		xml_data = cxdx_get_user_data(saa);
		/*If there is XML user data we must be able to parse it*/
		if(xml_data.s && xml_data.len > 0) {
			LM_DBG("Parsing user data string from SAA\n");
			s = parse_user_data(xml_data);
			if(!s) {
				LM_ERR("Unable to parse user data XML string\n");
				rerrno = R_SAR_FAILED;
				goto error;
			}
			LM_DBG("Successfully parse user data XML setting ref to 1 (we are "
				   "referencing it)\n");
			s->ref_count =
					1; //no need to lock as nobody else will be referencing this piece of memory just yet
		} else {
			if(data->require_user_data) {
				LM_ERR("We require User data for this assignment/register and "
					   "none was supplied\n");
				rerrno = R_SAR_FAILED;
				result = CSCF_RETURN_FALSE;
				goto done;
			}
		}

		//here we update the contacts and also build the new contact header for the 200 OK reply
		if(update_contacts(req, data->domain, &data->public_identity,
				   data->sar_assignment_type, &s, &ccf1, &ccf2, &ecf1, &ecf2,
				   &data->contact_header, &ri)
				<= 0) {
			LM_ERR("Error processing REGISTER\n");
			rerrno = R_SAR_FAILED;
			goto error;
		}

		if(data->contact_header) {
			LM_DBG("Updated contacts: %.*s\n", data->contact_header->data_len,
					data->contact_header->buf);
		} else {
			LM_DBG("Updated unreg contact\n");
		}
	}

success:
	update_stat(accepted_registrations, 1);

done:
	if(!data->script_assignment)
		reg_send_reply_transactional(req, data->contact_header, t);
	LM_DBG("DBG:SAR Async CDP callback: ... Done resuming transaction\n");

	create_return_code(result);

	//release our reference on subscription (s)
	if(s)
		ul.unref_subscription(s);

	//free memory
	if(saa)
		cdpb.AAAFreeMessage(&saa);
	if(t) {
		//        del_nonshm_lump_rpl(&req->reply_lump);
		tmb.unref_cell(t);
	}
	//free path vector pkg memory
	//    reset_path_vector(req);

	tmb.t_continue_skip_timer(data->tindex, data->tlabel, data->act);
	free_saved_transaction_data(data);
	return;

error:
	create_return_code(-2);
	if(!data->script_assignment)
		reg_send_reply_transactional(req, data->contact_header, t);

error_no_send: //if we don't have the transaction then we can't send a transaction response
	update_stat(rejected_registrations, 1);
	//free memory
	if(saa)
		cdpb.AAAFreeMessage(&saa);
	if(t) {
		//        del_nonshm_lump_rpl(&req->reply_lump);
		tmb.unref_cell(t);
	}
	tmb.t_continue(data->tindex, data->tlabel, data->act);
	free_saved_transaction_data(data);
	return;
}

/*!
 * The Contact to back up at the HSS (TS 23.380 4.6.2): the REGISTER's whole
 * Contact header field value, parameters included, as TS 29.229 6.3.48 has the
 * Contact AVP carry it - the restored binding needs its feature tags
 * (+sip.instance, +g.3gpp.icsi-ref...) as much as its URI. A registration time
 * given in an Expires header rather than as a parameter is appended as one, so
 * the restored binding is granted what this REGISTER was granted and not
 * default_expires.
 * @param msg - the REGISTER
 * @param c - the contact of msg to back up
 * @param out - set to a pkg copy the caller frees
 * @returns 0 on success, -1 on error
 */
static int build_restoration_contact(
		struct sip_msg *msg, contact_t *c, str *out)
{
	static const char expires_param[] = ";expires=";
	str value;
	int expires = -1;

	value.s = c->name.s;
	value.len = c->len;
	trim(&value);

	if(!c->expires)
		expires = cscf_get_expires_hdr(msg, 0);

	out->len = value.len + sizeof(expires_param) - 1 + INT2STR_MAX_LEN;
	out->s = pkg_malloc(out->len);
	if(!out->s) {
		PKG_MEM_ERROR;
		out->len = 0;
		return -1;
	}
	memcpy(out->s, value.s, value.len);
	if(expires >= 0)
		out->len = value.len
				   + snprintf(out->s + value.len, out->len - value.len, "%s%d",
						   expires_param, expires);
	else
		out->len = value.len;
	return 0;
}

/**
 * Create and send a Server-Assignment-Request and returns the Answer received for it.
 * This function performs the Server Assignment operation.
 * @param msg - the SIP message to send for
 * @param public_identity - the public identity of the user
 * @param server_name - local name of the S-CSCF to save on the HSS
 * @param assignment_type - type of the assignment
 * @param data_available - if the data is already available
 * @returns the SAA
 */
int cxdx_send_sar(struct sip_msg *msg, str public_identity,
		str private_identity, str server_name, int assignment_type,
		int data_available, saved_transaction_t *transaction_data)
{
	AAAMessage *sar = 0;
	AAASession *session = 0;
	unsigned int hash = 0, label = 0;
	struct hdr_field *hdr;
	str call_id;
	int restoration_info_sent = 0;

	session = cdpb.AAACreateSession(0);

	sar = cdpb.AAACreateRequest(IMS_Cx, IMS_SAR, Flag_Proxyable, session);
	if(session) {
		cdpb.AAADropSession(session);
		session = 0;
	}
	if(!sar)
		goto error1;

	if(msg && send_vs_callid_avp) {
		call_id = cscf_get_call_id(msg, &hdr);
		if(call_id.len > 0 && call_id.s) {
			if(!cxdx_add_call_id(sar, call_id))
				LM_WARN("Failed to add call-id to SAR.... continuing... "
						"assuming non-critical\n");
		}
	}

	if(!cxdx_add_destination_realm(sar, cxdx_dest_realm))
		goto error1;

	if(!cxdx_add_vendor_specific_appid(
			   sar, IMS_vendor_id_3GPP, IMS_Cx, 0 /*IMS_Cx*/))
		goto error1;
	if(!cxdx_add_auth_session_state(sar, 1))
		goto error1;

	if(!cxdx_add_public_identity(sar, public_identity))
		goto error1;
	if(!cxdx_add_server_name(sar, server_name))
		goto error1;
	if(private_identity.len)
		if(!cxdx_add_user_name(sar, private_identity))
			goto error1;
	if(!cxdx_add_server_assignment_type(sar, assignment_type))
		goto error1;
	if(!cxdx_add_userdata_available(sar, data_available))
		goto error1;

	/*
	 * TS 23.380 4.6.2: back up the Contact and the Path leading to it
	 * (normally just the P-CSCF) at the HSS during (RE_)REGISTRATION, so a
	 * restarted S-CSCF can ask for them back later instead of only the
	 * profile. msg is the REGISTER only for these two types; for any other
	 * assignment_type it is the request being served (INVITE, MESSAGE...),
	 * whose Path/Contact have nothing to do with the served user's binding.
	 *
	 * Gated behind scscf_restoration_info_enabled: Supported-Features goes
	 * out with the M bit set on a SAR that carries SCSCF-Restoration-Info
	 * (TS 29.229 7.2.1), which an HSS without IMSRestorationInd must reject
	 * (DIAMETER_ERROR_FEATURE_UNSUPPORTED, or DIAMETER_AVP_UNSUPPORTED if it
	 * has no Supported-Features support at all) -- failing the REGISTER.
	 * Default off so an unpatched HSS keeps working; turn on only once the
	 * HSS is known to support the feature.
	 */
	if(scscf_restoration_info_enabled) {
		if(msg
				&& (assignment_type == AVP_IMS_SAR_REGISTRATION
						|| assignment_type == AVP_IMS_SAR_RE_REGISTRATION)) {
			str restoration_path = {0, 0}, path_received = {0, 0};
			str restoration_contact = {0, 0}, restoration_callid = {0, 0};
			contact_t *c;

			if(path_enabled)
				build_path_vector(msg, &restoration_path, &path_received);

			c = get_first_contact(msg);
			if(c && restoration_path.len
					&& build_restoration_contact(msg, c, &restoration_contact)
							   < 0)
				LM_WARN("Failed to build the Contact to back up at the "
						"HSS.... continuing... assuming non-critical\n");

			/* trimmed as pack_ci() stores it, or a restored binding would
			 * not match the UE's next REGISTER on Call-ID */
			restoration_callid = cscf_get_call_id(msg, &hdr);
			trim_trailing(&restoration_callid);

			if(private_identity.len && restoration_path.len
					&& restoration_contact.len) {
				if(cxdx_add_scscf_restoration_info(sar, private_identity,
						   restoration_path, restoration_contact,
						   restoration_callid))
					restoration_info_sent = 1;
				else
					LM_WARN("Failed to add SCSCF-Restoration-Info to SAR.... "
							"continuing... assuming non-critical\n");
			}
			if(restoration_contact.s)
				pkg_free(restoration_contact.s);
		}

		/*
		 * TS 29.229 table 7.1.1 IMSRestorationInd: the HSS sends restoration
		 * information back only to an S-CSCF that says it supports the
		 * feature, so every SAR says so once enabled -- NO_ASSIGNMENT and
		 * UNREGISTERED_USER being the ones that ask for it back. 7.2.1: the
		 * M bit is set only when this SAR itself uses the feature, i.e.
		 * carries SCSCF-Restoration-Info.
		 */
		if(!cxdx_add_supported_features(sar,
				   AVP_IMS_Feature_List_ID_IMS_Restoration_Indication,
				   restoration_info_sent))
			goto error1;
	}

	if(msg && tmb.t_get_trans_ident(msg, &hash, &label) < 0) {
		// it's ok cause we can call this async with a message for ul callbacks!
		LM_DBG("SIP message without transaction... must be a ul callback\n");
		//return 0;
	}

	if(cxdx_forced_peer.len)
		cdpb.AAASendMessageToPeer(sar, &cxdx_forced_peer,
				(void *)async_cdp_callback, (void *)transaction_data);
	else
		cdpb.AAASendMessage(
				sar, (void *)async_cdp_callback, (void *)transaction_data);

	return 0;

error1: //Only free SAR IFF it has not been passed to CDP
	if(sar)
		cdpb.AAAFreeMessage(&sar);

	return -1;
}

/*
 * $Id$
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


#include "server_assignment.h"
#include "ims_registrar_scscf_mod.h"
#include "registrar_notify.h"
#include "usrloc_cb.h"

extern str scscf_name_str;
extern int sar_on_unreg_expiry;

void ul_impu_inserted(impurecord_t *r, ucontact_t *c, int type, void *param)
{

	LM_DBG("Received notification of UL IMPU insert for IMPU <%.*s>\n",
			r->public_identity.len, r->public_identity.s);

	LM_DBG("Registering for callbacks on this IMPU for contact insert, update, "
		   "delete or expire to send notifications if there are any "
		   "subscriptions\n");
	ul.register_ulcb(r, 0, UL_IMPU_NEW_CONTACT, ul_contact_changed,
			0); //this allows us to receive cbs on new contact for IMPU
	ul.register_ulcb(r, 0,
			UL_IMPU_UPDATE_CONTACT | UL_IMPU_EXPIRE_CONTACT
					| UL_IMPU_DELETE_CONTACT | UL_IMPU_DELETE_CONTACT_IMPLICIT,
			ul_contact_changed, 0);

	LM_DBG("Asking for the no contact delete callback on every IMPU of the "
		   "implicit set, the SAR is sent for its anchor only so that there "
		   "is one SAR per implicit set\n");
	//TODO only do this if a flag in the IMPU record identifies this as the implicit set anchor
	if(ul.register_ulcb(r, 0,
			   r->is_primary ? (UL_IMPU_REG_NC_DELETE | UL_IMPU_UNREG_EXPIRED)
							 : UL_IMPU_REG_NC_DELETE,
			   ul_impu_removed, 0)
			< 0) {
		LM_ERR("can not register callback for no contacts delete or IMPI "
			   "expire\n");
		return;
	}
}

static const char *nw_dereg_type_name(int assignment_type)
{
	switch(assignment_type) {
		case AVP_IMS_SAR_TIMEOUT_DEREGISTRATION:
			return "TIMEOUT_DEREGISTRATION";
		case AVP_IMS_SAR_ADMINISTRATIVE_DEREGISTRATION:
			return "ADMINISTRATIVE_DEREGISTRATION";
		default:
			return "?";
	}
}

/* The profile of an unregistered identity is dropped, its validity
 * (ims_usrloc_scscf unreg_validity) is over. With sar_on_unreg_expiry set
 * the HSS is told; without it the HSS keeps the S-CSCF name.
 * TS 29.228 6.1.2.1: with a de-registration type the HSS moves an identity
 * in state Unregistered to Not Registered and clears the S-CSCF name.
 * TS 29.229 6.3.15: no registration timer has run for such an identity, so
 * this is ADMINISTRATIVE_DEREGISTRATION and not TIMEOUT_DEREGISTRATION. */
static void ul_impu_unreg_expired(impurecord_t *r)
{
	if(r->reg_state != IMPU_UNREGISTERED) {
		LM_DBG("unregistered validity of <%.*s> is over: no SAR, state is "
			   "%d\n",
				r->public_identity.len, r->public_identity.s, r->reg_state);
		return;
	}
	if(!sar_on_unreg_expiry) {
		LM_DBG("unregistered profile of <%.*s> expired: no SAR, "
			   "sar_on_unreg_expiry is not set\n",
				r->public_identity.len, r->public_identity.s);
		return;
	}
	if(!r->send_sar_on_delete) {
		LM_INFO("unregistered profile of <%.*s> expired: no SAR, the HSS "
				"knows already (de-registration by the UE or by the HSS)\n",
				r->public_identity.len, r->public_identity.s);
		return;
	}
	if(!r->s || r->s->private_identity.len <= 0) {
		LM_WARN("unregistered profile of <%.*s> expired: no SAR, the record "
				"has no subscription data\n",
				r->public_identity.len, r->public_identity.s);
		return;
	}
	LM_INFO("unregistered profile of <%.*s> expired: SAR with "
			"Server-Assignment-Type %d (%s)\n",
			r->public_identity.len, r->public_identity.s,
			AVP_IMS_SAR_ADMINISTRATIVE_DEREGISTRATION,
			nw_dereg_type_name(AVP_IMS_SAR_ADMINISTRATIVE_DEREGISTRATION));
	cxdx_send_sar(NULL, r->public_identity, r->s->private_identity,
			scscf_name_str, AVP_IMS_SAR_ADMINISTRATIVE_DEREGISTRATION,
			AVP_IMS_SAR_USER_DATA_NOT_AVAILABLE, 0);
	r->reg_state = IMPU_NOT_REGISTERED;
}

void ul_impu_removed(impurecord_t *r, ucontact_t *c, int type, void *param)
{
	/* TS 29.229 6.3.15 Server-Assignment-Type:
	 * - ADMINISTRATIVE_DEREGISTRATION: the S-CSCF de-registered the identity
	 *   for administrative reasons or network issues (reg event report of a
	 *   P-CSCF, operator command);
	 * - TIMEOUT_DEREGISTRATION: the registration timer has expired.
	 * The STORE_SERVER_NAME variant is not used: it tells the HSS that the
	 * S-CSCF keeps the user data, and the record may be deleted right after
	 * this callback.
	 * USER_DEREGISTRATION belongs to the UE's REGISTER and does not come
	 * through here. */
	int assignment_type = AVP_IMS_SAR_TIMEOUT_DEREGISTRATION;
	int data_available = AVP_IMS_SAR_USER_DATA_NOT_AVAILABLE;

	//we only send SAR if the REGISTRATION state is (NOT) IMPU_NOT_REGISTERED and if send_sar_on_delete is set
	//send_sar_on_delete is set by default - only unset if impu is deleted due to explicit dereg
	LM_DBG("Received notification of UL IMPU removed for IMPU <%.*s>\n",
			r->public_identity.len, r->public_identity.s);

	if(type == UL_IMPU_UNREG_EXPIRED) {
		ul_impu_unreg_expired(r);
		return;
	}

	if(r->nw_dereg_admin)
		assignment_type = AVP_IMS_SAR_ADMINISTRATIVE_DEREGISTRATION;

	if(!r->is_primary) {
		/* the SAR of the anchor covers the implicit registration set
		 * (TS 29.228 6.1.2.1: the HSS moves the identities to Not
		 * Registered and clears the S-CSCF name), follow it here */
		if(r->reg_state != IMPU_NOT_REGISTERED && r->send_sar_on_delete) {
			LM_DBG("network de-registration of <%.*s>: not the anchor of "
				   "the implicit set, no SAR - not registered\n",
					r->public_identity.len, r->public_identity.s);
			r->reg_state = IMPU_NOT_REGISTERED;
		}
		r->nw_dereg_admin = 0;
		return;
	}

	if(r->reg_state != IMPU_NOT_REGISTERED && r->send_sar_on_delete && r->s) {
		LM_DBG("Sending SAR to DeRegister [%.*s] (pvt: <%.*s>)\n",
				r->public_identity.len, r->public_identity.s,
				r->s->private_identity.len, r->s->private_identity.s);
		LM_INFO("network de-registration of <%.*s>: SAR with "
				"Server-Assignment-Type %d (%s)\n",
				r->public_identity.len, r->public_identity.s, assignment_type,
				nw_dereg_type_name(assignment_type));
		cxdx_send_sar(NULL, r->public_identity, r->s->private_identity,
				scscf_name_str, assignment_type, data_available, 0);
		r->nw_dereg_admin = 0;
		/* TS 29.228 6.1.2.1: the HSS moves the identity to Not Registered
		 * and clears the S-CSCF name, so the record is not "unregistered
		 * with a stored profile" afterwards */
		r->reg_state = IMPU_NOT_REGISTERED;
	} else if(r->reg_state == IMPU_NOT_REGISTERED) {
		LM_INFO("network de-registration of <%.*s>: no SAR, the identity is "
				"not registered\n",
				r->public_identity.len, r->public_identity.s);
	} else if(!r->send_sar_on_delete) {
		LM_INFO("network de-registration of <%.*s>: no SAR, the HSS knows "
				"already (de-registration by the UE or by the HSS)\n",
				r->public_identity.len, r->public_identity.s);
	} else {
		LM_WARN("network de-registration of <%.*s>: no SAR, the record has "
				"no subscription data\n",
				r->public_identity.len, r->public_identity.s);
	}
}

void ul_contact_changed(impurecord_t *r, ucontact_t *c, int type, void *param)
{

	LM_DBG("Received notification of type %d on contact Address <%.*s>\n", type,
			c->c.len, c->c.s);

	if(!r->shead) {
		LM_DBG("There are no subscriptions for this IMPU therefore breaking "
			   "out now as nothing to do\n");
		return;
	}
	//
	if(type == UL_IMPU_DELETE_CONTACT) {
		LM_DBG("Received notification of UL CONTACT DELETE\n");
		event_reg(0, r, c, IMS_REGISTRAR_CONTACT_UNREGISTERED, 0, 0, 0, 0, 0);
	} else if(type == UL_IMPU_EXPIRE_CONTACT) {
		LM_DBG("Received notification of UL CONTACT EXPIRED\n");
		event_reg(0, r, c, IMS_REGISTRAR_CONTACT_EXPIRED, 0, 0, 0, 0, 0);
	}
}

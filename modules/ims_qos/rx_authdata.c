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
 *
 * History:
 * --------
 *  2011-02-02  initial version (jason.penton)
 */

#include "../../sr_module.h"
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "../../parser/contact/parse_contact.h"
#include "../../locking.h"
#include "../tm/tm_load.h"
#include "../../modules/dialog_ng/dlg_load.h"
#include "../../modules/dialog_ng/dlg_hash.h"
#include "../ims_usrloc_pcscf/usrloc.h"

#include "../../mem/shm_mem.h"
#include "../../parser/sdp/sdp_helpr_funcs.h"
#include "../../parser/sdp/sdp.h"
#include "../../parser/parse_rr.h"
#include "../cdp/cdp_load.h"
#include "rx_authdata.h"
#include "rx_avp.h"
#include "../../lib/ims/ims_getters.h"
#include "mod.h"

int create_new_regsessiondata(str* domain, str* aor, rx_authsessiondata_t** session_data) {

	int len = (domain->len + 1) + aor->len + sizeof(rx_authsessiondata_t);
	rx_authsessiondata_t* p_session_data = shm_malloc(len);
	if (!p_session_data) {
		LM_ERR("no more shm memory\n");
		return -1;
	}
	memset(p_session_data, 0, len);

	p_session_data->subscribed_to_signaling_path_status = 1;
        p_session_data->must_terminate_dialog = 0; /*irrelevent for reg session data this will always be 0 */

	char* p = (char*)(p_session_data + 1);
	p_session_data->domain.s = p;
	memcpy(p, domain->s, domain->len);
	p_session_data->domain.len = domain->len;
	p += domain->len;
	*p++ = '\0';

	p_session_data->registration_aor.s = p;
	memcpy(p, aor->s, aor->len);
	p_session_data->registration_aor.len = aor->len;
	p += aor->len;
	if (p != (((char*)p_session_data) + len)) {
		LM_ERR("buffer over/underflow\n");
		shm_free(p_session_data);
		p_session_data = 0;
		return -1;
	}
	*session_data = p_session_data;

	return 1;
}

int create_new_callsessiondata(str* callid, str* ftag, str* ttag, str* identifier, str* ip, rx_authsessiondata_t** session_data) {

	int len = callid->len + ftag->len + ttag->len + identifier->len + ip->len + sizeof(rx_authsessiondata_t);
	rx_authsessiondata_t* call_session_data = shm_malloc(len);
	if (!call_session_data){
		LM_ERR("no more shm mem trying to create call_session_data of size %d\n", len);
		return -1;
	}
	memset(call_session_data, 0, len);
	call_session_data->subscribed_to_signaling_path_status = 0; //this is for a media session not regitration
        call_session_data->must_terminate_dialog = 0; //this is used to determine if the dialog must be torn down when the CDP session terminates

	char *p = (char*)(call_session_data + 1);

	if (callid && callid->len>0 && callid->s) {
		LM_DBG("Copying callid [%.*s] into call session data\n", callid->len, callid->s);
		call_session_data->callid.s = p;
		memcpy(call_session_data->callid.s, callid->s, callid->len);
                call_session_data->callid.len = callid->len;
		p+=callid->len;
	}
	if (ftag && ftag->len > 0 && ftag->s) {
		LM_DBG("Copying ftag [%.*s] into call session data\n", ftag->len, ftag->s);
		call_session_data->ftag.s = p;
		memcpy(call_session_data->ftag.s, ftag->s, ftag->len);
                call_session_data->ftag.len = ftag->len;
		p += ftag->len;
	}
	if (ttag && ttag->len > 0 && ttag->s) {
		LM_DBG("Copying ttag [%.*s] into call session data\n", ttag->len, ttag->s);
		call_session_data->ttag.s = p;
		memcpy(call_session_data->ttag.s, ttag->s, ttag->len);
                call_session_data->ttag.len = ttag->len;
		p += ttag->len;
	}
	if (identifier && identifier->len > 0 && identifier->s) {
		LM_DBG("Copying identifier [%.*s] into call session data\n", identifier->len, identifier->s);
		call_session_data->identifier.s = p;
		memcpy(call_session_data->identifier.s, identifier->s, identifier->len);
                call_session_data->identifier.len = identifier->len;
		p += identifier->len;
	}
	if (ip && ip->len > 0 && ip->s) {
		LM_DBG("Copying ip [%.*s] into call session data\n", ip->len, ip->s);
		call_session_data->ip.s = p;
		memcpy(call_session_data->ip.s, ip->s, ip->len);
                call_session_data->ip.len = ip->len;
		p += ip->len;
	}
	if (p != ((char*)(call_session_data) + len)) {
		LM_ERR("buffer under/overflow\n");
		shm_free(call_session_data);
		return -1;
	}

	*session_data = call_session_data;
	return 1;
}






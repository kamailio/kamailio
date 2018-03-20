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


#include "isc.h"

/**
 *	Forwards the message to the application server.
 * - Marks the message
 * - fills routes
 * - replaces dst_uri
 * @param msg - the SIP message
 * @param m  - the isc_match that matched with info about where to forward it 
 * @param mark  - the isc_mark that should be used to mark the message
 * @returns #ISC_RETURN_TRUE if OK, #ISC_RETURN_ERROR if not
 */
int isc_forward(struct sip_msg *msg, isc_match *m, isc_mark *mark, int firstflag) {
	struct cell *t;
	unsigned int hash, label;
	ticks_t fr_timeout, fr_inv_timeout;
	LM_DBG("marking for AS <%.*s>\n", m->server_name.len, m->server_name.s);

	isc_mark_set(msg, m, mark);
	/* change destination so it forwards to the app server */
	if (msg->dst_uri.s)
		pkg_free(msg->dst_uri.s);
	msg->dst_uri.s = pkg_malloc(m->server_name.len);
	if (!msg->dst_uri.s) {
		LM_ERR("error allocating %d bytes\n", m->server_name.len);
		return ISC_RETURN_ERROR;
	}
	msg->dst_uri.len = m->server_name.len;
	memcpy(msg->dst_uri.s, m->server_name.s, m->server_name.len);

	/* append branch if last trigger failed */
	if (is_route_type(FAILURE_ROUTE) && !firstflag)
		append_branch(msg, &(msg->first_line.u.request.uri), &(msg->dst_uri), 0, Q_UNSPECIFIED, 0, 0, 0, 0, 0, 0);

	// Determines the tm transaction identifiers.
	// If no transaction, then creates one

	if (isc_tmb.t_get_trans_ident(msg, &hash, &label) < 0) {
		LM_DBG("SIP message without transaction. OK - first request\n");
		if (isc_tmb.t_newtran(msg) < 0)
			LM_INFO("Failed creating SIP transaction\n");
		if (isc_tmb.t_get_trans_ident(msg, &hash, &label) < 0) {
			LM_INFO("SIP message still without transaction\n");
		} else {
			LM_DBG("New SIP message transaction %u %u\n", hash, label);
		}
	} else {
		LM_INFO("Transaction %u %u exists. Retransmission?\n", hash, label);
	}

	/* set the timeout timers to a lower value */

	t = isc_tmb.t_gett();
	fr_timeout = t->fr_timeout;
	fr_inv_timeout = t->fr_inv_timeout;
	t->fr_timeout = S_TO_TICKS(isc_fr_timeout) / 1000;
	t->fr_inv_timeout = S_TO_TICKS(isc_fr_inv_timeout) / 1000;

	/* send it */
	isc_tmb.t_relay(msg, 0, 0);

	/* recover the timeouts */
	t->fr_timeout = fr_timeout;
	t->fr_inv_timeout = fr_inv_timeout;

	LM_INFO(">>       msg was fwded to AS\n");

	return ISC_RETURN_TRUE;
}

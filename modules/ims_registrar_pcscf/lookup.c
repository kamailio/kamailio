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

#include <string.h>
#include "reg_mod.h"
#include "lookup.h"

extern usrloc_api_t ul;

/*! \brief
 * Lookup contact in the database and rewrite Request-URI
 * \return:  1 : contacts found and returned
 *          -1 : not found
 *          -2 : error
 */
int lookup_transport(struct sip_msg* _m, udomain_t* _d, str* _uri) {
    str uri;
    pcontact_t* pcontact;
    char tmp[MAX_URI_SIZE];
    str received_host = {0,0};
    str tmp_s;
    int ret = 1;

    if (_m->new_uri.s) uri = _m->new_uri;
    else uri = _m->first_line.u.request.uri;

    //now lookup in usrloc
    ul.lock_udomain(_d, &uri, &received_host, _m->rcv.src_port);
    if (ul.get_pcontact(_d, &uri, &received_host, _m->rcv.src_port, &pcontact) != 0) { //need to insert new contact
	LM_WARN("received request for contact that we don't know about\n");
	ret = -1;
	goto done;
    }
    
    if (pcontact->received_proto != _m->rcv.proto) {
	reset_dst_uri(_m);
	memset(tmp, 0, MAX_URI_SIZE);	
	switch (pcontact->received_proto) {
	    case PROTO_TCP:
		snprintf(tmp, MAX_URI_SIZE, "%.*s;transport=tcp", pcontact->aor.len, pcontact->aor.s);
		break;
	    case PROTO_UDP:
		snprintf(tmp, MAX_URI_SIZE, "%.*s;transport=udp", pcontact->aor.len, pcontact->aor.s);
		break;
	    default:
		LM_WARN("unsupported transport [%d]\n", pcontact->received_proto);
		ret = -2;
		goto done;
	}

	tmp_s.s = tmp;
	tmp_s.len = strlen(tmp);
	if (set_dst_uri(_m, &tmp_s) < 0) {
	    LM_ERR("failed to set dst_uri for terminating UE\n");
	    ret = -2;
	    goto done;
	}	
	LM_DBG("Changed dst URI transport for UE to [%.*s]\n", tmp_s.len, tmp_s.s);
    }
	
done:
    ul.unlock_udomain(_d, &uri, &received_host, _m->rcv.src_port);
    return ret;
}


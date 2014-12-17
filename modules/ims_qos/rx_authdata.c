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

int create_new_regsessiondata(str* domain, str* aor, str *ip, int ip_version, int recv_port, rx_authsessiondata_t** session_data) {

	int len = (domain->len + 1) + aor->len + ip->len + sizeof(rx_authsessiondata_t);
	rx_authsessiondata_t* p_session_data = shm_malloc(len);
	if (!p_session_data) {
		LM_ERR("no more shm memory\n");
		return -1;
	}
	memset(p_session_data, 0, len);

	p_session_data->subscribed_to_signaling_path_status = 1;
        p_session_data->must_terminate_dialog = 0; /*irrelevent for reg session data this will always be 0 */

	p_session_data->session_has_been_opened = 0; /*0 has not been opened 1 has been opened*/
	p_session_data->ip_version = ip_version;
	p_session_data->recv_port = recv_port;
	
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
	
	p_session_data->ip.s = p;
	memcpy(p, ip->s, ip->len);
	p_session_data->ip.len = ip->len;
	p += ip->len;
	
	if (p != (((char*)p_session_data) + len)) {
		LM_ERR("buffer over/underflow\n");
		shm_free(p_session_data);
		p_session_data = 0;
		return -1;
	}
	*session_data = p_session_data;

	return 1;
}

int create_new_callsessiondata(str* callid, str* ftag, str* ttag, str* identifier, int identifier_type, str* ip, int ip_version, rx_authsessiondata_t** session_data) {

	int len = callid->len + ftag->len + ttag->len + identifier->len + ip->len + sizeof(rx_authsessiondata_t);
	rx_authsessiondata_t* call_session_data = shm_malloc(len);
	if (!call_session_data){
		LM_ERR("no more shm mem trying to create call_session_data of size %d\n", len);
		return -1;
	}
	memset(call_session_data, 0, len);
	call_session_data->subscribed_to_signaling_path_status = 0; //this is for a media session not regitration
        call_session_data->must_terminate_dialog = 0; //this is used to determine if the dialog must be torn down when the CDP session terminates
	
	call_session_data->first_current_flow_description=0;
	call_session_data->first_new_flow_description=0;
	call_session_data->ip_version = ip_version;
	call_session_data->identifier_type = identifier_type;
	
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

/* Param current tells us if this a current fd or a new fd to add*/
int add_flow_description(rx_authsessiondata_t* session_data, int stream_num, str *media, str *req_sdp_ip_addr, str *req_sdp_port,
			str *rpl_sdp_ip_addr, str *rpl_sdp_port, str *rpl_sdp_transport, str *req_sdp_raw_stream, str *rpl_sdp_raw_stream, int direction, int current) {
    
    flow_description_t *fd = 0;
    flow_description_t *tmp, *tmp1 = 0;
    
    int len = media->len + req_sdp_ip_addr->len + req_sdp_port->len + rpl_sdp_ip_addr->len + rpl_sdp_port->len + rpl_sdp_transport->len + req_sdp_raw_stream->len + rpl_sdp_raw_stream->len + sizeof(flow_description_t);
    fd = shm_malloc(len);
    if (!fd){
	    LM_ERR("no more shm mem trying to create new flow description of size %d\n", len);
	    return -1;
    }
    memset(fd, 0, len);
    
    fd->direction = direction;
    fd->stream_num = stream_num;
    
    char *p = (char*)(fd + 1);

    if (media && media->len>0 && media->s) {
	    LM_DBG("Copying media [%.*s] into flow description\n", media->len, media->s);
	    fd->media.s = p;
	    memcpy(fd->media.s, media->s, media->len);
	    fd->media.len = media->len;
	    p+=media->len;
    }
    if (req_sdp_ip_addr && req_sdp_ip_addr->len>0 && req_sdp_ip_addr->s) {
	    LM_DBG("Copying req_sdp_ip_addr [%.*s] into flow description\n", req_sdp_ip_addr->len, req_sdp_ip_addr->s);
	    fd->req_sdp_ip_addr.s = p;
	    memcpy(fd->req_sdp_ip_addr.s, req_sdp_ip_addr->s, req_sdp_ip_addr->len);
	    fd->req_sdp_ip_addr.len = req_sdp_ip_addr->len;
	    p+=req_sdp_ip_addr->len;
    }
    if (req_sdp_port && req_sdp_port->len>0 && req_sdp_port->s) {
	    LM_DBG("Copying req_sdp_port [%.*s] into flow description\n", req_sdp_port->len, req_sdp_port->s);
	    fd->req_sdp_port.s = p;
	    memcpy(fd->req_sdp_port.s, req_sdp_port->s, req_sdp_port->len);
	    fd->req_sdp_port.len = req_sdp_port->len;
	    p+=req_sdp_port->len;
    }
    if (rpl_sdp_ip_addr && rpl_sdp_ip_addr->len>0 && rpl_sdp_ip_addr->s) {
	    LM_DBG("Copying rpl_sdp_ip_addr [%.*s] into flow description\n", rpl_sdp_ip_addr->len, rpl_sdp_ip_addr->s);
	    fd->rpl_sdp_ip_addr.s = p;
	    memcpy(fd->rpl_sdp_ip_addr.s, rpl_sdp_ip_addr->s, rpl_sdp_ip_addr->len);
	    fd->rpl_sdp_ip_addr.len = rpl_sdp_ip_addr->len;
	    p+=rpl_sdp_ip_addr->len;
    }
    if (rpl_sdp_port && rpl_sdp_port->len>0 && rpl_sdp_port->s) {
	    LM_DBG("Copying rpl_sdp_port [%.*s] into flow description\n", rpl_sdp_port->len, rpl_sdp_port->s);
	    fd->rpl_sdp_port.s = p;
	    memcpy(fd->rpl_sdp_port.s, rpl_sdp_port->s, rpl_sdp_port->len);
	    fd->rpl_sdp_port.len = rpl_sdp_port->len;
	    p+=rpl_sdp_port->len;
    }
    if (rpl_sdp_transport && rpl_sdp_transport->len>0 && rpl_sdp_transport->s) {
	    LM_DBG("Copying rpl_sdp_transport [%.*s] into flow description\n", rpl_sdp_transport->len, rpl_sdp_transport->s);
	    fd->rpl_sdp_transport.s = p;
	    memcpy(fd->rpl_sdp_transport.s, rpl_sdp_transport->s, rpl_sdp_transport->len);
	    fd->rpl_sdp_transport.len = rpl_sdp_transport->len;
	    p+=rpl_sdp_transport->len;
    }
    if (req_sdp_raw_stream && req_sdp_raw_stream->len>0 && req_sdp_raw_stream->s) {
	    LM_DBG("Copying req_sdp_raw_stream [%.*s] into flow description\n", req_sdp_raw_stream->len, req_sdp_raw_stream->s);
	    fd->req_sdp_raw_stream.s = p;
	    memcpy(fd->req_sdp_raw_stream.s, req_sdp_raw_stream->s, req_sdp_raw_stream->len);
	    fd->req_sdp_raw_stream.len = req_sdp_raw_stream->len;
	    p+=req_sdp_raw_stream->len;
    }
    if (rpl_sdp_raw_stream && rpl_sdp_raw_stream->len>0 && rpl_sdp_raw_stream->s) {
	    LM_DBG("Copying rpl_sdp_raw_stream [%.*s] into flow description\n", rpl_sdp_raw_stream->len, rpl_sdp_raw_stream->s);
	    fd->rpl_sdp_raw_stream.s = p;
	    memcpy(fd->rpl_sdp_raw_stream.s, rpl_sdp_raw_stream->s, rpl_sdp_raw_stream->len);
	    fd->rpl_sdp_raw_stream.len = rpl_sdp_raw_stream->len;
	    p+=rpl_sdp_raw_stream->len;
    }
	
    if (p != ((char*)(fd) + len)) {
	    LM_ERR("buffer under/overflow\n");
	    shm_free(fd);
	    return -1;
    }

    fd->next=0;

    if(current){
	LM_DBG("Adding current flow description\n");
	    if(session_data->first_current_flow_description == 0) {
		LM_DBG("This is the first\n");
		session_data->first_current_flow_description = fd;
	} else{
	    LM_DBG("This is NOT the first - adding to the list\n");
	    tmp = session_data->first_current_flow_description;
	     while (tmp) {
		tmp1 = tmp->next;
		if(!tmp1) {
		    break;
		}
		tmp = tmp1;
	    }
	    tmp->next = fd;
	}
    } else {
	LM_DBG("Adding new flow description\n");
	    if(session_data->first_new_flow_description == 0) {
		LM_DBG("This is the first\n");
		session_data->first_new_flow_description = fd;
	} else{
	    LM_DBG("This is NOT the first - adding to the list\n");
	    tmp = session_data->first_new_flow_description;
	    //scrolls to last valid entry
	    while (tmp) {
		tmp1 = tmp->next;
		if(!tmp1) {
		    break;
		}
		tmp = tmp1;
	    }
	    tmp->next = fd;
	}
    }
    
    return 1;
}

/* Param current tells us if this a current fd or a new fd to add*/
void free_flow_description(rx_authsessiondata_t* session_data, int current) {
    
    flow_description_t *flow_description;
    flow_description_t *flow_description_tmp;
    if(!session_data){
	return;
    }
    
    if(current) {
	LM_DBG("Destroy current flow description\n");
	flow_description = session_data->first_current_flow_description;
	if(!flow_description) {
	    return;
	}
    } else {
	LM_DBG("Destroy new flow description\n");
	flow_description = session_data->first_new_flow_description;
	if(!flow_description) {
	    return;
	}
    }
    
    while (flow_description) {
        flow_description_tmp = flow_description->next;
        shm_free(flow_description);
	flow_description = 0;
	flow_description = flow_description_tmp;
    }
}

void free_callsessiondata(rx_authsessiondata_t* session_data) {

    if(!session_data){
	return;
    }
    
    LM_DBG("Destroy current flow description\n");
    free_flow_description(session_data, 1);
    
    LM_DBG("Destroy new flow description\n");
    free_flow_description(session_data, 0);
    
    LM_DBG("Destroy session data\n");
    shm_free(session_data);
    session_data = 0;
}

void show_callsessiondata(rx_authsessiondata_t* session_data) {
    
    flow_description_t *flow_description;
    
    if(!session_data){
	return;
    }
    
    LM_DBG("Session data:\n");
    LM_DBG("=====================\n");
    LM_DBG("Call id [%.*s]\n", session_data->callid.len, session_data->callid.s);
    LM_DBG("Domain [%.*s]\n", session_data->domain.len, session_data->domain.s);
    LM_DBG("Ftag [%.*s]\n", session_data->ftag.len, session_data->ftag.s);
    LM_DBG("Ttag [%.*s]\n", session_data->ttag.len, session_data->ttag.s);
    LM_DBG("Identifier [%.*s]\n", session_data->identifier.len, session_data->identifier.s);
    LM_DBG("Registration AOR [%.*s]\n", session_data->registration_aor.len, session_data->registration_aor.s);
    LM_DBG("IP [%.*s]\n", session_data->ip.len, session_data->ip.s);
    LM_DBG("IP version [%d]\n", session_data->ip_version);
    LM_DBG("Must terminate dialog [%d]\n", session_data->must_terminate_dialog);
    LM_DBG("Subscribed to signalling path status [%d]\n", session_data->subscribed_to_signaling_path_status);
    
    flow_description = session_data->first_current_flow_description;
    while(flow_description) {
	LM_DBG("Current Flow description [%d]\n", flow_description->stream_num);
	LM_DBG("\tMedia [%.*s]\n", flow_description->media.len, flow_description->media.s);
	LM_DBG("\tReq_sdp_ip_addr [%.*s]\n", flow_description->req_sdp_ip_addr.len, flow_description->req_sdp_ip_addr.s);
	LM_DBG("\tReq_sdp_port [%.*s]\n", flow_description->req_sdp_port.len, flow_description->req_sdp_port.s);
	LM_DBG("\tReq_sdp_raw_stream [%.*s]\n", flow_description->req_sdp_raw_stream.len, flow_description->req_sdp_raw_stream.s);
	LM_DBG("\tRpl_sdp_ip_addr [%.*s]\n", flow_description->rpl_sdp_ip_addr.len, flow_description->rpl_sdp_ip_addr.s);
	LM_DBG("\tRpl_sdp_port [%.*s]\n", flow_description->rpl_sdp_port.len, flow_description->rpl_sdp_port.s);
	LM_DBG("\tRpl_sdp_raw_stream [%.*s]\n", flow_description->rpl_sdp_raw_stream.len, flow_description->rpl_sdp_raw_stream.s);
	LM_DBG("\tRpl_sdp_transport [%.*s]\n", flow_description->rpl_sdp_transport.len, flow_description->rpl_sdp_transport.s);
	LM_DBG("\tDirection [%d]\n", flow_description->direction);
	flow_description = flow_description->next;
    }
    flow_description = session_data->first_new_flow_description;
    while(flow_description) {
	LM_DBG("New Flow description [%d]\n", flow_description->stream_num);
	LM_DBG("\tMedia [%.*s]\n", flow_description->media.len, flow_description->media.s);
	LM_DBG("\tReq_sdp_ip_addr [%.*s]\n", flow_description->req_sdp_ip_addr.len, flow_description->req_sdp_ip_addr.s);
	LM_DBG("\tReq_sdp_port [%.*s]\n", flow_description->req_sdp_port.len, flow_description->req_sdp_port.s);
	LM_DBG("\tReq_sdp_raw_stream [%.*s]\n", flow_description->req_sdp_raw_stream.len, flow_description->req_sdp_raw_stream.s);
	LM_DBG("\tRpl_sdp_ip_addr [%.*s]\n", flow_description->rpl_sdp_ip_addr.len, flow_description->rpl_sdp_ip_addr.s);
	LM_DBG("\tRpl_sdp_port [%.*s]\n", flow_description->rpl_sdp_port.len, flow_description->rpl_sdp_port.s);
	LM_DBG("\tRpl_sdp_raw_stream [%.*s]\n", flow_description->rpl_sdp_raw_stream.len, flow_description->rpl_sdp_raw_stream.s);
	LM_DBG("\tRpl_sdp_transport [%.*s]\n", flow_description->rpl_sdp_transport.len, flow_description->rpl_sdp_transport.s);
	LM_DBG("\tDirection [%d]\n", flow_description->direction);
	flow_description = flow_description->next;
    }
    
    LM_DBG("=====================\n");
}


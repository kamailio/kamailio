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

#include "../../parser/contact/contact.h"
#include "save.h"
#include "reg_mod.h"
#include "ul_callback.h"
#include "subscribe.h"

#include "../pua/pua_bind.h"

extern struct tm_binds tmb;
extern usrloc_api_t ul;
extern time_t time_now;
extern unsigned int pending_reg_expires;
extern int subscribe_to_reginfo;
extern int subscription_expires;
extern pua_api_t pua;

struct sip_msg* get_request_from_reply(struct sip_msg* reply)
{
    struct cell *t;
	t = tmb.t_gett();
	if (!t || t == (void*) -1) {
		LM_ERR("Reply without transaction\n");
		return 0;
	}
	if (t)
		return t->uas.request;
	else
		return 0;

}

/**
 * Calculates the expiration time for one contact.
 * Tries to use the Expiration header, if not present then use the
 * expires parameter of the contact, if param not present it defaults
 * to the default value.
 * Also checks
 * @param c - the contact to calculate for
 * @param expires_hdr - value of expires hdr if present, if not -1
 * @param local_time_now - the local time
 * @returns the time of expiration
 */
static inline int calc_contact_expires(contact_t *c,int expires_hdr, int local_time_now)
{
    unsigned int r = 0;
	if (expires_hdr >= 0)
		r = expires_hdr;

	if (c && c->expires && c->expires->body.len) {
		str2int(&(c->expires->body), (unsigned int*) &r);
	}
	return local_time_now + r;
}


/**
 * Updates the registrar with the new values
 * @param req - the REGISTER request - to extract NAT info
 * @param rpl - the REGISTER reply - to extract contact info
 * @param is_star - whether this was a STAR contact header
 * @param expires_hdr - value of the Expires header
 * @param public_id - array of public identities attached to this contact
 * @param public_id_cnt - size of the public_id array
 * @param service_route - array of Service-Routes
 * @param service_route_cnt - size of the service_route array
 * @param requires_nat - if to create pinholes
 * @returns the maximum expiration time, -1 on error
 */
static inline int update_contacts(struct sip_msg *req,struct sip_msg *rpl, udomain_t* _d, unsigned char is_star,int expires_hdr,
        str *public_id,int public_id_cnt,str *service_route,int service_route_cnt, int requires_nat)
{
	int local_time_now, expires=0;
	struct hdr_field* h;
	contact_t* c;
	struct sip_uri puri;
	struct sip_uri parsed_received;
	struct pcontact_info ci;
	pcontact_t* pcontact;
	char srcip[50];
	int_str val;

	pcscf_act_time();
	local_time_now = time_now;
	if (is_star) {
		/* first of all, we shouldn't get here...
		 * then, we will update on NOTIFY */
		return 0;
	}

	// Set the structure to "0", to make sure it's properly initialized
	memset(&ci, 0, sizeof(struct pcontact_info));

	for (h = rpl->contact; h; h = h->next) {
		if (h->type == HDR_CONTACT_T && h->parsed)
			for (c = ((contact_body_t*) h->parsed)->contacts; c; c = c->next) {
				expires = calc_contact_expires(c, expires_hdr, local_time_now);
				if (parse_uri(c->uri.s, c->uri.len, &puri) < 0) {
					LM_DBG("Error parsing Contact URI <%.*s>\n", c->uri.len, c->uri.s);
					continue;
				}
				//build contact info
				ci.expires = expires;
				ci.public_ids = public_id;
				ci.num_public_ids = public_id_cnt;
				ci.service_routes = service_route;
				ci.num_service_routes = service_route_cnt;
				ci.reg_state = PCONTACT_REGISTERED;

				ci.received_host.len = 0;
				ci.received_host.s = 0;
				ci.received_port = 0;
				ci.received_proto = 0;

				// Received Info: First try AVP, otherwise simply take the source of the request:
				memset(&val, 0, sizeof(int_str));
				if (rcv_avp_name.n!=0 && search_first_avp(rcv_avp_type, rcv_avp_name, &val, 0) && val.s.len > 0) {
					if (val.s.len>RECEIVED_MAX_SIZE) {
						LM_ERR("received too long\n");
						goto error;
					}
					if (parse_uri(val.s.s, val.s.len, &parsed_received) < 0) {
						LM_DBG("Error parsing Received URI <%.*s>\n", val.s.len, val.s.s);
						continue;
					}
					ci.received_host = parsed_received.host;
					ci.received_port = parsed_received.port_no;
					ci.received_proto = parsed_received.proto;
				} else {
					ci.received_host.len = ip_addr2sbuf(&req->rcv.src_ip, srcip, sizeof(srcip));
					ci.received_host.s = srcip;
					ci.received_port = req->rcv.src_port;
					ci.received_proto = req->rcv.proto;
				}
				// Set to default, if not set:
				if (ci.received_port == 0) ci.received_port = 5060;

				
				ul.lock_udomain(_d, &c->uri, &ci.received_host, ci.received_port);
				if (ul.get_pcontact(_d, &c->uri, &ci.received_host, ci.received_port, &pcontact) != 0) { //need to insert new contact
					if ((expires-local_time_now)<=0) { //remove contact - de-register
						LM_DBG("This is a de-registration for contact <%.*s> but contact is not in usrloc - ignore\n", c->uri.len, c->uri.s);
						goto next_contact;
					} 
				    
					LM_DBG("Adding pcontact: <%.*s>, expires: %d which is in %d seconds\n", c->uri.len, c->uri.s, expires, expires-local_time_now);

					if (ul.insert_pcontact(_d, &c->uri, &ci, &pcontact) != 0) {
						LM_ERR("Failed inserting new pcontact\n");
					} else {
						//register for callbacks on this contact so we can send PUBLISH to SCSCF should status change
						LM_DBG("registering for UL callback\n");
						ul.register_ulcb(pcontact, PCSCF_CONTACT_DELETE | PCSCF_CONTACT_EXPIRE, callback_pcscf_contact_cb, NULL);
						//we also need to subscribe to reg event of this contact at SCSCF
					}
				} else { //contact already exists - update
					LM_DBG("contact already exists and is in state (%d) : [%s]\n",pcontact->reg_state, reg_state_to_string(pcontact->reg_state));
					if ((expires-local_time_now)<=0) { //remove contact - de-register
						LM_DBG("This is a de-registration for contact <%.*s>\n", c->uri.len, c->uri.s);
						if (ul.delete_pcontact(_d, &c->uri, &ci.received_host, ci.received_port, pcontact) != 0) {
							LM_ERR("failed to delete pcscf contact <%.*s>\n", c->uri.len, c->uri.s);
						}
					} else { //update contact
						LM_DBG("Updating contact: <%.*s>, old expires: %li, new expires: %i which is in %i seconds\n", c->uri.len, c->uri.s,
								pcontact->expires-local_time_now,
								expires,
								expires-local_time_now);
						if (ul.update_pcontact(_d, &ci, pcontact) != 0) {
							LM_ERR("failed to update pcscf contact\n");
						}
						pcontact->expires = expires;
					}
				}
next_contact:
				ul.unlock_udomain(_d, &c->uri, &ci.received_host, ci.received_port);
			}
	}
	return 1;
error:
	return 0;
}

/**
 * Save contact based on REGISTER request. this will be a pending save, until we receive response
 * from SCSCF. If no response after pending_timeout seconds, the contacts is removed
 */
int save_pending(struct sip_msg* _m, udomain_t* _d) {
	contact_body_t* cb = 0;
	int cexpires = 0;
	pcontact_t* pcontact;
	contact_t* c;
	struct pcontact_info ci;
	int_str val;
	struct sip_uri parsed_received;
	char srcip[50];

	memset(&ci, 0, sizeof(struct pcontact_info));

	cb = cscf_parse_contacts(_m);
	if (!cb || (!cb->contacts)) {
		LM_ERR("No contact headers\n");
		goto error;
	}
	c = cb->contacts;
	if (!c) {
		LM_ERR("no valid contact to register\n");
		goto error;
	}
	//TODO: need support for multiple contacts - currently assume one contact
	//make sure this is not a de-registration
	int expires_hdr = cscf_get_expires_hdr(_m, 0);
	if (expires_hdr < 0) {
		//no global header we have to check the contact expiry
		if (c && c->expires && c->expires->body.len) {
				str2int(&(c->expires->body), (unsigned int*) &cexpires);
		}
		if (!cexpires){ //assume de-registration
			LM_DBG("not doing pending reg on de-registration\n");
			return 1;
		}
	}
	pcscf_act_time();
	int local_time_now = time_now;
	int expires = calc_contact_expires(c, expires_hdr, local_time_now);
	if (expires <= 0) {
		LM_DBG("not doing pending reg on de-registration\n");
		return 1;
	}
	LM_DBG("contact requesting to expire in %d seconds\n", expires-local_time_now);

	/*populate CI with bare minimum*/
	ci.num_public_ids=0;
	ci.num_service_routes=0;
	ci.expires=local_time_now + pending_reg_expires;
	ci.reg_state=PCONTACT_REG_PENDING;

	// Received Info: First try AVP, otherwise simply take the source of the request:
	memset(&val, 0, sizeof(int_str));
	if (rcv_avp_name.n != 0
			&& search_first_avp(rcv_avp_type, rcv_avp_name, &val, 0)
			&& val.s.len > 0) {
		if (val.s.len > RECEIVED_MAX_SIZE) {
			LM_ERR("received too long\n");
			goto error;
		}
		if (parse_uri(val.s.s, val.s.len, &parsed_received) < 0) {
			LM_DBG("Error parsing Received URI <%.*s>\n", val.s.len, val.s.s);
			goto error;
		}
		ci.received_host = parsed_received.host;
		ci.received_port = parsed_received.port_no;
		ci.received_proto = parsed_received.proto;
	} else {
		ci.received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip,
				sizeof(srcip));
		ci.received_host.s = srcip;
		ci.received_port = _m->rcv.src_port;
		ci.received_proto = _m->rcv.proto;
	}
	// Set to default, if not set:
	if (ci.received_port == 0)
		ci.received_port = 5060;

	ul.lock_udomain(_d, &c->uri, &ci.received_host, ci.received_port);
	if (ul.get_pcontact(_d, &c->uri, &ci.received_host, ci.received_port, &pcontact) != 0) { //need to insert new contact
		LM_DBG("Adding pending pcontact: <%.*s>\n", c->uri.len, c->uri.s);
		if (ul.insert_pcontact(_d, &c->uri, &ci, &pcontact) != 0) {
			LM_ERR("Failed inserting new pcontact\n");
		} else {
			LM_DBG("registering for UL callback\n");
			ul.register_ulcb(pcontact, PCSCF_CONTACT_DELETE | PCSCF_CONTACT_EXPIRE | PCSCF_CONTACT_UPDATE, callback_pcscf_contact_cb, NULL);
		}
	} else { //contact already exists - update
		LM_DBG("Contact already exists - not doing anything for now\n");
	}
	ul.unlock_udomain(_d, &c->uri, &ci.received_host, ci.received_port);

	return 1;

error:
	LM_DBG("Error saving pending contact\n");
	return -1;
}

/**
 * Save the contacts and their associated public ids.
 * @param rpl - the SIP Register 200 OK response that contains the Expire and Contact and P-associated-uri headers
 * @param _d - domain
 * @param _cflags - flags
 * @returns #CSCF_RETURN_TRUE if OK, #CSCF_RETURN_ERROR on error
 */
int save(struct sip_msg* _m, udomain_t* _d, int _cflags) {
	struct sip_msg* req;
	int expires_hdr = 0;
	contact_body_t* cb = 0;
	str *public_ids=0;
	int num_public_ids = 0;
	str *service_routes=0;
	int num_service_routes = 0;
	pv_elem_t *presentity_uri_pv;
	
	//get request from reply
	req = get_request_from_reply(_m);
	if (!req) {
		LM_ERR("Unable to get request from reply for REGISTER. No transaction\n");
		goto error;
	}
	expires_hdr = cscf_get_expires_hdr(_m, 0);
	cb = cscf_parse_contacts(_m);
	if (!cb || (!cb->contacts && !cb->star)) {
		LM_ERR("No contact headers and not *\n");
		goto error;
	}
	cscf_get_p_associated_uri(_m, &public_ids, &num_public_ids, 1);
	service_routes = cscf_get_service_route(_m, &num_service_routes, 1);

	//update contacts
	if (!update_contacts(req, _m, _d, cb->star, expires_hdr, public_ids, num_public_ids, service_routes, num_service_routes, 0)) {
		LM_ERR("failed to update pcontact\n");
		goto error;
	}

	if(subscribe_to_reginfo == 1){
	    
	    //use the first p_associated_uri - i.e. the default IMPU
	    LM_DBG("Subscribe to reg event for primary p_associated_uri");
	    if(num_public_ids > 0){
		//find the first routable (not a tel: URI and use that that presentity)
		//if you can not find one then exit
		int i = 0;
		int found_presentity_uri=0;
		while (i < num_public_ids && found_presentity_uri == 0)
		{
		    //check if public_id[i] is NOT a tel URI - if it isn't then concert to pv format and set found presentity_uri to 1
		    if (strncasecmp(public_ids[i].s,"tel:",4)==0) {
			LM_DBG("This is a tel URI - it is not routable so we don't use it to subscribe");
			i++;
		    }
		    else {
			//convert primary p_associated_uri to pv_elem_t
			if(pv_parse_format(&public_ids[i], &presentity_uri_pv)<0) {
				LM_ERR("wrong format[%.*s]\n",public_ids[i].len, public_ids[i].s);
				goto error;
			}
			found_presentity_uri=1;
		    }
		}
		if(found_presentity_uri!=1){
		    LM_ERR("Could not find routable URI in p_assoiated_uri list - failed to subscribe");
		    goto error;
		}
	    }else{
		//Now some how check if there is a pua record and what the presentity uri is from there - if nothing there
		LM_DBG("No p_associated_uri in 200 OK this must be a de-register - we ignore this - will unsubscribe when the notify is received");
		goto done;
		
	    }
	    reginfo_subscribe_real(_m, presentity_uri_pv, service_routes, subscription_expires);
	    pv_elem_free_all(presentity_uri_pv);
	}
    
done:
	if (public_ids && public_ids->s) pkg_free(public_ids);
	if (service_routes && service_routes->s) pkg_free(service_routes);
	return 1;

error:
	if (public_ids && public_ids->s) pkg_free(public_ids);
	if (service_routes && service_routes->s) pkg_free(service_routes);
	return -1;

}

/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * Copyright (C) 2019 Aleksandar Yosifov
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

#include "../../core/parser/contact/contact.h"
#include "save.h"
#include "ims_registrar_pcscf_mod.h"
#include "ul_callback.h"
#include "subscribe.h"

#include "../pua/pua_bind.h"
#include "../ims_ipsec_pcscf/cmd.h"
#include "sec_agree.h"

extern struct tm_binds tmb;
extern usrloc_api_t ul;
extern time_t time_now;
extern unsigned int pending_reg_expires;
extern int subscribe_to_reginfo;
extern int subscription_expires;
extern pua_api_t pua;
extern ipsec_pcscf_api_t ipsec_pcscf;

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
	struct pcontact_info ci;
	pcontact_t* pcontact;
	unsigned short port, proto;
	char *alias_start, *p, *port_s, *proto_s;
	char portbuf[5];
	str alias_s;

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
		if (h->type == HDR_CONTACT_T && h->parsed) {
			for (c = ((contact_body_t*) h->parsed)->contacts; c; c = c->next) {
				expires = calc_contact_expires(c, expires_hdr, local_time_now);
				if (parse_uri(c->uri.s, c->uri.len, &puri) < 0) {
					LM_DBG("Error parsing Contact URI <%.*s>\n", c->uri.len, c->uri.s);
					continue;
				}
				//build contact info
				ci.aor = c->uri;
				ci.expires = expires;
				ci.public_ids = public_id;
				ci.num_public_ids = public_id_cnt;
				ci.service_routes = service_route;
				ci.num_service_routes = service_route_cnt;
				ci.reg_state = PCONTACT_REGISTERED|PCONTACT_REG_PENDING|PCONTACT_REG_PENDING_AAR;   //we don't want to add contacts that did not come through us (pcscf)
				
				ci.received_host.len = 0;
				ci.received_host.s = 0;
				ci.received_port = 0;
				ci.received_proto = 0;
				port = puri.port_no ? puri.port_no : 5060;
				ci.via_host = puri.host;
				ci.via_port = port;
				ci.via_prot = puri.proto;
				ci.searchflag = SEARCH_NORMAL; /* this must be reset for each contact iteration */

				if (puri.params.len > 6 && (alias_start = _strnistr(puri.params.s, "alias=", puri.params.len)) != NULL) {
					LM_DBG("contact has an alias [%.*s] - we can use that as the received\n", puri.params.len, puri.params.s);
					alias_s.len = puri.params.len - (alias_start - puri.params.s) - 6;
					alias_s.s = alias_start + 6;
					LM_DBG("alias [%.*s]\n", alias_s.len, alias_s.s);
					p = _strnistr(alias_s.s, "~", alias_s.len);
					if (p!=NULL) {
						ci.received_host.s = alias_s.s;
						ci.received_host.len = p - alias_s.s;
						LM_DBG("alias(host) [%.*s]\n", ci.received_host.len, ci.received_host.s);
						port_s = p+1;
						p = _strnistr(port_s, "~", alias_s.len - ci.received_host.len);
						if (p!=NULL) {
							LM_DBG("alias(port) [%.*s]\n", (int)(p - port_s) , port_s);
							memset(portbuf, 0, 5);
							memcpy(portbuf, port_s, (p-port_s));
							port = atoi(portbuf);
							LM_DBG("alias(port) [%d]\n", port);
							
							proto_s = p + 1;
							memset(portbuf, 0, 5);
							memcpy(portbuf, proto_s, 1);
							proto = atoi(portbuf);
							LM_DBG("alias(proto) [%d]\n", proto);
							ci.received_port = port;
							ci.received_proto = proto;
							ci.searchflag = SEARCH_RECEIVED;
						}
					}
				} 
				
				ul.lock_udomain(_d, &puri.host, port, puri.proto);
				if (ul.get_pcontact(_d, &ci, &pcontact) != 0) { //need to insert new contact
					if ((expires-local_time_now)<=0) { //remove contact - de-register
						LM_DBG("This is a de-registration for contact <%.*s> but contact is not in usrloc - ignore\n", c->uri.len, c->uri.s);
						goto next_contact;
					} 
					LM_DBG("We don't add contact from the 200OK that did not go through us (ie, not present in explicit REGISTER that went through us\n");
				} else { //contact already exists - update
					LM_DBG("contact already exists and is in state (%d) : [%s]\n",pcontact->reg_state, reg_state_to_string(pcontact->reg_state));
					if ((expires-local_time_now)<=0) { //remove contact - de-register
						LM_DBG("This is a de-registration for contact <%.*s>\n", c->uri.len, c->uri.s);
						if (ul.delete_pcontact(_d, pcontact) != 0) {
							LM_ERR("failed to delete pcscf contact <%.*s>\n", c->uri.len, c->uri.s);
						}
                                                //TODO_LATEST replace above
					} else { //update contact
						LM_DBG("Updating contact: <%.*s>, old expires: %li, new expires: %i which is in %i seconds\n", c->uri.len, c->uri.s,
								pcontact->expires-local_time_now,
								expires,
								expires-local_time_now);
						ci.reg_state = PCONTACT_REGISTERED;
						if (ul.update_pcontact(_d, &ci, pcontact) != 0) {
							LM_DBG("failed to update pcscf contact\n");
						}else{
							// Register callback to destroy related tunnels to this contact.
							// The registration should be exact here, after the successfuly registration of the UE
							LM_DBG("ul.register_ulcb(pcontact, PCSCF_CONTACT_EXPIRE|PCSCF_CONTACT_DELETE...)\n");
							if(ul.register_ulcb(pcontact, PCSCF_CONTACT_EXPIRE|PCSCF_CONTACT_DELETE, ipsec_pcscf.ipsec_on_expire, NULL) != 1){
								LM_DBG("Error subscribing for contact\n");
							}

							// After successful registration try to unregister all callbacks for pending contacts ralated to this contact.
							ul.unreg_pending_contacts_cb(_d, pcontact, PCSCF_CONTACT_EXPIRE);
						}
						pcontact->expires = expires;
					}
				}
next_contact:
				ul.unlock_udomain(_d, &puri.host, port, puri.proto);
			}
		}
	}
	return 1;
}

/**
 * Save contact based on REGISTER request. this will be a pending save, until we receive response
 * from SCSCF. If no response after pending_timeout seconds, the contacts is removed. Can only be used from REQUEST ROUTE
 */
int save_pending(struct sip_msg* _m, udomain_t* _d) {
	contact_body_t* cb = 0;
	int cexpires = 0;
	pcontact_t* pcontact;
	contact_t* c;
	struct pcontact_info ci;
	struct via_body* vb;
	unsigned short port, proto;
	int_str val;
	struct sip_uri parsed_received;
	char srcip[50];

	memset(&ci, 0, sizeof(struct pcontact_info));
        
	vb = cscf_get_ue_via(_m);
	port = vb->port?vb->port:5060;
	proto = vb->proto;

	cb = cscf_parse_contacts(_m);
	if (!cb || (!cb->contacts)) {
		LM_ERR("No contact headers\n");
		goto error;
	}
        
	c = cb->contacts;
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

	LM_DBG("Save pending contact with AOR [%.*s], proto %d, port %d\n", c->uri.len, c->uri.s, proto, port);
	LM_DBG("contact requesting to expire in %d seconds\n", expires-local_time_now);

	/*populate CI with bare minimum*/
	ci.via_host = vb->host;
	ci.via_port = port;
	ci.via_prot = proto;
	ci.aor = c->uri;
	ci.num_public_ids=0;
	ci.num_service_routes=0;
	ci.expires=local_time_now + pending_reg_expires;
	ci.reg_state=PCONTACT_ANY;
	ci.searchflag=SEARCH_RECEIVED;  //we want to make sure we are very specific with this search to make sure we get the correct contact to put into reg_pending.

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

	// Parse security parameters
	security_t* sec_params = NULL;
	if((sec_params = cscf_get_security(_m)) == NULL) {
		LM_DBG("Will save pending contact without security parameters\n");
	}

	// Parse security-verify parameters
	security_t* sec_verify_params = NULL;
	if((sec_verify_params = cscf_get_security_verify(_m)) == NULL){
		LM_DBG("Will save pending contact without security-verify parameters\n");
	}else{
		if(sec_params){
			// for REGISTER request try to set spi pc and spi ps from security-verify header
			sec_params->data.ipsec->spi_ps = sec_verify_params->data.ipsec->spi_us;
			sec_params->data.ipsec->spi_pc = sec_verify_params->data.ipsec->spi_uc;

			// Get from verify header pcscf server and client ports
			sec_params->data.ipsec->port_ps = sec_verify_params->data.ipsec->port_us;
			sec_params->data.ipsec->port_pc = sec_verify_params->data.ipsec->port_uc;

			LM_DBG("Will save pending contact with security-verify parameters, spc_ps %u, spi_pc %u, port_ps %u, port_pc %u\n",
					sec_params->data.ipsec->spi_ps, sec_params->data.ipsec->spi_pc, sec_params->data.ipsec->port_ps, sec_params->data.ipsec->port_pc);
		}
	}

	ul.lock_udomain(_d, &ci.via_host, ci.via_port, ci.via_prot);
	if (ul.get_pcontact(_d, &ci, &pcontact) != 0) { //need to insert new contact
		ipsec_pcscf.ipsec_reconfig(); // try to clean all ipsec SAs/Policies if there is no registered contacts

		LM_DBG("Adding pending pcontact: <%.*s>\n", c->uri.len, c->uri.s);
		ci.reg_state=PCONTACT_REG_PENDING;
		if (ul.insert_pcontact(_d, &c->uri, &ci, &pcontact) != 0) {
			LM_ERR("Failed inserting new pcontact\n");
		} else {
			LM_DBG("registering for UL callback\n");
			ul.register_ulcb(pcontact, PCSCF_CONTACT_DELETE | PCSCF_CONTACT_EXPIRE | PCSCF_CONTACT_UPDATE, callback_pcscf_contact_cb, NULL);

			// Update security parameters only for the pending contacts
			if(sec_params){
				if(ul.update_temp_security(_d, sec_params->type, sec_params, pcontact) != 0){
					LM_ERR("Error updating temp security\n");
				}
			}
		}
	} else { //contact already exists - update
        LM_DBG("Contact already exists - not doing anything for now\n");
	}

	ul.unlock_udomain(_d, &ci.via_host, ci.via_port, ci.via_prot);


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
	int contact_has_sos=-1;
	contact_t* chi; //contact header information
	struct hdr_field* h;
	//get request from reply
	req = get_request_from_reply(_m);
	if (!req) {
		LM_ERR("Unable to get request from reply for REGISTER. No transaction\n");
		goto error;
	}
	expires_hdr = cscf_get_expires_hdr(_m, 0);
	
	if((parse_headers(_m, HDR_CONTACT_F, 0) == -1) || !_m->contact) {
		LM_ERR("cannot get the Contact header from the SIP message in saving action in PCSCF\n");
		goto error;
	}

	if(!_m->contact->parsed && parse_contact(_m->contact) < 0) {
		LM_ERR("Couldn t parse Contact Header \n");
		goto error;
	}
	
	cb = ((contact_body_t *)_m->contact->parsed);
	if (!cb || (!cb->contacts && !cb->star)) {
		LM_DBG("No contact headers and not *\n");
		goto error;
	}
	
	for (h = _m->contact; h; h = h->next) {
	 if (h->type == HDR_CONTACT_T && h->parsed) {
	    for (chi = ((contact_body_t*) h->parsed)->contacts; chi; chi = chi->next) {
	      contact_has_sos = cscf_get_sos_uri_param(chi->uri);
	      if(contact_has_sos!=-1){
	        break;
	      }
	    }
	  }
	}
	
	cscf_get_p_associated_uri(_m, &public_ids, &num_public_ids, 1);
	service_routes = cscf_get_service_route(_m, &num_service_routes, 1);

	//update contacts
	if (!update_contacts(req, _m, _d, cb->star, expires_hdr, public_ids, num_public_ids, service_routes, num_service_routes, 0)) {
		LM_ERR("failed to update pcontact\n");
		goto error;
	}

	if(subscribe_to_reginfo == 1 && contact_has_sos < 1){
	    
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

/** 
 * Functions to force or check the service-routes
 *
 * Copyright (c) 2013 Carsten Bock, ng-voice GmbH
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
 */

#include "service_routes.h"
#include "reg_mod.h"
#include "save.h"
#include "../../data_lump.h"
#include "../../lib/ims/ims_getters.h"

#define STR_APPEND(dst,src)\
        {memcpy((dst).s+(dst).len,(src).s,(src).len);\
        (dst).len = (dst).len + (src).len;}

// ID of current message
static unsigned int current_msg_id = 0;
// Pointer to current contact_t
static pcontact_t * c = NULL;

extern usrloc_api_t ul;
extern int ignore_contact_rxport_check;
static str * asserted_identity;
static str * registration_contact;

/*!
 * \brief Parse the message and find first occurrence of Route header field.
 * \param _m SIP message 
 * \return -1 or -2 on a parser error, 0 if there is a Route HF and 1 if there is no Route HF
 */
static inline int find_first_route(struct sip_msg* _m)
{
	if (parse_headers(_m, HDR_ROUTE_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	} else {
		if (_m->route) {
			if (parse_rr(_m->route) < 0) {
				LM_ERR("failed to parse Route HF\n");
				return -2;
			}
			return 0;
		} else {
			LM_DBG("No Route headers found\n");
			return 1;
		}
	}
}

/*!
 * \brief Find and parse next Route header field
 * \param _m SIP message
 * \param _hdr SIP header
 * \return negative on failure, 0 if the Route header was already parsed, 1 if no next
 * Route header could be found
 */
static inline int find_next_route(struct sip_msg* _m, struct hdr_field** _hdr)
{
	struct hdr_field* ptr;

	ptr = (*_hdr)->next;

	/* Try to find already parsed Route headers */
	while(ptr) {
		if (ptr->type == HDR_ROUTE_T) goto found;
		ptr = ptr->next;
	}

	/* There are no already parsed Route headers, try to find next
	 * occurrence of Route header
	 */
	if (parse_headers(_m, HDR_ROUTE_F, 1) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	if ((_m->last_header->type!=HDR_ROUTE_T) || (_m->last_header==*_hdr)) {
		LM_DBG("No next Route HF found\n");
		return 1;
	}

	ptr = _m->last_header;

 found:
	if (parse_rr(ptr) < 0) {
		LM_ERR("failed to parse Route body\n");
		return -2;
	}

	*_hdr = ptr;
	return 0;
}

int checkcontact(struct sip_msg* _m, pcontact_t * c) {
	int security_server_port = -1;
	str received_host = {0, 0};
	char srcip[50];	

	LM_DBG("Port %d (search %d), Proto %d (search %d), reg_state %s (search %s)\n",
		c->received_port, _m->rcv.src_port, c->received_proto, _m->rcv.proto,
		reg_state_to_string(c->reg_state), reg_state_to_string(PCONTACT_REGISTERED)
	);

	if (c->security) {
		switch (c->security->type) {
		case SECURITY_IPSEC:
			security_server_port = c->security->data.ipsec->port_uc;
			break;
		case SECURITY_TLS:
		case SECURITY_NONE:
			break;
		}
	} else if (c->security_temp) {
		switch (c->security->type) {
		case SECURITY_IPSEC:
			security_server_port = c->security->data.ipsec->port_uc;
			break;
		case SECURITY_TLS:
		case SECURITY_NONE:
			break;
		}
	}

	if ((ignore_reg_state || (c->reg_state == PCONTACT_REGISTERED)) 
                && (ignore_contact_rxport_check || (c->received_port == _m->rcv.src_port) || (security_server_port == _m->rcv.src_port))
                && (ignore_contact_rxport_check||(c->received_proto == _m->rcv.proto))) {
            
		received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip, sizeof(srcip));
		received_host.s = srcip;
		LM_DBG("Received host len %d (search %d)\n", c->received_host.len, received_host.len);
		// Then check the length:
		if (c->received_host.len == received_host.len) {
			LM_DBG("Received host %.*s (search %.*s)\n",
				c->received_host.len, c->received_host.s,
				received_host.len, received_host.s);

			// Finally really compare the "received_host"
			if (!memcmp(c->received_host.s, received_host.s, received_host.len))
				return 0;
		}
	}
	return 1;
}

/**
 * get PContact-Structure for message
 * (search only once per Request)
 */
pcontact_t * getContactP(struct sip_msg* _m, udomain_t* _d) {
	ppublic_t * p;
	contact_body_t *b = 0;
	contact_t *ct;
	str received_host = {0, 0};
	char srcip[50];	

	received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip, sizeof(srcip));
			received_host.s = srcip;
			
	if (_m->id != current_msg_id) {
		current_msg_id = _m->id;
		c = NULL;

		if (is_registered_fallback2ip == 2) {
			LM_DBG("Searching in usrloc for %.*s:%i (Proto %i)\n",
				received_host.len, received_host.s,
				_m->rcv.src_port, _m->rcv.proto);

			if (ul.get_pcontact_by_src(_d, &received_host, _m->rcv.src_port, _m->rcv.proto, &c) == 1) {
				LM_DBG("No entry in usrloc for %.*s:%i (Proto %i) found!\n", received_host.len, received_host.s, _m->rcv.src_port, _m->rcv.proto);
			} else {
				if (checkcontact(_m, c) != 0) {
					c = NULL;
				}
			}
		}

		if (c == NULL) {
			b = cscf_parse_contacts(_m);

			if (b && b->contacts) {
				for (ct = b->contacts; ct; ct = ct->next) {
					if (ul.get_pcontact(_d, &ct->uri, &received_host, _m->rcv.src_port, &c) == 0) {
						if (checkcontact(_m, c) != 0) {
							c = NULL;
						} else {
							break;
						}
					}
				}
			} else {
				LM_WARN("No contact-header found?!?\n");
			}
		}

		if ((c == NULL) && (is_registered_fallback2ip == 1)) {
			LM_INFO("Contact not found based on Contact-header, trying IP/Port/Proto\n");
			received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip, sizeof(srcip));
			received_host.s = srcip;
			if (ul.get_pcontact_by_src(_d, &received_host, _m->rcv.src_port, _m->rcv.proto, &c) == 1) {
				LM_DBG("No entry in usrloc for %.*s:%i (Proto %i) found!\n", received_host.len, received_host.s, _m->rcv.src_port, _m->rcv.proto);
			} else {
				if (checkcontact(_m, c) != 0) {
					c = NULL;
				}
			}
		}
	}
	asserted_identity = NULL;
	registration_contact = NULL;
	if (c) {
		registration_contact = &c->contact_user;
		p = c->head;
		while (p) {
			if (p->is_default == 1)
				asserted_identity = &p->public_identity;
			p = p->next;
		}
	}

	return c;
}

pcontact_t * getContactP_from_via(struct sip_msg* _m, udomain_t* _d) {
	ppublic_t * p;
	struct via_body *vb;

	vb = cscf_get_ue_via(_m);
	if (!vb) {
		LM_WARN("no via header.....strange!\n");
		return NULL;
	}

	if (vb->port == 0)
		vb->port = 5060;

	if (_m->id != current_msg_id) {
		current_msg_id = _m->id;
		c = NULL;
		LM_DBG("Looking for <%d://%.*s:%d>\n", vb->proto, vb->host.len, vb->host.s, vb->port);
		if (ul.get_pcontact_by_src(_d, &vb->host, vb->port, vb->proto, &c) == 1)
			LM_WARN("No entry in usrloc for %.*s:%i (Proto %i) found!\n", vb->host.len, vb->host.s, vb->port, vb->proto);
	}

	asserted_identity = NULL;
	if (c) {
		p = c->head;
		while (p) {
			if (p->is_default == 1)
				asserted_identity = &p->public_identity;
			p = p->next;
		}
	}

	return c;
}

/**
 * Check, if a user-agent follows the indicated service-routes
 */
int check_service_routes(struct sip_msg* _m, udomain_t* _d) {
	struct sip_uri uri;
	int i;
	struct hdr_field *hdr;
	rr_t *r;
	char srcip[20];
	str received_host;
	pcontact_t * c = getContactP(_m, _d);
	/* Contact not found => not following service-routes */
	if (c == NULL) return -1;

	/* Search for the first Route-Header: */
	if (find_first_route(_m) < 0) return -1;

	LM_DBG("Got %i Route-Headers.\n", c->num_service_routes);

	received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip, sizeof(srcip));
	received_host.s = srcip;
	
	/* Lock this record while working with the data: */
	ul.lock_udomain(_d, &c->aor, &received_host, _m->rcv.src_port);

	/* Check the route-set: */
	if (_m->route) {
		hdr = _m->route;
		LM_DBG("hdr is %p\n", hdr);
		/* Check, if the first host is ourself: */
		r = (rr_t*)hdr->parsed;
		if (r) {
			LM_DBG("Route is %.*s\n", r->nameaddr.uri.len, r->nameaddr.uri.s);
			/* Skip first headers containing myself: */
			while (r && (parse_uri(r->nameaddr.uri.s, r->nameaddr.uri.len, &uri) == 0)
			  && check_self(&uri.host,uri.port_no?uri.port_no:SIP_PORT,0)) {
				LM_DBG("Self\n");
				/* Check for more headers and fail, if it was the last one
				   Check, if service-routes are indicated.
				   If yes, request is not following service-routes */
				if (find_next_route(_m, &hdr) != 0) r = NULL;
				else r = (rr_t*)hdr->parsed;

				if (!r && (c->num_service_routes > 0)) {
					LM_DBG("Not enough route-headers in Message\n");
					goto error;
				}
				LM_DBG("hdr is %p\n", hdr);
				LM_DBG("r is %p\n", r);
				if (r)
					LM_DBG("Next Route is %.*s\n", r->nameaddr.uri.len, r->nameaddr.uri.s);
			}
			LM_DBG("We have %d service-routes\n", c->num_service_routes);
			/* Then check the following headers: */
			for (i=0; i< c->num_service_routes; i++) {
				LM_DBG("Route must be: %.*s\n", c->service_routes[i].len, c->service_routes[i].s);

				/* No more Route-Headers? Not following service-routes */
				if (!r) {
					LM_DBG("No more route headers in message.\n");
					 goto error;
				}
				
				LM_DBG("Route is: %.*s\n", r->nameaddr.uri.len, r->nameaddr.uri.s);
 				
				/* Check length: */
				if (r->nameaddr.uri.len != c->service_routes[i].len) {
					LM_DBG("Length does not match.\n");
					 goto error;
				}
				/* Check contents: */
				if (strncasecmp(r->nameaddr.uri.s, c->service_routes[i].s, c->service_routes[i].len) != 0) {
					LM_DBG("String comparison failed.\n");
					 goto error;
				}
				if (find_next_route(_m, &hdr) != 0) r = NULL;
				else r = (rr_t*)hdr->parsed;
			}

			/* Check, if it was the last route-header in the message: */
			if (r) {
				LM_DBG("Too many route headers in message.\n");
				 goto error;
			}
		} else {
			LM_WARN("Strange: Route-Header is present, but not parsed?!?");
			if (c->num_service_routes > 0) goto error;
		}
	} else {
		LM_DBG("No route header in Message.\n");
		/* No route-header? Check, if service-routes are indicated.
		   If yes, request is not following service-routes */
		if (c->num_service_routes > 0) goto error;
	}
	/* Unlock domain */
	ul.unlock_udomain(_d, &c->aor, &received_host, _m->rcv.src_port);
	return 1;
error:
	/* Unlock domain */
	ul.unlock_udomain(_d, &c->aor, &received_host, _m->rcv.src_port);
	return -1;
}

static str route_start={"Route: <",8};
static str route_sep={">, <",4};
static str route_end={">\r\n",3};

/**
 * Force Service routes (upon request)
 */
int force_service_routes(struct sip_msg* _m, udomain_t* _d) {
	struct hdr_field *it;
	int i;
	str new_route_header;
	struct lump* lmp = NULL;
	char * buf;
	pcontact_t * c = getContactP(_m, _d);
	char srcip[20];
	str received_host;
	
	// Contact not found => not following service-routes
	if (c == NULL) return -1;

	/* we need to be sure we have seen all HFs */
	parse_headers(_m, HDR_EOH_F, 0);

	/* Save current buffer */
	buf = _m->buf;

	// Delete old Route headers:
	if (_m->route) {
		for (it = _m->route; it; it = it->next) {
			if (it->type == HDR_ROUTE_T) {
				if ((lmp = del_lump(_m, it->name.s - buf, it->len, HDR_ROUTE_T)) == 0) {
					LM_ERR("del_lump failed \n");
					return -1;
				}
			}
		}
	}

	/* Reset dst_uri if previously set either by loose route or manually */
	if (_m->dst_uri.s && _m->dst_uri.len) {
		pkg_free(_m->dst_uri.s);
		_m->dst_uri.s = NULL;
		_m->dst_uri.len = 0;
	}
	
	received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip, sizeof(srcip));
	received_host.s = srcip;

	/* Lock this record while working with the data: */
	ul.lock_udomain(_d, &c->aor, &received_host, _m->rcv.src_port);

	if (c->num_service_routes > 0) {
		/* Create anchor for new Route-Header: */
		lmp = anchor_lump(_m, _m->headers->name.s - buf,0,0);
		if (lmp == 0) {
			LM_ERR("Failed to get anchor lump\n");
			goto error;
		}	
		/* Calculate the length: */
		new_route_header.len = route_start.len +
			route_end.len + (c->num_service_routes-1) * route_sep.len;

		for(i=0; i< c->num_service_routes; i++)
			new_route_header.len+=c->service_routes[i].len;		
		/* Allocate the memory for this new header: */
		new_route_header.s = pkg_malloc(new_route_header.len);
		if (!new_route_header.s) {
			LM_ERR("Error allocating %d bytes\n", new_route_header.len);
			goto error;
		}
		
		/* Construct new header */
		new_route_header.len = 0;
		STR_APPEND(new_route_header, route_start);
		for(i=0; i < c->num_service_routes; i++) {
			if (i) STR_APPEND(new_route_header, route_sep);
			STR_APPEND(new_route_header, c->service_routes[i]);
		}
		STR_APPEND(new_route_header, route_end);

		LM_DBG("Setting route header to <%.*s> \n", new_route_header.len, new_route_header.s);

		if ((lmp = insert_new_lump_after(lmp, new_route_header.s, new_route_header.len, HDR_ROUTE_T)) == 0) {
			LM_ERR("Error inserting new route set\n");
			pkg_free(new_route_header.s);
			goto error;
		}

		LM_DBG("Setting dst_uri to <%.*s> \n", c->service_routes[0].len,
			c->service_routes[0].s);

		if (set_dst_uri(_m, &c->service_routes[0]) !=0 ) {
			LM_ERR("Error setting new dst uri\n");
			goto error;
		}
	}
	/* Unlock domain */
	ul.unlock_udomain(_d, &c->aor, &received_host, _m->rcv.src_port);
	return 1;
error:
	/* Unlock domain */
	ul.unlock_udomain(_d, &c->aor, &received_host, _m->rcv.src_port);
	return -1;
	
}

/**
 * Check, if source is registered.
 */
int is_registered(struct sip_msg* _m, udomain_t* _d) {
	if (getContactP(_m, _d) != NULL) return 1;
	return -1;	
}

/**
 * Get the current asserted identity for the user
 */
str * get_asserted_identity(struct sip_msg* _m) {
	if (_m->id != current_msg_id) {
		LM_ERR("Unable to get asserted identity: Please call is_registered first!\n");
		return NULL;
	} else return asserted_identity;
}


/**
 * Get the contact used during registration of this user
 */
str * get_registration_contact(struct sip_msg* _m) {
	if (_m->id != current_msg_id) {
		LM_ERR("Unable to get contact used during registration: Please call is_registered first!\n");
		return NULL;
	} else return registration_contact;
}


/**
 * checked if passed identity is an asserted identity
 */
int assert_identity(struct sip_msg* _m, udomain_t* _d, str identity) {
	// Public identities of this contact
	struct ppublic * p;
	//remove <> braces if there are
	if(identity.s[0]=='<' && identity.s[identity.len-1]=='>') {
		identity.s++;
		identity.len -= 2;
	}
	LM_DBG("Identity to assert: %.*s\n", identity.len, identity.s);
	
	if (getContactP(_m, _d) != NULL) {
		for (p = c->head; p; p = p->next) {
			LM_DBG("Public identity: %.*s\n", p->public_identity.len, p->public_identity.s);
			    /* Check length: */
			    if (identity.len == p->public_identity.len) {
				    /* Check contents: */
				    if (strncasecmp(identity.s, p->public_identity.s, identity.len) == 0) {
					    LM_DBG("Match!\n");
					    return 1;
				    }
			    } else LM_DBG("Length does not match.\n");
			}
		}
	LM_INFO("Contact not found based on Contact, trying IP/Port/Proto\n");
	str received_host = {0, 0};
	char srcip[50];	
	
	received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip, sizeof(srcip));
	received_host.s = srcip;
	if (ul.assert_identity(_d, &received_host, _m->rcv.src_port, _m->rcv.proto, &identity) == 0)
		return -1;
	else
		return 1;
}


/**
 * Add proper asserted identities based on registration
 */

static str p_asserted_identity_s={"P-Asserted-Identity: ",21};
static str p_asserted_identity_m={"<",1};
static str p_asserted_identity_e={">\r\n",3};

int assert_called_identity(struct sip_msg* _m, udomain_t* _d) {
	
	int ret=CSCF_RETURN_FALSE;
	str called_party_id={0,0},x={0,0};
	struct sip_msg* req;
	struct hdr_field *h=0;
		
	//get request from reply
	req = get_request_from_reply(_m);
	if (!req) {
		LM_ERR("Unable to get request from reply for REGISTER. No transaction\n");
		goto error;
	}
	
	called_party_id = cscf_get_public_identity_from_called_party_id(req, &h);
	
		
	if (!called_party_id.len){
		goto error;	
	}else{
		LM_DBG("Called Party ID from request: %.*s\n", called_party_id.len, called_party_id.s);	
		x.len = p_asserted_identity_s.len+p_asserted_identity_m.len+called_party_id.len+p_asserted_identity_e.len;
		x.s = pkg_malloc(x.len);
		if (!x.s){
			LM_ERR("P_assert_called_identity: Error allocating %d bytes\n",	x.len);
			x.len=0;
			goto error;		
		}
		x.len=0;
		STR_APPEND(x,p_asserted_identity_s);
		STR_APPEND(x,p_asserted_identity_m);
		STR_APPEND(x,called_party_id);
		STR_APPEND(x,p_asserted_identity_e);
		
		if (cscf_add_header(_m,&x,HDR_OTHER_T))
			ret = CSCF_RETURN_TRUE;
		else
		    goto error;
	}
	
	return ret;
	
error:
	ret=CSCF_RETURN_FALSE;
	return ret;
}


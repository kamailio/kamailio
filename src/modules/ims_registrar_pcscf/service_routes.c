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
#include "ims_registrar_pcscf_mod.h"
#include "save.h"
#include "../../core/parser/parse_via.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/data_lump.h"
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
		switch (c->security_temp->type) {
		case SECURITY_IPSEC:
			security_server_port = c->security_temp->data.ipsec->port_uc;
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
			if (!memcmp(c->received_host.s, received_host.s, received_host.len)) {
                            LM_DBG("check contact passed\n");
				return 0;
		}
	}
	}
        LM_DBG("check contact failed\n");
	return 1;
}

/**
 * get PContact-Structure for message
 * (search only once per Request)
 */

pcontact_t * getContactP(struct sip_msg* _m, udomain_t* _d, enum pcontact_reg_states reg_state, char service_routes[][MAXROUTESIZE], int num_service_routes) {
	ppublic_t * p;
	contact_body_t *b = 0;
	contact_t *ct;
	pcontact_info_t search_ci;
	str received_host = {0, 0};
	char srcip[50];
	struct via_body *vb;
	unsigned short port, proto;
	str host;
	sip_uri_t contact_uri;
	int mustRetryViaSearch = 0;
	int mustRetryReceivedSearch = 0;

	LM_DBG("number of service routes to look for is %d\n", num_service_routes);
    b = cscf_parse_contacts(_m);

    if (_m->first_line.type == SIP_REPLY && _m->contact && _m->contact->parsed && b->contacts) {
		mustRetryViaSearch = 1;
		mustRetryReceivedSearch = 1;
        LM_DBG("This is a reply - to look for contact we favour the contact header above the via (b2bua)... if no contact we will use last via\n");
        ct = b->contacts;
        host = ct->uri;
        if (parse_uri(ct->uri.s, ct->uri.len, &contact_uri) != 0) {
            LM_WARN("Failed to parse contact [%.*s]\n", ct->uri.len, ct->uri.s);
            return NULL;
        }
        host = contact_uri.host;
        port = contact_uri.port_no ? contact_uri.port_no : 5060;
        proto = contact_uri.proto;
        if (proto == 0) {
            LM_DBG("Contact protocol not specified - using received\n");
            proto = _m->rcv.proto;
        }
    } else {
        if (_m->first_line.type == SIP_REPLY)
            LM_DBG("This is a reply but we are forced to use the via header\n");
        else
            LM_DBG("This is a request - using first via to find contact\n");

        vb = cscf_get_ue_via(_m);
        host = vb->host;
        port = vb->port ? vb->port : 5060;
        proto = vb->proto;
    }

    LM_DBG("searching for contact with host:port:proto contact [%d://%.*s:%d]\n", proto, host.len, host.s, port);

    received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip, sizeof (srcip));
    received_host.s = srcip;

//    if (_m->id != current_msg_id) {
        current_msg_id = _m->id;
        c = NULL;
	//search_ci.reg_state = PCONTACT_REGISTERED;          //we can do this because this function is always called expecting a REGISTERED contact
	memset(&search_ci, 0, sizeof(struct pcontact_info));
    search_ci.reg_state = reg_state;
	search_ci.received_host.s = received_host.s;
	search_ci.received_host.len = received_host.len;
	search_ci.received_port = _m->rcv.src_port;
	search_ci.received_proto = _m->rcv.proto;
	search_ci.searchflag = SEARCH_RECEIVED;
	search_ci.num_service_routes = 0;
	if (is_registered_fallback2ip == 1) {
		search_ci.searchflag = SEARCH_NORMAL;
	} 
	search_ci.via_host = host;
	search_ci.via_port = port;
	search_ci.via_prot = proto;
	search_ci.aor.s = 0;
	search_ci.aor.len = 0;
	
	int size = num_service_routes==0?1:num_service_routes;
	str s_service_routes[size];
	int i;
	for (i=0;i<num_service_routes;i++) {
		s_service_routes[i].s = service_routes[i];
		s_service_routes[i].len = strlen(service_routes[i]);
		LM_DBG("Setting service routes str for pos %d to %.*s", i, s_service_routes[i].len, s_service_routes[i].s);
	}
	if (num_service_routes > 0) {
		LM_DBG("asked to search for specific service routes...\n");
		search_ci.service_routes = s_service_routes;
		search_ci.num_service_routes = num_service_routes;
		search_ci.extra_search_criteria = SEARCH_SERVICE_ROUTES;
	}

//	b = cscf_parse_contacts(_m);
tryagain:
	if (b && b->contacts) {
		for (ct = b->contacts; ct; ct = ct->next) {
			search_ci.aor = ct->uri;
			if (ul.get_pcontact(_d, &search_ci, &c) == 0) {
				if (checkcontact(_m, c) != 0) {
					c = NULL;
				} else {
					break;
				}
			}
		}
	} else {
		LM_WARN("No contact-header found...\n");
	}

	if ((c == NULL) && (is_registered_fallback2ip == 1)) {
		LM_INFO("Contact not found based on Contact-header, trying IP/Port/Proto\n");
		//			received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip, sizeof(srcip));
		//			received_host.s = srcip;
		search_ci.searchflag = SEARCH_RECEIVED;
		if (ul.get_pcontact(_d, &search_ci, &c) == 1) {
			LM_DBG("No entry in usrloc for %.*s:%i (Proto %i) found!\n", received_host.len, received_host.s, _m->rcv.src_port, _m->rcv.proto);
		} else {
			if (checkcontact(_m, c) != 0) {
				c = NULL;
			}
		}
	}

	if ((c == NULL) && (is_registered_fallback2ip == 2)) {
		LM_INFO("Contact not found based on IP/Port/Proto, trying Contact-header\n");
		search_ci.searchflag = SEARCH_NORMAL;
		if (ul.get_pcontact(_d, &search_ci, &c) == 1) {
		} else {
			if (checkcontact(_m, c) != 0) {
				c = NULL;
			}
		}
	}

	asserted_identity = NULL;
	registration_contact = NULL;
	if (c) {
                LM_DBG("Trying to set asserted identity field");
		registration_contact = &c->contact_user;
		p = c->head;
		while (p) {
                        LM_DBG("Checking through contact users");
			if (p->is_default == 1) {
                            LM_DBG("Found default contact user so setting asserted identity");
                            asserted_identity = &p->public_identity;
                        }
			p = p->next;
		}
	}
        if (asserted_identity != NULL && asserted_identity->len > 0) {
            LM_DBG("Have set the asserted_identity param to [%.*s]\n", asserted_identity->len, asserted_identity->s);
        } else {
            LM_DBG("Asserted identity not set");
        }
            
    
//    LM_DBG("pcontact flag is [%d]\n", c->flags);
//    if (c && (c->flags & (1<<FLAG_READFROMDB)) != 0) {
//        LM_DBG("we have a contact that was read fresh from the DB....\n");
//    }
	if (!c && mustRetryViaSearch) {
		LM_DBG("This is a reply so we will search using the last via once more...\n");
		vb = cscf_get_ue_via(_m);
		search_ci.via_host = vb->host;
		search_ci.via_port = vb->port ? vb->port : 5060;
		search_ci.via_prot = vb->proto;
		mustRetryViaSearch = 0;
		goto tryagain;
	}
	
	if (!c && mustRetryReceivedSearch) {
		LM_DBG("This is a reply and we still don't have a match - will try src ip/port of message\n");
		search_ci.via_host = received_host;
		search_ci.via_port = _m->rcv.src_port;
		search_ci.via_prot = _m->rcv.proto;
		mustRetryReceivedSearch = 0;
		goto tryagain;
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
	char routes[MAXROUTES][MAXROUTESIZE];
	unsigned int num_routes = 0;
	
	struct via_body * vb;
	unsigned short port;
	unsigned short proto;
	/* Contact not found => not following service-routes */
//	if (c == NULL) return -1;

	/* Search for the first Route-Header: */
	if (find_first_route(_m) < 0) return -1;

//	LM_DBG("Got %i Route-Headers.\n", c->num_service_routes);

	vb = cscf_get_ue_via(_m);
	port = vb->port?vb->port:5060;
	proto = vb->proto;
	
	/* Lock this record while working with the data: */
	ul.lock_udomain(_d, &vb->host, port, proto);
    
	if (_m->route) {
		hdr = _m->route;
		r = (rr_t*)hdr->parsed;
		//get rid of ourselves from route header
		if (r) {
			LM_DBG("Route is %.*s\n", r->nameaddr.uri.len, r->nameaddr.uri.s);
			while (r && (parse_uri(r->nameaddr.uri.s, r->nameaddr.uri.len, &uri) == 0)
			  && check_self(&uri.host,uri.port_no?uri.port_no:SIP_PORT,0)) {
				LM_DBG("Self\n");
				/* Check for more headers and fail, if it was the last one
				   Check, if service-routes are indicated.
				   If yes, request is not following service-routes */
				if (find_next_route(_m, &hdr) != 0) 
					r = NULL;
				else 
					r = (rr_t*)hdr->parsed;
				
				LM_DBG("hdr is %p\n", hdr);
				LM_DBG("r is %p\n", r);
				if (r)
					LM_DBG("Next Route is %.*s\n", r->nameaddr.uri.len, r->nameaddr.uri.s);
			}
			
			int i = 0;
			while (r) {
				memset(routes[i],0,MAXROUTESIZE);
				memcpy(routes[i], r->nameaddr.uri.s, r->nameaddr.uri.len);
				
				if (find_next_route(_m, &hdr) != 0) 
					r = NULL;
				else 
					r = (rr_t*)hdr->parsed;
				
				i += 1;
				num_routes += 1;
			}
			
			LM_DBG("num_routes is %d\n", num_routes);
			if (num_routes == 0) {
				LM_DBG("Request doesn't have any route headers (except those pointing here), to check service-route...ignoring\n");		
				goto error;
			}
			for (i=0; i<num_routes; i++) {
				LM_DBG("route %d for checking is %s\n", i, routes[i]);
			}
			pcontact_t * c = getContactP(_m, _d, PCONTACT_REGISTERED, routes, num_routes);
			if (!c) {
				LM_DBG("no contact found in usrloc when checking for service route\n");
				goto error;
			}
			
			LM_DBG("we have a contact which satisifes the routes...\n");
			ul.unlock_udomain(_d, &vb->host, port, proto);
			return 1;
		}
	} else {
		LM_DBG("Request doesn't have any route headers to check service-route...ignoring\n");
		goto error;
	}
	
	pcontact_t * c = getContactP(_m, _d, PCONTACT_REGISTERED, 0, 0);
	if (!c) {
		LM_DBG("no contact found in usrloc when checking for service route\n");
		goto error;
	}

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
	ul.unlock_udomain(_d, &vb->host, port, proto);
	return 1;
error:
	/* Unlock domain */
	ul.unlock_udomain(_d, &vb->host, port, proto);
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
	pcontact_t * c = getContactP(_m, _d, PCONTACT_REGISTERED,0 ,0);
//	char srcip[20];
//	str received_host;
        struct via_body* vb;
        unsigned short port;
        unsigned short proto;
	
	// Contact not found => not following service-routes
	if (c == NULL) return -1;

	/* we need to be sure we have seen all HFs */
	parse_headers(_m, HDR_EOH_F, 0);

        vb = cscf_get_ue_via(_m);
        port = vb->port?vb->port:5060;
        proto = vb->proto;
        
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
	
//	received_host.len = ip_addr2sbuf(&_m->rcv.src_ip, srcip, sizeof(srcip));
//	received_host.s = srcip;

	/* Lock this record while working with the data: */
	ul.lock_udomain(_d, &vb->host, port, proto);

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
	ul.unlock_udomain(_d, &vb->host, port, proto);
	return 1;
error:
	/* Unlock domain */
	ul.unlock_udomain(_d, &vb->host, port, proto);
	return -1;
	
    return 1;
}

/**
 * Check, if source is registered.
 */
int is_registered(struct sip_msg* _m, udomain_t* _d) {
	if (getContactP(_m, _d, PCONTACT_REGISTERED, 0 ,0) != NULL) return 1;
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
	
	if (getContactP(_m, _d, PCONTACT_REGISTERED|PCONTACT_REG_PENDING_AAR|PCONTACT_REG_PENDING, 0, 0) != NULL) {
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

int pcscf_unregister(udomain_t* _d, str * uri, str * received_host, int received_port) {
	int result = -1;
	struct pcontact * pcontact;
	struct pcontact_info ci;
    	memset(&ci, 0, sizeof (struct pcontact_info));

	pcontact_info_t search_ci;
	memset(&ci, 0, sizeof(struct pcontact_info));
	
	sip_uri_t contact_uri;
        if (parse_uri(uri->s, uri->len, &contact_uri) != 0) {
            LM_WARN("Failed to parse aor [%.*s]\n", uri->len, uri->s);
            return -1;
        }

        search_ci.received_host.s = received_host->s;
        search_ci.received_host.len = received_host->len;
        search_ci.received_port = received_port;
        search_ci.received_proto = contact_uri.proto? contact_uri.proto : PROTO_UDP;
        search_ci.searchflag = SEARCH_RECEIVED;
        search_ci.via_host.s = received_host->s;
        search_ci.via_host.len = received_host->len;
        search_ci.via_port = received_port;
        search_ci.via_prot = search_ci.received_proto;
        search_ci.aor.s = uri->s;
        search_ci.aor.len = uri->len;
		search_ci.reg_state = PCONTACT_ANY;

	if (ul.get_pcontact(_d, &search_ci, &pcontact) == 0) {
		/* Lock this record while working with the data: */
		ul.lock_udomain(_d, &pcontact->via_host, pcontact->via_port, pcontact->via_proto);

		LM_DBG("Updating contact [%.*s]: setting state to PCONTACT_DEREG_PENDING_PUBLISH\n", pcontact->aor.len, pcontact->aor.s);

		ci.reg_state = PCONTACT_DEREG_PENDING_PUBLISH;
		ci.num_service_routes = 0;
		if (ul.update_pcontact(_d, &ci, pcontact) == 0) result = 1;

		/* Unlock domain */
		ul.unlock_udomain(_d, &pcontact->via_host, pcontact->via_port, pcontact->via_proto);
	}
	return result;
}


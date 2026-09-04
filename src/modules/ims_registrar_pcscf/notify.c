/*
 * pua_reginfo module - Presence-User-Agent Handling of reg events
 *
 * Copyright (C) 2011-2012 Carsten Bock, carsten@ng-voice.com
 * http://www.ng-voice.com
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
 * * History:
 * ========
 *
 * Nov 2013 Richard Good migrated pua_reginfo functionality to ims_registrar_pcscf
 *
 */

#include "notify.h"
#include "ims_registrar_pcscf_mod.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_uri.h"
#include "../../modules/ims_usrloc_pcscf/usrloc.h"
#include "ul_callback.h"
#include <libxml/parser.h>
#include <libxml2/libxml/globals.h>
#include "gruu.h"

#include "subscribe.h"

#include "../pua/pua_bind.h"

#define NOTIFY_MAX_IMPUS 32

/*<?xml version="1.0"?>
<reginfo xmlns="urn:ietf:params:xml:ns:reginfo" version="0" state="full">
.<registration aor="sip:carsten@ng-voice.com" id="0xb33fa860" state="active">
..<contact id="0xb33fa994" state="active" event="registered" expires="3600">
...<uri>sip:carsten@10.157.87.36:43582;transport=udp</uri>
...<unknown-param name="+g.3gpp.cs-voice"></unknown-param>
...<unknown-param name="+g.3gpp.icsi-ref">urn0X0.0041FB74E7B54P-1022urn-70X0P+03gpp-application.ims.iari.gsma-vs</unknown-param>
...<unknown-param name="audio"></unknown-param>
...<unknown-param name="+g.oma.sip-im.large-message"></unknown-param>
...<unknown-param name="+g.3gpp.smsip"></unknown-param>
...<unknown-param name="language">en,fr</unknown-param>
...<unknown-param name="+g.oma.sip-im"></unknown-param>
...<unknown-param name="expires">600000</unknown-param>
..</contact>
.</registration>
</reginfo> */

#define STATE_ACTIVE 1
#define STATE_TERMINATED 0
#define STATE_UNKNOWN -1

#define EVENT_UNKNOWN -1
#define EVENT_REGISTERED 0
#define EVENT_UNREGISTERED 1
#define EVENT_TERMINATED 2
#define EVENT_CREATED 3
#define EVENT_REFRESHED 4
#define EVENT_EXPIRED 5
#define EVENT_DEACTIVATED 6

#define RESULT_ERROR -1
#define RESULT_CONTACTS_FOUND 1
#define RESULT_TERMINATED_SUCCESS 1


extern usrloc_api_t ul;
extern time_t time_now;

extern int subscribe_to_reginfo;

extern ims_registrar_pcscf_params_t _imsregp_params;

char *xmlGetAttrContentByName(xmlNodePtr node, const char *name);

int process_contact(udomain_t *_d, int expires, str contact_uri,
		int contact_state, pcontact_t **out_contact)
{
	char bufport[5], *rest, *sep, *val, *port, *trans;
	pcontact_t *pcontact;
	struct pcontact_info ci;
	struct sip_uri puri, uri;
	unsigned int received_proto, received_port_len;
	int local_time_now, rest_len, val_len, has_alias;
	int ret = RESULT_CONTACTS_FOUND;

	if(out_contact)
		*out_contact = NULL;

	pcscf_act_time();
	local_time_now = time_now;

	has_alias = 0;

	//get contact
	//if does not exist then add it
	//if it does exist check if state it terminated - if so delete it, if not update it

	memset(&ci, 0, sizeof(struct pcontact_info));
	ci.num_public_ids = 0;
	ci.num_service_routes = 0;

	LM_DBG("Processing contact using contact from NOTIFY [%.*s]\n",
			contact_uri.len, contact_uri.s);
	if(parse_uri(contact_uri.s, contact_uri.len, &puri) < 0) {
		LM_DBG("Error parsing Contact URI <%.*s>\n", contact_uri.len,
				contact_uri.s);
		return RESULT_ERROR;
	}

	expires = local_time_now
			  + expires; //turn expires into correct time since epoch format
	LM_DBG("Changed expires to format time since the epoch: %d", expires);
	ci.expires = expires;
	ci.reg_state = PCONTACT_ANY;

	ul.lock_udomain(_d, &puri.host, puri.port_no, puri.proto);
	ci.aor = contact_uri;
	ci.via_host = puri.host;
	ci.via_prot = puri.proto;
	ci.via_port = puri.port_no;

	/* parse the uri in the NOTIFY */
	if(parse_uri(contact_uri.s, contact_uri.len, &uri) != 0) {
		LM_ERR("Unable to parse contact in SIP notify [%.*s]\n",
				contact_uri.len, contact_uri.s);
		ret = RESULT_ERROR;
		goto done;
	}
	/*check for alias - NAT */
	rest = uri.sip_params.s;
	rest_len = uri.sip_params.len;

	while(rest_len >= ALIAS_LEN) {
		if(strncmp(rest, ALIAS, ALIAS_LEN) == 0) {
			has_alias = 1;
			break;
		}
		sep = memchr(rest, 59 /* ; */, rest_len);
		if(sep == NULL) {
			LM_DBG("no alias param\n");
			break;
		} else {
			rest_len = rest_len - (sep - rest + 1);
			rest = sep + 1;
		}
	}

	if(has_alias) {
		val = rest + ALIAS_LEN;
		val_len = rest_len - ALIAS_LEN;
		port = memchr(val, 126 /* ~ */, val_len);
		if(port == NULL) {
			LM_ERR("no '~' in alias param value\n");
			ret = RESULT_ERROR;
			goto done;
		}
		port++;
		//            received_port = atoi(port);
		trans = memchr(port, 126 /* ~ */, val_len - (port - val));
		if(trans == NULL) {
			LM_ERR("no second '~' in alias param value\n");
			ret = RESULT_ERROR;
			goto done;
		}

		received_port_len = trans - port;

		trans = trans + 1;
		received_proto = *trans - 48 /* char 0 */;

		memcpy(bufport, port, received_port_len);
		bufport[received_port_len] = 0;

		ci.received_host.s = val;
		ci.received_host.len = port - val - 1;
		LM_DBG("Setting received host in search to [%.*s]\n",
				ci.received_host.len, ci.received_host.s);
		ci.received_port = atoi(bufport);
		LM_DBG("Setting received port in search to %d\n", ci.received_port);
		ci.received_proto = received_proto;
		LM_DBG("Setting received proto in search to %d\n", ci.received_proto);
		ci.searchflag = SEARCH_RECEIVED;
	} else {
		LM_DBG("Contact in NOTIFY does not have an alias....\n");
	}

	if(ul.get_pcontact(_d, &ci, &pcontact, 0) != 0) { //contact does not exist
		if(contact_state == STATE_TERMINATED) {
			LM_DBG("This contact: <%.*s> is in state terminated and is not in "
				   "usrloc, ignore\n",
					contact_uri.len, contact_uri.s);
			ret = RESULT_CONTACTS_FOUND;
			goto done;
		}
		LM_WARN("This contact: <%.*s> is in state active and is not in usrloc "
				"- must be another contact on a different P so going to "
				"ignore\n",
				contact_uri.len, contact_uri.s);
		//		LM_DBG("This contact: <%.*s> is in state active and is not in usrloc so adding it to usrloc, expires: %d which is in %d seconds\n", contact_uri.len, contact_uri.s, expires, expires-local_time_now);
		//		if (ul.insert_pcontact(_d, &contact_uri, &ci, &pcontact) != 0) {
		//			LM_ERR("Failed inserting new pcontact\n");
		//			ret = RESULT_ERROR;
		//			goto done;
		//		} else {
		//			//register for callbacks on this contact so we can send PUBLISH to SCSCF should status change
		//			LM_DBG("registering for UL callback\n");
		//			ul.register_ulcb(pcontact, PCSCF_CONTACT_DELETE | PCSCF_CONTACT_EXPIRE, callback_pcscf_contact_cb, NULL);
		//		}
	} else { //contact exists
		if(out_contact)
			*out_contact = pcontact;
		if(contact_state == STATE_TERMINATED) {
			LM_DBG("This contact <%.*s> is in state terminated and is in "
				   "usrloc so removing it from usrloc\n",
					contact_uri.len, contact_uri.s);
			if(_imsregp_params.delete_delay <= 0) {
				//delete contact
				if(ul.delete_pcontact(_d, pcontact) != 0) {
					LM_DBG("failed to delete pcscf contact <%.*s> - not a "
						   "problem "
						   "this may have been removed by de registration",
							contact_uri.len, contact_uri.s);
				}
			} else {
				// rather than delete update the pcontact with expire value
				ci.expires = local_time_now + _imsregp_params.delete_delay;
				if(ul.update_pcontact(_d, &ci, pcontact) != 0) {
					LM_DBG("failed to update pcscf contact on de-register\n");
				}
			}
			/*TODO_LATEST - put this back */
		} else { //state is active
			//update this contact
			LM_DBG("This contact: <%.*s> is in state active and is in usrloc "
				   "so just updating - old expires: %" TIME_T_FMT
				   ", new expires: %i which is in %i seconds\n",
					contact_uri.len, contact_uri.s,
					TIME_T_CAST(pcontact->expires), expires,
					expires - local_time_now);
			ci.reg_state = PCONTACT_REGISTERED;
			if(ul.update_pcontact(_d, &ci, pcontact) != 0) {
				LM_ERR("failed to update pcscf contact\n");
				ret = RESULT_ERROR;
				goto done;
			}
			pcontact->expires = expires;
		}
	}

done:
	ul.unlock_udomain(_d, &puri.host, puri.port_no, puri.proto);
	return ret;
}

static void pcscf_normalize_param_value(str *value)
{
	if(!value || !value->s || value->len < 2)
		return;
	if(value->s[0] == '"' && value->s[value->len - 1] == '"') {
		value->s++;
		value->len -= 2;
	}
	if(value->len >= 2 && value->s[0] == '<'
			&& value->s[value->len - 1] == '>') {
		value->s++;
		value->len -= 2;
	}
}

static int apply_gruu_from_unknown_params(
		xmlNodePtr params, udomain_t *domain, pcontact_t *pcontact)
{
	char *param_name = NULL;
	char *param_value = NULL;
	char *pub_gruu = NULL;
	char *temp_gruu = NULL;
	char *instance_id = NULL;
	gruu_fields_t gf;
	str value = {0};
	int ret = 0;

	if(!domain || !pcontact)
		return -1;
	memset(&gf, 0, sizeof(gf));

	while(params) {
		if(xmlStrcasecmp(params->name, BAD_CAST "unknown-param") != 0)
			goto next_param;
		param_name = xmlGetAttrContentByName(params, "name");
		if(param_name == NULL)
			goto next_param;
		param_value = (char *)xmlNodeGetContent(params);
		value.s = (param_value != NULL) ? param_value : "";
		value.len = strlen(value.s);
		pcscf_normalize_param_value(&value);
		if(strcasecmp(param_name, "pub-gruu") == 0) {
			pub_gruu = param_value;
			param_value = NULL;
		} else if(strcasecmp(param_name, "temp-gruu") == 0) {
			temp_gruu = param_value;
			param_value = NULL;
		} else if(strcasecmp(param_name, "+sip.instance") == 0) {
			instance_id = param_value;
			param_value = NULL;
		}
	next_param:
		if(param_name) {
			xmlFree(param_name);
			param_name = NULL;
		}
		if(param_value) {
			xmlFree(param_value);
			param_value = NULL;
		}
		params = params->next;
	}
	if(pub_gruu || temp_gruu || instance_id) {
		str inst = {0}, pub = {0}, temp = {0};

		if(pub_gruu) {
			pub.s = pub_gruu;
			pub.len = strlen(pub_gruu);
			pcscf_normalize_param_value(&pub);
		}
		if(temp_gruu) {
			temp.s = temp_gruu;
			temp.len = strlen(temp_gruu);
			pcscf_normalize_param_value(&temp);
		}
		if(instance_id) {
			inst.s = instance_id;
			inst.len = strlen(instance_id);
			pcscf_normalize_param_value(&inst);
		}
		ret = pcscf_gruu_fields_dup(&gf, instance_id ? &inst : NULL,
				pub_gruu ? &pub : NULL, temp_gruu ? &temp : NULL);
		if(pub_gruu)
			xmlFree(pub_gruu);
		if(temp_gruu)
			xmlFree(temp_gruu);
		if(instance_id)
			xmlFree(instance_id);
		if(ret < 0)
			return -1;
		ret = pcscf_apply_gruu(domain, pcontact, &gf);
	} else {
		ret = -1;
	}
	return ret;
}

int reginfo_parse_state(char *s)
{
	if(s == NULL) {
		return STATE_UNKNOWN;
	}
	switch(strlen(s)) {
		case 6:
			if(strncmp(s, "active", 6) == 0)
				return STATE_ACTIVE;
			break;
		case 10:
			if(strncmp(s, "terminated", 10) == 0)
				return STATE_TERMINATED;
			break;
		default:
			LM_ERR("Unknown State %s\n", s);
			return STATE_UNKNOWN;
	}
	LM_ERR("Unknown State %s\n", s);
	return STATE_UNKNOWN;
}

int reginfo_parse_event(char *s)
{
	if(s == NULL) {
		return EVENT_UNKNOWN;
	}
	switch(strlen(s)) {
		case 7:
			if(strncmp(s, "created", 7) == 0)
				return EVENT_CREATED;
			if(strncmp(s, "expired", 7) == 0)
				return EVENT_EXPIRED;
			break;
		case 9:
			if(strncmp(s, "refreshed", 9) == 0)
				return EVENT_CREATED;
			break;
		case 10:
			if(strncmp(s, "registered", 10) == 0)
				return EVENT_REGISTERED;
			if(strncmp(s, "terminated", 10) == 0)
				return EVENT_TERMINATED;
			break;
		case 11:
			if(strncmp(s, "deactivated", 11) == 0)
				return EVENT_DEACTIVATED;
			break;
		case 12:
			if(strncmp(s, "unregistered", 12) == 0)
				return EVENT_UNREGISTERED;
			break;
		default:
			LM_ERR("Unknown Event %s\n", s);
			return EVENT_UNKNOWN;
	}
	LM_ERR("Unknown Event %s\n", s);
	return EVENT_UNKNOWN;
}

xmlNodePtr xmlGetNodeByName(xmlNodePtr parent, const char *name)
{
	xmlNodePtr cur = parent;
	xmlNodePtr match = NULL;
	while(cur) {
		if(xmlStrcasecmp(cur->name, (unsigned char *)name) == 0)
			return cur;
		match = xmlGetNodeByName(cur->children, name);
		if(match)
			return match;
		cur = cur->next;
	}
	return NULL;
}

char *xmlGetAttrContentByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = node->properties;
	while(attr) {
		if(xmlStrcasecmp(attr->name, (unsigned char *)name) == 0)
			return (char *)xmlNodeGetContent(attr->children);
		attr = attr->next;
	}
	return NULL;
}

int process_body(struct sip_msg *msg, str notify_body, udomain_t *domain)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr doc_root = NULL, registrations = NULL, contacts = NULL,
			   uris = NULL;
	str aor = {0, 0};
	str callid = {0, 0};
	str contact_uri = {0, 0};
	str received = {0, 0};
	str path = {0, 0};
	str user_agent = {0, 0};
	int reg_state, contact_state, event, expires, result,
			final_result = RESULT_ERROR;
	int barred_registration = 0;
	char *expires_char = 0, *cseq_char = 0, *registration_state = 0,
		 *contact_state_s = 0, *event_s = 0, *barred_s = 0;
	int cseq = 0;
	pv_elem_t *presentity_uri_pv;

	doc = xmlParseMemory(notify_body.s, notify_body.len);
	if(doc == NULL) {
		LM_ERR("Error while parsing the xml body message, Body is:\n%.*s\n",
				notify_body.len, notify_body.s);
		return -1;
	}
	doc_root = xmlGetNodeByName(doc->children, "reginfo");
	if(doc_root == NULL) {
		LM_ERR("while extracting the reginfo node\n");
		goto error;
	}
	registrations = doc_root->children;
	while(registrations) {
		/* Only process registration sub-items */
		if(xmlStrcasecmp(registrations->name, BAD_CAST "registration") != 0)
			goto next_registration;
		registration_state = xmlGetAttrContentByName(registrations, "state");
		reg_state = reginfo_parse_state(registration_state);
		if(reg_state == STATE_UNKNOWN) {
			LM_ERR("No state for this registration!\n");
			goto next_registration;
		}
		aor.s = xmlGetAttrContentByName(registrations, "aor");
		if(aor.s == NULL) {
			LM_ERR("No AOR for this registration!\n");
			goto next_registration;
		}
		aor.len = strlen(aor.s);
		LM_DBG("AOR %.*s has reg_state \"%d\"\n", aor.len, aor.s, reg_state);
		barred_registration = 0;
		barred_s = xmlGetAttrContentByName(registrations, "barred");
		if(barred_s && strcasecmp(barred_s, "true") == 0)
			barred_registration = 1;
		if(barred_s) {
			xmlFree(barred_s);
			barred_s = NULL;
		}

		if(reg_state == STATE_TERMINATED) {
			//TODO we if there is an IMPU record state here we should delete all contacts associated to it
			//Right now we do it go through all the contacts

			LM_DBG("AOR %.*s is in state terminated so unsubscribing from "
				   "reginfo\n",
					aor.len, aor.s);
			//we return a successful result here even if no contacts
			final_result = RESULT_TERMINATED_SUCCESS;

			if(pv_parse_format(&aor, &presentity_uri_pv) < 0) {
				LM_ERR("wrong format[%.*s] - failed unsubscribing to reginfo\n",
						aor.len, aor.s);
			}
			reginfo_subscribe_real(msg, presentity_uri_pv, 0, 0, 0);
			pv_elem_free_all(presentity_uri_pv);
		}

		/* Now lets process the Contact's from this Registration: */
		contacts = registrations->children;
		while(contacts) {
			if(xmlStrcasecmp(contacts->name, BAD_CAST "contact") != 0)
				goto next_contact;
			callid.s = xmlGetAttrContentByName(contacts, "callid");
			if(callid.s == NULL) {
				LM_DBG("No Call-ID for this contact!\n");
				callid.len = 0;
			} else {
				callid.len = strlen(callid.s);
				LM_DBG("contact has callid <%.*s>\n", callid.len, callid.s);
			}

			received.s = xmlGetAttrContentByName(contacts, "received");
			if(received.s == NULL) {
				LM_DBG("No received for this contact!\n");
				received.len = 0;
			} else {
				received.len = strlen(received.s);
				LM_DBG("contact has received <%.*s>\n", received.len,
						received.s);
			}

			path.s = xmlGetAttrContentByName(contacts, "path");
			if(path.s == NULL) {
				LM_DBG("No path for this contact!\n");
				path.len = 0;
			} else {
				path.len = strlen(path.s);
				LM_DBG("contact has path <%.*s>\n", path.len, path.s);
			}

			user_agent.s = xmlGetAttrContentByName(contacts, "user_agent");
			if(user_agent.s == NULL) {
				LM_DBG("No user_agent for this contact!\n");
				user_agent.len = 0;
			} else {
				user_agent.len = strlen(user_agent.s);
				LM_DBG("contact has user_agent <%.*s>\n", user_agent.len,
						user_agent.s);
			}
			event_s = xmlGetAttrContentByName(contacts, "event");
			event = reginfo_parse_event(event_s);
			if(event == EVENT_UNKNOWN) {
				LM_ERR("No event for this contact - going to next contact!\n");
				goto next_contact;
			}
			expires_char = xmlGetAttrContentByName(contacts, "expires");
			if(expires_char == NULL) {
				LM_ERR("No expires for this contact - going to next "
					   "contact!\n");
				goto next_contact;
			}
			expires = atoi(expires_char);
			if(expires < 0) {
				LM_ERR("No valid expires for this contact - going to next "
					   "contact!\n");
				goto next_contact;
			}

			contact_state_s = xmlGetAttrContentByName(contacts, "state");
			contact_state = reginfo_parse_state(contact_state_s);
			if(contact_state == STATE_UNKNOWN) {
				LM_ERR("No state for this contact - going to next contact!\n");
				goto next_contact;
			}

			LM_DBG("Contact state %d: Event \"%d\", expires %d\n",
					contact_state, event, expires);


			cseq_char = xmlGetAttrContentByName(contacts, "cseq");
			if(cseq_char == NULL) {
				LM_DBG("No cseq for this contact!\n");
			} else {
				cseq = atoi(cseq_char);
				if(cseq < 0) {
					LM_DBG("No valid cseq for this contact!\n");
				}
			}

			/* Now lets process the URI's from this Contact: */
			{
				str reg_impus[NOTIFY_MAX_IMPUS];
				str reg_barred[NOTIFY_MAX_IMPUS];
				char *reg_impu_buf[NOTIFY_MAX_IMPUS];
				int reg_impu_n = 0;
				int reg_barred_n = 0;
				int reg_i;
				pcontact_t *sync_pcontact = NULL;
				struct pcontact_info ci = {0};
				char *uri_barred_s = NULL;

				memset(reg_impus, 0, sizeof(reg_impus));
				memset(reg_barred, 0, sizeof(reg_barred));
				memset(reg_impu_buf, 0, sizeof(reg_impu_buf));

				uris = contacts->children;
				while(uris) {
					if(xmlStrcasecmp(uris->name, BAD_CAST "uri") != 0)
						goto next_uri;
					contact_uri.s = (char *)xmlNodeGetContent(uris);
					if(contact_uri.s == NULL) {
						LM_ERR("No URI for this contact - going to next "
							   "registration!\n");
						goto next_registration;
					}
					contact_uri.len = strlen(contact_uri.s);
					LM_DBG("Contact: %.*s\n", contact_uri.len, contact_uri.s);

					{
						pcontact_t *pcontact = NULL;
						result = process_contact(domain, expires, contact_uri,
								contact_state, &pcontact);
						if(result != RESULT_ERROR
								&& contact_state == STATE_ACTIVE
								&& pcontact != NULL)
							sync_pcontact = pcontact;
					}

					if(reg_impu_n < NOTIFY_MAX_IMPUS) {
						reg_impu_buf[reg_impu_n] =
								(char *)pkg_malloc(contact_uri.len);
						if(reg_impu_buf[reg_impu_n]) {
							memcpy(reg_impu_buf[reg_impu_n], contact_uri.s,
									contact_uri.len);
							reg_impus[reg_impu_n].s = reg_impu_buf[reg_impu_n];
							reg_impus[reg_impu_n].len = contact_uri.len;
							if(barred_registration) {
								reg_barred[reg_barred_n++] =
										reg_impus[reg_impu_n];
							} else {
								uri_barred_s =
										xmlGetAttrContentByName(uris, "barred");
								if(uri_barred_s
										&& strcasecmp(uri_barred_s, "true") == 0
										&& reg_barred_n < NOTIFY_MAX_IMPUS)
									reg_barred[reg_barred_n++] =
											reg_impus[reg_impu_n];
								if(uri_barred_s) {
									xmlFree(uri_barred_s);
									uri_barred_s = NULL;
								}
							}
							reg_impu_n++;
						}
					}

					if(contact_uri.s && (strlen(contact_uri.s) > 0)) {
						xmlFree(contact_uri.s);
						contact_uri.s = NULL;
					}

					/* Process the result */
					if(final_result != RESULT_CONTACTS_FOUND)
						final_result = result;
				next_uri:
					uris = uris->next;
				}

				if(reg_impu_n == 0 && aor.s && aor.len > 0
						&& reg_impu_n < NOTIFY_MAX_IMPUS) {
					reg_impu_buf[0] = (char *)pkg_malloc(aor.len);
					if(reg_impu_buf[0]) {
						memcpy(reg_impu_buf[0], aor.s, aor.len);
						reg_impus[0].s = reg_impu_buf[0];
						reg_impus[0].len = aor.len;
						reg_impu_n = 1;
						if(barred_registration) {
							reg_barred[0] = reg_impus[0];
							reg_barred_n = 1;
						}
					}
				}

				if(sync_pcontact && contact_state == STATE_ACTIVE
						&& reg_impu_n > 0) {
					if(path.len > 0) {
						memset(&ci, 0, sizeof(ci));
						ci.path = &path;
						if(ul.update_pcontact(domain, &ci, sync_pcontact)
								!= 0) {
							LM_ERR("failed to persist notify path for <%.*s>\n",
									sync_pcontact->aor.len,
									sync_pcontact->aor.s);
						}
					}
					if(apply_gruu_from_unknown_params(
							   contacts->children, domain, sync_pcontact)
							!= 0) {
						LM_DBG("pcscf_apply_gruu failed from NOTIFY\n");
					}
					if(ul.update_contact_impus) {
						if(ul.update_contact_impus(domain, sync_pcontact,
								   reg_impus, reg_impu_n, 0,
								   reg_barred_n > 0 ? reg_barred : NULL,
								   reg_barred_n)
								!= 0) {
							LM_ERR("failed to sync IMPUs/barred from NOTIFY "
								   "for "
								   "<%.*s>\n",
									sync_pcontact->aor.len,
									sync_pcontact->aor.s);
						}
					}
				}

				for(reg_i = 0; reg_i < reg_impu_n; reg_i++) {
					if(reg_impu_buf[reg_i])
						pkg_free(reg_impu_buf[reg_i]);
				}
			}
		next_contact:
			if(cseq_char && (strlen(cseq_char) > 0)) {
				xmlFree(cseq_char);
				cseq_char = NULL;
			}
			if(expires_char && (strlen(expires_char) > 0)) {
				xmlFree(expires_char);
				expires_char = NULL;
			}
			if(user_agent.s && (strlen(user_agent.s) > 0)) {
				xmlFree(user_agent.s);
				user_agent.s = NULL;
			}
			if(path.s && (strlen(path.s) > 0)) {
				xmlFree(path.s);
				path.s = NULL;
			}
			if(received.s && (strlen(received.s) > 0)) {
				xmlFree(received.s);
				received.s = NULL;
			}
			if(callid.s && (strlen(callid.s) > 0)) {
				xmlFree(callid.s);
				callid.s = NULL;
			}
			if(contact_state_s && (strlen(contact_state_s) > 0)) {
				xmlFree(contact_state_s);
				contact_state_s = NULL;
			}
			if(event_s && (strlen(event_s) > 0)) {
				xmlFree(event_s);
				event_s = NULL;
			}
			if(barred_s && (strlen(barred_s) > 0)) {
				xmlFree(barred_s);
				barred_s = NULL;
			}

			contacts = contacts->next;
		}

	next_registration:
		// if (ul_record) ul.release_urecord(ul_record);
		/* Unlock the domain for this AOR: */
		//if (aor_key.len > 0)
		//	ul.unlock_udomain(domain, &aor_key);
		if(registration_state && (strlen(registration_state) > 0)) {
			xmlFree(registration_state);
			registration_state = NULL;
		}
		if(barred_s && (strlen(barred_s) > 0)) {
			xmlFree(barred_s);
			barred_s = NULL;
		}
		if(aor.len > 0 && aor.s) {
			xmlFree(aor.s);
			aor.s = NULL;
			aor.len = 0;
		}

		registrations = registrations->next;
	}
error:
	/* Free the XML-Document */
	if(doc)
		xmlFreeDoc(doc);
	return final_result;
}

int reginfo_handle_notify(struct sip_msg *msg, char *domain, char *s2)
{

	LM_DBG("Handling notify\n");
	str body;
	int result = 1;

	if(subscribe_to_reginfo != 1) {
		LM_ERR("Received a NOTIFY for reg-info but I have not SUBSCRIBED for "
			   "them.  Ignoring");
		return -1;
	}

	/* If not done yet, parse the whole message now: */
	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("Error parsing headers\n");
		return -1;
	}
	if(get_content_length(msg) == 0) {
		LM_DBG("Content length = 0\n");
		/* No Body? Then there is no published information available, which is ok. */
		return 1;
	} else {
		body.s = get_body(msg);
		if(body.s == NULL) {
			LM_ERR("cannot extract body from msg\n");
			return -1;
		}
		body.len = get_content_length(msg);
	}

	LM_DBG("Body is %.*s\n", body.len, body.s);

	result = process_body(msg, body, (udomain_t *)domain);


	return result;
}

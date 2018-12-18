/**
 * Copyright (C) 2018 Jose Luis Verdeguer
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

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/sr_module.h"
#include "secfilter.h"

/* get 'user-agent' header */
int secf_get_ua(struct sip_msg *msg, str *ua)
{
	ua->len = 0;

	if(msg == NULL)
		return -2;
	if(parse_headers(msg, HDR_USERAGENT_F, 0) != 0)
		return 1;
	if(msg->user_agent == NULL || msg->user_agent->body.s == NULL)
		return 1;

	ua->s = msg->user_agent->body.s;
	ua->len = msg->user_agent->body.len;

	return 0;
}


/* get 'from' header */
int secf_get_from(struct sip_msg *msg, str *name, str *user, str *domain)
{
	struct to_body *hdr;
	struct sip_uri parsed_uri;

	name->len = 0;
	user->len = 0;
	domain->len = 0;

	if(msg == NULL)
		return -1;
	if(parse_from_header(msg) < 0)
		return -1;
	if(msg->from == NULL || msg->from->body.s == NULL)
		return 1;

	hdr = get_from(msg);
	if(hdr->display.s != NULL) {
		name->s = hdr->display.s;
		name->len = hdr->display.len;

		if(name->len > 1 && name->s[0] == '"'
				&& name->s[name->len - 1] == '"') {
			name->s = name->s + 1;
			name->len = name->len - 2;
		}
	}

	if(parse_uri(hdr->uri.s, hdr->uri.len, &parsed_uri) < 0)
		return -1;

	if(parsed_uri.user.s != NULL) {
		user->s = parsed_uri.user.s;
		user->len = parsed_uri.user.len;
	}

	if(parsed_uri.host.s != NULL) {
		domain->s = parsed_uri.host.s;
		domain->len = parsed_uri.host.len;
	}

	return 0;
}


/* get 'to' header */
int secf_get_to(struct sip_msg *msg, str *name, str *user, str *domain)
{
	struct to_body *hdr;
	struct sip_uri parsed_uri;

	if(msg == NULL)
		return -1;
	if(parse_to_header(msg) < 0)
		return -1;
	if(msg->to == NULL || msg->to->body.s == NULL)
		return 1;

	hdr = get_to(msg);
	if(hdr->display.s != NULL) {
		name->s = hdr->display.s;
		name->len = hdr->display.len;

		if(name->len > 1 && name->s[0] == '"'
				&& name->s[name->len - 1] == '"') {
			name->s = name->s + 1;
			name->len = name->len - 2;
		}
	}

	if(parse_uri(hdr->uri.s, hdr->uri.len, &parsed_uri) < 0)
		return -1;

	if(parsed_uri.user.s != NULL) {
		user->s = parsed_uri.user.s;
		user->len = parsed_uri.user.len;
	}

	if(parsed_uri.host.s != NULL) {
		domain->s = parsed_uri.host.s;
		domain->len = parsed_uri.host.len;
	}

	return 0;
}


/* get 'contact' header */
int secf_get_contact(struct sip_msg *msg, str *user, str *domain)
{
	str contact = {NULL, 0};
	struct sip_uri parsed_uri;

	if(msg == NULL)
		return -1;
	if(msg->contact == NULL)
		return 1;
	if(!msg->contact->parsed && (parse_contact(msg->contact) < 0)) {
		LM_ERR("Error parsing contact header (%.*s)\n", msg->contact->body.len,
				msg->contact->body.s);
		return -1;
	}
	if(((contact_body_t *)msg->contact->parsed)->contacts
			&& ((contact_body_t *)msg->contact->parsed)->contacts->uri.s != NULL
			&& ((contact_body_t *)msg->contact->parsed)->contacts->uri.len
					   > 0) {
		contact.s = ((contact_body_t *)msg->contact->parsed)->contacts->uri.s;
		contact.len =
				((contact_body_t *)msg->contact->parsed)->contacts->uri.len;
	}
	if(contact.s == NULL)
		return 1;

	if(parse_uri(contact.s, contact.len, &parsed_uri) < 0) {
		LM_ERR("Error parsing contact uri header (%.*s)\n", contact.len,
				contact.s);
		return -1;
	}

	user->s = parsed_uri.user.s;
	user->len = parsed_uri.user.len;

	domain->s = parsed_uri.host.s;
	domain->len = parsed_uri.host.len;

	return 0;
}

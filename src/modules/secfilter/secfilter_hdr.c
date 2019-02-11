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
	if(msg == NULL) {
		LM_DBG("SIP msg is empty\n");
		return -1;
	}
	if(parse_headers(msg, HDR_USERAGENT_F, 0) != 0) {
		LM_DBG("cannot parse the User-agent header\n");
		return 1;
	}
	if(msg->user_agent == NULL || msg->user_agent->body.s == NULL) {
		LM_DBG("cannot parse the User-agent header\n");
		return 1;
	}

	ua->s = msg->user_agent->body.s;
	ua->len = msg->user_agent->body.len;

	return 0;
}


/* get 'from' header */
int secf_get_from(struct sip_msg *msg, str *name, str *user, str *domain)
{
	struct to_body *hdr;
	struct sip_uri uri;

	if(msg == NULL) {
		LM_DBG("SIP msg is empty\n");
		return -1;
	}
	if(parse_from_header(msg) < 0) {
		LM_DBG("cannot parse the From header\n");
		return 1;
	}
	if(msg->from == NULL || msg->from->body.s == NULL) {
		LM_DBG("cannot parse the From header\n");
		return 1;
	}

	hdr = get_from(msg);
	if(hdr->display.s != NULL && hdr->display.len > 0) {
		name->s = hdr->display.s;
		name->len = hdr->display.len;

		if(name->len > 1 && name->s[0] == '"'
				&& name->s[name->len - 1] == '"') {
			name->s = name->s + 1;
			name->len = name->len - 2;
		}
	}

	if(parse_uri(hdr->uri.s, hdr->uri.len, &uri) < 0) {
		LM_DBG("cannot parse the From URI header\n");
		return 1;
	}

	if(uri.user.s == NULL) {
		LM_DBG("cannot parse the From User\n");
		return 1;
	}
	user->s = uri.user.s;
	user->len = uri.user.len;

	if(uri.host.s == NULL) {
		LM_DBG("cannot parse the From Domain\n");
		return 1;
	}
	domain->s = uri.host.s;
	domain->len = uri.host.len;

	return 0;
}


/* get 'to' header */
int secf_get_to(struct sip_msg *msg, str *name, str *user, str *domain)
{
	struct to_body *hdr;
	struct sip_uri uri;

	if(msg == NULL) {
		LM_DBG("SIP msg is empty\n");
		return -1;
	}
	if(parse_to_header(msg) < 0) {
		LM_DBG("cannot parse the To header\n");
		return 1;
	}
	if(msg->to == NULL || msg->to->body.s == NULL) {
		LM_DBG("cannot parse the To header\n");
		return 1;
	}

	hdr = get_to(msg);
	if(hdr->display.s != NULL && hdr->display.len > 0) {
		name->s = hdr->display.s;
		name->len = hdr->display.len;

		if(name->len > 1 && name->s[0] == '"'
				&& name->s[name->len - 1] == '"') {
			name->s = name->s + 1;
			name->len = name->len - 2;
		}
	}

	if(parse_uri(hdr->uri.s, hdr->uri.len, &uri) < 0) {
		LM_DBG("cannot parse the To URI header\n");
		return 1;
	}

	if(uri.user.s == NULL) {
		LM_DBG("cannot parse the To User\n");
		return 1;
	}
	user->s = uri.user.s;
	user->len = uri.user.len;

	if(uri.host.s == NULL) {
		LM_DBG("cannot parse the To Domain\n");
		return 1;
	}
	domain->s = uri.host.s;
	domain->len = uri.host.len;

	return 0;
}


/* get 'contact' header */
int secf_get_contact(struct sip_msg *msg, str *user, str *domain)
{
	struct sip_uri uri;
	contact_t *contact;

	if(msg == NULL) {
		LM_DBG("SIP msg is empty\n");
		return -1;
	}
	if((parse_headers(msg, HDR_CONTACT_F, 0) == -1) || !msg->contact) {
		LM_DBG("cannot get the Contact header from the SIP message\n");
		return 1;
	}

	if(!msg->contact->parsed && parse_contact(msg->contact) < 0) {
		LM_DBG("cannot parse the Contact header\n");
		return 1;
	}

	contact = ((contact_body_t *)msg->contact->parsed)->contacts;
	if(!contact) {
		LM_DBG("cannot parse the Contact header\n");
		return 1;
	}

	if(parse_uri(contact->uri.s, contact->uri.len, &uri) < 0) {
		LM_DBG("cannot parse the Contact URI\n");
		return 1;
	}

	if(uri.user.s == NULL) {
		LM_DBG("cannot parse the Contact User\n");
		return 1;
	}
	user->s = uri.user.s;
	user->len = uri.user.len;

	if(uri.host.s == NULL) {
		LM_DBG("cannot parse the Contact Domain\n");
		return 1;
	}
	domain->s = uri.host.s;
	domain->len = uri.host.len;

	return 0;
}

/*
 * Copyright (C) 2005 iptelorg GmbH
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

/*!
 * \file
 * \brief Kamailio core :: UID handling
 * \ingroup core
 * Module: \ref core
 */

#include "id.h"
#include "parser/parse_from.h"
#include "parser/parse_uri.h"
#include "parser/digest/digest.h"
#include "ut.h"

static str uid_name = STR_STATIC_INIT(AVP_UID);
static str did_name = STR_STATIC_INIT(AVP_DID);


/**
 * Set From UID
 */
int set_from_uid(str* uid)
{
	struct search_state s;
	int_str val, name;
	avp_t* a;

	name.s = uid_name;
	a = search_first_avp(AVP_CLASS_USER | AVP_TRACK_FROM | AVP_NAME_STR, name, 0, &s);
	while(a) {
		destroy_avp(a);
		a = search_next_avp(&s, 0);
	}

	val.s = *uid;
	return add_avp(AVP_CLASS_USER | AVP_TRACK_FROM | AVP_NAME_STR | AVP_VAL_STR, name, val);
}


/** Extract username attribute from authorized credentials */
static inline str* cred_user(struct sip_msg* msg)
{
	struct hdr_field* h;
	auth_body_t* cred;

	get_authorized_cred(msg->proxy_auth, &h);
	if (!h) get_authorized_cred(msg->authorization, &h);
	if (!h) return 0;
	cred = (auth_body_t*)(h->parsed);
	if (!cred || !cred->digest.username.user.len) return 0;
	return &cred->digest.username.user;
}

/**
 * Set From UID
 */
int get_from_uid(str* uid, struct sip_msg* msg)
{
	static char buf[MAX_URI_SIZE];
	struct to_body* from;
	struct sip_uri puri;
	str* du;
	int_str val, name;

	name.s = uid_name;
	if (search_first_avp(AVP_CLASS_USER | AVP_TRACK_FROM | AVP_NAME_STR, name, &val, 0)) {
		*uid = val.s;
		return 1;
	} else {
		du = cred_user(msg);
		if (du) {
			     /* Try digest username first */
			*uid = *du;
		} else {
			     /* Get From URI username */
			if (parse_from_header(msg) < 0) {
				LM_ERR("unable to parse From header\n");
				return -1;
			}
			from = get_from(msg);
			if (parse_uri(from->uri.s, from->uri.len, &puri) == -1) {
				LM_ERR("unable to parsie From URI\n");
				return -1;
			}
		
			if (puri.user.len > MAX_URI_SIZE) {
				LM_ERR("username too long\n");
				return -1;
			}
			memcpy(buf, puri.user.s, puri.user.len);
			uid->s = buf;
			uid->len = puri.user.len;
			strlower(uid);
		}
		
		val.s = *uid;
		add_avp(AVP_CLASS_USER | AVP_TRACK_FROM | AVP_NAME_STR | AVP_VAL_STR, name, val);
		return 0;
	}
}

/** Get to UID
 */
int get_to_uid(str* uid, struct sip_msg* msg)
{
	static char buf[MAX_URI_SIZE];
	struct to_body* to;
	struct sip_uri puri;
	char* p;
	int_str val, name;

	name.s = uid_name;
	if (search_first_avp(AVP_CLASS_USER | AVP_TRACK_TO | AVP_NAME_STR, name, &val, 0)) {
		*uid = val.s;
		return 1;
	} else {
		if (msg->REQ_METHOD == METHOD_REGISTER) {
			if ((msg->to==0) && 
				(parse_headers(msg, HDR_TO_F, 0) < 0 || msg->to == 0)) {
				LM_DBG("Error while parsing To URI: to header bad or missing\n");
				return -1;
			}
			to = get_to(msg);
			if (parse_uri(to->uri.s, to->uri.len, &puri) == -1) {
				LM_DBG("Error while parsing To URI\n");
				return -1;
			}
			p = puri.user.s;
			uid->len = puri.user.len;
		} else {
			if (!msg->parsed_uri_ok && (parse_sip_msg_uri(msg) < 0)) {
				LM_DBG("Error while parsing the Request-URI\n");
				return -1;
			}
			p = msg->parsed_uri.user.s;
			uid->len = msg->parsed_uri.user.len;
		}
			
		if (uid->len > MAX_URI_SIZE) {
			LM_DBG("Username too long\n");
			return -1;
		}
		if (p == NULL || uid->len == 0) {
			LM_DBG("Username is empty\n");
			return -1;
		}
		memcpy(buf, p, uid->len);
		uid->s = buf;
		strlower(uid);

		val.s = *uid;
		add_avp(AVP_CLASS_USER | AVP_TRACK_TO | AVP_NAME_STR | AVP_VAL_STR, name, val);
		return 0;
	}
}


/**
 * Set To UID
 */
int set_to_uid(str* uid)
{
	struct search_state s;
	int_str val, name;
	avp_t* a;

	name.s = uid_name;
	a = search_first_avp(AVP_CLASS_USER | AVP_TRACK_TO | AVP_NAME_STR, name, 0, &s);
	while(a) {
		destroy_avp(a);
		a = search_next_avp(&s, 0);
	}

	val.s = *uid;
	return add_avp(AVP_CLASS_USER | AVP_TRACK_TO | AVP_NAME_STR | AVP_VAL_STR, name, val);
}


/**
 * Return current To domain id
 */
int get_to_did(str* did, struct sip_msg* msg)
{
	int_str val, name;

	name.s = did_name;
	if (search_first_avp(AVP_TRACK_TO | AVP_NAME_STR, name, &val, 0)) {
		*did = val.s;
		return 1;
	} 
	return 0;
}


/**
 * Return current To domain id
 */
int get_from_did(str* did, struct sip_msg* msg)
{
	int_str val, name;

	name.s = did_name;
	if (search_first_avp(AVP_TRACK_FROM | AVP_NAME_STR, name, &val, 0)) {
		*did = val.s;
		return 1;
	} 
	return 0;
}


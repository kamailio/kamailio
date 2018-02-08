/*
 * PUA_JSON module interface
 *
 * Copyright (C) 2010-2014 2600Hz
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */

#include <json.h>
#include "../json/api.h"
#include "../presence/bind_presence.h"

#include "defs.h"
#include "pua_json_publish.h"

extern json_api_t json_api;
extern presence_api_t presence_api;
extern int pua_include_entity;

str str_event_message_summary = str_init("message-summary");
str str_event_dialog = str_init("dialog");
str str_event_presence = str_init("presence");

str str_username_col = str_init("username");
str str_domain_col = str_init("domain");
str str_body_col = str_init("body");
str str_expires_col = str_init("expires");
str str_received_time_col = str_init("received_time");
str str_presentity_uri_col = str_init("presentity_uri");
str str_priority_col = str_init("priority");

str str_event_col = str_init("event");
str str_contact_col = str_init("contact");
str str_callid_col = str_init("callid");
str str_from_tag_col = str_init("from_tag");
str str_to_tag_col = str_init("to_tag");
str str_etag_col = str_init("etag");
str str_sender_col = str_init("sender");

str str_presence_note_busy = str_init("Busy");
str str_presence_note_otp = str_init("On the Phone");
str str_presence_note_idle = str_init("Idle");
str str_presence_note_offline = str_init("Offline");
str str_presence_act_busy = str_init("<rpid:busy/>");
str str_presence_act_otp = str_init("<rpid:on-the-phone/>");
str str_presence_status_offline = str_init("closed");
str str_presence_status_online = str_init("open");

str str_null_string = str_init("NULL");

int pua_json_update_presentity(str *event, str *realm, str *user, str *etag, str *sender, str *body, int expires, int new_t, int replace) {
	int ret;
	if(!event->len)
	{
		LM_ERR("presence event must be set\n");
		return -1;
	}
	if (!realm->len) {	
		LM_ERR("presence realm must be set\n");
		return -1;
	}
	if (!user->len) {
		LM_ERR("presence user must be set\n");
		return -1;
	}
	if (!etag->len) {
		LM_ERR("presence etag must be set\n");
		return -1;
	}
	if (!sender->len) {
		LM_ERR("presence sender must be set\n");
		return -1;
	}
	if (!body->len) {
		LM_ERR("presence body must be set\n");
		return -1;
	}
	ret = presence_api.update_presentity(
			event, realm, user, etag, sender, body, expires, new_t, replace);
	return ret;
}

int pua_json_publish_presence_to_presentity(struct json_object *json_obj) {
	int ret = 1;
	int len;
	str from = {0, 0};
	str from_user = {0, 0}, to_user = {0, 0};
	str from_realm = {0, 0};
	str callid = {0, 0};
	str state = {0, 0};
	str event = str_init("presence");
	str presence_body = {0, 0};
	str activity = str_init("");
	str note = str_init("Available");
	str status = str_presence_status_online;
	int expires = 0;

	char *body = (char *)pkg_malloc(PRESENCE_BODY_BUFFER_SIZE);
	if (body == NULL) {
		LM_ERR("Error allocating buffer for publish\n");
		ret = -1;
		goto error;
	}

	json_api.extract_field(json_obj, BLF_JSON_FROM, &from);
	json_api.extract_field(json_obj, BLF_JSON_FROM_USER, &from_user);
	json_api.extract_field(json_obj, BLF_JSON_FROM_REALM, &from_realm);
	json_api.extract_field(json_obj, BLF_JSON_TO_USER, &to_user);
	json_api.extract_field(json_obj, BLF_JSON_CALLID, &callid);
	json_api.extract_field(json_obj, BLF_JSON_STATE, &state);

	struct json_object *ExpiresObj =  json_api.get_object(json_obj, BLF_JSON_EXPIRES);
	if (ExpiresObj != NULL) {
		expires = json_object_get_int(ExpiresObj);
	}

	if (!from_user.len || !to_user.len || !state.len) {
		LM_ERR("missing one of From / To / State\n");
		goto error;
	}

	if (!strcmp(state.s, "early")) {
		note = str_presence_note_busy;
		activity = str_presence_act_busy;

	} else if (!strcmp(state.s, "confirmed")) {
		note = str_presence_note_otp;
		activity = str_presence_act_otp;

	} else if (!strcmp(state.s, "offline")) {
		note = str_presence_note_offline;
		status = str_presence_status_offline;

	}; // else {
	//	note = str_presence_note_idle;
	//}

	len = snprintf(body, PRESENCE_BODY_BUFFER_SIZE, PRESENCE_BODY, from_user.s,
			callid.s, status.s, note.s, activity.s, note.s);

	presence_body.s = body;
	presence_body.len = len;

	pua_json_update_presentity(&event, &from_realm, &from_user, &callid, &from, &presence_body, expires, 1, 1);

 error:

	if(body)
		pkg_free(body);

	return ret;
}

int pua_json_publish_mwi_to_presentity(struct json_object *json_obj) {
	int ret = 1;
	int len;
	str event = str_init("message-summary");
	str from = {0, 0};
	str from_user = {0, 0};
	str from_realm = {0, 0};
	str callid = {0, 0};
	str mwi_waiting = {0, 0}, mwi_voice_message = {0, 0}, mwi_new = {0, 0}, mwi_saved = {0, 0}, mwi_urgent = {0, 0}, mwi_urgent_saved = {0, 0}, mwi_account = {0, 0}, mwi_body = {0, 0};
	int expires = 0;

	char *body = (char *)pkg_malloc(MWI_BODY_BUFFER_SIZE);
	if(body == NULL) {
		LM_ERR("Error allocating buffer for publish\n");
		ret = -1;
		goto error;
	}

	json_api.extract_field(json_obj, BLF_JSON_FROM, &from);
	json_api.extract_field(json_obj, BLF_JSON_FROM_USER, &from_user);
	json_api.extract_field(json_obj, BLF_JSON_FROM_REALM, &from_realm);
	json_api.extract_field(json_obj, BLF_JSON_CALLID, &callid);

	json_api.extract_field(json_obj, MWI_JSON_WAITING, &mwi_waiting);
	json_api.extract_field(
			json_obj, MWI_JSON_VOICE_MESSAGE, &mwi_voice_message);
	json_api.extract_field(json_obj, MWI_JSON_NEW, &mwi_new);
	json_api.extract_field(json_obj, MWI_JSON_SAVED, &mwi_saved);
	json_api.extract_field(json_obj, MWI_JSON_URGENT, &mwi_urgent);
	json_api.extract_field(json_obj, MWI_JSON_URGENT_SAVED, &mwi_urgent_saved);
	json_api.extract_field(json_obj, MWI_JSON_ACCOUNT, &mwi_account);

	struct json_object *ExpiresObj =
			json_api.get_object(json_obj, BLF_JSON_EXPIRES);
	if (ExpiresObj != NULL) {
		expires = json_object_get_int(ExpiresObj);
	}

	if (mwi_new.len > 0) {
		len = snprintf(body, MWI_BODY_BUFFER_SIZE, MWI_BODY, mwi_waiting.len,
				mwi_waiting.s, mwi_account.len, mwi_account.s, mwi_new.len,
				mwi_new.s, mwi_saved.len, mwi_saved.s, mwi_urgent.len,
				mwi_urgent.s, mwi_urgent_saved.len, mwi_urgent_saved.s);
	} else if (mwi_voice_message.len > 0) {
		len = snprintf(body, MWI_BODY_BUFFER_SIZE, MWI_BODY_VOICE_MESSAGE,
				mwi_waiting.len, mwi_waiting.s, mwi_account.len, mwi_account.s,
				mwi_voice_message.len, mwi_voice_message.s);
	} else {
		len = snprintf(body, MWI_BODY_BUFFER_SIZE, MWI_BODY_NO_VOICE_MESSAGE,
				mwi_waiting.len, mwi_waiting.s, mwi_account.len, mwi_account.s);
	}

	mwi_body.s = body;
	mwi_body.len = len;

	pua_json_update_presentity(&event, &from_realm, &from_user, &callid, &from, &mwi_body, expires, 1, 1);

 error:

	if(body)
		pkg_free(body);

	return ret;
}


int pua_json_publish_dialoginfo_to_presentity(struct json_object *json_obj) {
	int ret = 1;
	int len;
	str from = {0, 0}, to = {0, 0}, pres = {0, 0};
	str from_user = {0, 0}, to_user = {0, 0}, pres_user = {0, 0};
	str from_realm = {0, 0}, pres_realm = {0, 0};
	str from_uri = {0, 0}, to_uri = {0, 0};
	str callid = {0, 0}, fromtag = {0, 0}, totag = {0, 0};
	str state = {0, 0};
	str direction = {0, 0};
	char sender_buf[SENDER_BUFFER_SIZE + 1];
	str sender = {0, 0};
	str dialoginfo_body = {0, 0};
	int expires = 0;
	str event = str_init("dialog");
	char to_tag_buffer[TO_TAG_BUFFER_SIZE + 1];
	char from_tag_buffer[FROM_TAG_BUFFER_SIZE + 1];

	char *body = (char *)pkg_malloc(DIALOGINFO_BODY_BUFFER_SIZE);
	if(body == NULL) {
		LM_ERR("Error allocating buffer for publish\n");
		ret = -1;
		goto error;
	}

	json_api.extract_field(json_obj, BLF_JSON_PRES, &pres);
	json_api.extract_field(json_obj, BLF_JSON_PRES_USER, &pres_user);
	json_api.extract_field(json_obj, BLF_JSON_PRES_REALM, &pres_realm);
	json_api.extract_field(json_obj, BLF_JSON_FROM, &from);
	json_api.extract_field(json_obj, BLF_JSON_FROM_USER, &from_user);
	json_api.extract_field(json_obj, BLF_JSON_FROM_REALM, &from_realm);
	json_api.extract_field(json_obj, BLF_JSON_FROM_URI, &from_uri);
	json_api.extract_field(json_obj, BLF_JSON_TO, &to);
	json_api.extract_field(json_obj, BLF_JSON_TO_USER, &to_user);
	json_api.extract_field(json_obj, BLF_JSON_TO_URI, &to_uri);
	json_api.extract_field(json_obj, BLF_JSON_CALLID, &callid);
	json_api.extract_field(json_obj, BLF_JSON_FROMTAG, &fromtag);
	json_api.extract_field(json_obj, BLF_JSON_TOTAG, &totag);
	json_api.extract_field(json_obj, BLF_JSON_DIRECTION, &direction);
	json_api.extract_field(json_obj, BLF_JSON_STATE, &state);

	struct json_object *ExpiresObj =
			json_api.get_object(json_obj, BLF_JSON_EXPIRES);
	if (ExpiresObj != NULL) {
		expires = json_object_get_int(ExpiresObj);
	}

	if (!from.len || !to.len || !state.len) {
		LM_ERR("missing one of From / To / State\n");
		goto error;
	}

	if (!pres.len || !pres_user.len || !pres_realm.len) {
		pres = from;
		pres_user = from_user;
		pres_realm = from_realm;
	}

	if (!from_uri.len)
		from_uri = from;

	if (!to_uri.len)
		to_uri = to;

	if (fromtag.len > 0) {
		fromtag.len = snprintf(from_tag_buffer, TO_TAG_BUFFER_SIZE, LOCAL_TAG,
				fromtag.len, fromtag.s);
		fromtag.s = from_tag_buffer;
	}

	if (totag.len > 0) {
		totag.len = snprintf(to_tag_buffer, FROM_TAG_BUFFER_SIZE, REMOTE_TAG,
				totag.len, totag.s);
		totag.s = to_tag_buffer;
	}

	if (callid.len) {
		if (pua_include_entity) {
			len = snprintf(body, DIALOGINFO_BODY_BUFFER_SIZE, DIALOGINFO_BODY,
					pres.len, pres.s, callid.len, callid.s, callid.len,
					callid.s, fromtag.len, fromtag.s, totag.len, totag.s,
					direction.len, direction.s, state.len, state.s,
					from_user.len, from_user.s, from.len, from.s, from_uri.len,
					from_uri.s, to_user.len, to_user.s, to.len, to.s,
					to_uri.len, to_uri.s);
		} else {
			len = snprintf(body, DIALOGINFO_BODY_BUFFER_SIZE, DIALOGINFO_BODY_2,
					pres.len, pres.s, callid.len, callid.s, callid.len,
					callid.s, fromtag.len, fromtag.s, totag.len, totag.s,
					direction.len, direction.s, state.len, state.s,
					from_user.len, from_user.s, from.len, from.s, to_user.len,
					to_user.s, to.len, to.s);
		}
	} else {
		len = snprintf(body, DIALOGINFO_BODY_BUFFER_SIZE, DIALOGINFO_EMPTY_BODY,
				pres.len, pres.s);
	}

	sender.len = snprintf(sender_buf, SENDER_BUFFER_SIZE, "sip:%s", callid.s);
	sender.s = sender_buf;

	dialoginfo_body.s = body;
	dialoginfo_body.len = len;

	pua_json_update_presentity(&event, &pres_realm, &pres_user, &callid, &sender, &dialoginfo_body, expires, 1, 1);

 error:

	if(body)
		pkg_free(body);

	return ret;
}


int pua_json_publish(struct sip_msg* msg, char *json) {
	str event_name = {0, 0}, event_package = {0, 0};
	struct json_object *json_obj = NULL;
	int ret = 1;

	/* extract info from json and construct xml */
	json_obj = json_api.json_parse(json);
	if (json_obj == NULL) {
		ret = -1;
		goto error;
	}

	json_api.extract_field(json_obj, BLF_JSON_EVENT_NAME, &event_name);
	if (event_name.len == 6 && strncmp(event_name.s, "update", 6) == 0) {
		json_api.extract_field(json_obj, BLF_JSON_EVENT_PKG, &event_package);
		if (event_package.len == str_event_dialog.len
				&& strncmp(event_package.s, str_event_dialog.s, event_package.len) == 0) {
			ret = pua_json_publish_dialoginfo_to_presentity(
					json_obj);
		} else if (event_package.len == str_event_message_summary.len
				&& strncmp(event_package.s, str_event_message_summary.s, event_package.len) == 0) {
			ret = pua_json_publish_mwi_to_presentity(json_obj);
		} else if (event_package.len == str_event_presence.len
				&& strncmp(event_package.s, str_event_presence.s, event_package.len) == 0) {
			ret = pua_json_publish_presence_to_presentity(
					json_obj);
		}
	}

error:

	if (json_obj)
		json_object_put(json_obj);

	return ret;
}

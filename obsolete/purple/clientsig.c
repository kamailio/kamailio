/* Kamailio PURPLE MODULE
 * 
 * Copyright (C) 2008 Atos Worldline
 * Contact: Eric PTAK <eric.ptak@atosorigin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <glib.h>

#include "../../dprint.h"
#include "../../mem/mem.h"

#include "defines.h"
#include "clientsig.h"
#include "purple.h"
#include "purple_sip.h"
#include "mapping.h"
#include "hashtable.h"
#include "utils.h"

#include <libpurple/account.h>
#include <libpurple/connection.h>
#include <libpurple/status.h>

static void signed_on(PurpleConnection *gc) {
	LM_DBG("signed-on with <%s>\n", gc->account->username);
}

static void signing_on(PurpleConnection *gc) {
	LM_DBG("signing-on with <%s>...\n", gc->account->username);
}

static void signed_off(PurpleConnection *gc) {
	LM_DBG("signed-off with <%s>\n", gc->account->username);
	purple_account_set_enabled(gc->account, UI_ID, FALSE);
}

static void signing_off(PurpleConnection *gc) {
	LM_DBG("signing-off with <%s>...\n", gc->account->username);
}

static void account_error_changed(PurpleAccount *acc, const PurpleConnectionErrorInfo *old_error, const PurpleConnectionErrorInfo *current_error) {
	if (current_error) {
		LM_DBG("new account error : <%s>\n", current_error->description);
	}
}

static void account_connecting(PurpleAccount *acc) {
	LM_DBG("trying to connect with <%s>\n", acc->username);
}

static void account_enabled(PurpleAccount *acc) {
	LM_DBG("account <%s> enabled...\n", acc->username);
}

static void account_disabled(PurpleAccount *acc) {
	LM_DBG("account <%s> disabled...\n", acc->username);
}


static void buddy_status_changed(PurpleBuddy *buddy, PurpleStatus *old_status, PurpleStatus *status) {
        PurplePlugin *prpl;
        PurplePluginProtocolInfo *prpl_info = NULL;
 	char *sip_from = find_sip_user(buddy->name);
	int d = hashtable_get_counter(buddy->name);
	PurpleStatusPrimitive primitive;
	enum purple_publish_basic basic;
	enum purple_publish_activity activity;
       	char *statustext = NULL, *tmp = NULL, *new;
	const char *end;

	LM_DBG("buddy <%s> has changed status\n", buddy->name);
	if ((sip_from) && (d>0)) {
		primitive = purple_status_type_get_primitive(purple_status_get_type(status));
		primitive_parse(primitive, &basic, &activity);


//		char *note = purple_status_get_attr_string(status, "message");

	        prpl = purple_find_prpl(purple_account_get_protocol_id(buddy->account));

		if (prpl != NULL)
			prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

		if (prpl_info && prpl_info->status_text && buddy->account->gc) {
			tmp = prpl_info->status_text(buddy);

			if(tmp && !g_utf8_validate(tmp, -1, &end)) {
				new = g_strndup(tmp, g_utf8_pointer_to_offset(tmp, end));
				g_free(tmp);
				tmp = new;
			}

			if(tmp) {
				g_strdelimit(tmp, "\n", ' ');
				purple_str_strip_char(tmp, '\r');
			}
			statustext = tmp;
		}


		LM_DBG("<%s> translated to <%s>, sending publish (note = %s)\n", buddy->name, sip_from, statustext);
		purple_send_sip_publish(sip_from, buddy->name, basic, activity, statustext);

		pkg_free(sip_from);
		g_free(statustext);
	}
}

static void buddy_signed_on(PurpleBuddy *buddy) {
	char *sip_from;
	int d;
	LM_DBG("buddy <%s> signed on\n", buddy->name);
	sip_from = find_sip_user(buddy->name);
	if (sip_from) {
		LM_DBG("<%s> translated to <%s>\n", buddy->name, sip_from);
	}
	else {
		LM_DBG("cannot translate <%s>\n", buddy->name);
		pkg_free(sip_from);
		return;
	}
	d = hashtable_get_counter(buddy->name);
	if (d>0) {
		if (purple_send_sip_publish(sip_from, buddy->name, PURPLE_BASIC_OPEN, PURPLE_ACTIVITY_AVAILABLE, NULL) < 0)
			LM_ERR("error sending PUBLISH for %s\n", buddy->name);
		else 
			LM_DBG("<%s> referenced %d times, PUBLISH sent\n", buddy->name, d);
		pkg_free(sip_from);
	}
	else 
		LM_DBG("%s is no more referenced, cannot publish\n", buddy->name);
	
}

static void buddy_signed_off(PurpleBuddy *buddy) {
	char *sip_from;
	int d;
	LM_DBG("buddy <%s> signed off\n", buddy->name);
	sip_from = find_sip_user(buddy->name);
	if (sip_from) {
		LM_DBG("<%s> translated to <%s>\n", buddy->name, sip_from);
	}
	else {
		LM_DBG("cannot translate <%s>\n", buddy->name);
		pkg_free(sip_from);
		return;
	}
	d = hashtable_get_counter(buddy->name);
	if (d>0) {
		if (purple_send_sip_publish(sip_from, buddy->name, PURPLE_BASIC_CLOSED, 0, NULL) < 0)
			LM_ERR("error sending PUBLISH for %s\n", buddy->name);
		else 
			LM_DBG("<%s> referenced %d times, PUBLISH sent\n", buddy->name, d);
		pkg_free(sip_from);
	}
	else 
		LM_DBG("%s is no more referenced, cannot publish\n", buddy->name);
	
	
}

static void buddy_added(PurpleBuddy *buddy) {
	LM_DBG("%s added to %s buddy list\n", buddy->name, buddy->account->username);
}

void client_connect_signals(void) {
	static int handle;
	purple_signal_connect(purple_connections_get_handle(), "signed-on", &handle,
			PURPLE_CALLBACK(signed_on), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signed-off", &handle,
			PURPLE_CALLBACK(signed_off), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signing-on", &handle,
			PURPLE_CALLBACK(signing_on), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signing-off", &handle,
			PURPLE_CALLBACK(signing_off), NULL);

	purple_signal_connect(purple_accounts_get_handle(), "account-error-changed", &handle,
			PURPLE_CALLBACK(account_error_changed), NULL);
	purple_signal_connect(purple_accounts_get_handle(), "account-connecting", &handle,
			PURPLE_CALLBACK(account_connecting), NULL);
	purple_signal_connect(purple_accounts_get_handle(), "account-enabled", &handle,
			PURPLE_CALLBACK(account_enabled), NULL);
	purple_signal_connect(purple_accounts_get_handle(), "account-disabled", &handle,
			PURPLE_CALLBACK(account_disabled), NULL);

	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-on", &handle,
			PURPLE_CALLBACK(buddy_signed_on), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-off", &handle,
			PURPLE_CALLBACK(buddy_signed_off), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-status-changed", &handle,
			PURPLE_CALLBACK(buddy_status_changed), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-added", &handle,
			PURPLE_CALLBACK(buddy_added), NULL);
}



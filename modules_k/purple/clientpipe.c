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
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "../../dprint.h"
#include "../../cfg/cfg_struct.h"

#include "purple.h"
#include "purplepipe.h"
#include "purple_sip.h"
#include "clientpipe.h"
#include "mapping.h"
#include "hashtable.h"
#include "clientaccount.h"
#include "utils.h"

#include <libpurple/account.h>
#include <libpurple/accountopt.h>
#include <libpurple/conversation.h>
#include <libpurple/connection.h>
#include <libpurple/core.h>
#include <libpurple/debug.h>
#include <libpurple/eventloop.h>
#include <libpurple/ft.h>
#include <libpurple/log.h>
#include <libpurple/notify.h>
#include <libpurple/plugin.h>
#include <libpurple/prefs.h>
#include <libpurple/prpl.h>
#include <libpurple/pounce.h>
#include <libpurple/savedstatuses.h>
#include <libpurple/sound.h>
#include <libpurple/status.h>
#include <libpurple/util.h>
#include <libpurple/whiteboard.h>
#include <libpurple/xmlnode.h>

static void pipe_handle_message(struct purple_message *message) {
	LM_DBG("handling message cmd\n");
	PurpleAccount *account = NULL;
	extern_account_t *accounts = NULL;
	extern_user_t *users = NULL;
	int naccounts = 0, nusers = 0;
	int i, j;

	PurpleConversation *conv = NULL;
	LM_DBG("calling find_accounts(\"%s\", &naccounts)\n", message->from);
	accounts = find_accounts(message->from, &naccounts);
	LM_DBG("found %d extra account(s) for <%s>", naccounts, message->from);

	LM_DBG("calling find_users(\"%s\", &nusers)\n", message->to);
	users = find_users(message->to, &nusers);
	LM_DBG("found %d extra user(s) for <%s>", nusers, message->to);

	for (i = 0; i < naccounts; i++) {
		LM_DBG("calling client_find_account(\"%s\")\n", accounts[i].username);
		account = client_find_account(&accounts[i]);
		if ((account) && purple_account_is_connected(account)) {
			//enable_account(account);
			for (j = 0; j < nusers; j++) {
				if (!strcmp(accounts[i].protocol, users[j].protocol)) {
					LM_DBG("mathing protocol found: %s\n", accounts[i].protocol);
					conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, users[j].username, account);
					if (conv == NULL)
						conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, users[j].username);
					purple_conv_im_send(purple_conversation_get_im_data(conv), message->body);
					break;
				}
			}
		}
		else if (account == NULL)
			LM_DBG("not account found neither created\n");
		else if (purple_account_is_disconnected(account))
			LM_DBG("account is disconnected cannot send message\n");
	}
	if (accounts)
		extern_account_free(accounts, naccounts);
	if (users)
		extern_user_free(users, nusers);

}

static void pipe_handle_publish(struct purple_publish *publish) {
	PurpleAccount *account = NULL;
	extern_account_t *accounts = NULL;
	int naccounts = 0;
	int i;

	LM_DBG("calling find_accounts(\"%s\", &naccoutns)\n", publish->from);
	accounts = find_accounts(publish->from, &naccounts);
	LM_DBG("found %d extra account(s) for <%s>", naccounts, publish->from);

	for (i = 0; i < naccounts; i++) {
		LM_DBG("calling client_find_account(\"%s\")\n", accounts[i].username);
		account = client_find_account(&accounts[i]);
		if (account) {
		
			if (publish->basic == PURPLE_BASIC_OPEN) {
				client_enable_account(account);
				LM_DBG("basic = open, setting up new status... %s,%d,%s\n", account[i].username, publish->primitive, publish->note);
				PurpleStatusType *type = purple_account_get_status_type_with_primitive(account, publish->primitive);
				if (purple_status_type_get_attr(type, "message")) {
					purple_account_set_status(account, purple_status_type_get_id(type), TRUE, "message", publish->note, NULL);
				} else {
					purple_account_set_status(account, purple_status_type_get_id(type), TRUE, NULL);
				}
			}

			else if (publish->basic == PURPLE_BASIC_CLOSED){
				LM_DBG("basic = closed, setting up new status to offline... %s\n", account[i].username);
				PurpleStatusType *type = purple_account_get_status_type_with_primitive(account, PURPLE_STATUS_OFFLINE);
				purple_account_set_status(account, purple_status_type_get_id(type), TRUE, NULL);
			}
			
		}
	}

	if (accounts)
		extern_account_free(accounts, naccounts);

}

static void pipe_handle_subscribe(struct purple_subscribe *subscribe) {
	PurpleAccount *account = NULL;
	extern_account_t *accounts = NULL;
	extern_user_t *users = NULL;
	int naccounts = 0, nusers = 0;
	int i, j;
	PurpleBuddy *buddy = NULL;

	int d = 0;

	const char *note;
	enum purple_publish_basic basic;
	enum purple_publish_activity activity;

	LM_DBG("calling find_accounts(\"%s\", &naccounts)\n", subscribe->from);
	accounts = find_accounts(subscribe->from, &naccounts);
	LM_DBG("found %d extra account(s) for <%s>", naccounts, subscribe->from);
	
	LM_DBG("calling find_users(\"%s\", &nusers)\n", subscribe->to);
	users = find_users(subscribe->to, &nusers);
	LM_DBG("found %d extra user(s) for <%s>", nusers, subscribe->to);

	for (i = 0; i < naccounts; i++) {
		LM_DBG("calling client_find_account(\"%s\")\n", accounts[i].username);
		account = client_find_account(&accounts[i]);
		//if ((account) && (purple_account_is_connected(account) || purple_account_is_connecting(account))) {
		if (account) {
			for (j = 0; j < nusers; j++) {
				if (!strcmp(accounts[i].protocol, users[j].protocol)) {
					LM_DBG("found matching protocol: %s\n", accounts[i].protocol);

					LM_DBG("subscribe expires : %d\n", subscribe->expires);
					if (subscribe->expires == 0)
						d = hashtable_dec_counter(users[j].username);
					else
						d = hashtable_inc_counter(users[j].username);
					
					LM_DBG("<%s> is now referenced %d times\n", users[j].username, d);
					if (d == 0) {
						LM_DBG("<%s> is no more referenced, removing presence...\n", users[j].username);
						if (purple_send_sip_publish(subscribe->to, users[j].username, PURPLE_BASIC_CLOSED, 0, NULL) < 0)
							LM_ERR("error sending presence for %s", subscribe->to);
						else
							LM_DBG("presence message sent successfully\n");
					}

					else {
	
						buddy = purple_find_buddy(account, users[j].username);
						if (buddy == NULL) {
							LM_DBG("<%s> not found in <%s> buddy list, adding\n", users[j].username, accounts[i].username);
							buddy = purple_buddy_new(account, users[j].username, users[j].username);
							//purple_blist_add_buddy(buddy, NULL, NULL, NULL);
							purple_account_add_buddy(account, buddy);
						}
						else {
							LM_DBG("<%s> found in <%s> buddy list, sending publish\n", users[j].username, accounts[i].username);
							PurplePresence *presence = purple_buddy_get_presence(buddy);
							PurpleStatus *status = purple_presence_get_active_status(presence);
							PurpleStatusType *type = purple_status_get_type(status);
							PurpleStatusPrimitive primitive = purple_status_type_get_primitive(type);
							note = purple_status_get_attr_string(status, "message");
							primitive_parse(primitive, &basic, &activity);

							if (purple_send_sip_publish(subscribe->to, users[j].username, basic, activity, note) < 0)
								LM_ERR("error sending presence for %s", subscribe->to);
							else
								LM_DBG("presence message sent successfully\n");
							
						}	
	
					}

					break;
				}
			}
		}
	}
	if (accounts)
		extern_account_free(accounts, naccounts);
	if (users)
		extern_user_free(users, nusers);
	
}

void pipe_reader(gpointer data, gint fd, PurpleInputCondition condition) {
	struct purple_cmd *cmd;
	if (read(fd, &cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to read from command pipe: %s\n", strerror(errno));
		return;
	}
	
	/* update the local config framework structures */
	cfg_update();

	switch (cmd->type) {
	        case PURPLE_MESSAGE_CMD:
		LM_DBG("received message cmd via pipe from <%s> to <%s>\n", cmd->message.from, cmd->message.to);
		pipe_handle_message(&cmd->message);
		break;

		case PURPLE_SUBSCRIBE_CMD:
		LM_DBG("received subscribe cmd via pipe from <%s> to <%s>\n", cmd->subscribe.from, cmd->subscribe.to);
		pipe_handle_subscribe(&cmd->subscribe);
		break;

		case PURPLE_PUBLISH_CMD:
		LM_DBG("received publish cmd via pipe from <%s>\n", cmd->publish.from);
		pipe_handle_publish(&cmd->publish);
		break;

		default:
		LM_ERR("unknown cmd type 0x%x\n", cmd->type);
	}
	purple_free_cmd(cmd);
}



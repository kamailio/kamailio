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
#include <libpurple/account.h>

#include "../../dprint.h"

#include "defines.h"
#include "clientaccount.h"
#include "mapping.h"

extern PurpleProxyInfo *proxy;

PurpleAccount *client_find_account(extern_account_t *account) {
	PurpleAccount *r;

	char* plugin;
	char username[255];
	memset(username, 0, 255);

	if (strcmp(account->protocol, "gtalk") == 0) {
		sprintf(username, "%s%s", account->username, "/sip");
		plugin = "prpl-jabber";
	}
	else {
		sprintf(username, "%s", account->username);
		plugin = account->protocol;
	}
	

	LM_DBG("searching purple account for %s with plugin %s \n", username, plugin);
	r = purple_accounts_find(username, plugin);
	if (r) {
		LM_DBG("account %s found\n", username);
		return r;
	}

	LM_DBG("account %s not found, creating.\n", username);
	r = purple_account_new(username, plugin);
	purple_account_set_password(r, account->password);
	purple_account_set_remember_password(r, TRUE);

	if (proxy != NULL)
		purple_account_set_proxy_info(r, proxy);

	if (strcmp(account->protocol, "gtalk") == 0)
		purple_account_set_string(r, "connect_server", "talk.google.com");

	purple_accounts_add(r);
	
	return r;
}

void client_enable_account(PurpleAccount *account) {

	if ((account) && !purple_account_get_enabled(account, UI_ID)) {
		LM_DBG("account %s disabled, enabling...\n", account->username);
		purple_account_set_enabled(account, UI_ID, TRUE);
	}

	if ((account) && purple_account_is_disconnected(account)) {
		LM_DBG("account %s disconnected, reconnecting...\n", account->username);
		purple_account_connect(account);
		LM_DBG("account %s connection called\n", account->username);
	}
}



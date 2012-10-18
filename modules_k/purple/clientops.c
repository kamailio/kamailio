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
#include <stdlib.h>
#include <string.h>
#include <libpurple/conversation.h>

#include "../../dprint.h"
#include "../../mem/mem.h"

#include "clientops.h"
#include "purple.h"
#include "purple_sip.h"
#include "mapping.h"

void write_conv(PurpleConversation *conv, const char *who, const char *alias, const char *message, PurpleMessageFlags flags, time_t mtime) {
	char *sip_to, *sip_from;
	purple_conversation_clear_message_history(conv);
	if (flags == PURPLE_MESSAGE_RECV) {
		LM_DBG("IM received from <%s> to <%s>\n", who, conv->account->username);
		sip_to = find_sip_user(conv->account->username);
		if (sip_to == NULL) {
			LM_DBG("cannot retrieve sip uri for <%s>\n", conv->account->username);
			return;
		}
		LM_DBG("<%s> translated to <%s>\n", conv->account->username, sip_to);

		sip_from = find_sip_user((char*) who);
		if (sip_from == NULL) {
			LM_DBG("cannot retrieve sip uri for <%s>\n", who);
			pkg_free(sip_to);
			return;
		}
		LM_DBG("<%s> translated to <%s>\n", who, sip_from);
		
		LM_DBG("sending sip message\n");
		if (purple_send_sip_msg(sip_to, sip_from, (char*) message) < 0)
			LM_ERR("error sending sip message\n");
		
		pkg_free(sip_to);
		pkg_free(sip_from);
	}
}



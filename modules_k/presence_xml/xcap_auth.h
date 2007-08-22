/*
 * $Id: xcap_auth.h 1337 2006-12-07 18:05:05Z anca_vamanu $
 *
 * presence_xml module - 
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2007-04-12  initial version (anca)
 */
#ifndef _AUTH_XML_H_
#define _AUTH_XML_H_

#include <libxml/parser.h>
#include "../../str.h"
#include "../presence/subscribe.h"
#include "../xcap_client/xcap_callbacks.h"

int pres_watcher_allowed(subs_t* subs);

xmlNodePtr get_rule_node(subs_t* subs, xmlDocPtr xcap_tree );
int get_rules_doc(str* user, str* domain, int type, str** rules_doc);
int pres_get_rules_doc(str* user, str* domain, str** rules_doc);

#endif

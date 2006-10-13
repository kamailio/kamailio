/*
 * $Id$
 *
 * presence module - presence server implementation
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
 *  2006-08-15  initial version (anca)
 */

#ifndef PIDF_H
#define PIDF_H

#include "../../str.h"
#include "notify.h"
#include <libxml/parser.h>

xmlNodePtr xmlDocGetNodeByName(xmlDocPtr doc, const char *name, const char *ns);
char *xmlNodeGetNodeContentByName(xmlNodePtr root, const char *name,
		const char *ns);
xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name);
char *xmlNodeGetAttrContentByName(xmlNodePtr node, const char *name);
str* get_final_notify_body( subs_t *subs, str* notify_body, 
		xmlNodePtr rule_node);

str* create_winfo_xml(watcher_t*, int n, char* version,char* resource,
		int STATE_FLAG );
str* agregate_xmls(str** body_array, int n);
int update_xml (str* body, str * new_body);
str* build_off_nbody(str p_user, str p_domain, str* etag);

#endif /* PIDF_H */


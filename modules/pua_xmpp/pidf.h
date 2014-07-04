/*
 * $Id: pidf.h 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_xmpp module - presence SIP - XMPP Gateway
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 * History:
 * --------
 *  2007-03-29  initial version (anca)
 */
#ifndef _PU_PIDF_
#define _PU_PIDF
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/parser.h>

char *xmlNodeGetAttrContentByName(xmlNodePtr node, const char *name);
xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name);
xmlNodePtr xmlDocGetNodeByName(xmlDocPtr doc, const char *name, const char *ns);
xmlNodePtr xmlNodeGetNodeByName(xmlNodePtr node, const char *name, 
		const char *ns);
char *xmlNodeGetNodeContentByName(xmlNodePtr root, const char *name,
		const char *ns);
#endif

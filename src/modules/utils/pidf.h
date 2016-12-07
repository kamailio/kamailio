/*
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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

/*! \file
 * \brief Kamailio utils :: PIDF handling
 * \ingroup utils
 */


#ifndef UTILS_PIDF_H
#define UTILS_PIDF_H

#include "../../str.h"
#include <libxml/parser.h>

xmlNodePtr xmlNodeGetNodeByName(xmlNodePtr node, const char *name,
															const char *ns);
xmlNodePtr xmlDocGetNodeByName(xmlDocPtr doc, const char *name, const char *ns);
xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name);

char *xmlNodeGetNodeContentByName(xmlNodePtr root, const char *name,
		const char *ns);
char *xmlNodeGetAttrContentByName(xmlNodePtr node, const char *name);

time_t xml_parse_dateTime(char* xml_time_str);

#endif 

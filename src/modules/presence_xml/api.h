/*
 * presence_xml module -
 *
 * Copyright (C) Kamailio project
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

#ifndef PXML_API_H
#define PXML_API_H
#include "../../core/str.h"

typedef int (*pres_check_basic_t)(
		struct sip_msg *, str presentity_uri, str status);
typedef int (*pres_check_activities_t)(
		struct sip_msg *, str presentity_uri, str activity);

typedef struct presence_xml_binds
{
	pres_check_basic_t pres_check_basic;
	pres_check_activities_t pres_check_activities;
} presence_xml_api_t;

typedef int (*bind_presence_xml_f)(presence_xml_api_t *);

int bind_presence_xml(struct presence_xml_binds *);

inline static int presence_xml_load_api(presence_xml_api_t *pxb)
{
	bind_presence_xml_f bind_presence_xml_exports;
	if(!(bind_presence_xml_exports = (bind_presence_xml_f)find_export(
				 "bind_presence_xml", 1, 0))) {
		LM_ERR("Failed to import bind_presence_xml\n");
		return -1;
	}
	return bind_presence_xml_exports(pxb);
}

#endif /*PXML_API_H*/

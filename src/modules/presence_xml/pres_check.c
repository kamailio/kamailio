/*
 * Copyright (C) 2011 Crocodile RCS Ltd.
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
 */


#include <stdio.h>
#include <libxml/parser.h>

#include "pres_check.h"
#include "pidf.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../str.h"
#include "../presence/event_list.h"

int presxml_check_basic(struct sip_msg *msg, str presentity_uri, str status)
{
	str *presentity = NULL;
	struct sip_uri parsed_uri;
	pres_ev_t *ev;
	static str event = str_init("presence");
	int retval = -1;
	xmlDocPtr xmlDoc = NULL;
	xmlNodePtr tuple = NULL, basicNode = NULL;
	char *basicVal = NULL;

	if (parse_uri(presentity_uri.s, presentity_uri.len, &parsed_uri) < 0)
	{
		LM_ERR("bad uri: %.*s\n", presentity_uri.len, presentity_uri.s);
		return -1;
	}

	ev = pres_contains_event(&event, NULL);
	if (ev == NULL)
	{
		LM_ERR("event presence is not registered\n");
		return -1;
	}

	presentity = pres_get_presentity(presentity_uri, ev, NULL, NULL);

	if (presentity == NULL || presentity->len <= 0 || presentity->s == NULL)
	{
		LM_DBG("cannot get presentity for %.*s\n", presentity_uri.len, presentity_uri.s);
		return -1;
	}

	if ((xmlDoc = xmlParseMemory(presentity->s, presentity->len)) == NULL)
	{
		LM_ERR("while parsing XML memory\n");
		goto error;
	}

	if ((tuple = xmlDocGetNodeByName(xmlDoc, "tuple", NULL)) == NULL)
	{
		LM_ERR("unable to extract 'tuple'\n");
		goto error;
	}

	while (tuple != NULL)
	{
		if (xmlStrcasecmp(tuple->name, (unsigned char *) "tuple") == 0)
		{
			if ((basicNode = xmlNodeGetNodeByName(tuple, "basic", NULL)) == NULL)
			{
				LM_ERR("while extracting 'basic' node\n");
				goto error;
			}

			if ((basicVal = (char *) xmlNodeGetContent(basicNode)) == NULL)
			{
				LM_ERR("while getting 'basic' content\n");
				goto error;
			}

			if (strncasecmp(basicVal, status.s, status.len) == 0)
				retval = 1;

			xmlFree(basicVal);
		}
		tuple = tuple->next;
	}
error:
	if (xmlDoc != NULL)
		xmlFreeDoc(xmlDoc);
	pres_free_presentity(presentity, ev);
	return retval;
}

int presxml_check_activities(struct sip_msg *msg, str presentity_uri, str activity)
{
	str *presentity = NULL;
	struct sip_uri parsed_uri;
	pres_ev_t *ev;
	static str event = str_init("presence");
	char *nodeName = NULL;
	int retval = -1;
	xmlDocPtr xmlDoc = NULL;
	xmlNodePtr person = NULL, activitiesNode = NULL, activityNode = NULL;

	if (parse_uri(presentity_uri.s, presentity_uri.len, &parsed_uri) < 0)
	{
		LM_ERR("bad uri: %.*s\n", presentity_uri.len, presentity_uri.s);
		return -1;
	}

	ev = pres_contains_event(&event, NULL);
	if (ev == NULL)
	{
		LM_ERR("event presence is not registered\n");
		return -1;
	}

	if ((nodeName = pkg_malloc(activity.len + 1)) == NULL)
	{
		LM_ERR("cannot pkg_malloc for nodeName\n");
		return -1;		
	}
	memcpy(nodeName, activity.s, activity.len);
	nodeName[activity.len] = '\0';

	presentity = pres_get_presentity(presentity_uri, ev, NULL, NULL);

	if (presentity == NULL || presentity->len <= 0 || presentity->s == NULL)
	{
		LM_DBG("cannot get presentity for %.*s\n", presentity_uri.len, presentity_uri.s);
		goto error;
	}

	if ((xmlDoc = xmlParseMemory(presentity->s, presentity->len)) == NULL)
	{
		LM_ERR("while parsing XML memory\n");
		goto error;
	}

	if ((person = xmlDocGetNodeByName(xmlDoc, "person", NULL)) == NULL)
	{
		LM_DBG("unable to extract 'person'\n");
		retval = -2;
		goto error;
	}

	while (person != NULL)
	{
		if (xmlStrcasecmp(person->name, (unsigned char *) "person") == 0)
		{
			if ((activitiesNode = xmlNodeGetNodeByName(person, "activities", NULL)) == NULL)
			{
				LM_DBG("unable to extract 'activities' node\n");
				if (retval <= 0)
				{
					retval = -2;
				}
				break;
			}

			if (activitiesNode->children == NULL)
			{
				LM_DBG("activities node has no children\n");
				if (retval <= 0)
				{
					retval = -2;
				}
				break;
			}

			if ((activityNode = xmlNodeGetNodeByName(activitiesNode, nodeName, NULL)) != NULL)
			{
				retval = 1;
			}
		}
		person = person->next;
	}
error:
	if (nodeName != NULL)
		pkg_free(nodeName);
	if (xmlDoc != NULL)
		xmlFreeDoc(xmlDoc);
	if(presentity != NULL)
		pres_free_presentity(presentity, ev);
	return retval;
}

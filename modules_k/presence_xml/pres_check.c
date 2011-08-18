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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <libxml/parser.h>

#include "pres_check.h"
#include "pidf.h"
#include "../../mod_fix.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../str.h"
#include "../presence/event_list.h"

int fixup_presxml_check(void **param, int param_no)
{
        if(param_no==1)
        {
                return fixup_spve_null(param, 1);
        } else if(param_no==2) {
                return fixup_spve_null(param, 1);
        }
        return 0;
}

int presxml_check_basic(struct sip_msg *msg, char *presentity_uri, char *status)
{
	str uri, basic;
	str *presentity = NULL;
	struct sip_uri parsed_uri;
	pres_ev_t *ev;
	static str event = str_init("presence");
	int retval = -1;
	xmlDocPtr xmlDoc = NULL;
	xmlNodePtr tuple = NULL, basicNode = NULL;
	char *basicVal = NULL;

	if (fixup_get_svalue(msg, (gparam_p)presentity_uri, &uri) != 0)
	{
		LM_ERR("invalid presentity uri parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)status, &basic) != 0)
	{
		LM_ERR("invalud status parameter\n");
		return -1;
	}

	if (parse_uri(uri.s, uri.len, &parsed_uri) < 0)
	{
		LM_ERR("bad uri: %.*s\n", uri.len, uri.s);
		return -1;
	}

	ev = pres_contains_event(&event, NULL);
	if (ev == NULL)
	{
		LM_ERR("event presence is not registered\n");
		return -1;
	}

	presentity = pres_get_presentity(uri, ev, NULL, NULL);

	if (presentity == NULL || presentity->len <= 0 || presentity->s == NULL)
	{
		LM_DBG("cannot get presentity for %.*s\n", uri.len, uri.s);
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

			if (strncasecmp(basicVal, basic.s, basic.len) == 0)
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

int presxml_check_activities(struct sip_msg *msg, char *presentity_uri, char *activity)
{
	str uri, act;
	str *presentity = NULL;
	struct sip_uri parsed_uri;
	pres_ev_t *ev;
	static str event = str_init("presence");
	char *nodeName = NULL;
	int retval = -1;
	xmlDocPtr xmlDoc = NULL;
	xmlNodePtr person = NULL, activitiesNode = NULL, activityNode = NULL;

	if (fixup_get_svalue(msg, (gparam_p)presentity_uri, &uri) != 0)
	{
		LM_ERR("invalid presentity uri parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)activity, &act) != 0)
	{
		LM_ERR("invalid activity parameter\n");
		return -1;
	}

	if (parse_uri(uri.s, uri.len, &parsed_uri) < 0)
	{
		LM_ERR("bad uri: %.*s\n", uri.len, uri.s);
		return -1;
	}

	ev = pres_contains_event(&event, NULL);
	if (ev == NULL)
	{
		LM_ERR("event presence is not registered\n");
		return -1;
	}

	if ((nodeName = pkg_malloc(act.len + 1)) == NULL)
	{
		LM_ERR("cannot pkg_malloc for nodeName\n");
		return -1;		
	}
	memcpy(nodeName, act.s, act.len);
	nodeName[act.len] = '\0';

	presentity = pres_get_presentity(uri, ev, NULL, NULL);

	if (presentity == NULL || presentity->len <= 0 || presentity->s == NULL)
	{
		LM_DBG("cannot get presentity for %.*s\n", uri.len, uri.s);
		return -1;
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
				LM_DBG("unable to extract 'actvities' node\n");
				retval = -2;
				goto error;
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
	pres_free_presentity(presentity, ev);
	return retval;
}

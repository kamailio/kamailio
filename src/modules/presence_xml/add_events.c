/*
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
 * History:
 * --------
 *  2007-04-17  initial version (anca)
 */

/*!
 * \file
 * \brief Kamailio Presence_XML :: Several event packages, presence, presence.winfo, dialog;sla 
 * \ingroup presence_xml
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include "../../parser/parse_content.h"
#include "../../data_lump_rpl.h"
#include "../../ut.h"
#include "xcap_auth.h"
#include "notify_body.h"
#include "add_events.h"
#include "presence_xml.h"

extern int disable_presence;
extern int disable_winfo;
extern int disable_bla;
extern int disable_xcapdiff;

static str pu_415_rpl  = str_init("Unsupported media type");

int xml_add_events(void)
{
	pres_ev_t event;
	
	if (!disable_presence) {
		/* constructing presence event */
		memset(&event, 0, sizeof(pres_ev_t));
		event.name.s= "presence";
		event.name.len= 8;

		event.content_type.s= "application/pidf+xml";
		event.content_type.len= 20;

		event.type= PUBL_TYPE;
		event.req_auth= 1;
		event.apply_auth_nbody= pres_apply_auth;
		event.get_auth_status= pres_watcher_allowed;
		event.agg_nbody= pres_agg_nbody;
		event.evs_publ_handl= xml_publ_handl;
		event.free_body= free_xml_body;
		event.default_expires= 3600;
		event.get_rules_doc= pres_get_rules_doc;
		event.get_pidf_doc= pres_get_pidf_doc;
		if(pres_add_event(&event)< 0)
		{
			LM_ERR("while adding event presence\n");
			return -1;
		}		
		LM_DBG("added 'presence' event to presence module\n");
	}

	if (!disable_winfo) {
		/* constructing presence.winfo event */
		memset(&event, 0, sizeof(pres_ev_t));
		event.name.s= "presence.winfo";
		event.name.len= 14;

		event.content_type.s= "application/watcherinfo+xml";
		event.content_type.len= 27;
		event.type= WINFO_TYPE;
		event.free_body= free_xml_body;
		event.default_expires= 3600;

		if(pres_add_event(&event)< 0)
		{
			LM_ERR("while adding event presence.winfo\n");
			return -1;
		}
		LM_DBG("added 'presence.winfo' event to presence module\n");
	}
	
	if (!disable_bla) {
		/* constructing bla event */
		memset(&event, 0, sizeof(pres_ev_t));
		event.name.s= "dialog;sla";
		event.name.len= 10;

		event.etag_not_new= 1;
		event.evs_publ_handl= xml_publ_handl;
		event.content_type.s= "application/dialog-info+xml";
		event.content_type.len= 27;
		event.type= PUBL_TYPE;
		event.free_body= free_xml_body;
		event.default_expires= 3600;
		if(pres_add_event(&event)< 0)
		{
			LM_ERR("while adding event dialog;sla\n");
			return -1;
		}
		LM_DBG("added 'dialog;sla' event to presence module\n");
	}
	
	if (!disable_xcapdiff) {
		/* constructing xcap-diff event */
		memset(&event, 0, sizeof(pres_ev_t));
		event.name.s= "xcap-diff";
		event.name.len= 9;

		event.content_type.s= "application/xcap-diff+xml";
		event.content_type.len= 25;

		event.type= PUBL_TYPE;
		event.default_expires= 3600;
		if(pres_add_event(&event)< 0)
		{
			LM_ERR("while adding event xcap-diff\n");
			return -1;
		}
		LM_DBG("added 'xcap-diff' event to presence module\n");
	}

	return 0;
}
/*
 * in event specific publish handling - only check is good body format
 */
int	xml_publ_handl(struct sip_msg* msg)
{	
	str body= {0, 0};
	xmlDocPtr doc= NULL;

	if ( get_content_length(msg) == 0 )
		return 1;
	
	body.s=get_body(msg);
	if (body.s== NULL) 
	{
		LM_ERR("cannot extract body from msg\n");
		goto error;
	}
	/* content-length (if present) must be already parsed */

	body.len = get_content_length( msg );
	doc= xmlParseMemory( body.s, body.len );
	if(doc== NULL)
	{
		LM_ERR("bad body format\n");
		if(slb.freply(msg, 415, &pu_415_rpl) < 0)
		{
			LM_ERR("while sending '415 Unsupported media type' reply\n");
		}
		goto error;
	}
	xmlFreeDoc(doc);
	xmlCleanupParser();
	xmlMemoryDump();
	return 1;

error:
	xmlFreeDoc(doc);
	xmlCleanupParser();
	xmlMemoryDump();
	return -1;

}	

/*
 * pua module - presence user agent module
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
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>

#include "event_list.h"
#include "add_events.h"
#include "pua.h"
#include "pidf.h"

extern int dlginfo_increase_version;

int pua_add_events(void)
{
	/* add presence */
	if(add_pua_event(PRESENCE_EVENT, "presence", "application/pidf+xml", 
				pres_process_body)< 0)
	{
		LM_ERR("while adding event presence\n");
		return -1;
	}

	/* add dialog */
	if (dlginfo_increase_version) {
		if(add_pua_event(DIALOG_EVENT, "dialog", "application/dialog-info+xml",
					bla_process_body)< 0)
		{
			LM_ERR("while adding event dialog w/ dlginfo_increase_version\n");
			return -1;
		}
	} else {
		if(add_pua_event(DIALOG_EVENT, "dialog", "application/dialog-info+xml",
					dlg_process_body)< 0)
		{
			LM_ERR("while adding event dialog w/o dlginfo_increase_version\n");
			return -1;
		}
	}

	/* add dialog;sla */
	if(add_pua_event(BLA_EVENT, "dialog;sla", "application/dialog-info+xml",
				bla_process_body)< 0)
	{
		LM_ERR("while adding event dialog;sla\n");
		return -1;
	}

	/* add message-summary*/
	if(add_pua_event(MSGSUM_EVENT, "message-summary", 
				"application/simple-message-summary", mwi_process_body)< 0)
	{
		LM_ERR("while adding event message-summary\n");
		return -1;
	}
	
	/* add presence;winfo */
	if(add_pua_event(PWINFO_EVENT, "presence.winfo", NULL, NULL)< 0)
	{
		LM_ERR("while adding event presence.winfo\n");
		return -1;
	}
	
	/* add application/reginfo+xml */
	if (dlginfo_increase_version) {
		if(add_pua_event(REGINFO_EVENT, "reg", "application/reginfo+xml", reginfo_process_body)< 0) {
			LM_ERR("while adding event application/reginfo+xml with version increase\n");
			return -1;
		}
	} else {
		if(add_pua_event(REGINFO_EVENT, "reg", "application/reginfo+xml", dlg_process_body)< 0) {
			LM_ERR("while adding event application/reginfo+xml\n");
			return -1;
		}
	}
	
	/* add xcap-diff */
	if(add_pua_event(XCAPDIFF_EVENT, "xcap-diff",
				"application/xcap-diff+xml", 0)< 0)
	{
		LM_ERR("while adding event xcap-diff\n");
		return -1;
	}

	return 0;

}	

int pres_process_body(publ_info_t* publ, str** fin_body, int ver, str** tuple_param)
{

	xmlDocPtr doc= NULL;
	xmlNodePtr node= NULL;
	char* tuple_id= NULL, *person_id= NULL;
	int tuple_id_len= 0;
	char buf[50];
	str* body= NULL;
	int alloc_tuple= 0;
	str* tuple= NULL;

	doc= xmlParseMemory(publ->body->s, publ->body->len );
	if(doc== NULL)
	{
		LM_ERR("while parsing xml memory\n");
		goto error;
	}

	node= xmlDocGetNodeByName(doc, "tuple", NULL);
	if(node == NULL)
	{
		LM_ERR("while extracting tuple node\n");
		goto error;
	}
	tuple= *(tuple_param);

	tuple_id= xmlNodeGetAttrContentByName(node, "id");
	if(tuple_id== NULL)
	{

		if(tuple== NULL)	// generate a tuple_id
		{
			tuple_id= buf;
			tuple_id_len= sprintf(tuple_id, "%p", publ);
			tuple_id[tuple_id_len]= '\0'; 

			tuple=(str*)pkg_malloc(sizeof(str));
			if(tuple== NULL)
			{
				LM_ERR("No more memory\n");
				goto error;
			}
			tuple->s= (char*)pkg_malloc(tuple_id_len* sizeof(char));
			if(tuple->s== NULL)
			{
				LM_ERR("NO more memory\n");
				goto error;
			}
			memcpy(tuple->s, tuple_id, tuple_id_len);
			tuple->len= tuple_id_len;

			*tuple_param= tuple;
			alloc_tuple= 1;

			LM_DBG("allocated tuple_id\n\n");
		}
		else
		{
			tuple_id= buf;
			tuple_id_len= tuple->len;
			memcpy(tuple_id, tuple->s, tuple_id_len);
			tuple_id[tuple_id_len]= '\0';
		}
		/* add tuple id */
		if(!xmlNewProp(node, BAD_CAST "id", BAD_CAST tuple_id))
		{
			LM_ERR("while extracting xml"
						" node\n");
			goto error;
		}
	}
	else
	{
		if(tuple== NULL)
		{
			strcpy(buf, tuple_id);
			xmlFree(tuple_id);
			tuple_id= buf;
			tuple_id_len= strlen(tuple_id);
		
			tuple=(str*)pkg_malloc(sizeof(str));
			if(tuple== NULL)
			{
				LM_ERR("No more memory\n");
				goto error;
			}
			alloc_tuple= 1;
			tuple->s= (char*)pkg_malloc(tuple_id_len* sizeof(char));
			if(tuple->s== NULL)
			{
				LM_ERR("NO more memory\n");
				goto error;
			}
			memcpy(tuple->s, tuple_id, tuple_id_len);
			tuple->len= tuple_id_len;
			*tuple_param= tuple;
		}
	}

	node= xmlDocGetNodeByName(doc, "person", NULL);
	if(node)
	{
		LM_DBG("found person node\n");
		person_id= xmlNodeGetAttrContentByName(node, "id");
		if(person_id== NULL)
		{	
			if(!xmlNewProp(node, BAD_CAST "id", BAD_CAST tuple_id))
			{
				LM_ERR("while extracting xml"
						" node\n");
				goto error;
			}
		}
		else
		{
			xmlFree(person_id);
		}
	}	
	body= (str*)pkg_malloc(sizeof(str));
	if(body== NULL)
	{
		LM_ERR("NO more memory left\n");
		goto error;
	}
	memset(body, 0, sizeof(str));
	xmlDocDumpFormatMemory(doc,(xmlChar**)(void*)&body->s, &body->len, 1);	
	if(body->s== NULL || body->len== 0)
	{
		LM_ERR("while dumping xml format\n");
		goto error;
	}	
	xmlFreeDoc(doc);
	doc= NULL;
	
	*fin_body= body;
	xmlMemoryDump();
	xmlCleanupParser();
	return 1;

error:
	if(doc)
		xmlFreeDoc(doc);
	if(body)
		pkg_free(body);
	if(tuple && alloc_tuple)
	{
		if(tuple->s)
			pkg_free(tuple->s);
		pkg_free(tuple);
		tuple= NULL;
	}
	return -1;

}	

int bla_process_body(publ_info_t* publ, str** fin_body, int ver, str** tuple)
{
	xmlNodePtr node= NULL;
	xmlDocPtr doc= NULL;
	char* version;
	str* body= NULL;
	int len;
	str* init_body;

	init_body= publ->body;

	doc= xmlParseMemory(init_body->s, init_body->len );
	if(doc== NULL)
	{
		LM_ERR("while parsing xml memory\n");
		goto error;
	}
	/* change version and state*/
	node= xmlDocGetNodeByName(doc, "dialog-info", NULL);
	if(node == NULL)
	{
		LM_ERR("while extracting dialog-info node\n");
		goto error;
	}
	version= int2str(ver,&len);
	version[len]= '\0';

	if( xmlSetProp(node, (const xmlChar *)"version",(const xmlChar*)version)== NULL)
	{
		LM_ERR("while setting version attribute\n");
		goto error;	
	}
	body= (str*)pkg_malloc(sizeof(str));
	if(body== NULL)
	{
		LM_ERR("NO more memory left\n");
		goto error;
	}
	memset(body, 0, sizeof(str));
	xmlDocDumpFormatMemory(doc, (xmlChar**)(void*)&body->s, &body->len, 1);	

	xmlFreeDoc(doc);
	doc= NULL;
	*fin_body= body;	
	if(*fin_body== NULL)
		LM_DBG("NULL fin_body\n");

	xmlMemoryDump();
	xmlCleanupParser();
	LM_DBG("successful\n");
	return 1;

error:
	if(doc)
		xmlFreeDoc(doc);
	if(body)
		pkg_free(body);
	
	xmlMemoryDump();
	xmlCleanupParser();
	return -1;
}

int reginfo_process_body(publ_info_t* publ, str** fin_body, int ver, str** tuple)
{
	xmlNodePtr node= NULL;
	xmlDocPtr doc= NULL;
	char* version;
	str* body= NULL;
	int len;
	str* init_body;

	init_body= publ->body;

	doc= xmlParseMemory(init_body->s, init_body->len );
	if(doc== NULL) {
		LM_ERR("while parsing xml memory\n");
		goto error;
	}
	/* change version and state*/
	node= xmlDocGetNodeByName(doc, "reginfo", NULL);
	if(node == NULL) {
		LM_ERR("while extracting dialog-info node\n");
		goto error;
	}
	version= int2str(ver,&len);
	version[len]= '\0';

	if( xmlSetProp(node, (const xmlChar *)"version",(const xmlChar*)version)== NULL) {
		LM_ERR("while setting version attribute\n");
		goto error;	
	}
	body= (str*)pkg_malloc(sizeof(str));
	if(body== NULL) {
		LM_ERR("NO more memory left\n");
		goto error;
	}
	memset(body, 0, sizeof(str));
	xmlDocDumpFormatMemory(doc, (xmlChar**)(void*)&body->s, &body->len, 1);	

	xmlFreeDoc(doc);
	doc= NULL;
	*fin_body= body;	
	if(*fin_body== NULL)
		LM_DBG("NULL fin_body\n");

	xmlMemoryDump();
	xmlCleanupParser();
	LM_DBG("successful\n");
	return 1;

error:
	if(doc)
		xmlFreeDoc(doc);
	if(body)
		pkg_free(body);
	
	xmlMemoryDump();
	xmlCleanupParser();
	return -1;
}

int mwi_process_body(publ_info_t* publ, str** fin_body, int ver, str** tuple)
{
	*fin_body= publ->body;
	return 0;
}

int dlg_process_body(publ_info_t* publ, str** fin_body, int ver, str** tuple)
{
	*fin_body= publ->body;
	return 0;
}


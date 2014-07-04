/*
 * presence_conference module - mariusbucur
 *
 * Copyright (C) 2010 Marius Bucur
 * Copyright (C) 2006 Voice Sistem S.R.L.
 * Copyright (C) 2008 Klaus Darilion, IPCom
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
 * 2010-07-12  initial version (mariusbucur)
 */
/*! \file
 * \brief Kamailio Presence_Conference :: Notify body handling
 * \ingroup presence_conference
 */

#define MAX_INT_LEN 11 /* 2^32: 10 chars + 1 char sign */

#include <string.h>
#include <stdlib.h>
#include <libxml/parser.h>

#include "../../mem/mem.h"
#include "../presence/utils_func.h"
#include "../presence/hash.h"
#include "../presence/event_list.h"
#include "../presence/presence.h"
#include "../presence/presentity.h"
#include "notify_body.h"
#include "pidf.h"

str* agregate_xmls(str* pres_user, str* pres_domain, str** body_array, int n, int off_index);

void free_xml_body(char* body)
{
	if(body== NULL)
		return;

	xmlFree(body);
}


str* conf_agg_nbody(str* pres_user, str* pres_domain, str** body_array, int n, int off_index)
{
	str* n_body= NULL;

	LM_DBG("[pres_user]=%.*s [pres_domain]= %.*s, [n]=%d\n",
			pres_user->len, pres_user->s, pres_domain->len, pres_domain->s, n);

	if(body_array== NULL)
		return NULL;

	n_body = agregate_xmls(pres_user, pres_domain, body_array, n, off_index);
	LM_DBG("[n_body]=%p\n", n_body);
	if(n_body) {
		LM_DBG("[*n_body]=%.*s\n",
			n_body->len, n_body->s);
	}
	if(n_body== NULL && n!= 0)
	{
		LM_ERR("while aggregating body\n");
	}

	return n_body;
}	

str* agregate_xmls(str* pres_user, str* pres_domain, str** body_array, int n, int off_index)
{
	int i, j = 0;
	
	if(body_array == NULL || n == 0)
		return 0;

	xmlDocPtr  doc = NULL;
	xmlNodePtr root_node = NULL;
	xmlNsPtr   namespace = NULL;

	xmlNodePtr p_root= NULL;
	xmlDocPtr* xml_array ;
	xmlNodePtr node = NULL;
	str *body= NULL;
	char buf[MAX_URI_SIZE+1];

	LM_DBG("[pres_user]=%.*s [pres_domain]= %.*s, [n]=%d\n",
			pres_user->len, pres_user->s, pres_domain->len, pres_domain->s, n);

	xml_array = (xmlDocPtr*)pkg_malloc( n*sizeof(xmlDocPtr) );
	if(unlikely(xml_array == NULL))
	{
		LM_ERR("while allocating memory");
		return NULL;
	}
	memset(xml_array, 0, n*sizeof(xmlDocPtr)) ;

	/* parse all the XML documents */
	for(i=0; i<n; i++)
	{
		if(body_array[i] == NULL )
			continue;

		xml_array[j] = xmlParseMemory( body_array[i]->s, body_array[i]->len );
		
		/* LM_DBG("parsing XML body: [n]=%d, [i]=%d, [j]=%d xml_array[j]=%p\n", n, i, j, xml_array[j] ); */

		if(unlikely(xml_array[j] == NULL))
		{
			LM_ERR("while parsing xml body message\n");
			goto error;
		}
		j++;
		
	}

	if(j == 0)  /* no body */
	{
		if(xml_array)
			pkg_free(xml_array);
		return NULL;
	}

	/* n: number of bodies in total */
	/* j: number of useful bodies; created XML structures */
	/* i: loop counter */
	/* LM_DBG("number of bodies in total [n]=%d, number of useful bodies [j]=%d\n", n, j ); */

	/* create the new NOTIFY body  */
	if ( (pres_user->len + pres_domain->len + 1) > MAX_URI_SIZE ) {
		LM_ERR("entity URI too long, maximum=%d\n", MAX_URI_SIZE);
		return NULL;
	}
	memcpy(buf, pres_user->s, pres_user->len);
	buf[pres_user->len] = '@';
	memcpy(buf + pres_user->len + 1, pres_domain->s, pres_domain->len);
	buf[pres_user->len + 1 + pres_domain->len]= '\0';

	doc = xmlNewDoc(BAD_CAST "1.0");
	if(unlikely(doc == NULL))
		goto error;

	root_node = xmlNewNode(NULL, BAD_CAST "conference-info");
	if(unlikely(root_node == NULL))
		goto error;

	xmlDocSetRootElement(doc, root_node);
	namespace = xmlNewNs(root_node, BAD_CAST "urn:ietf:params:xml:ns:conference-info", NULL);
	if (unlikely(namespace == NULL)) {
		LM_ERR("creating namespace failed\n");
		goto error;
	}
	xmlSetNs(root_node, namespace);
	/* The version must be increased for each new document and is a 32bit int.
	   The aux_body_processing function will take care of setting the right attribute
	   depending on the subscription for which the notify is being sent.
	*/
	xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "0");
	xmlNewProp(root_node, BAD_CAST "state", BAD_CAST "full" );
	xmlNewProp(root_node, BAD_CAST "entity", BAD_CAST buf);

	/* loop over all bodies and create the aggregated body */
	for(i=0; i<j; i++)
	{
		p_root= xmlDocGetRootElement(xml_array[i]);
		if(unlikely(p_root == NULL)) {
			LM_ERR("while geting the xml_tree root element\n");
			goto error;
		}
		/* just checking that the root element is "conference-info" as it should RFC4575 */
		if(unlikely(xmlStrcasecmp(p_root->name, BAD_CAST "conference-info") != 0))
		{
			LM_ERR("root element is not \"conference-info\"\n");
			goto error;
		}
		/* the root "conference-info" element should always have children */
		if (p_root->children) {
			for (node = p_root->children; node != NULL; node = node->next) {
					if(xmlAddChild(root_node, xmlCopyNode(node, 1)) == NULL) {
						LM_ERR("while adding child\n");
						goto error;
					}
			}
		}
		/* we only take the most recent subscription as
		   in this phase non partial states will be sent
		*/
		if(i != off_index)
			break;
	}

	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL) {
		ERR_MEM(PKG_MEM_STR);
	}

	xmlDocDumpFormatMemory(doc,(xmlChar**)(void*)&body->s, 
			&body->len, 1);	

	for(i=0; i<j; i++)
	{
		if(xml_array[i]!=NULL)
			xmlFreeDoc(xml_array[i]);
	}
	if (doc)
		xmlFreeDoc(doc);
	if(xml_array!=NULL)
		pkg_free(xml_array);
    
	return body;

error:
	LM_ERR("error in presence_conference agg_nbody\n");
	if(xml_array!=NULL)
	{
		for(i=0; i<j; i++)
		{
			if(xml_array[i]!=NULL)
				xmlFreeDoc(xml_array[i]);
		}
		pkg_free(xml_array);
	}
	if(body)
		pkg_free(body);

	return NULL;
}

str *conf_body_setversion(subs_t *subs, str *body) {
	char version_str[MAX_INT_LEN + 2];//for the null terminating character \0
	snprintf(version_str, MAX_INT_LEN, "%d", subs->version);
	if (!body) {
		return NULL;
	}
	
	xmlDocPtr doc = xmlParseMemory(body->s, body->len);
	if(!doc) {
		goto error;
	}
	xmlNodePtr conf_info = xmlDocGetRootElement(doc);
	if(!conf_info) {
		goto error;
	}
	if(!xmlSetProp(conf_info, BAD_CAST "version", BAD_CAST version_str)) {
		goto error;
	}
	xmlDocDumpFormatMemory(doc,(xmlChar**)(void*)&body->s, 
			&body->len, 1);
	return NULL;
error:
	LM_ERR("error in presence_conference conf_body_setversion\n");
	return NULL;
}

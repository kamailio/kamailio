/*
 * presence_dialoginfo module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 * Copyright (C) 2008 Klaus Darilion, IPCom
 * Copyright (C) 2022 Matteo Brancaleoni, VoiSmart
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
 * \brief Kamailio Presence_reginfo :: Notify BODY handling
 * \ingroup presence_reginfo
 */

#define MAX_INT_LEN 11 /* 2^32: 10 chars + 1 char sign */

#include <string.h>
#include <stdlib.h>
#include <libxml/parser.h>

#include "../../core/mem/mem.h"
#include "../presence/hash.h"
#include "../presence/presence.h"


str *agregate_xmls(str *pres_user, str *pres_domain, str **body_array, int n);

void free_xml_body(char *body)
{
	if(body == NULL)
		return;

	xmlFree(body);
	body = NULL;
}

str *reginfo_agg_nbody(str *pres_user, str *pres_domain, str **body_array,
		int n, int off_index)
{
	str *n_body = NULL;

	LM_DBG("[pres_user]=%.*s [pres_domain]= %.*s, [n]=%d\n", pres_user->len,
			pres_user->s, pres_domain->len, pres_domain->s, n);

	if(body_array == NULL) {
		return NULL;
	}

	n_body = agregate_xmls(pres_user, pres_domain, body_array, n);
	LM_DBG("[n_body]=%p\n", n_body);
	if(n_body) {
		LM_DBG("[*n_body]=%.*s\n", n_body->len, n_body->s);
	}
	if(n_body == NULL && n != 0) {
		LM_ERR("while aggregating body\n");
	}

	xmlCleanupParser();
	xmlMemoryDump();

	return n_body;
}

str *agregate_xmls(str *pres_user, str *pres_domain, str **body_array, int n)
{
	int i, j = 0;

	str *body = NULL;

	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	xmlNsPtr namespace = NULL;
	xmlNodePtr p_root = NULL;
	xmlDocPtr *xml_array;
	xmlNodePtr node = NULL;
	xmlNodePtr next_node = NULL;

	LM_DBG("[pres_user]=%.*s [pres_domain]= %.*s, [n]=%d\n", pres_user->len,
			pres_user->s, pres_domain->len, pres_domain->s, n);

	xml_array = (xmlDocPtr *)pkg_malloc(n * sizeof(xmlDocPtr));
	if(xml_array == NULL) {
		PKG_MEM_ERROR;
		return NULL;
	}
	memset(xml_array, 0, n * sizeof(xmlDocPtr));

	/* parse all the XML documents into xml_array[] */
	for(i = 0; i < n; i++) {
		if(body_array[i] == NULL)
			continue;

		xml_array[j] = NULL;
		xml_array[j] = xmlParseMemory(body_array[i]->s, body_array[i]->len);
		if(xml_array[j] == NULL) {
			LM_ERR("while parsing xml body message\n");
			goto error;
		}
		j++;
	}

	if(j == 0) /* no body */
	{
		LM_DBG("no body to be built\n");
		goto error;
	}

	/* n: number of bodies in total */
	/* j: number of useful bodies; created XML structures */
	/* i: loop counter */

	/* create the new NOTIFY body  */
	doc = xmlNewDoc(BAD_CAST "1.0");
	if(doc == 0) {
		LM_ERR("unable to create xml document\n");
		goto error;
	}

	root_node = xmlNewNode(NULL, BAD_CAST "reginfo");
	if(root_node == 0) {
		goto error;
	}

	xmlDocSetRootElement(doc, root_node);
	namespace = xmlNewNs(
			root_node, BAD_CAST "urn:ietf:params:xml:ns:reginfo", NULL);
	if(!namespace) {
		LM_ERR("creating namespace failed\n");
	}
	xmlSetNs(root_node, namespace);

	/* The version must be increased for each new document and is a 32bit int.
	 * As the version is different for each watcher, we can not set here the
	 * correct value. Thus, we just put here a placeholder which will be
	 * replaced by the correct value in the aux_body_processing callback.
	 * Thus we have CPU intensive XML aggregation only once and can use
	 * quick search&replace in the per-watcher aux_body_processing callback.
	 * We use 11 chracters as an signed int (although RFC says unsigned int we
	 * use signed int as presence module stores "version" in DB as
	 * signed int) has max. 10 characters + 1 character for the sign
	 */
	xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "00000000000");
	xmlNewProp(root_node, BAD_CAST "state", BAD_CAST "full");

	/* loop over all bodies and create the aggregated body */
	for(i = 0; i < j; i++) {
		// get the root reginfo element
		p_root = xmlDocGetRootElement(xml_array[i]);
		if(p_root == NULL) {
			LM_ERR("the xml_tree root element is null\n");
			goto error;
		}

		// get the children registration elements
		if(p_root->children) {
			// loop over registration elements
			for(node = p_root->children; node; node = next_node) {
				next_node = node->next;
				if(node->type != XML_ELEMENT_NODE) {
					continue;
				}
				LM_DBG("node type: Element, name: %s\n", node->name);

				/* we do not copy the node, but unlink it and then add it ot the new node
				 * this destroys the original document but we do not need it anyway.
				 */
				xmlUnlinkNode(node);
				if(xmlAddChild(root_node, node) == NULL) {
					xmlFreeNode(node);
					LM_ERR("while adding child\n");
					goto error;
				}
			} // end of loop over registration elements
		}
	} // end of loop over all bodies

	// convert to string & cleanup
	body = (str *)pkg_malloc(sizeof(str));
	if(body == NULL) {
		ERR_MEM(PKG_MEM_STR);
	}

	xmlDocDumpFormatMemory(doc, (xmlChar **)(void *)&body->s, &body->len, 1);

	for(i = 0; i < j; i++) {
		if(xml_array[i] != NULL)
			xmlFreeDoc(xml_array[i]);
	}
	if(doc) {
		xmlFreeDoc(doc);
	}
	if(xml_array)
		pkg_free(xml_array);

	xmlCleanupParser();
	xmlMemoryDump();

	return body;

	// error handling, cleanup and return NULL
error:
	if(xml_array != NULL) {
		for(i = 0; i < j; i++) {
			if(xml_array[i] != NULL)
				xmlFreeDoc(xml_array[i]);
		}
		pkg_free(xml_array);
	}
	if(body) {
		pkg_free(body);
	}
	if(doc) {
		xmlFreeDoc(doc);
	}

	return NULL;
}

str *reginfo_body_setversion(subs_t *subs, str *body)
{
	char *version_start = 0;
	char version[MAX_INT_LEN + 2]; /* +2 becasue of trailing " and \0 */
	int version_len;
	str *aux_body = NULL;

	if(!body) {
		return NULL;
	}

	/* xmlDocDumpFormatMemory creates \0 terminated string */
	/* version parameters starts at minimum at character 30 */
	if(body->len < 37) {
		LM_ERR("body string too short!\n");
		return NULL;
	}
	version_start = strstr(body->s + 30, "version=");
	if(!version_start) {
		LM_ERR("version string not found!\n");
		return NULL;
	}
	version_start += 9;

	/* safety check for placeholder - if it is body not set by the module,
	 * don't update the version */
	if(strncmp(version_start, "00000000000\"", 12) != 0)
		return NULL;

	version_len = snprintf(version, MAX_INT_LEN + 2, "%d\"", subs->version);
	if(version_len >= MAX_INT_LEN + 2) {
		LM_ERR("failed to convert 'version' to string (truncated output)\n");
		return NULL;
	} else if(version_len < 0) {
		LM_ERR("failed to convert 'version' to string (encoding error)\n");
		return NULL;
	}

	aux_body = (str *)pkg_malloc(sizeof(str));
	if(aux_body == NULL) {
		PKG_MEM_ERROR_FMT("error allocating memory for aux body str\n");
		return NULL;
	}
	memset(aux_body, 0, sizeof(str));
	aux_body->s = (char *)pkg_malloc(body->len * sizeof(char));
	if(aux_body->s == NULL) {
		pkg_free(aux_body);
		PKG_MEM_ERROR_FMT("error allocating memory for aux body buffer\n");
		return NULL;
	}
	memcpy(aux_body->s, body->s, body->len);
	aux_body->len = body->len;

	/* again but on the copied str, no checks needed */
	version_start = strstr(aux_body->s + 30, "version=");
	version_start += 9;
	/* Replace the placeholder 00000000000 with the version.
	 * Put the padding behind the ""
	 */
	LM_DBG("replace version with \"%s\n", version);
	memcpy(version_start, version, version_len);
	memset(version_start + version_len, ' ', 12 - version_len);

	xmlDocPtr doc =
			xmlReadMemory(aux_body->s, aux_body->len, "noname.xml", NULL, 0);
	if(doc == NULL) {
		LM_ERR("error allocation xmldoc\n");
		pkg_free(aux_body->s);
		pkg_free(aux_body);
		return NULL;
	}
	pkg_free(aux_body->s);
	xmlDocDumpFormatMemory(
			doc, (xmlChar **)(void *)&aux_body->s, &aux_body->len, 1);
	xmlFreeDoc(doc);
	xmlCleanupParser();
	xmlMemoryDump();

	return aux_body;
}

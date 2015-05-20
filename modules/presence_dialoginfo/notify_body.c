/*
 * $Id: notify_body.c 1337 2006-12-07 18:05:05Z bogdan_iancu $
 *
 * presence_dialoginfo module -  
 *
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
 *  2008-08-25  initial version (kd)
 */
/*! \file
 * \brief Kamailio Presence_XML :: Notify BODY handling
 * \ingroup presence_xml
 */

#define MAX_INT_LEN 11 /* 2^32: 10 chars + 1 char sign */
#define DEF_TRYING_NODE 1
#define DEF_PROCEEDING_NODE 2
#define DEF_EARLY_NODE 4
#define DEF_CONFIRMED_NODE 8
#define DEF_TERMINATED_NODE 16


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

str* agregate_xmls(str* pres_user, str* pres_domain, str** body_array, int n);
int check_relevant_state (xmlChar * dialog_id, xmlDocPtr * xml_array, int total_nodes);

extern int force_single_dialog;
extern int force_dummy_dialog;

void free_xml_body(char* body)
{
	if(body== NULL)
		return;

	xmlFree(body);
	body= NULL;
}

#define DIALOGINFO_EMPTY_BODY "<dialog-info>\
<dialog id=\"615293b33c62dec073e05d9421e9f48b\" direction=\"recipient\">\
<state>terminated</state>\
</dialog>\
</dialog-info>"

#define DIALOGINFO_EMPTY_BODY_SIZE 512

str* dlginfo_agg_nbody_empty(str* pres_user, str* pres_domain)
{
	str* n_body= NULL;
	str* body_array;
	char* body;

	LM_DBG("creating empty dialog for [pres_user]=%.*s [pres_domain]= %.*s\n",
			pres_user->len, pres_user->s, pres_domain->len, pres_domain->s);

	/* dcm: note to double check - pkg allocation might not be needed */
	body_array = (str*)pkg_malloc(sizeof(str));
	if(body_array==NULL) {
		LM_ERR("No more pkg\n");
		return NULL;
	}
	body = (char*)pkg_malloc(DIALOGINFO_EMPTY_BODY_SIZE);
	if(body==NULL) {
		LM_ERR("No more pkg\n");
		pkg_free(body_array);
		return NULL;
	}

	sprintf(body, DIALOGINFO_EMPTY_BODY);//, pres_user->len, pres_user->s, pres_domain->len, pres_domain->s);
	body_array->s = body;
	body_array->len = strlen(body);


	n_body= agregate_xmls(pres_user, pres_domain, &body_array, 1);
	LM_DBG("[n_body]=%p\n", n_body);
	if(n_body!=NULL) {
		LM_DBG("[*n_body]=%.*s\n",n_body->len, n_body->s);
	} else {
		LM_ERR("issues while aggregating body\n");
	}

	pkg_free(body);
	pkg_free(body_array);


	xmlCleanupParser();
	xmlMemoryDump();

	return n_body;
}

str* dlginfo_agg_nbody(str* pres_user, str* pres_domain, str** body_array, int n, int off_index)
{
	str* n_body= NULL;

	LM_DBG("[pres_user]=%.*s [pres_domain]= %.*s, [n]=%d\n",
			pres_user->len, pres_user->s, pres_domain->len, pres_domain->s, n);

	if(body_array== NULL && (!force_dummy_dialog))
		return NULL;

	if(body_array== NULL)
		return dlginfo_agg_nbody_empty(pres_user, pres_domain);

	n_body= agregate_xmls(pres_user, pres_domain, body_array, n);
	LM_DBG("[n_body]=%p\n", n_body);
	if(n_body) {
		LM_DBG("[*n_body]=%.*s\n",
				n_body->len, n_body->s);
	}
	if(n_body== NULL && n!= 0)
	{
		LM_ERR("while aggregating body\n");
	}

	xmlCleanupParser();
	xmlMemoryDump();

	return n_body;
}	

str* agregate_xmls(str* pres_user, str* pres_domain, str** body_array, int n)
{
	int i, j= 0;

	xmlDocPtr  doc = NULL;
	xmlNodePtr root_node = NULL;
	xmlNsPtr   namespace = NULL;
	/*int winner_priority = -1, priority;*/

	xmlNodePtr p_root= NULL;
	xmlDocPtr* xml_array ;
	xmlNodePtr node = NULL;
	xmlNodePtr terminated_node = NULL;
	xmlNodePtr early_node = NULL;
	xmlNodePtr confirmed_node = NULL;
	xmlNodePtr proceed_node = NULL;
	xmlNodePtr trying_node = NULL;
	xmlNodePtr next_node = NULL;


	char *state = NULL;
	xmlChar *dialog_id = NULL;
	int node_id = -1;

	xmlNodePtr winner_dialog_node = NULL ;
	str *body= NULL;

	char buf[MAX_URI_SIZE+1];

	LM_DBG("[pres_user]=%.*s [pres_domain]= %.*s, [n]=%d\n",
			pres_user->len, pres_user->s, pres_domain->len, pres_domain->s, n);

	xml_array = (xmlDocPtr*)pkg_malloc( n*sizeof(xmlDocPtr));
	if(xml_array== NULL)
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

		xml_array[j] = NULL;
		xml_array[j] = xmlParseMemory( body_array[i]->s, body_array[i]->len );

		/* LM_DBG("parsing XML body: [n]=%d, [i]=%d, [j]=%d xml_array[j]=%p\n", n, i, j, xml_array[j] ); */

		if( xml_array[j]== NULL)
		{
			LM_ERR("while parsing xml body message\n");
			goto error;
		}
		j++;

	} 

	if(j== 0)  /* no body */
	{
		LM_DBG("no body to be built\n");
		goto error;
	}

	/* n: number of bodies in total */
	/* j: number of useful bodies; created XML structures */
	/* i: loop counter */
	/* LM_DBG("number of bodies in total [n]=%d, number of useful bodies [j]=%d\n", n, j ); */

	/* create the new NOTIFY body  */
	if ( (pres_user->len + pres_domain->len + 1 + 4 + 1) >= MAX_URI_SIZE) {
		LM_ERR("entity URI too long, maximum=%d\n", MAX_URI_SIZE);
		goto error;
	}
	memcpy(buf, "sip:", 4);
	memcpy(buf+4, pres_user->s, pres_user->len);
	buf[pres_user->len+4] = '@';
	memcpy(buf + pres_user->len + 5, pres_domain->s, pres_domain->len);
	buf[pres_user->len + 5 + pres_domain->len]= '\0';

	doc = xmlNewDoc(BAD_CAST "1.0");
	if(doc==0) {
		LM_ERR("unable to create xml document\n");
		goto error;
	}

	root_node = xmlNewNode(NULL, BAD_CAST "dialog-info");
	if(root_node==0)
		goto error;

	xmlDocSetRootElement(doc, root_node);
	namespace = xmlNewNs(root_node, BAD_CAST "urn:ietf:params:xml:ns:dialog-info", NULL);
	if (!namespace) {
		LM_ERR("creating namespace failed\n");
	}
	xmlSetNs(root_node, namespace);
	/* The version must be increased for each new document and is a 32bit int.
	   As the version is different for each watcher, we can not set here the
	   correct value. Thus, we just put here a placeholder which will be
	   replaced by the correct value in the aux_body_processing callback.
	   Thus we have CPU intensive XML aggregation only once and can use
	   quick search&replace in the per-watcher aux_body_processing callback.
	   We use 11 chracters as an signed int (although RFC says unsigned int we
	   use signed int as presence module stores "version" in DB as
	   signed int) has max. 10 characters + 1 character for the sign
	   */
	xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "00000000000");
	xmlNewProp(root_node, BAD_CAST  "state",  BAD_CAST "full" );
	xmlNewProp(root_node, BAD_CAST "entity",  BAD_CAST buf);

	/* loop over all bodies and create the aggregated body */
	for(i=0; i<j; i++)
	{
		/* LM_DBG("[n]=%d, [i]=%d, [j]=%d xml_array[i]=%p\n", n, i, j, xml_array[j] ); */
		p_root= xmlDocGetRootElement(xml_array[i]);
		if(p_root ==NULL) {
			LM_ERR("the xml_tree root element is null\n");
			goto error;
		}
		if (p_root->children) {
			for (node = p_root->children; node; node = next_node) {
				next_node = node->next;
				if (node->type == XML_ELEMENT_NODE) {
					LM_DBG("node type: Element, name: %s\n", node->name);
					/* we do not copy the node, but unlink it and then add it ot the new node
					 * this destroys the original document but we do not need it anyway.
					 * using "copy" instead of "unlink" would also copy the namespace which 
					 * would then be declared redundant (libxml unfortunately can not remove 
					 * namespaces)
					 */
					if (!force_single_dialog || (j==1)) {
						xmlUnlinkNode(node);
						if(xmlAddChild(root_node, node)== NULL) {
							xmlFreeNode(node);
							LM_ERR("while adding child\n");
							goto error;
						}
					} else {
						/* try to put only the most important into the XML document
						 * order of importance: terminated->trying->proceeding->confirmed->early
						 * - check first the relevant states and jump priority for them
						 */
						if(strcasecmp((char*)node->name,"dialog") == 0)
						{
							if(dialog_id) xmlFree(dialog_id);
							dialog_id = xmlGetProp(node,(const xmlChar *)"id");
							if(dialog_id) {
								node_id = i;
								LM_DBG("Dialog id for this node : %s\n", dialog_id);
							} else {
								LM_DBG("No dialog id for this node - index: %d\n", i);
							}
						}
						state = xmlNodeGetNodeContentByName(node, "state", NULL);
						if(state) {
							LM_DBG("state element content = %s\n", state);
							if (strcasecmp(state,"terminated") == 0)
							{
								LM_DBG("found terminated state\n" );
								terminated_node = node;
							} else if (strcasecmp(state,"confirmed") == 0 && node_id == i) {
								/*  here we check if confirmed is terminated or not
								 *  if it is not we are in the middle of the conversation
								 */
								if(check_relevant_state(dialog_id, xml_array, j) >= DEF_TERMINATED_NODE)
								{
									LM_DBG("confirmed state for dialog %s, but it is not latest state\n", dialog_id );
								}else{
									LM_DBG("confirmed state for dialog %s and latest state for this dialog\n", dialog_id );
									confirmed_node = node;
								}


							} else if (strcasecmp(state,"early") == 0 && node_id == i) {
								if(check_relevant_state(dialog_id, xml_array, j)  >= DEF_CONFIRMED_NODE)
								{
									LM_DBG("early state for dialog %s, but it is not latest state\n", dialog_id );
								}else{
									LM_DBG("early state for dialog %s and latest state for this dialog\n", dialog_id );
									early_node = node;
								}
							} else if (strcasecmp(state,"proceeding") == 0 && node_id == i) {
								if(check_relevant_state(dialog_id, xml_array, j)  >= DEF_EARLY_NODE)
								{
									LM_DBG("proceeding state for dialog %s, but it is not latest state\n", dialog_id );
								}else{
									LM_DBG("proceeding state for dialog %s and latest state for this dialog\n", dialog_id );
									proceed_node = node;
								}
							} else if (strcasecmp(state,"trying") == 0 && node_id == i) {
								if(check_relevant_state(dialog_id, xml_array, j)  >= DEF_PROCEEDING_NODE)
								{
									LM_DBG("trying state for dialog %s, but it is not latest state\n", dialog_id );
								}else{
									LM_DBG("trying state for dialog %s and latest state for this dialog\n", dialog_id );
									trying_node = node;
								}
							}
							if(early_node != NULL) {
								winner_dialog_node = early_node;
							} else if(confirmed_node != NULL) {
									winner_dialog_node = confirmed_node;
							} else if(proceed_node != NULL) {
									winner_dialog_node = proceed_node;
							} else if(trying_node != NULL) {
									winner_dialog_node = trying_node;
							} else if(terminated_node != NULL) {
									winner_dialog_node = terminated_node;
							} else {
								/* assume a failure somewhere and all above nodes are NULL */
								winner_dialog_node = node;
							}
							/*
							if(winner_dialog_node == NULL) {
								priority = get_dialog_state_priority(state);
								if (priority > winner_priority) {
									winner_priority = priority;
									LM_DBG("new winner priority = %s (%d)\n", state, winner_priority);
									winner_dialog_node = node;
								}
							}
							*/
							xmlFree(state);
						}
					}
				}
			}
		}
	}

	if (force_single_dialog && (j!=1)) {
		if(winner_dialog_node == NULL) {
			LM_ERR("no winning node found\n");
			goto error;
		}
		xmlUnlinkNode(winner_dialog_node);
		if(xmlAddChild(root_node, winner_dialog_node)== NULL) {
			xmlFreeNode(winner_dialog_node);
			LM_ERR("while adding winner-child\n");
			goto error;
		}
	}

	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL) {
		ERR_MEM(PKG_MEM_STR);
	}

	xmlDocDumpFormatMemory(doc,(xmlChar**)(void*)&body->s, 
			&body->len, 1);	

	if(dialog_id!=NULL) xmlFree(dialog_id);
	for(i=0; i<j; i++) {
		if(xml_array[i]!=NULL)
			xmlFreeDoc( xml_array[i]);
	}
	if(doc) xmlFreeDoc(doc);
	if(xml_array) pkg_free(xml_array);

	xmlCleanupParser();
	xmlMemoryDump();

	return body;

error:
	if(xml_array!=NULL)
	{
		for(i=0; i<j; i++)
		{
			if(xml_array[i]!=NULL)
				xmlFreeDoc( xml_array[i]);
		}
		pkg_free(xml_array);
	}
	if(body) pkg_free(body);
	if(dialog_id) xmlFree(dialog_id);
	if(doc) xmlFreeDoc(doc);

	return NULL;
}

/*
int get_dialog_state_priority(char *state) {
	if (strcasecmp(state,"terminated") == 0)
		return 0;
	if (strcasecmp(state,"trying") == 0)
		return 1;
	if (strcasecmp(state,"proceeding") == 0)
		return 2;
	if (strcasecmp(state,"confirmed") == 0)
		return 3;
	if (strcasecmp(state,"early") == 0)
		return 4;

	return 0;
}
*/

/* returns 16 -> terminated, 8 -> confirmed, 4 -> early */
int check_relevant_state (xmlChar * dialog_id, xmlDocPtr * xml_array, int total_nodes)
{
	int result = 0;
	int i = 0;
	int node_id = -1;
	char *state;
	xmlChar *dialog_id_tmp = NULL;
	xmlNodePtr p_root;
	xmlNodePtr node = NULL;
	for (i = 0; i < total_nodes; i++)
	{
		p_root = xmlDocGetRootElement (xml_array[i]);
		if (p_root == NULL)
		{
			LM_DBG ("the xml_tree root element is null\n");
		} else {
			if (p_root->children)
				for (node = p_root->children; node; node = node->next)
				{
					if (node->type == XML_ELEMENT_NODE)
					{
						if (strcasecmp ((char*)node->name, "dialog") == 0)
						{
							/* Getting the node id so we would be sure
							 * that terminate state from same one the same */
							if(dialog_id_tmp) xmlFree(dialog_id_tmp);
							dialog_id_tmp = xmlGetProp (node, (const xmlChar *) "id");
							if(dialog_id_tmp) node_id = i;
						}
						state = xmlNodeGetNodeContentByName (node, "state", NULL);
						if (state)
						{
							/* check if state is terminated for this dialog. */
							if ((strcasecmp (state, "terminated") == 0)
									&& (node_id == i) && (node_id >= 0)
									&& (strcasecmp ((char*)dialog_id_tmp, (char*)dialog_id) == 0))
							{
								LM_DBG ("Found terminated in dialog %s\n",
										dialog_id);
								result += DEF_TERMINATED_NODE;
							}
							/* check if state is confirmed for this dialog. */
							if ((strcasecmp (state, "confirmed") == 0)
									&& (node_id == i) && (node_id >= 0)
									&& (strcasecmp ((char*)dialog_id_tmp, (char*)dialog_id) == 0))
							{
								LM_DBG ("Found confirmed in dialog %s\n", dialog_id);
								result += DEF_CONFIRMED_NODE;
							}
							if ((strcasecmp (state, "early") == 0)
									&& (node_id == i) && (node_id >= 0)
									&& (strcasecmp ((char*)dialog_id_tmp, (char*)dialog_id) == 0))
							{
								LM_DBG ("Found early in dialog %s\n", dialog_id);
								result += DEF_EARLY_NODE;
							}
							if ((strcasecmp (state, "proceeding") == 0)
									&& (node_id == i) && (node_id >= 0)
									&& (strcasecmp ((char*)dialog_id_tmp, (char*)dialog_id) == 0))
							{
								LM_DBG ("Found proceeding in dialog %s\n", dialog_id);
								result += DEF_PROCEEDING_NODE;
							}
							if ((strcasecmp (state, "trying") == 0)
									&& (node_id == i) && (node_id >= 0)
									&& (strcasecmp ((char*)dialog_id_tmp, (char*)dialog_id) == 0))
							{
								LM_DBG ("Found trying in dialog %s\n", dialog_id);
								result += DEF_TRYING_NODE;
							}

							xmlFree (state);
						}
					}
				}
		}
	}
	if(dialog_id_tmp) xmlFree(dialog_id_tmp);
	LM_DBG ("result cheching dialog %s is %d\n", dialog_id, result);
	return result;
}


str *dlginfo_body_setversion(subs_t *subs, str *body) {
	char *version_start=0;
	char version[MAX_INT_LEN + 2]; /* +2 becasue of trailing " and \0 */
	int version_len;

	if (!body) {
		return NULL;
	}

	/* xmlDocDumpFormatMemory creates \0 terminated string */
	/* version parameters starts at minimum at character 34 */
	if (body->len < 41) {
		LM_ERR("body string too short!\n");
		return NULL;
	}
	version_start = strstr(body->s + 34, "version=");
	if (!version_start) {
		LM_ERR("version string not found!\n");
		return NULL;
	}
	version_start += 9;

	/* safety check for placeholder - if it is body not set by the module,
	 * don't update the version */
	if(strncmp(version_start, "00000000000\"", 12)!=0)
		return NULL;

	version_len = snprintf(version, MAX_INT_LEN + 2,"%d\"", subs->version);
	if (version_len >= MAX_INT_LEN + 2) {
		LM_ERR("failed to convert 'version' to string\n");
		memcpy(version_start, "00000000000\"", 12);
		return NULL;
	}
	/* Replace the placeholder 00000000000 with the version.
	 * Put the padding behind the ""
	 */
	LM_DBG("replace version with \"%s\n",version);
	memcpy(version_start, version, version_len);
	memset(version_start + version_len, ' ', 12 - version_len);

	return NULL;
}

/*
 * $Id: xcap_auth.c 1337 2006-12-07 18:05:05Z bogdan_iancu $
 *
 * presence_xml module - 
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
 *  2007-04-11  initial version (anca)
 */

/*! \file
 * \brief Kamailio Presence_XML :: XCAP authentication
 * \ingroup presence_xml
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libxml/parser.h>

#include "../../str.h"
#include "../../dprint.h"
#include "../../parser/parse_uri.h"
#include "../presence/utils_func.h"
#include "../presence/hash.h"
#include "presence_xml.h"
#include "xcap_auth.h"
#include "pidf.h"

extern str xcapauth_userdel_reason;

int http_get_rules_doc(str user, str domain, str* rules_doc);

int pres_watcher_allowed(subs_t* subs)
{
	xmlDocPtr xcap_tree= NULL;
	xmlNodePtr node= NULL,  actions_node = NULL;
	xmlNodePtr sub_handling_node = NULL;
	char* sub_handling = NULL;
	int ret = 0;

	/* if force_active set status to active*/
	if(force_active)
	{
		subs->status= ACTIVE_STATUS;
		subs->reason.s= NULL;
		subs->reason.len= 0;
		return 0;
	}

	if(subs->auth_rules_doc== NULL)
	{
		subs->status= PENDING_STATUS;
		subs->reason.s= NULL;
		subs->reason.len= 0;
		return 0;
	}

	xcap_tree= xmlParseMemory(subs->auth_rules_doc->s,
			subs->auth_rules_doc->len);
	if(xcap_tree== NULL)
	{
		LM_ERR("parsing xml memory\n");
		return -1;
	}

	node= get_rule_node(subs, xcap_tree);
	if(node== NULL)
	{
		/* if no rule node was found and the previous state was active -> set the
		 * state to terminated with reason xcapauth_userdel_reason (default "probation") */
		if(subs->status != PENDING_STATUS)
		{
			subs->status= TERMINATED_STATUS;
			subs->reason= xcapauth_userdel_reason;
		}
		goto done;
	}

	subs->status= PENDING_STATUS;
	subs->reason.s= NULL;
	subs->reason.len= 0;

	/* process actions */
	actions_node = xmlNodeGetChildByName(node, "actions");
	if(actions_node == NULL)
	{
		LM_DBG("actions_node NULL\n");
		goto done;
	}
	LM_DBG("actions_node->name= %s\n",
			actions_node->name);
			
	sub_handling_node = xmlNodeGetChildByName(actions_node, "sub-handling");
	if(sub_handling_node== NULL)
	{
		LM_DBG("sub_handling_node NULL\n");
		goto done;
	}
	sub_handling = (char*)xmlNodeGetContent(sub_handling_node);
		LM_DBG("sub_handling_node->name= %s\n",
			sub_handling_node->name);
	LM_DBG("sub_handling_node->content= %s\n",
			sub_handling);
	
	if(sub_handling== NULL)
	{
		LM_ERR("Couldn't get sub-handling content\n");
		ret = -1;
		goto done;
	}
	if( strncmp((char*)sub_handling, "block",5 )==0)
	{	
		subs->status = TERMINATED_STATUS;;
		subs->reason.s= "rejected";
		subs->reason.len = 8;
	}
	else	
	if( strncmp((char*)sub_handling, "confirm",7 )==0)
	{	
		subs->status = PENDING_STATUS;
	}
	else
	if( strncmp((char*)sub_handling , "polite-block",12 )==0)
	{	
		subs->status = ACTIVE_STATUS;
		subs->reason.s= "polite-block";
		subs->reason.len = 12;
	}
	else
	if( strncmp((char*)sub_handling , "allow",5 )==0)
	{
		subs->status = ACTIVE_STATUS;
	}
	else
	{
		LM_ERR("unknown subscription handling action\n");
		ret = -1;
	}

done:
	if(sub_handling)
		xmlFree(sub_handling);
	xmlFreeDoc(xcap_tree);
	return ret;
}

xmlNodePtr get_rule_node(subs_t* subs, xmlDocPtr xcap_tree )
{
	str w_uri= {0, 0};
	char* id = NULL, *domain = NULL, *time_cont= NULL;
	int apply_rule = -1;
	xmlNodePtr ruleset_node = NULL, node1= NULL, node2= NULL;
	xmlNodePtr cond_node = NULL, except_node = NULL;
	xmlNodePtr identity_node = NULL, sphere_node = NULL;
	xmlNodePtr iden_child;
	xmlNodePtr validity_node, time_node;
	time_t t_init, t_fin, t;
	int valid= 0;


	uandd_to_uri(subs->watcher_user, subs->watcher_domain, &w_uri);
	if(w_uri.s == NULL)
	{
		LM_ERR("while creating uri\n");
		return NULL;
	}
	ruleset_node = xmlDocGetNodeByName(xcap_tree, "ruleset", NULL);
	if(ruleset_node == NULL)
	{
		LM_DBG("ruleset_node NULL\n");
		goto error;

	}	
	for(node1 = ruleset_node->children ; node1; node1 = node1->next)
	{
		if(xmlStrcasecmp(node1->name, (unsigned char*)"text")==0 )
				continue;

		/* process conditions */
		LM_DBG("node1->name= %s\n", node1->name);

		cond_node = xmlNodeGetChildByName(node1, "conditions");
		if(cond_node == NULL)
		{	
			LM_DBG("cond node NULL\n");
			goto error;
		}
		LM_DBG("cond_node->name= %s\n", cond_node->name);

		validity_node = xmlNodeGetChildByName(cond_node, "validity");
		if(validity_node !=NULL)
		{
			LM_DBG("found validity tag\n");
		
			t= time(NULL);
		
			/* search all from-until pair */
			for(time_node= validity_node->children; time_node;
					time_node= time_node->next)
			{
				if(xmlStrcasecmp(time_node->name, (unsigned char*)"from")!= 0)
				{
					continue;
				}
				time_cont= (char*)xmlNodeGetContent(time_node);
				t_init= xml_parse_dateTime(time_cont);
				xmlFree(time_cont);
				if(t_init< 0)
				{
					LM_ERR("failed to parse xml dateTime\n");
					goto error;
				}

				if(t< t_init)
				{
					LM_DBG("the lower time limit is not respected\n");
					continue;
				}
				
				time_node= time_node->next;
				while(1)
				{
					if(time_node== NULL)
					{	
						LM_ERR("bad formatted xml doc:until child not found in"
								" validity pair\n");
						goto error;
					}
					if( xmlStrcasecmp(time_node->name, 
								(unsigned char*)"until")== 0)
						break;
					time_node= time_node->next;
				}
				
				time_cont= (char*)xmlNodeGetContent(time_node);
				t_fin= xml_parse_dateTime(time_cont);
				xmlFree(time_cont);

				if(t_fin< 0)
				{
					LM_ERR("failed to parse xml dateTime\n");
					goto error;
				}
			
				if(t <= t_fin)
				{
					LM_DBG("the rule is active at this time\n");
					valid= 1;
				}
			
			}
		
			if(!valid)
			{
				LM_DBG("the rule is not active at this time\n");
				continue;
			}

		}	
	
		sphere_node = xmlNodeGetChildByName(cond_node, "sphere");
		if(sphere_node!= NULL)
		{
			/* check to see if matches presentity current sphere */
			/* ask presence for sphere information */
			
			char* sphere= pres_get_sphere(&subs->pres_uri);
			if(sphere)
			{
				char* attr= (char*)xmlNodeGetContent(sphere_node);
				if(xmlStrcasecmp((unsigned char*)attr, (unsigned char*)sphere)!= 0)
				{
					LM_DBG("sphere condition not respected\n");
					pkg_free(sphere);
					xmlFree(attr);
					continue;
				}
				pkg_free(sphere);
				xmlFree(attr);
	
			}
				
			/* if the user has not define a sphere -> 
			 *						consider the condition true*/
		}

		identity_node = xmlNodeGetChildByName(cond_node, "identity");
		if(identity_node == NULL)
		{
			LM_WARN("didn't find identity tag\n");
			continue;
		}	
		
		iden_child= xmlNodeGetChildByName(identity_node, "one");
		if(iden_child)	
		{
			for(node2 = identity_node->children; node2; node2 = node2->next)
			{
				if(xmlStrcasecmp(node2->name, (unsigned char*)"one")!= 0)
					continue;
				
				id = xmlNodeGetAttrContentByName(node2, "id");	
				if(id== NULL)
				{
					LM_ERR("while extracting attribute\n");
					goto error;
				}
				if((strlen(id)== w_uri.len && 
							(strncmp(id, w_uri.s, w_uri.len)==0)))	
				{
					apply_rule = 1;
					xmlFree(id);
					break;
				}
				xmlFree(id);
			}
		}	

		/* search for many node*/
		iden_child= xmlNodeGetChildByName(identity_node, "many");
		if(iden_child)	
		{
			domain = NULL;
			for(node2 = identity_node->children; node2; node2 = node2->next)
			{
				if(xmlStrcasecmp(node2->name, (unsigned char*)"many")!= 0)
					continue;
	
				domain = xmlNodeGetAttrContentByName(node2, "domain");
				if(domain == NULL)
				{	
					LM_DBG("No domain attribute to many\n");
				}
				else	
				{
					LM_DBG("<many domain= %s>\n", domain);
					if((strlen(domain)!= subs->from_domain.len && 
								strncmp(domain, subs->from_domain.s,
									subs->from_domain.len) ))
					{
						xmlFree(domain);
						continue;
					}	
				}
				xmlFree(domain);
				apply_rule = 1;
				if(node2->children == NULL)       /* there is no exception */
					break;

				for(except_node = node2->children; except_node;
						except_node= except_node->next)
				{
					if(xmlStrcasecmp(except_node->name, (unsigned char*)"except"))
						continue;

					id = xmlNodeGetAttrContentByName(except_node, "id");	
					if(id!=NULL)
					{
						if((strlen(id)- 1== w_uri.len && 
								(strncmp(id, w_uri.s, w_uri.len)==0)))	
						{
							xmlFree(id);
							apply_rule = 0;
							break;
						}
						xmlFree(id);
					}	
					else
					{
						domain = NULL;
						domain = xmlNodeGetAttrContentByName(except_node, "domain");
						if(domain!=NULL)
						{
							LM_DBG("Found except domain= %s\n- strlen(domain)= %d\n",
									domain, (int)strlen(domain));
							if(strlen(domain)==subs->from_domain.len &&
								(strncmp(domain,subs->from_domain.s , subs->from_domain.len)==0))	
							{
								LM_DBG("except domain match\n");
								xmlFree(domain);
								apply_rule = 0;
								break;
							}
							xmlFree(domain);
						}	

					}	
				}
				if(apply_rule== 1)  /* if a match was found no need to keep searching*/
					break;

			}		
		}
		if(apply_rule ==1)
			break;
	}

	LM_DBG("apply_rule= %d\n", apply_rule);
	if(w_uri.s!=NULL)
		pkg_free(w_uri.s);

	if( !apply_rule || !node1)
		return NULL;

	return node1;

error:
	if(w_uri.s)
		pkg_free(w_uri.s);
	return NULL;
}	

int pres_get_rules_doc(str* user, str* domain, str** rules_doc)
{
	
	return get_rules_doc(user, domain, NULL, PRES_RULES, rules_doc);
}

int pres_get_pidf_doc(str *user, str *domain, str *file_uri, str **rules_doc)
{
	return get_rules_doc(user, domain, file_uri, PIDF_MANIPULATION, rules_doc);
}

int get_rules_doc(str* user, str* domain, str *file_uri, int type, str** rules_doc)
{
	db_key_t query_cols[3];
	db_val_t query_vals[3];
	db_key_t result_cols[1];
	int n_query_cols = 0;
	db1_res_t *result = 0;
	db_row_t *row;
	db_val_t *row_vals;
	str body;
	str* doc= NULL;
	int n_result_cols= 0, xcap_doc_col;
	static str tmp1 = str_init("doc_type");
	static str tmp2 = str_init("doc_uri");
	static str tmp3 = str_init("username");
	static str tmp4 = str_init("domain");
	static str tmp5 = str_init("doc");

	if(force_active)
	{
		*rules_doc= NULL;
		return 0;
	}
	LM_DBG("[user]= %.*s\t[domain]= %.*s", 
			user->len, user->s,	domain->len, domain->s);

	/* first search in database */
	query_cols[n_query_cols] = &tmp1;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= type;
	n_query_cols++;

	if (file_uri != NULL)
	{
		query_cols[n_query_cols] = &tmp2;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = *file_uri;
		n_query_cols++;
	}
	else if (user != NULL && domain != NULL)
	{
		query_cols[n_query_cols] = &tmp3;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = *user;
		n_query_cols++;
	
		query_cols[n_query_cols] = &tmp4;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = *domain;
		n_query_cols++;
	}
	else
	{
		LM_ERR("Need to specify file uri _OR_ username and domain\n");
		return -1;
	}
	
	result_cols[xcap_doc_col= n_result_cols++] = &tmp5;
	
	if (pxml_dbf.use_table(pxml_db, &xcap_table) < 0) 
	{
		LM_ERR("in use_table-[table]= %.*s\n", xcap_table.len, xcap_table.s);
		return -1;
	}

	if( pxml_dbf.query(pxml_db, query_cols, 0 , query_vals, result_cols, 
				n_query_cols, 1, 0, &result)<0)
	{
		LM_ERR("while querying table xcap for [user]=%.*s\t[domain]= %.*s\n",
				user->len, user->s,	domain->len, domain->s);
		if(result)
			pxml_dbf.free_result(pxml_db, result);
		return -1;
	}
	if(result== NULL)
		return -1;

	if(result->n<= 0)
	{
		LM_DBG("No document found in db table for [user]=%.*s"
			"\t[domain]= %.*s\t[doc_type]= %d\n",user->len, user->s,
			domain->len, domain->s, type);
		
		if (!integrated_xcap_server && type != PRES_RULES)
		{
			LM_WARN("Cannot retrieve non pres-rules documents from"
				"external XCAP server\n");
		}
		else if(!integrated_xcap_server)
		{
			if(http_get_rules_doc(*user, *domain, &body)< 0)
			{
				LM_ERR("sending http GET request to xcap server\n");		
				goto error;
			}
			if(body.s && body.len)
				goto done; 
		}
		pxml_dbf.free_result(pxml_db, result);
		return 0;
	}	
	
	row = &result->rows[xcap_doc_col];
	row_vals = ROW_VALUES(row);

	body.s = (char*)row_vals[0].val.string_val;
	if(body.s== NULL)
	{
		LM_ERR("Xcap doc NULL\n");
		goto error;
	}	
	body.len = strlen(body.s);
	if(body.len== 0)
	{
		LM_ERR("Xcap doc empty\n");
		goto error;
	}			
	LM_DBG("xcap document:\n%.*s", body.len,body.s);

done:
	doc= (str*)pkg_malloc(sizeof(str));
	if(doc== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	doc->s= (char*)pkg_malloc(body.len* sizeof(char));
	if(doc->s== NULL)
	{
		pkg_free(doc);
		ERR_MEM(PKG_MEM_STR);
	}
	memcpy(doc->s, body.s, body.len);
	doc->len= body.len;

	*rules_doc= doc;

	if(result)
		pxml_dbf.free_result(pxml_db, result);

	return 1;

error:
	if(result)
		pxml_dbf.free_result(pxml_db, result);

	return -1;

}

int http_get_rules_doc(str user, str domain, str* rules_doc)
{
	str uri;
	xcap_doc_sel_t doc_sel;
	char* doc= NULL;
	xcap_serv_t* xs;
	xcap_get_req_t req;

	memset(&req, 0, sizeof(xcap_get_req_t));
	if(uandd_to_uri(user, domain, &uri)< 0)
	{
		LM_ERR("constructing uri\n");
		goto error;
	}

	doc_sel.auid.s= "pres-rules";
	doc_sel.auid.len= strlen("pres-rules");
	doc_sel.doc_type= PRES_RULES;
	doc_sel.type= USERS_TYPE;
	doc_sel.xid= uri;
	doc_sel.filename.s= "index";
	doc_sel.filename.len= 5;

	/* need the whole document so the node selector is NULL */
	/* don't know which is the authoritative server for the user
	 * so send request to all in the list */
	req.doc_sel= doc_sel;

	xs= xs_list;
	while(xs)
	{
		req.xcap_root= xs->addr;
		req.port= xs->port;
		doc= xcap_GetNewDoc(req, user, domain);
		if(doc!=NULL)
			break;
		xs = xs->next;
	}

	rules_doc->s= doc;
	rules_doc->len= doc?strlen(doc):0;
	
	return 0;

error:
	return -1;
}

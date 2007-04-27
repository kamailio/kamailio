/*
 * $Id: xcap_auth.c 1337 2006-12-07 18:05:05Z bogdan_iancu $
 *
 * presence_xml module - 
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2007-04-11  initial version (anca)
 */

#include "../../str.h"
#include "../../dprint.h"
#include "../presence/utils_func.h"
#include "presence_xml.h"
#include "xcap_auth.h"
#include "pidf.h"

extern char* xcap_table;
extern int force_active;
extern db_func_t pxml_dbf;
extern db_con_t *pxml_db;

int is_watcher_allowed( subs_t* subs, xmlDocPtr xcap_tree );

int pres_watcher_allowed(subs_t* subs)
{
	xmlDocPtr doc= NULL;
	int ret_code= 0;
	
	/* if force_active set status to active*/
	if(force_active)
	{
		subs->status.s= "active";
		subs->status.len= 6;
		return 1;
	}
	/* else search in xcap_table */

	doc= get_xcap_tree(subs->to_user, subs->to_domain);
	if(doc== NULL)
	{
		DBG("PRESENCE_XML:pres_watcher_allowed: No xcap document found\n");
		return 0;
	}
	ret_code= is_watcher_allowed(subs, doc);
	if(ret_code==0 )
	{
		DBG("PRESENCE_XML:pres_watcher_allowed: The subscriber didn't match"
					" the conditions\n");
	}

	xmlFreeDoc(doc);
	return ret_code;			// status might possibly been changed

}	

xmlNodePtr get_rule_node(subs_t* subs, xmlDocPtr xcap_tree )
{
	str w_uri= {0, 0};
	char* id = NULL, *domain = NULL;
	int apply_rule = -1;
	xmlNodePtr ruleset_node = NULL, node1= NULL, node2= NULL;
	xmlNodePtr cond_node = NULL, except_node = NULL;
	xmlNodePtr identity_node = NULL, validity_node =NULL, sphere_node = NULL;

	uandd_to_uri(subs->from_user, subs->from_domain, &w_uri);
	if(w_uri.s == NULL)
	{
		LOG(L_ERR, "PRESENCE_XML: is_watcher_allowed:Error while creating uri\n");
		return NULL;
	}
	ruleset_node = xmlDocGetNodeByName(xcap_tree, "ruleset", NULL);
	if(ruleset_node == NULL)
	{
		DBG( "PRESENCE_XML:is_watcher_allowed: ruleset_node NULL\n");
		goto error;

	}	
	for(node1 = ruleset_node->children ; node1; node1 = node1->next)
	{
		if(xmlStrcasecmp(node1->name, (unsigned char*)"text")==0 )
				continue;

		/* process conditions */
		DBG("PRESENCE_XML:is_watcher_allowed:node1->name= %s\n",node1->name);

		cond_node = xmlNodeGetChildByName(node1, "conditions");
		if(cond_node == NULL)
		{	
			DBG( "PRESENCE_XML:is_watcher_allowed:cond node NULL\n");
			goto error;
		}
		DBG("PRESENCE_XML:is_watcher_allowed:cond_node->name= %s\n",
				cond_node->name);

		validity_node = xmlNodeGetChildByName(cond_node, "validity");
		if(validity_node !=NULL)
		{
			DBG("PRESENCE_XML:is_watcher_allowed:found validity tag\n");

		}	
		sphere_node = xmlNodeGetChildByName(cond_node, "sphere");

		identity_node = xmlNodeGetChildByName(cond_node, "identity");
		if(identity_node == NULL)
		{
			LOG(L_ERR, "PRESENCE_XML:is_watcher_allowed:ERROR didn't found"
					" identity tag\n");
			goto error;
		}	
		id = NULL;
		
		if(strcmp ((const char*)identity_node->children->name, "one") == 0)	
			for(node2 = identity_node->children; node2; node2 = node2->next)
			{
				if(xmlStrcasecmp(node2->name, (unsigned char*)"text")== 0)
					continue;

				id = xmlNodeGetAttrContentByName(node2, "id");	
				if((strlen(id)== w_uri.len && 
							(strncmp(id, w_uri.s, w_uri.len)==0)))	
				{
					apply_rule = 1;
					break;
				}
			}	
		else
		{	
			domain = NULL;
			for(node2 = identity_node->children; node2; node2 = node2->next)
			{
				if(xmlStrcasecmp(node2->name, (unsigned char*)"text")== 0)
					continue;
	
				domain = xmlNodeGetAttrContentByName(node2, "domain");
			
				if(domain == NULL)
				{	
					apply_rule = 1;
					break;
				}
				else	
					if((strlen(domain)!= subs->from_domain.len && 
								strncmp(domain, subs->from_domain.s,
									subs->from_domain.len) ))
						continue;

				apply_rule = 1;
				if(node2->children == NULL)       /* there is no exception */
					break;

				for(except_node = node2->children; except_node;
						except_node= except_node->next)
				{
					if(xmlStrcasecmp(except_node->name, 
								(unsigned char*)"text")== 0)
						continue;

					id = xmlNodeGetAttrContentByName(except_node, "id");	
					if(id!=NULL)
					{
						if((strlen(id)== w_uri.len && (strncmp(id, w_uri.s,
											w_uri.len)==0)))	
						{
							apply_rule = 0;
							break;
						}
					}	
					else
					{
						domain = NULL;
						domain = xmlNodeGetAttrContentByName(except_node,
								"domain");
						if((domain!=NULL && strlen(domain)== 
									subs->from_domain.len &&
						(strncmp(domain,subs->from_domain.s , 
								 subs->from_domain.len)==0)))	
						{
							apply_rule = 0;
							break;
						}
					}	
					if (apply_rule == 0)
						break;
				}
				if(apply_rule ==1 || apply_rule==0)
					break;

			}		
		}
		if(apply_rule ==1 || apply_rule==0)
					break;
	}


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

int is_watcher_allowed( subs_t* subs, xmlDocPtr xcap_tree )
{
	xmlNodePtr node= NULL,  actions_node = NULL;
	xmlNodePtr sub_handling_node = NULL;
	char* sub_handling = NULL;

	node= get_rule_node(subs, xcap_tree);
	if(node== NULL)
		return 0;

	/* process actions */	
	actions_node = xmlNodeGetChildByName(node, "actions");
	if(actions_node == NULL)
	{	
		DBG( "PRESENCE_XML:is_watcher_allowed: actions_node NULL\n");
		return 0;
	}
	DBG("PRESENCE_XML:is_watcher_allowed:actions_node->name= %s\n",
			actions_node->name);
			
	sub_handling_node = xmlNodeGetChildByName(actions_node, "sub-handling");
	if(sub_handling_node== NULL)
	{	
		DBG( "PRESENCE_XML:is_watcher_allowed:sub_handling_node NULL\n");
		return 0;
	}
	sub_handling = (char*)xmlNodeGetContent(sub_handling_node);
		DBG("PRESENCE_XML:is_watcher_allowed:sub_handling_node->name= %s\n",
			sub_handling_node->name);
	DBG("PRESENCE_XML:is_watcher_allowed:sub_handling_node->content= %s\n",
			sub_handling);
	
	if(sub_handling== NULL)
	{
		LOG(L_ERR, "PRESENCE_XML:is_watcher_allowed:ERROR Couldn't get"
				" sub-handling content\n");
		return -1;
	}
	if( strncmp((char*)sub_handling, "block",5 )==0)
	{	
		subs->status.s = "terminated";
		subs->status.len = 10;
		subs->reason.s= "rejected";
		subs->reason.len = 8;
	}
	
	if( strncmp((char*)sub_handling, "confirm",7 )==0)
	{	
		subs->status.s = "pending";
		subs->status.len = 7;
	}
	
	if( strncmp((char*)sub_handling , "polite-block",12 )==0)
	{	
		subs->status.s = "active";
		subs->status.len = 6;
		subs->reason.s= "polite-block";
		subs->reason.len = 12;
	}
		
	if( strncmp((char*)sub_handling , "allow",5 )==0)
	{	
		subs->status.s = "active";
		subs->status.len = 6;
		subs->reason.s = NULL;
	}

	if(node)
		return 1;
	else
		return 0;
}


xmlDocPtr get_xcap_tree(str user, str domain)
{
	db_key_t query_cols[5];
	db_val_t query_vals[5];
	db_key_t result_cols[3];
	int n_query_cols = 0;
	db_res_t *result = 0;
	db_row_t *row ;	
	db_val_t *row_vals ;
	str body ;
	xmlDocPtr xcap_tree =NULL;

	query_cols[n_query_cols] = "username";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = user.s;
	query_vals[n_query_cols].val.str_val.len = user.len;
	n_query_cols++;
	
	query_cols[n_query_cols] = "domain";
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = domain.s;
	query_vals[n_query_cols].val.str_val.len = domain.len;
	n_query_cols++;
	
	query_cols[n_query_cols] = "doc_type";
	query_vals[n_query_cols].type = DB_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= PRES_RULES;
	n_query_cols++;

	result_cols[0] = "xcap";

	if (pxml_dbf.use_table(pxml_db, xcap_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE_XML:get_xcap_tree: Error in use_table\n");
		return NULL;
	}

	if( pxml_dbf.query(pxml_db, query_cols, 0 , query_vals, result_cols, 
				n_query_cols, 1, 0, &result)<0)
	{
		LOG(L_ERR, "PRESENCE_XML:get_xcap_tree:Error while querying table xcap for"
		" [username]=%.*s , domain=%.*s\n",user.len, user.s, domain.len,
		domain.s);
		goto error;
	}
	if(result== NULL)
		return NULL;

	if(result && result->n<=0)
	{
		DBG("PRESENCE_XML:get_xcap_tree:The query in table xcap for"
				" [username]=%.*s , domain=%.*s returned no result\n",
				user.len, user.s, domain.len, domain.s);
		goto error;
	}
	DBG("PRESENCE_XML:get_xcap_tree:The query in table xcap for"
			" [username]=%.*s , domain=%.*s returned result",user.len,
			user.s, domain.len, domain.s );

	row = &result->rows[0];
	row_vals = ROW_VALUES(row);

	body.s = row_vals[0].val.str_val.s;
	if(body.s== NULL)
	{
		DBG("PRESENCE_XML:get_xcap_tree: Xcap doc NULL\n");
		goto error;
	}	
	body.len = strlen(body.s);
	if(body.len== 0)
	{
		DBG("PRESENCE_XML:get_xcap_tree: Xcap doc empty\n");
		goto error;
	}			
	
	DBG("PRESENCE_XML:get_xcap_tree: xcap body:\n%.*s", body.len,body.s);
	
	xcap_tree = xmlParseMemory(body.s, body.len);
	if(xcap_tree == NULL)
	{
		LOG(L_ERR,"PRESENCE_XML:get_xcap_tree: ERROR while parsing memory\n");
		goto error;
	}

	if(result!=NULL)
		pxml_dbf.free_result(pxml_db, result);

	return xcap_tree;

error:
	if(result!=NULL)
		pxml_dbf.free_result(pxml_db, result);
	return NULL;
}




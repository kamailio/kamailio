/*
 * $Id$
 *
 * presence module - presence server implementation
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
 *  2006-08-15  initial version (anca)
 */

#include <string.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>

#include "../../parser/parse_content.h"
#include "../../data_lump.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "pidf.h"




/*
 * apply procedure f to each xmlNodePtr in doc matched by xpath
 */

xmlAttrPtr xmlNodeGetAttrByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = node->properties;
	while (attr) {
		if (xmlStrcasecmp(attr->name, (unsigned char*)name) == 0)
			return attr;
		attr = attr->next;
	}
	return NULL;
}

char *xmlNodeGetAttrContentByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = xmlNodeGetAttrByName(node, name);
	if (attr)
		return (char*)xmlNodeGetContent(attr->children);
	else
		return NULL;
}

xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name)
{
	xmlNodePtr cur = node->children;
	while (cur) {
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

xmlNodePtr xmlNodeGetNodeByName(xmlNodePtr node, const char *name,
															const char *ns)
{
	xmlNodePtr cur = node;
	while (cur) {
		xmlNodePtr match = NULL;
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0) {
			if (!ns || (cur->ns && xmlStrcasecmp(cur->ns->prefix,
							(unsigned char*)ns) == 0))
				return cur;
		}
		match = xmlNodeGetNodeByName(cur->children, name, ns);
		if (match)
			return match;
		cur = cur->next;
	}
	return NULL;
}

char *xmlNodeGetNodeContentByName(xmlNodePtr root, const char *name,
		const char *ns)
{
	xmlNodePtr node = xmlNodeGetNodeByName(root, name, ns);
	if (node)
		return (char*)xmlNodeGetContent(node->children);
	else
		return NULL;
}

xmlNodePtr xmlDocGetNodeByName(xmlDocPtr doc, const char *name, const char *ns)
{
	xmlNodePtr cur = doc->children;
	return xmlNodeGetNodeByName(cur, name, ns);
}

char *xmlDocGetNodeContentByName(xmlDocPtr doc, const char *name, 
		const char *ns)
{
	xmlNodePtr node = xmlDocGetNodeByName(doc, name, ns);
	if (node)
		return (char*)xmlNodeGetContent(node->children);
	else
		return NULL;
}


void xmlNodeMapByName(xmlNodePtr node, const char *name, const char *ns, 
		      void (f)(xmlNodePtr, void*), void *data)
{
	xmlNodePtr cur = node;
	if (!f)
		return;
	while (cur) {
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0) {
			if (!ns || (cur->ns && xmlStrcasecmp(cur->ns->prefix, 
							(unsigned char*)ns) == 0))
				f(cur, data);
		}
		/* visit children */
		xmlNodeMapByName(cur->children, name, ns, f, data);

		cur = cur->next;
	}
}

void xmlDocMapByName(xmlDocPtr doc, const char *name, const char *ns,
			   void (f)(xmlNodePtr, void*), void *data )
{
	xmlNodePtr cur = doc->children;
	xmlNodeMapByName(cur, name, ns, f, data);
}



str* agregate_xmls(str** body_array, int n)

{
	int i, j= 0, append ;
	xmlNodePtr p_root= NULL, new_p_root= NULL ;
	xmlDocPtr* xml_array ;
	xmlNodePtr node = NULL;
	xmlNodePtr add_node = NULL ;
	str *body= NULL;
	char* id= NULL, *tuple_id = NULL;

	xml_array = (xmlDocPtr*)pkg_malloc( (n+1)*sizeof(xmlDocPtr));

	if(!xml_array)
	{
	
		LOG(L_ERR,"PRESENCE:agregate_xmls: Error while alocating memory");
		return NULL;
	}
	
	memset(xml_array, 0, (n+1)*sizeof(xmlDocPtr)) ;

	for(i=0; i<n;i++)
	{
		if(body_array[i] == NULL)
			continue;

		xml_array[j] = NULL;
		xml_array[j] = xmlParseMemory( body_array[i]->s, body_array[i]->len );
		
		if( xml_array[j]== NULL)
		{
			LOG(L_ERR,"PRESENCE:agregate_xmls: ERROR while parsing xml body message\n");
			goto error;
		}
		j++;

	} 	

	j--;
	p_root = xmlDocGetNodeByName( xml_array[j], "presence", NULL);
	if(p_root ==NULL)
	{
		LOG(L_ERR,"PRESENCE:agregate_xmls: ERROR while geting the xml_tree root\n");
		goto error;
	}

	for(i= j; i>=0; i--)
	{
		new_p_root= xmlDocGetNodeByName( xml_array[i], "presence", NULL);
		if(new_p_root ==NULL)
		{
			LOG(L_ERR,"PRESENCE:agregate_xmls: ERROR while geting the xml_tree root\n");
			goto error;
		}

		node= xmlNodeGetChildByName(new_p_root, "tuple");
		if(node== NULL)
		{
			LOG(L_ERR, "PRESENCE:agregate_xmls: ERROR couldn't "
					"extract tuple node\n");
			goto error;
		}
		tuple_id= xmlNodeGetAttrContentByName(node, "id");
		if(tuple_id== NULL)
		{
			LOG(L_ERR, "PRESENCE:agregate_xmls: Error while extracting tuple id\n");
			goto error;
		}
		append= 1;
		for (node = p_root->children; node!=NULL; node = node->next)
		{		
			if( xmlStrcasecmp(node->name,(unsigned char*)"text")==0)
				continue;
			
			if( xmlStrcasecmp(node->name,(unsigned char*)"tuple")==0)
			{
				id = xmlNodeGetAttrContentByName(node, "id");
				if(id== NULL)
				{
					LOG(L_ERR, "PRESENCE:agregate_xmls: Error while extracting tuple id\n");
					goto error;
				}
				
				if(xmlStrcasecmp((unsigned char*)tuple_id,
							(unsigned char*)id )== 0)
				{
					append = 0;
					xmlFree(id);
					break;
				}
				xmlFree(id);
			}
		}
		xmlFree(tuple_id);
		tuple_id= NULL;

		if(append) 
		{	
			for(node= new_p_root->children; node; node= node->next)
			{	
				add_node= xmlCopyNode(node, 1);
				if(add_node== NULL)
				{
					LOG(L_ERR, "PRESENCE:agregate_xmls: Error while copying node\n");
					goto error;
				}
				if(xmlAddChild(p_root, add_node)== NULL)
				{
					LOG(L_ERR,"PRESENCE:agregate_xmls:Error while adding child\n");
					goto error;
				}
								
			}
		}
	}

	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL)
	{
		LOG(L_ERR,"PRESENCE:agregate_xmls:Error while allocating memory\n");
		goto error;
	}

	xmlDocDumpFormatMemory(xml_array[j],(xmlChar**) &body->s, 
			&body->len, 1);	

  	for(i=0; i<=j; i++)
	{
		if(xml_array[i]!=NULL)
			xmlFreeDoc( xml_array[i]);
	}
	if(xml_array!=NULL)
		pkg_free(xml_array);
    
	xmlCleanupParser();
    xmlMemoryDump();

	return body;

error:
	if(xml_array!=NULL)
	{
		for(i=0; i<=j; i++)
		{
			if(xml_array[i]!=NULL)
				xmlFreeDoc( xml_array[i]);
		}
		pkg_free(xml_array);
	}
	if(tuple_id)
		xmlFree(tuple_id);
	if(body)
		pkg_free(body);

	return NULL;
}
str* offline_nbody(str* body)
{
	xmlDocPtr doc= NULL;
	xmlDocPtr new_doc= NULL;
	xmlNodePtr node, tuple_node= NULL;
	xmlNodePtr root_node, add_node, pres_node;
	str* new_body;

	doc= xmlParseMemory(body->s, body->len);
	if(doc==  NULL)
	{
		LOG(L_ERR, "PRESENCE:offline_nbody: ERROR while parsing xml memory\n");
		return NULL;
	}
	node= xmlDocGetNodeByName(doc, "basic", NULL);
	if(node== NULL)
	{
		LOG(L_ERR, "PRESENCE:offline_nbody: ERROR while extracting basic node\n");
		goto error;
	}
	xmlNodeSetContent(node, (const unsigned char*)"closed");

	tuple_node= xmlDocGetNodeByName(doc, "tuple", NULL);
	if(node== NULL)
	{
		LOG(L_ERR, "PRESENCE:offline_nbody: ERROR while extracting tuple node\n");
		goto error;
	}
	pres_node= xmlDocGetNodeByName(doc, "presence", NULL);
	if(node== NULL)
	{
		LOG(L_ERR, "PRESENCE:offline_nbody: ERROR while extracting presence node\n");
		goto error;
	}


    new_doc = xmlNewDoc(BAD_CAST "1.0");
    if(new_doc==0)
		goto error;
	root_node= xmlCopyNode(pres_node, 2);
	if(root_node== NULL)
	{
		LOG(L_ERR, "PRESENCE:offline_nbody: Error while copying node\n");
		goto error;
	}
    xmlDocSetRootElement(new_doc, root_node);

  	add_node= xmlCopyNode(tuple_node, 1);
	if(add_node== NULL)
	{
		LOG(L_ERR, "PRESENCE:offline_nbody: Error while copying node\n");
		goto error;
	}
	xmlAddChild(root_node, add_node);

	new_body = (str*)pkg_malloc(sizeof(str));
	if(new_body == NULL)
	{
		LOG(L_ERR,"PRESENCE: build_off_nbody:Error while allocating memory\n");
		goto error;
	}
	memset(new_body, 0, sizeof(str));

	xmlDocDumpFormatMemory(new_doc,(xmlChar**) &new_body->s, &new_body->len, 1);

	xmlFreeDoc(doc);
	xmlFreeDoc(new_doc);
	xmlCleanupParser();
	xmlMemoryDump();

	return new_body;

error:
	if(doc)
		xmlFreeDoc(doc);
	if(new_doc)
		xmlFreeDoc(new_doc);
	return NULL;

}		

str* build_off_nbody(str p_user, str p_domain, str* etag)
{	
	xmlDocPtr doc = NULL, new_doc = NULL; 
	xmlNodePtr root_node = NULL, node = NULL, p_root= NULL;
	xmlNodePtr tuple_node = NULL, status_node = NULL, basic_node = NULL;

	db_key_t query_cols[4];
	db_op_t  query_ops[4];
	db_val_t query_vals[4];
	db_key_t result_cols[4];
	db_res_t *result = NULL;
	int body_col;
	db_row_t *row ;	
	db_val_t *row_vals;
	int n_result_cols = 0;
	int n_query_cols = 0;
	str old_body ;
	char * tuple_id = NULL, * entity = NULL;
	str* body;
	char* status = NULL;

	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL)
	{
		LOG(L_ERR,"PRESENCE: build_off_nbody:Error while allocating memory\n");
		return NULL;
	}
	memset(body, 0, sizeof(str));

	query_cols[n_query_cols] = "domain";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = p_domain.s;
	query_vals[n_query_cols].val.str_val.len = p_domain.len;
	n_query_cols++;

	query_cols[n_query_cols] = "username";
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = p_user.s;
	query_vals[n_query_cols].val.str_val.len = p_user.len;
	n_query_cols++;

	if(etag!= NULL)
	{
		query_cols[n_query_cols] = "etag";
		query_ops[n_query_cols] = OP_EQ;
		query_vals[n_query_cols].type = DB_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val.s = etag->s;
		query_vals[n_query_cols].val.str_val.len = etag->len;
		n_query_cols++;
	}

	result_cols[body_col=n_result_cols++] = "body" ;

	if (pa_dbf.use_table(pa_db, presentity_table) < 0) 
	{
		LOG(L_ERR, "PRESENCE: build_off_nbody: Error in use_table\n");
		goto error;
	}

	LOG(L_INFO,"PRESENCE: build_off_nbody: querying presentity\n");
	if (pa_dbf.query (pa_db, query_cols, query_ops, query_vals,
		 result_cols, n_query_cols, n_result_cols, 0,  &result) < 0) 
	{
		LOG(L_ERR, "PRESENCE: build_off_nbody: Error while querying"
				" presentity\n");
		goto error;
	}
	if(result== NULL)
		goto error;

	if (result && result->n <=0 )
	{
		LOG(L_ERR, "PRESENCE: build_off_nbody:The query returned no result\n");
		goto error;
	}

	row = &result->rows[0];
	row_vals = ROW_VALUES(row);
	old_body = row_vals[body_col].val.str_val ;
	DBG("PRESENCE: build_off_nbody:old-body:\n%.*s\n",old_body.len, old_body.s);

	doc = xmlParseMemory( old_body.s, old_body.len );
	if(doc== NULL) 
	{
		LOG(L_ERR,"PRESENCE: build_off_nbody: ERROR while parsing"
				" the xml body message\n");
		goto error;
	}
	
	p_root = xmlDocGetNodeByName(doc,"presence", NULL);
	status = xmlNodeGetNodeContentByName(p_root, "basic", NULL );
	if(status == NULL)
	{
		LOG(L_ERR, "PRESENCE:build_off_nbody: error while getting entity"
				" attribute\n");
		goto error;
	}

	if( etag && (strncmp (status,"closed", 6) == 0) )
	{
		LOG(L_INFO, "PRESENCE:build_off_nbody:The presentity status was"
				" already offline\n");
		goto error;
	}
	entity = xmlNodeGetAttrContentByName(p_root, "entity");
	if(entity == NULL)
	{
		LOG(L_ERR, "PRESENCE:build_off_nbody: error while getting entity"
				" attribute\n");
		goto error;
	}
	for (node = p_root->children; node!=NULL; node = node->next)
	{
		if(xmlStrcasecmp (node->name,(unsigned char*) "tuple") == 0)
		{
			tuple_id = xmlNodeGetAttrContentByName(node, "id");
			if(tuple_id == NULL)
			{
				LOG(L_ERR, "PRESENCE:build_off_nbody: error while getting"
						" entity attribute\n");
				goto error;
			}
		}	
	}

	LIBXML_TEST_VERSION;
	/* 
     * Creates a new document, a node and set it as a root node
     */
    new_doc = xmlNewDoc(BAD_CAST "1.0");
	if(new_doc==0)
		goto error;
    root_node = xmlNewNode(NULL, BAD_CAST "presence");
	if(root_node==0)
		goto error;
    xmlDocSetRootElement(new_doc, root_node);

    xmlNewProp(root_node, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:pidf");
	xmlNewProp(root_node, BAD_CAST "xmlns:dm",
			BAD_CAST "urn:ietf:params:xml:ns:pidf:data-model");
	xmlNewProp(root_node, BAD_CAST  "xmlns:rpid",
			BAD_CAST "urn:ietf:params:xml:ns:pidf:rpid" );
	xmlNewProp(root_node, BAD_CAST "xmlns:c",
			BAD_CAST "urn:ietf:params:xml:ns:pidf:cipid");
	xmlNewProp(root_node, BAD_CAST "entity", BAD_CAST entity);

	tuple_node =xmlNewChild(root_node, NULL, BAD_CAST "tuple", NULL) ;
	if( tuple_node ==NULL)
	{
		LOG(L_ERR, "PRESENCE:build_off_nbody: ERROR while adding child\n");
		goto error;
	}
	xmlNewProp(tuple_node, BAD_CAST "id", BAD_CAST tuple_id);
	
	status_node = xmlNewChild(tuple_node, NULL, BAD_CAST "status", NULL) ;
	if( status_node ==NULL)
	{
		LOG(L_ERR, "PRESENCE: build_off_nbody: ERROR while adding child\n");
		goto error;
	}
	
	basic_node = xmlNewChild(status_node, NULL, BAD_CAST "basic",
			BAD_CAST "closed") ;
	if( basic_node ==NULL)
	{
		LOG(L_ERR, "PRESENCE:build_off_nbody: ERROR while adding child\n");
		goto error;
	}
	xmlDocDumpFormatMemory(new_doc,(xmlChar**) &body->s, &body->len, 1);

	DBG("PRESENCE: build_off_nbody:new_body:\n%.*s\n",body->len, body->s);

	if(result !=NULL)
		pa_dbf.free_result(pa_db, result);

    /*free the document */
    xmlFreeDoc(doc);
	xmlFreeDoc(new_doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    /*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();

	xmlFree(status);
	xmlFree(entity);
	xmlFree(tuple_id);
	
    return body;

error:
	if(doc)
		xmlFreeDoc(doc);
	if(new_doc)
		xmlFreeDoc(new_doc);
	if(result)
		pa_dbf.free_result(pa_db, result);
	if(body)
	{
		if(body->s)
			xmlFree(body->s);
		pkg_free(body);
	}
	if(status)
		xmlFree(status);
	if(entity)
		xmlFree(entity);
	if(tuple_id)
		xmlFree(tuple_id);
	return NULL;

}

str* create_winfo_xml(watcher_t* watchers,int n, char* version,char* resource, int STATE_FLAG )
{
	xmlDocPtr doc = NULL;       /* document pointer */
    xmlNodePtr root_node = NULL, node = NULL;/* node pointers */
    xmlNodePtr w_list_node = NULL;	
    int i;
	char content[200];
	str *body;

	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL)
	{
		LOG(L_ERR,"PRESENCE:create_winfo_xmls:Error while allocating memory\n");
		return NULL;
	}

	memset(body, 0, sizeof(str));

    LIBXML_TEST_VERSION;

    /* 
     * Creates a new document, a node and set it as a root node
     */
    doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "watcherinfo");
    xmlDocSetRootElement(doc, root_node);

    xmlNewProp(root_node, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:watcherinfo");
    xmlNewProp(root_node, BAD_CAST "version", BAD_CAST version );
   
	if(STATE_FLAG & FULL_STATE_FLAG)
	{
		if( xmlNewProp(root_node, BAD_CAST "state", BAD_CAST "full") == NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding new"
					"attribute\n");
			goto error;
		}
	}
	else	
	{	
		if( xmlNewProp(root_node, BAD_CAST "state", 
					BAD_CAST "partial")== NULL) /* chage this */
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding new"
					"attribute\n");
			goto error;
		}
	}
	/* 
     * xmlNewChild() creates a new node, which is "attached" as child node
     * of root_node node. 
     */

     w_list_node =xmlNewChild(root_node, NULL, BAD_CAST "watcher-list",NULL);

	if( w_list_node == NULL)
	{
		LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding child\n");
		goto error;
	}
	xmlNewProp(w_list_node, BAD_CAST "resource", BAD_CAST resource);
	xmlNewProp(w_list_node, BAD_CAST "package", BAD_CAST "presence");

//	strcpy(content, "sip:");

    for( i =0; i<n; i++)
	{
		strncpy( content,watchers[i].uri.s, watchers[i].uri.len);
		content[ watchers[i].uri.len ]='\0';
		node = xmlNewChild(w_list_node, NULL, BAD_CAST "watcher",
				BAD_CAST content) ;
		if( node ==NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding"
					" child\n");
			goto error;
		}
		if(xmlNewProp(node, BAD_CAST "id", BAD_CAST watchers[i].id.s)== NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding"
					" new attribute\n");
			goto error;
		}	
		
		if(xmlNewProp(node, BAD_CAST "event", BAD_CAST "subscribe")== NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding new"
					" attribute\n");
			goto error;
		}	
		
		if(xmlNewProp(node, BAD_CAST "status", 
					BAD_CAST watchers[i].status.s )== NULL)
		{
			LOG(L_ERR, "PRESENCE: create_winfo_xml: ERROR while adding"
					" new attribute\n");
			goto error;
		}	
	}
    
    /* 
     * Dumping document to stdio or file
     */
    //xmlSaveFormatFileEnc("stdout", doc, "UTF-8", 1);
	xmlDocDumpFormatMemory(doc,(xmlChar**) &body->s, &body->len, 1);	

    /*free the document */
    if(doc)
		xmlFreeDoc(doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    /*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();

    return body;

error:
	if(body)
	{
		if(body->s)
			xmlFree(body->s);
		pkg_free(body);
	}
    if(doc)
		xmlFreeDoc(doc);
	return NULL;
}


str* get_final_notify_body( subs_t *subs, str* notify_body, xmlNodePtr rule_node)
{
	xmlNodePtr transf_node = NULL, node = NULL, dont_provide = NULL;
	xmlNodePtr doc_root = NULL, doc_node = NULL, provide_node = NULL;
	xmlNodePtr all_node = NULL;
	xmlDocPtr doc= NULL;
	char name[15];
	char service_uri_scheme[10];
	int i= 0, found = 0;
	str* new_body = NULL;
    char* class_cont = NULL, *occurence_ID= NULL, *service_uri= NULL;
	char* deviceID = NULL;
	char* content = NULL;
	char all_name[20];

	strcpy(all_name, "all-");

	new_body = (str*)pkg_malloc(sizeof(str));
	if(new_body == NULL)
	{
		LOG(L_ERR,"get_final_notify_body: ERROR while allocating memory\n");
		return NULL;
	}	

	memset(new_body, 0, sizeof(str));

	doc = xmlParseMemory(notify_body->s, notify_body->len);
	if(doc== NULL) 
	{
		LOG(L_ERR,"get_final_notify_body: ERROR while parsing the xml body"
				" message\n");
		goto error;
	}
	doc_root = xmlDocGetNodeByName(doc,"presence", NULL);
	if(doc_root == NULL)
	{
		LOG(L_ERR,"PRESENCE:get_final_notify_body:ERROR while extracting"
				" the transformation node\n");
		goto error;
	}

	transf_node = xmlNodeGetChildByName(rule_node, "transformations");
	if(transf_node == NULL)
	{
		LOG(L_ERR,"PRESENCE:get_final_notify_body:ERROR while extracting"
				" the transformation node\n");
		goto error;
	}
	
	for(node = transf_node->children; node; node = node->next )
	{
		if(xmlStrcasecmp(node->name, (unsigned char*)"text")== 0)
			continue;

		DBG("PRESENCE:get_final_notify_body:transf_node->name:%s\n",node->name);

		strcpy((char*)name ,(char*)(node->name + 8));
		strcpy(all_name+4, name);
		
		if(xmlStrcasecmp((unsigned char*)name,(unsigned char*)"services") == 0)
			strcpy(name, "tuple");
		if(strncmp((char*)name,"person", 6) == 0)
			name[6] = '\0';

		doc_node = xmlNodeGetNodeByName(doc_root, name, NULL);
		if(doc_node == NULL)
			continue;
		DBG("PRESENCE:get_final_notify_body:searched doc_node->name:%s\n",name);
	
		content = (char*)xmlNodeGetContent(node);
		if(content)
		{
			DBG("PRESENCE:get_final_notify_body: content = %s\n", content);
		
			if(xmlStrcasecmp((unsigned char*)content,
					(unsigned char*) "FALSE") == 0)
			{
				DBG("PRESENCE:get_final_notify_body:found content false\n");
				while( doc_node )
				{
					xmlUnlinkNode(doc_node);	
					xmlFreeNode(doc_node);
					doc_node = xmlNodeGetChildByName(doc_root, name);
				}
				xmlFree(content);
				continue;
			}
		
			if(xmlStrcasecmp((unsigned char*)content,
					(unsigned char*) "TRUE") == 0)
			{
				DBG("PRESENCE:get_final_notify_body:found content true\n");
				xmlFree(content);
				continue;
			}
			xmlFree(content);
		}

		while (doc_node )
		{
			if (xmlStrcasecmp(doc_node->name,(unsigned char*)"text")==0)
			{
				doc_node = doc_node->next;
				continue;
			}

			if (xmlStrcasecmp(doc_node->name,(unsigned char*)name)!=0)
			{
				break;
			}
			all_node = xmlNodeGetChildByName(node, all_name) ;
		
			if( all_node )
			{
				DBG("PRESENCE:get_final_notify_body: must provide all\n");
				doc_node = doc_node->next;
				continue;
			}

			found = 0;
			class_cont = xmlNodeGetNodeContentByName(doc_node, "class", 
					NULL);
			if(class_cont == NULL)
				DBG("PRESENCE:get_final_notify_body: no class tag found\n");
			else
				DBG("PRESENCE:get_final_notify_body found class = %s\n",
						class_cont);

			occurence_ID = xmlNodeGetAttrContentByName(doc_node, "id");
			if(occurence_ID == NULL)
				DBG("PRESENCE:get_final_notify_body: no id found\n");
			else
				DBG("PRESENCE:get_final_notify_body found id = %s\n",
						occurence_ID);


			deviceID = xmlNodeGetNodeContentByName(doc_node, "deviceID",
					NULL);	
			if(deviceID== NULL)
				DBG("PRESENCE:get_final_notify_body: no deviceID found\n");
			else
				DBG("PRESENCE:get_final_notify_body found deviceID = %s\n",
						deviceID);


			service_uri = xmlNodeGetNodeContentByName(doc_node, "contact",
					NULL);	
			if(service_uri == NULL)
				DBG("PRESENCE:get_final_notify_body: no service_uri found\n");
			else
				DBG("PRESENCE:get_final_notify_body found service_uri = %s\n",
						service_uri);



			if(service_uri!= NULL)
			{
				while(service_uri[i]!= ':')
				{
					service_uri_scheme[i] = service_uri[i];
					i++;
				}
				service_uri_scheme[i] = '\0';
				DBG("PRESENCE:get_final_notify_body:service_uri_scheme: %s\n",
						service_uri_scheme);
			}

			provide_node = node->children;
				
			while ( provide_node!= NULL )
			{
				if(xmlStrcasecmp(provide_node->name,(unsigned char*) "text")==0)
				{
					provide_node = 	provide_node->next;
					continue;
				}

				if(xmlStrcasecmp(provide_node->name,(unsigned char*)"class")== 0
						&& class_cont )
				{
					content = (char*)xmlNodeGetContent(provide_node);

					if(content&& xmlStrcasecmp((unsigned char*)content,
								(unsigned char*)class_cont) == 0)
					{
						found = 1;
						DBG("PRESENCE:get_final_notify_body: found class= %s",
								class_cont);
						xmlFree(content);
						break;
					}
					if(content)
						xmlFree(content);
				}
				if(xmlStrcasecmp(provide_node->name,
							(unsigned char*) "deviceID")==0&&deviceID )
				{
					content = (char*)xmlNodeGetContent(provide_node);

					if(content && xmlStrcasecmp ((unsigned char*)content,
								(unsigned char*)deviceID) == 0)
					{
						found = 1;
						DBG("PRESENCE:get_final_notify_body: found deviceID="
								" %s", deviceID);
						xmlFree(content);
						break;
					}
					if(content)
						xmlFree(content);

				}
				if(xmlStrcasecmp(provide_node->name,
							(unsigned char*)"occurence-id")== 0&& occurence_ID)
				{
					content = (char*)xmlNodeGetContent(provide_node);
					if(content && xmlStrcasecmp ((unsigned char*)content,
								(unsigned char*)occurence_ID) == 0)
					{
						found = 1;
						DBG("PRESENCE:get_final_notify_body:" 
								" found occurenceID= %s\n", occurence_ID);
						xmlFree(content);
						break;
					}
					if(content)
						xmlFree(content);

				}
				if(xmlStrcasecmp(provide_node->name,
							(unsigned char*)"service-uri")== 0 && service_uri)
				{
					content = (char*)xmlNodeGetContent(provide_node);
					if(content&& xmlStrcasecmp ((unsigned char*)content,
								(unsigned char*)service_uri) == 0)
					{
						found = 1;
						DBG("PRESENCE:get_final_notify_body: found"
								" service_uri= %s", service_uri);
						xmlFree(content);
						break;
					}
					if(content)
						xmlFree(content);

				}
			
				if(xmlStrcasecmp(provide_node->name,
							(unsigned char*)"service-uri-scheme")==0
						&& service_uri_scheme)
				{
					content = (char*)xmlNodeGetContent(provide_node);
					DBG("PRESENCE:get_final_notify_body:"
							" service_uri_scheme=%s\n",content);
					if(content && xmlStrcasecmp((unsigned char*)content,
								(unsigned char*)service_uri_scheme) == 0)
					{
						found = 1;
						DBG("PRESENCE:get_final_notify_body: found"
								" service_uri_scheme= %s", service_uri_scheme);
						xmlFree(content);
						break;
					}	
					if(content)
						xmlFree(content);

				}

				provide_node = provide_node->next;
			}
			
			if(found == 0)
			{
				DBG("PRESENCE:get_final_notify_body: delete node: %s\n",
						doc_node->name);
				dont_provide = doc_node;
				doc_node = doc_node->next;
				xmlUnlinkNode(dont_provide);	
				xmlFreeNode(dont_provide);
			}	
			else
				doc_node = doc_node->next;
	
		}	
	}
	xmlDocDumpFormatMemory(doc,(xmlChar**) &new_body->s, &new_body->len, 1);
	DBG("PRESENCE:get_final_notify_body: body = \n%.*s\n", new_body->len,
			new_body->s);

    xmlFreeDoc(doc);

    xmlCleanupParser();

    xmlMemoryDump();

	xmlFree(class_cont);
	xmlFree(occurence_ID);
	xmlFree(deviceID);
	xmlFree(service_uri);

    return new_body;
error:
    if(doc)
		xmlFreeDoc(doc);
	if(new_body)
	{
		if(new_body->s)
			xmlFree(new_body->s);
		pkg_free(new_body);
	}
	if(class_cont)
		xmlFree(class_cont);
	if(occurence_ID)
		xmlFree(occurence_ID);
	if(deviceID)
		xmlFree(deviceID);
	if(service_uri)
		xmlFree(service_uri);

	return NULL;
}	


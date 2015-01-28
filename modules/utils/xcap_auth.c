/*
 * xcap_auth.c
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
 *
 * Copyright (C) 2009 Juha Heinanen
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

/*!
 * \file
 * \brief Kamailio utils :: 
 * \ingroup utils
 * Module: \ref utils
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libxml/parser.h>

#include "../../str.h"
#include "../../dprint.h"
#include "../../pvar.h"
#include "../../parser/parse_uri.h"
#include "../../modules/presence/subscribe.h"
#include "../../modules/presence/utils_func.h"
#include "../../modules/presence/hash.h"
#include "../../modules/xcap_client/xcap_callbacks.h"
#include "utils.h"
#include "pidf.h"

xmlNodePtr get_rule_node(subs_t* subs, xmlDocPtr xcap_tree)
{
    str w_uri = {0, 0};
    char* id = NULL, *domain = NULL, *time_cont= NULL;
    int apply_rule = -1;
    xmlNodePtr ruleset_node = NULL, node1= NULL, node2= NULL;
    xmlNodePtr cond_node = NULL, except_node = NULL;
    xmlNodePtr identity_node = NULL;
    xmlNodePtr iden_child;
    xmlNodePtr validity_node, time_node;
    time_t t_init, t_fin, t;
    int valid= 0;

    uandd_to_uri(subs->from_user, subs->from_domain, &w_uri);
    if (w_uri.s == NULL) {
	LM_ERR("while creating uri\n");
	return NULL;
    }
    ruleset_node = xmlDocGetNodeByName(xcap_tree, "ruleset", NULL);
    if (ruleset_node == NULL) {
	LM_DBG("ruleset_node NULL\n");
	goto error;
    }	
    for (node1 = ruleset_node->children; node1; node1 = node1->next) {
	if (xmlStrcasecmp(node1->name, (unsigned char*)"text") == 0)
	    continue;

	/* process conditions */
	LM_DBG("node1->name= %s\n", node1->name);
	
	cond_node = xmlNodeGetChildByName(node1, "conditions");
	if(cond_node == NULL) {
	    LM_DBG("cond node NULL\n");
	    goto error;
	}
	LM_DBG("cond_node->name= %s\n", cond_node->name);

	validity_node = xmlNodeGetChildByName(cond_node, "validity");
	if (validity_node != NULL) {
	    LM_DBG("found validity tag\n");
	    
	    t= time(NULL);
		
	    /* search all from-until pair */
	    for (time_node = validity_node->children; time_node;
		time_node = time_node->next) {
		if (xmlStrcasecmp(time_node->name, (unsigned char*)"from")!= 0)
		    continue;

		time_cont= (char*)xmlNodeGetContent(time_node);
		t_init= xml_parse_dateTime(time_cont);
		xmlFree(time_cont);
		if (t_init< 0) {
		    LM_ERR("failed to parse xml dateTime\n");
		    goto error;
		}

		if (t< t_init) {
		    LM_DBG("the lower time limit is not respected\n");
		    continue;
		}
				
		time_node= time_node->next;
		while (1) {
		    if (time_node == NULL) {
			LM_ERR("bad formatted xml doc:until child not found in"
			       " validity pair\n");
			goto error;
		    }
		    if( xmlStrcasecmp(time_node->name, 
				      (unsigned char*)"until")== 0)
			break;
		    time_node= time_node->next;
		}
				
		time_cont = (char*)xmlNodeGetContent(time_node);
		t_fin= xml_parse_dateTime(time_cont);
		xmlFree(time_cont);
		
		if (t_fin< 0) {
		    LM_ERR("failed to parse xml dateTime\n");
		    goto error;
		}
			
		if (t <= t_fin) {
		    LM_DBG("the rule is active at this time\n");
		    valid= 1;
		}
			
	    }
		
	    if (!valid) {
		LM_DBG("the rule is not active at this time\n");
		continue;
	    }
	    
	}	

	identity_node = xmlNodeGetChildByName(cond_node, "identity");
	if (identity_node == NULL) {
	    LM_ERR("didn't find identity tag\n");
	    goto error;
	}	
		
	iden_child = xmlNodeGetChildByName(identity_node, "one");
	if(iden_child) {
	    for (node2 = identity_node->children; node2; node2 = node2->next) {
		if(xmlStrcasecmp(node2->name, (unsigned char*)"one")!= 0)
		    continue;
				
		id = xmlNodeGetAttrContentByName(node2, "id");	
		if(id== NULL) {
		    LM_ERR("while extracting attribute\n");
		    goto error;
		}
		if ((strlen(id)== w_uri.len && 
		     (strncmp(id, w_uri.s, w_uri.len)==0))) {
		    apply_rule = 1;
		    xmlFree(id);
		    break;
		}
		xmlFree(id);
	    }
	}	

	/* search for many node*/
	iden_child = xmlNodeGetChildByName(identity_node, "many");
	if (iden_child)	{
	    domain = NULL;
	    for (node2 = identity_node->children; node2; node2 = node2->next) {
		if (xmlStrcasecmp(node2->name, (unsigned char*)"many") != 0)
		    continue;
	
		domain = xmlNodeGetAttrContentByName(node2, "domain");
		if(domain == NULL) {
		    LM_DBG("No domain attribute to many\n");
		} else	{
		    LM_DBG("<many domain= %s>\n", domain);
		    if((strlen(domain)!= subs->from_domain.len && 
			strncmp(domain, subs->from_domain.s,
				subs->from_domain.len) )) {
			xmlFree(domain);
			continue;
		    }	
		}
		xmlFree(domain);
		apply_rule = 1;
		if (node2->children == NULL)       /* there is no exception */
		    break;

		for (except_node = node2->children; except_node;
		     except_node= except_node->next) {
		    if(xmlStrcasecmp(except_node->name,
				     (unsigned char*)"except"))
			continue;

		    id = xmlNodeGetAttrContentByName(except_node, "id");	
		    if (id != NULL) {
			if((strlen(id)- 1== w_uri.len && 
			    (strncmp(id, w_uri.s, w_uri.len)==0))) {
			    xmlFree(id);
			    apply_rule = 0;
			    break;
			}
			xmlFree(id);
		    } else {
			domain = NULL;
			domain = xmlNodeGetAttrContentByName(except_node,
							     "domain");
			if(domain!=NULL) {
			    LM_DBG("Found except domain= %s\n- strlen(domain)= %d\n",
				   domain, (int)strlen(domain));
			    if (strlen(domain)==subs->from_domain.len &&
				(strncmp(domain,subs->from_domain.s ,
					 subs->from_domain.len)==0)) {
				LM_DBG("except domain match\n");
				xmlFree(domain);
				apply_rule = 0;
				break;
			    }
			    xmlFree(domain);
			}	
		    }	
		}
		if (apply_rule == 1)  /* if a match was found no need to keep searching*/
		    break;
	    }
	}
	if (apply_rule ==1)
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

int pres_watcher_allowed(subs_t* subs)
{
    xmlDocPtr xcap_tree= NULL;
    xmlNodePtr node= NULL,  actions_node = NULL;
    xmlNodePtr sub_handling_node = NULL;
    char* sub_handling = NULL;
	
    subs->status= PENDING_STATUS;
    subs->reason.s= NULL;
    subs->reason.len= 0;

    if (subs->auth_rules_doc== NULL)
	return 0;

    xcap_tree= xmlParseMemory(subs->auth_rules_doc->s,
			      subs->auth_rules_doc->len);
    if (xcap_tree== NULL) {
	LM_ERR("parsing xml memory\n");
	return -1;
    }

    node= get_rule_node(subs, xcap_tree);
    if (node== NULL) {
	xmlFreeDoc(xcap_tree);
	return 0;
	}

    /* process actions */	
    actions_node = xmlNodeGetChildByName(node, "actions");
    if (actions_node == NULL) {
	LM_DBG("actions_node NULL\n");
	xmlFreeDoc(xcap_tree);
	return 0;
    }
    LM_DBG("actions_node->name= %s\n", actions_node->name);
			
    sub_handling_node = xmlNodeGetChildByName(actions_node, "sub-handling");
    if (sub_handling_node== NULL) {
	LM_DBG("sub_handling_node NULL\n");
	xmlFreeDoc(xcap_tree);
	return 0;
    }
    sub_handling = (char*)xmlNodeGetContent(sub_handling_node);
    LM_DBG("sub_handling_node->name= %s\n", sub_handling_node->name);
    LM_DBG("sub_handling_node->content= %s\n", sub_handling);
	
    if (sub_handling == NULL) {
	LM_ERR("Couldn't get sub-handling content\n");
	xmlFreeDoc(xcap_tree);
	return -1;
    }
    if (strncmp((char*)sub_handling, "block", 5) == 0) {
	subs->status = TERMINATED_STATUS;;
	subs->reason.s= "rejected";
	subs->reason.len = 8;
    } else	
	if (strncmp((char*)sub_handling, "confirm", 7) == 0) {
	    subs->status = PENDING_STATUS;
	} else
	    if (strncmp((char*)sub_handling , "polite-block", 12) == 0) {
		subs->status = ACTIVE_STATUS;
		subs->reason.s= "polite-block";
		subs->reason.len = 12;
	    }
	else
	    if (strncmp((char*)sub_handling, "allow", 5) == 0) {
		subs->status = ACTIVE_STATUS;
		subs->reason.s = NULL;
	    }
	    else {
		LM_ERR("unknown subscription handling action\n");
		xmlFreeDoc(xcap_tree);
		xmlFree(sub_handling);
		return -1;
	    }

	xmlFreeDoc(xcap_tree);
    xmlFree(sub_handling);

    return 0;

}

int get_rules_doc(str* user, str* domain, int type, str** rules_doc)
{
    db_key_t query_cols[5];
    db_val_t query_vals[5];
    db_key_t result_cols[3];
    int n_query_cols = 0;
    db1_res_t *result = 0;
    db_row_t *row;
    db_val_t *row_vals;
    str body;
    str* doc= NULL;
    int n_result_cols= 0, xcap_doc_col;
    static str tmp1 = str_init("username");
    static str tmp2 = str_init("domain");
    static str tmp3 = str_init("doc_type");
    static str tmp4 = str_init("doc");

    LM_DBG("[user]= %.*s\t[domain]= %.*s", 
	   user->len, user->s, domain->len, domain->s);

    query_cols[n_query_cols] = &tmp1;
    query_vals[n_query_cols].type = DB1_STR;
    query_vals[n_query_cols].nul = 0;
    query_vals[n_query_cols].val.str_val = *user;
    n_query_cols++;
    
    query_cols[n_query_cols] = &tmp2;
    query_vals[n_query_cols].type = DB1_STR;
    query_vals[n_query_cols].nul = 0;
    query_vals[n_query_cols].val.str_val = *domain;
    n_query_cols++;
    
    query_cols[n_query_cols] = &tmp3;
    query_vals[n_query_cols].type = DB1_INT;
    query_vals[n_query_cols].nul = 0;
    query_vals[n_query_cols].val.int_val= type;
    n_query_cols++;

    result_cols[xcap_doc_col= n_result_cols++] = &tmp4;

    if (pres_dbf.query(pres_dbh, query_cols, 0 , query_vals, result_cols, 
		       n_query_cols, 1, 0, &result) < 0) {
	LM_ERR("while querying table xcap for [user]=%.*s\t[domain]= %.*s\n",
	       user->len, user->s, domain->len, domain->s);
	if (result)
	    pres_dbf.free_result(pres_dbh, result);
	return -1;
    }

    if(result == NULL)
	return -1;

    if (result->n <= 0) {
	LM_DBG("No document found in db table for [user]=%.*s"
	       "\t[domain]= %.*s\t[doc_type]= %d\n",user->len, user->s,
	       domain->len, domain->s, type);
	pres_dbf.free_result(pres_dbh, result);
	return 0;
    }	
	
    row = &result->rows[xcap_doc_col];
    row_vals = ROW_VALUES(row);

    body.s = (char*)row_vals[0].val.string_val;
    if (body.s== NULL) {
	LM_ERR("Xcap doc NULL\n");
	goto error;
    }	
    body.len = strlen(body.s);
    if (body.len== 0) {
	LM_ERR("Xcap doc empty\n");
	goto error;
    }			
    LM_DBG("xcap document:\n%.*s", body.len,body.s);
    
    doc= (str*)pkg_malloc(sizeof(str));
    if (doc== NULL) {
	ERR_MEM(PKG_MEM_STR);
    }
    doc->s= (char*)pkg_malloc(body.len* sizeof(char));
    if (doc->s== NULL) {
	pkg_free(doc);
	ERR_MEM(PKG_MEM_STR);
    }
    memcpy(doc->s, body.s, body.len);
    doc->len= body.len;

    *rules_doc= doc;

    if (result)
	pres_dbf.free_result(pres_dbh, result);

    return 0;

error:
    if (result)
	pres_dbf.free_result(pres_dbh, result);

    return -1;

}


/* 
 * Checks from presence server xcap table if watcher is authorized
 * to subscribe event 'presence' of presentity.
 */
int xcap_auth_status(struct sip_msg* _msg, char* _sp1, char* _sp2)
{
    pv_spec_t *sp;
    pv_value_t pv_val;
    str watcher_uri, presentity_uri;
    struct sip_uri uri;
    str* rules_doc = NULL;
    subs_t subs;
    int res;

    if (pres_dbh == 0) {
	LM_ERR("function is disabled, to enable define pres_db_url\n");
	return -1;
    }

    sp = (pv_spec_t *)_sp1;

    if (sp && (pv_get_spec_value(_msg, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    watcher_uri = pv_val.rs;
	    if (watcher_uri.len == 0 || watcher_uri.s == NULL) {
		LM_ERR("missing watcher uri\n");
		return -1;
	    }
	} else {
	    LM_ERR("watcher pseudo variable value is not string\n");
	    return -1;
	}
    } else {
	LM_ERR("cannot get watcher pseudo variable value\n");
	return -1;
    }

    sp = (pv_spec_t *)_sp2;

    if (sp && (pv_get_spec_value(_msg, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    presentity_uri = pv_val.rs;
	    if (presentity_uri.len == 0 || presentity_uri.s == NULL) {
		LM_DBG("missing presentity uri\n");
		return -1;
	    }
	} else {
	    LM_ERR("presentity pseudo variable value is not string\n");
	    return -1;
	}
    } else {
	LM_ERR("cannot get presentity pseudo variable value\n");
	return -1;
    }

    if (parse_uri(presentity_uri.s, presentity_uri.len, &uri) < 0) {
	LM_ERR("failed to parse presentity uri\n");
	return -1;
    }
    res = get_rules_doc(&uri.user, &uri.host, PRES_RULES, &rules_doc);
    if ((res < 0) || (rules_doc == NULL) || (rules_doc->s == NULL)) {
	LM_DBG("no xcap rules doc found for presentity uri\n");
	return PENDING_STATUS;
    }

    if (parse_uri(watcher_uri.s, watcher_uri.len, &uri) < 0) {
	LM_ERR("failed to parse watcher uri\n");
	goto err;
    }

    subs.from_user = uri.user;
    subs.from_domain = uri.host;
    subs.pres_uri = presentity_uri;
    subs.auth_rules_doc = rules_doc;
    if (pres_watcher_allowed(&subs) < 0) {
	LM_ERR("getting status from rules document\n");
	goto err;
    }
    LM_DBG("auth status of watcher <%.*s> on presentity <%.*s> is %d\n",
	   watcher_uri.len, watcher_uri.s, presentity_uri.len, presentity_uri.s,
	   subs.status);
    pkg_free(rules_doc->s);
    pkg_free(rules_doc);
    return subs.status;

 err:
    pkg_free(rules_doc->s);
    pkg_free(rules_doc);
    return -1;
}

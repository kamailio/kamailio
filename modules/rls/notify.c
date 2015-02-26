/*
 * rls module - resource list server
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../ut.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../data_lump_rpl.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_event.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_cseq.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_rr.h"
#include "../../modules/tm/dlg.h"
#include "../presence/utils_func.h"
#include "../presence/hash.h"
#include "../../hashes.h"
#include "rls.h"
#include "notify.h"
#include "utils.h"
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

static str *multipart_body = NULL;
static int multipart_body_size = 0;

typedef struct res_param
{
    struct uri_link **next;
}res_param_t;

typedef struct uri_link
{
    char *uri;
    struct uri_link *next;
} uri_link_t;

int resource_uri_col=0, content_type_col, pres_state_col= 0,
	auth_state_col= 0, reason_col= 0;

xmlDocPtr constr_rlmi_doc(db1_res_t* result, str* rl_uri, int version,
		xmlNodePtr rl_node, char*** cid_array,
		str username, str domain);
void constr_multipart_body(const str *const content_type, const str *const body, str *cid, int boundary_len, char *boundary_string);

dlg_t* rls_notify_dlg(subs_t* subs);

void rls_notify_callback( struct cell *t, int type, struct tmcb_params *ps);

int parse_xcap_uri(char *uri, str *host, unsigned short *port, str *path);
int rls_get_resource_list(str *rl_uri, str *username, str *domain,
		xmlNodePtr *rl_node, xmlDocPtr *xmldoc);
int add_resource_to_list(char* uri, void* param);
int add_resource(char* uri, xmlNodePtr list_node, char * boundary_string, db1_res_t *result, int *len_est);

char *instance_id = "Scf8UhwQ";

int send_full_notify(subs_t* subs, xmlNodePtr rl_node, str* rl_uri,
		unsigned int hash_code)
{
	xmlDocPtr rlmi_body= NULL;
	xmlNodePtr list_node= NULL;
	db_key_t query_cols[1], update_cols[1], result_cols[5];
	db_val_t query_vals[1], update_vals[1];
	db1_res_t *result= NULL;
	int n_result_cols= 0;
	char* boundary_string;
	str rlsubs_did= {0, 0};
	str* rlmi_cont= NULL;
	uri_link_t *uri_list_head = NULL;
	int len_est;
	res_param_t param;
	int resource_added = 0; /* Flag to indicate that we have added at least one resource */
	multipart_body=NULL;
	db_query_f query_fn = rlpres_dbf.query_lock ? rlpres_dbf.query_lock : rlpres_dbf.query;

	LM_DBG("start\n");
	
	if(CONSTR_RLSUBS_DID(subs, &rlsubs_did)<0)
	{
		LM_ERR("cannot build rls subs did\n");
		goto error;
	}

	query_cols[0]= &str_rlsubs_did_col;
	query_vals[0].type = DB1_STR;
	query_vals[0].nul = 0;
	query_vals[0].val.str_val= rlsubs_did; 

	result_cols[resource_uri_col= n_result_cols++]= &str_resource_uri_col;
	result_cols[content_type_col= n_result_cols++]= &str_content_type_col;
	result_cols[pres_state_col= n_result_cols++]= &str_presence_state_col;
	result_cols[auth_state_col= n_result_cols++]= &str_auth_state_col;
	result_cols[reason_col= n_result_cols++]= &str_reason_col;

	update_cols[0]= &str_updated_col;
	update_vals[0].type = DB1_INT;
	update_vals[0].nul = 0;
	update_vals[0].val.int_val= NO_UPDATE_TYPE; 
	
	if (rlpres_dbf.use_table(rlpres_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}

	if (dbmode == RLS_DB_ONLY && rlpres_dbf.start_transaction)
	{
		if (rlpres_dbf.start_transaction(rlpres_db, DB_LOCKING_WRITE) < 0)
		{
			LM_ERR("in start_transaction\n");
			goto error;
		}
	}

	if(query_fn(rlpres_db, query_cols, 0, query_vals, result_cols,
					1, n_result_cols, NULL, &result )< 0)
	{
		LM_ERR("in sql query\n");
		goto error;
	}
	if(result == NULL)
	{
		LM_ERR("bad result\n");
		goto error;
	}

	if (result->n > 0)
	{
		if(rlpres_dbf.update(rlpres_db, query_cols, 0, query_vals,
				     update_cols, update_vals, 1, 1) < 0)
		{
			LM_ERR("in sql update\n");
			goto error;
		}
	}

	if (dbmode == RLS_DB_ONLY && rlpres_dbf.end_transaction)
	{
		if (rlpres_dbf.end_transaction(rlpres_db) < 0)
		{
			LM_ERR("in end_transaction\n");
			goto error;
		}
	}

	/* Allocate an initial buffer for the multipart body.
	 * This buffer will be reallocated if neccessary */
	multipart_body= (str*)pkg_malloc(sizeof(str));
	if(multipart_body== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

	multipart_body_size = BUF_REALLOC_SIZE;
	multipart_body->s = (char *)pkg_malloc(multipart_body_size);

	if(multipart_body->s== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
    
	multipart_body->len= 0;

	/* Create an empty rlmi document */
	len_est = create_empty_rlmi_doc(&rlmi_body, &list_node, rl_uri, subs->version, 1);
	xmlDocSetRootElement(rlmi_body, list_node);

	/* Find all the uri's to which we are subscribed */
	param.next = &uri_list_head;
	if(process_list_and_exec(rl_node, subs->watcher_user, subs->watcher_domain, add_resource_to_list,(void*)(&param))< 0)
	{
		LM_ERR("in process_list_and_exec function\n");
		goto error;
	}

	boundary_string= generate_string(BOUNDARY_STRING_LEN);
	
	while (uri_list_head)
	{
		uri_link_t *last = uri_list_head;
		if (add_resource(uri_list_head->uri, list_node, boundary_string, result, &len_est) >0)
		{
			if (resource_added == 0)
			{
				/* We have exceeded our length estimate without adding any resource.
				   We cannot send this resource, move on. */
				LM_ERR("Failed to add a single resource %d vs %d\n", len_est, rls_max_notify_body_len);
				uri_list_head = uri_list_head->next;
				if (last->uri) pkg_free(last->uri);
				pkg_free(last); 
			}
			else
			{
				LM_DBG("send_full_notify estimate exceeded %d vs %d\n", len_est, rls_max_notify_body_len);
				/* If add_resource returns > 0 the resource did not fit in our size limit */
				rlmi_cont= (str*)pkg_malloc(sizeof(str));
				if(rlmi_cont== NULL)
				{
					ERR_MEM(PKG_MEM_STR);
				}
				/* Where we are worried about length we won't use padding */
				xmlDocDumpFormatMemory(rlmi_body,(xmlChar**)(void*)&rlmi_cont->s,
							&rlmi_cont->len, 0);
				xmlFreeDoc(rlmi_body);

				if(agg_body_sendn_update(rl_uri, boundary_string, rlmi_cont,
						multipart_body, subs, hash_code)< 0)
				{
					LM_ERR("in function agg_body_sendn_update\n");
					goto error;
				}
                
				/* Create a new rlmi body, but not a full_state one this time */
				len_est = create_empty_rlmi_doc(&rlmi_body, &list_node, rl_uri, subs->version, 0);
				xmlDocSetRootElement(rlmi_body, list_node);
				multipart_body->len = 0;
				resource_added = 0;
			}
		}
		else
		{
			resource_added = 1;
			uri_list_head = uri_list_head->next;
			if (last->uri) pkg_free(last->uri);
			pkg_free(last);
		}
	}
    
	rlmi_cont= (str*)pkg_malloc(sizeof(str));
	if(rlmi_cont== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	xmlDocDumpFormatMemory(rlmi_body,(xmlChar**)(void*)&rlmi_cont->s,
			&rlmi_cont->len, (rls_max_notify_body_len == 0));
	xmlFreeDoc(rlmi_body);

	if(agg_body_sendn_update(rl_uri, boundary_string, rlmi_cont,
		multipart_body, subs, hash_code)< 0)
	{
		LM_ERR("in function agg_body_sendn_update\n");
		goto error;
	}

	xmlFree(rlmi_cont->s);
	pkg_free(rlmi_cont);

	if(multipart_body)			
	{
	    if (multipart_body->s)
			pkg_free(multipart_body->s);
		pkg_free(multipart_body);
	}
	multipart_body_size = 0;
	pkg_free(rlsubs_did.s);
	rlpres_dbf.free_result(rlpres_db, result);
	
	return 0;
error:
	if(rlmi_cont)
	{
		if(rlmi_cont->s)
			xmlFree(rlmi_cont->s);
		pkg_free(rlmi_cont);
	}
	if(multipart_body)
	{
		if(multipart_body->s)
			pkg_free(multipart_body->s);
		pkg_free(multipart_body);
	}
	multipart_body_size = 0;
	
	if(result)
		rlpres_dbf.free_result(rlpres_db, result);
	if(rlsubs_did.s)
		pkg_free(rlsubs_did.s);

	if (dbmode == RLS_DB_ONLY && rlpres_dbf.abort_transaction)
	{
		if (rlpres_dbf.abort_transaction(rlpres_db) < 0)
			LM_ERR("in abort_transaction");
	}
	return -1;
}

int agg_body_sendn_update(str* rl_uri, char* boundary_string, str* rlmi_body,
		str* multipart_body, subs_t* subs, unsigned int hash_code)
{
	char* cid;
	int len;
	str body= {0, 0};
	int init_len;

	cid= generate_cid(rl_uri->s, rl_uri->len);

	len= 2*strlen(boundary_string)+4+102+strlen(cid)+2+rlmi_body->len+50;
	if(multipart_body)
		len+= multipart_body->len;
	
	init_len= len;

	body.s= (char*)pkg_malloc((len+1)* sizeof(char));
	if(body.s== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	len=  sprintf(body.s, "--%s\r\n", boundary_string);
	len+= sprintf(body.s+ len , "Content-Transfer-Encoding: binary\r\n");
	len+= sprintf(body.s+ len , "Content-ID: <%s>\r\n", cid);	
	len+= sprintf(body.s+ len , 
			"Content-Type: application/rlmi+xml;charset=\"UTF-8\"\r\n");
	len+= sprintf(body.s+ len, "\r\n"); /*blank line*/
	memcpy(body.s+ len, rlmi_body->s, rlmi_body->len);
	len+= rlmi_body->len;
	len+= sprintf(body.s+ len, "\r\n"); /*blank line*/

	if(multipart_body)
	{
		memcpy(body.s+ len, multipart_body->s, multipart_body->len);
		len+= multipart_body->len;
	}
	len+= sprintf(body.s+ len, "--%s--\r\n", boundary_string);

	if(init_len< len)
	{
		LM_ERR("buffer size overflow init_size= %d\tlen= %d\n",init_len,len);
		goto error;
	}
	body.s[len]= '\0';
	body.len= len;

	/* send Notify */
	if(rls_send_notify(subs, &body, cid, boundary_string)< 0)
	{
		LM_ERR("when sending Notify\n");
		goto error;
	}
	/* update local_cseq in cache list watchers table */
	pkg_free(body.s);
	body.s= NULL;

	if (dbmode==RLS_DB_ONLY)
	{
		if (update_dialog_notify_rlsdb(subs) < 0)
		{
			LM_ERR( "updating DB\n" );
			goto error;
		}
	}
	else
	{
		if(pres_update_shtable(rls_table, hash_code,subs, LOCAL_TYPE)< 0)
		{
			LM_ERR("updating in hash table\n");
			goto error;
		}
	}

	return 0;

error:
	if(body.s)
		pkg_free(body.s);

	return -1;
}


int add_resource_instance(char* uri, xmlNodePtr resource_node,
		db1_res_t* result, char * boundary_string,
        int *len_est)
{
	xmlNodePtr instance_node= NULL;
	db_row_t *row;	
	db_val_t *row_vals;
	int i, cmp_code;
	char* auth_state= NULL;
	int auth_state_flag;
	int boundary_len = strlen(boundary_string);
	str cid;
	str content_type= {0, 0};
	str body= {0, 0};

	for(i= 0; i< result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);
		
		cmp_code= strncmp(row_vals[resource_uri_col].val.string_val, uri,
				strlen(uri));
		if(cmp_code> 0)
			break;

		if(cmp_code== 0)
		{
			auth_state_flag= row_vals[auth_state_col].val.int_val;
			auth_state= get_auth_string(auth_state_flag );
			if(auth_state== NULL)
			{
				LM_ERR("bad authorization status flag\n");
				goto error;
			}
			*len_est += strlen(auth_state) + 38; /* <instance id="12345678" state="[auth_state]" />r/n */

			if(auth_state_flag & ACTIVE_STATE)
			{
				cid.s= generate_cid(uri, strlen(uri));
				cid.len= strlen(cid.s);
				body.s= (char*)row_vals[pres_state_col].val.string_val;
				body.len= strlen(body.s);
				trim(&body);

				*len_est += cid.len + 8; /* cid="[cid]" */
				content_type.s = (char*)row_vals[content_type_col].val.string_val;
				content_type.len = strlen(content_type.s);
				*len_est += 4 + boundary_len
 						 + 35
						 + 16 + cid.len
						 + 18 + content_type.len
						 + 4 + body.len + 8;
			}
			else if(auth_state_flag & TERMINATED_STATE)
			{
				*len_est += strlen(row_vals[resource_uri_col].val.string_val) + 10; /* reason="[resaon]" */
			}
			if (rls_max_notify_body_len > 0 && *len_est > rls_max_notify_body_len)
			{
				/* We have a limit on body length set, and we were about to exceed it */
				return *len_est;
			}
            
			instance_node= xmlNewChild(resource_node, NULL, 
					BAD_CAST "instance", NULL);
			if(instance_node== NULL)
			{
				LM_ERR("while adding instance child\n");
				goto error;
			}
		
			/* OK, we are happy this will fit */
			/* Instance ID should be unique for each instance node
			   within a resource node.  The same instance ID can be
			   used in different resource nodes.  Instance ID needs
			   to remain the same for each resource instance in
			   future updates.  We can just use a common string
			   here because you will only get multiple instances
			   for a resource when the back-end SUBSCRIBE is forked
			   and pua does not support this.  If/when pua supports
			   forking of the SUBSCRIBEs it sends this will need to
			   be fixed properly. */
			xmlNewProp(instance_node, BAD_CAST "id",
					BAD_CAST instance_id);
			xmlNewProp(instance_node, BAD_CAST "state", BAD_CAST auth_state);

			if(auth_state_flag & ACTIVE_STATE)
			{
				constr_multipart_body (&content_type, &body, &cid, boundary_len, boundary_string);
				xmlNewProp(instance_node, BAD_CAST "cid", BAD_CAST cid.s);
			}
			else
			if(auth_state_flag & TERMINATED_STATE)
			{
				xmlNewProp(instance_node, BAD_CAST "reason", 
						BAD_CAST row_vals[reason_col].val.string_val);	
			}
		}
	}

	/* if record not found should not add a instance node */	
	return 0;
error:
	return -1;
}

int add_resource(char* uri, xmlNodePtr list_node, char * boundary_string, db1_res_t *result, int *len_est)
{
	xmlNodePtr resource_node= NULL;
    int res;

    if (rls_max_notify_body_len > 0)
    {
        *len_est += strlen (uri) + 35; /* <resource uri="[uri]"></resource>/r/n */
        if (*len_est > rls_max_notify_body_len)
        {
            return *len_est;
        }
    }
	resource_node= xmlNewChild(list_node, NULL, BAD_CAST "resource", NULL);
	if(resource_node== NULL)
	{
		goto error;
	}
	xmlNewProp(resource_node, BAD_CAST "uri", BAD_CAST uri);

    res = add_resource_instance(uri, resource_node, result, boundary_string, len_est);
	if(res < 0)
	{
		LM_ERR("while adding resource instance node\n");
		goto error;
	}

	return res;
error:
	return -1;
}

int add_resource_to_list(char* uri, void* param)
{
    struct uri_link **next = ((res_param_t*)param)->next;
    *next = pkg_malloc(sizeof(uri_link_t));
    if (*next == NULL)
	{
    	LM_ERR("while creating linked list element\n");
		goto error;
	}

    (*next)->next = NULL;
    (*next)->uri = pkg_malloc(strlen(uri) + 1);
    if ((*next)->uri == NULL)
	{
    	LM_ERR("while creating uri store\n");
        pkg_free(*next);
        *next = NULL;
		goto error;
	}
    strcpy((*next)->uri, uri);
    ((res_param_t*)param)->next = &(*next)->next;

	return 0;
error:
	return -1;
}

int create_empty_rlmi_doc(xmlDocPtr *rlmi_doc, xmlNodePtr *list_node, str *uri, int version, int full_state)
{
    /* length is an pessimitic estimate of the size of an empty document
       We calculate it once for performance reasons.
       We add in the uri length each time as this varies, and it is cheap to add */
    static int length = 0;
    char* rl_uri= NULL;
    int len;
    
    /* make new rlmi and multipart documents */
    *rlmi_doc= xmlNewDoc(BAD_CAST "1.0");
    if(*rlmi_doc== NULL)
    {
        LM_ERR("when creating new xml doc\n");
        return 0;
    }
    *list_node= xmlNewNode(NULL, BAD_CAST "list");
    if(*list_node== NULL)
	{
		LM_ERR("while creating new xml node\n");
        return 0;
	}
    rl_uri= (char*)pkg_malloc((uri->len+ 1)* sizeof(char));
    if(rl_uri==  NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
    memcpy(rl_uri, uri->s, uri->len);
    rl_uri[uri->len]= '\0';

    xmlNewProp(*list_node, BAD_CAST "uri", BAD_CAST rl_uri);
    xmlNewProp(*list_node, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:rlmi");
    xmlNewProp(*list_node, BAD_CAST "version",
            BAD_CAST int2str(version, &len));
    if (full_state)               
        xmlNewProp(*list_node, BAD_CAST "fullState", BAD_CAST "true");
    else
        xmlNewProp(*list_node, BAD_CAST "fullState", BAD_CAST "false");
	
    xmlDocSetRootElement(*rlmi_doc, *list_node);
    pkg_free(rl_uri);  /* xmlNewProp takes a copy, so we can free this now */

    if (length == 0)
	{
        /* We haven't found out how big an empty doc is
           Let's find out now ! */
        xmlChar* dumped_document;
        /* Where we are worried about length we won't use padding */
        xmlDocDumpFormatMemory( *rlmi_doc,&dumped_document, &length, 0);
        xmlFree(dumped_document);
        length -= uri->len; /* The uri varies, so we will add it each time */
	}
    return length + uri->len;
error:
    return 0;
}


void constr_multipart_body(const str *const content_type, const str *const body, str *cid, int boundary_len, char *boundary_string)
{
	char* buf= multipart_body->s;
	int length= multipart_body->len;
	int chunk_len;
	
	LM_DBG("start\n");

    chunk_len = 4 + boundary_len
                + 35
                + 16 + cid->len
                + 18 + content_type->len
                + 4 + body->len + 8;
		while(length + chunk_len >= multipart_body_size)
		{
			multipart_body_size += BUF_REALLOC_SIZE;
			multipart_body->s = (char*)pkg_realloc(multipart_body->s, multipart_body_size);
			if(multipart_body->s == NULL) 
			{
				ERR_MEM("constr_multipart_body");
			}
		}
		buf = multipart_body->s;

		length+= sprintf(buf+ length, "--%.*s\r\n",
            boundary_len, boundary_string);
		length+= sprintf(buf+ length, "Content-Transfer-Encoding: binary\r\n");
		length+= sprintf(buf+ length, "Content-ID: <%.*s>\r\n",
            cid->len, cid->s);
		length+= sprintf(buf+ length, "Content-Type: %.*s\r\n\r\n",
            content_type->len, content_type->s);
		length+= sprintf(buf+length,"%.*s\r\n\r\n",
            body->len, body->s);

	multipart_body->len = length;

error:

	return;
}

str* rls_notify_extra_hdr(subs_t* subs, char* start_cid, char* boundary_string)
{
	str* str_hdr= NULL;
	int len= 0, expires;

	str_hdr= (str*)pkg_malloc(sizeof(str));
	if(str_hdr== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memset(str_hdr, 0, sizeof(str));

	str_hdr->s= (char*)pkg_malloc(RLS_HDR_LEN* sizeof(char));
	if(str_hdr->s== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memcpy(str_hdr->s ,"Max-Forwards: ", 14);
	str_hdr->len = 14;
	len= sprintf(str_hdr->s+str_hdr->len, "%d", MAX_FORWARD);
	if(len<= 0)
	{
		LM_ERR("while printing in string\n");
		goto error;
	}	
	str_hdr->len+= len; 
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	memcpy(str_hdr->s+str_hdr->len  ,"Event: ", 7);
	str_hdr->len+= 7;
	memcpy(str_hdr->s+str_hdr->len, subs->event->name.s,
			subs->event->name.len);
	str_hdr->len+= subs->event->name.len;		
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	memcpy(str_hdr->s+str_hdr->len ,"Contact: <", 10);
	str_hdr->len += 10;
	memcpy(str_hdr->s+str_hdr->len,subs->local_contact.s,
			subs->local_contact.len);
	str_hdr->len +=  subs->local_contact.len;
	memcpy(str_hdr->s+str_hdr->len, ">", 1);
	str_hdr->len += 1;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	expires= subs->expires;

	if( expires> 0 )
		str_hdr->len+= sprintf(str_hdr->s+str_hdr->len,
			"Subscription-State: active;expires=%d\r\n", expires);
	else
		str_hdr->len+= sprintf(str_hdr->s+str_hdr->len,
			"Subscription-State: terminated;reason=timeout\r\n");

	str_hdr->len+= sprintf(str_hdr->s+str_hdr->len, "Require: eventlist\r\n");

	if(start_cid && boundary_string)
	{
		str_hdr->len+= sprintf(str_hdr->s+str_hdr->len,
			"Content-Type: multipart/related;type=\"application/rlmi+xml\"");
		str_hdr->len+= sprintf(str_hdr->s+str_hdr->len,
				";start=\"<%s>\";boundary=\"%s\"\r\n",
				start_cid, boundary_string);
	}		
	if(str_hdr->len> RLS_HDR_LEN)
	{
		LM_ERR("buffer size overflow\n");
		goto error;
	}
	str_hdr->s[str_hdr->len] = '\0';

	return str_hdr;

error:
	if(str_hdr)
	{
		if(str_hdr->s)
			pkg_free(str_hdr->s);
		pkg_free(str_hdr);
	}
	return NULL;

}

void rls_free_td(dlg_t* td)
{
	if(td)
	{
		if(td->loc_uri.s)
			pkg_free(td->loc_uri.s);
	
		if(td->rem_uri.s)
			pkg_free(td->rem_uri.s);

		if(td->route_set)
			free_rr(&td->route_set); 

		pkg_free(td);
	}	
}

int rls_send_notify(subs_t* subs, str* body, char* start_cid,
		char* boundary_string)
{
	dlg_t* td= NULL;
	str met= {"NOTIFY", 6};
	str* str_hdr= NULL;
	dialog_id_t* cb_param= NULL;
	int size;
	int rt;
	uac_req_t uac_r;

	LM_DBG("start\n");
	td= rls_notify_dlg(subs);
	if(td ==NULL)
	{
		LM_ERR("while building dlg_t structure\n");
		goto error;	
	}
	
	LM_DBG("constructed dlg_t struct\n");
	size= sizeof(dialog_id_t)+(subs->to_tag.len+ subs->callid.len+ 
			subs->from_tag.len) *sizeof(char);
	
	cb_param = (dialog_id_t*)shm_malloc(size);
	if(cb_param== NULL)
	{
		ERR_MEM(SHARE_MEM);
	}
	size= sizeof(dialog_id_t);
	
	cb_param->callid.s= (char*)cb_param + size;
	memcpy(cb_param->callid.s, subs->callid.s, subs->callid.len);
	cb_param->callid.len= subs->callid.len;
	size+= subs->callid.len;

	cb_param->to_tag.s= (char*)cb_param + size;
	memcpy(cb_param->to_tag.s, subs->to_tag.s, subs->to_tag.len);
	cb_param->to_tag.len= subs->to_tag.len;
	size+= subs->to_tag.len;

	cb_param->from_tag.s= (char*)cb_param + size;
	memcpy(cb_param->from_tag.s, subs->from_tag.s, subs->from_tag.len);
	cb_param->from_tag.len= subs->from_tag.len;
	
	LM_DBG("constructed cb_param\n");

	str_hdr= rls_notify_extra_hdr(subs, start_cid, boundary_string);
	if(str_hdr== NULL || str_hdr->s== NULL)
	{
		LM_ERR("while building extra headers\n");
		goto error;
	}
	LM_DBG("str_hdr= %.*s\n", str_hdr->len, str_hdr->s);

	set_uac_req(&uac_r, &met, str_hdr, body, td, TMCB_LOCAL_COMPLETED,
			rls_notify_callback, (void*)cb_param);

	rt = tmb.t_request_within(&uac_r);
	if(rt < 0)
	{
		LM_ERR("in function tmb.t_request_within\n");
		goto error;	
	}

	pkg_free(str_hdr->s);
	pkg_free(str_hdr);
	rls_free_td(td);
	return 0;

error:
	if(td)
		rls_free_td(td);
	if(cb_param)
		shm_free(cb_param);
	if(str_hdr)
	{
		if(str_hdr->s)
			pkg_free(str_hdr->s);
		pkg_free(str_hdr);
	}
	return -1;

}

dlg_t* rls_notify_dlg(subs_t* subs)
{
	dlg_t* td=NULL;

	td= (dlg_t*)pkg_malloc(sizeof(dlg_t));
	if(td== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

	memset(td, 0, sizeof(dlg_t));
	td->loc_seq.value = subs->local_cseq;
	td->loc_seq.is_set = 1;

	td->id.call_id = subs->callid;
	td->id.rem_tag = subs->from_tag;
	td->id.loc_tag =subs->to_tag;
	if(uandd_to_uri(subs->to_user, subs->to_domain, &td->loc_uri)< 0)
	{
		LM_ERR("while constructing uri from user and domain\n");
		goto error;
	}
	
	if(uandd_to_uri(subs->from_user, subs->from_domain, &td->rem_uri)< 0)
	{
		LM_ERR("while constructing uri from user and domain\n");
		goto error;
	}

	if(subs->contact.len ==0 || subs->contact.s == NULL )
	{
		LM_DBG("BAD BAD contact NULL\n");
		td->rem_target = td->rem_uri;
	}
	else
		td->rem_target = subs->contact;

	if(subs->record_route.s && subs->record_route.len)
	{
		if(parse_rr_body(subs->record_route.s, subs->record_route.len,
			&td->route_set)< 0)
		{
			LM_ERR("in function parse_rr_body\n");
			goto error;
		}
	}	
	td->state= DLG_CONFIRMED ;

	if (subs->sockinfo_str.len) {
		int port, proto;
        str host;
		char* tmp;
		if ((tmp = as_asciiz(&subs->sockinfo_str)) == NULL) {
			LM_ERR("no pkg mem left\n");
			goto error;
		}
		if (parse_phostport (tmp,&host.s,
				&host.len,&port, &proto )) {
			LM_ERR("bad sockinfo string\n");
			pkg_free(tmp);
			goto error;
		}
		pkg_free(tmp);
		td->send_sock = grep_sock_info (
			&host, (unsigned short) port, (unsigned short) proto);
	}
	
	return td;

error:
	if(td) rls_free_td(td);	

	return NULL;

}
void rls_notify_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	if(ps->param==NULL || *ps->param==NULL || 
			((dialog_id_t*)(*ps->param)) == NULL)
	{
		LM_DBG("message id not received\n");
		return;
	}
	
	LM_DBG("completed with status %d [to_tag:"
			"%.*s]\n",ps->code,
			((dialog_id_t*)(*ps->param))->to_tag.len, 
			((dialog_id_t*)(*ps->param))->to_tag.s);

	if(ps->code >= 300)
	{
		db_key_t db_keys[2];
		db_val_t db_vals[2];
		unsigned int hash_code;
		subs_t subs;

		memset(&subs, 0, sizeof(subs_t));

		subs.to_tag= ((dialog_id_t*)(*ps->param))->to_tag;
		subs.from_tag= ((dialog_id_t*)(*ps->param))->from_tag;
		subs.callid= ((dialog_id_t*)(*ps->param))->callid;

		if (dbmode != RLS_DB_ONLY)
		{
			/* delete from cache table */
			hash_code= core_hash(&subs.callid, &subs.to_tag , hash_size);

			if(pres_delete_shtable(rls_table,hash_code, &subs)< 0)
			{
				LM_ERR("record not found in hash table\n");
			}
	
			/* delete from database table */
			if (rls_dbf.use_table(rls_db, &rlsubs_table) < 0) 
			{
				LM_ERR("in use_table\n");
				goto done;
			}
		
			db_keys[0] =&str_to_tag_col;
			db_vals[0].type = DB1_STR;
			db_vals[0].nul = 0;
			db_vals[0].val.str_val = subs.to_tag;

			db_keys[1] =&str_callid_col;
			db_vals[1].type = DB1_STR;
			db_vals[1].nul = 0;
			db_vals[1].val.str_val = subs.callid;


			if (rls_dbf.delete(rls_db, db_keys, 0, db_vals, 2) < 0) 
				LM_ERR("cleaning expired messages\n");	
		}
		else
		{
			if (delete_rlsdb(&subs.callid, &subs.to_tag, NULL) < 0 )
			{
				LM_ERR( "unable to delete record from DB\n" );
			}
		}
	}	

done:	
	if(*ps->param !=NULL  )
		shm_free(*ps->param);
	return ;

}

int process_list_and_exec(xmlNodePtr list_node, str username, str domain,
		list_func_t function, void* param)
{
	xmlNodePtr node;
	str uri;
	int res = 0;

	for(node= list_node->children; node; node= node->next)
	{
		if(xmlStrcasecmp(node->name,(unsigned char*)"resource-list")==0)
		{
			str hostname, rl_uri;
			unsigned short port = 0;
			xmlNodePtr rl_node = NULL;
			xmlDocPtr rl_doc = NULL;
			str unescaped_uri;
			char buf[MAX_URI_SIZE];
			

			uri.s = XMLNodeGetNodeContentByName(node, "resource-list", NULL);
			if (uri.s == NULL)
			{
				LM_ERR("when extracting URI from node\n");
				return -1;
			}
			uri.len = strlen(uri.s);
			if (uri.len > MAX_URI_SIZE-1) {
			    LM_ERR("XCAP URI is too long\n");
			    xmlFree(uri.s);
			    return -1;
			}
			LM_DBG("got resource-list uri <%.*s>\n", uri.len, uri.s);

			unescaped_uri.s = buf;
			unescaped_uri.len = 0;
			if (un_escape(&uri, &unescaped_uri) < 0) {
			    LM_ERR("Error un-escaping XCAP URI\n");
			    xmlFree(uri.s);
			    return -1;
			}
			unescaped_uri.s[unescaped_uri.len] = 0;
			LM_DBG("got unescaped uri <%s>\n", unescaped_uri.s);

			if(parse_xcap_uri(unescaped_uri.s, &hostname, &port, &rl_uri)>0)
			{
				if (rls_integrated_xcap_server == 1
					&& (hostname.len == 0
						|| check_self(&hostname, 0, PROTO_NONE) == 1))
				{
					LM_DBG("fetching local <resource-list - %.*s>\n", uri.len, uri.s);
					if (rls_get_resource_list(&rl_uri, &username, &domain, &rl_node, &rl_doc)>0)
					{
						LM_DBG("calling myself for rl_node\n");
						res = process_list_and_exec(rl_node, username, domain, function, param);
						xmlFree(uri.s);
						xmlFreeDoc(rl_doc);
					}
					else
					{
						LM_ERR("<resource-list - %.*s> not found\n", uri.len, uri.s);
						xmlFree(uri.s);
						return -1;
					}
					
				}
				else
				{
					LM_ERR("<resource-list - %.*s> is not local - unsupported at this time\n", uri.len, uri.s);
					xmlFree(uri.s);
					return -1;
				}
			}
			else
			{
				LM_ERR("unable to parse URI for <resource-list - %.*s>\n", uri.len, uri.s);
				xmlFree(uri.s);
				return -1;
			}
		}
		else
                if(xmlStrcasecmp(node->name,(unsigned char*)"entry")== 0)
		{
			uri.s = XMLNodeGetAttrContentByName(node, "uri");
			if(uri.s== NULL)
			{
				LM_ERR("when extracting entry uri attribute\n");
				return -1;
			}
			LM_DBG("uri= %s\n", uri.s);
			if(function(uri.s, param)< 0)
			{
				LM_ERR("in function given as a parameter\n");
				xmlFree(uri.s);
				return -1;
			}
			xmlFree(uri.s);
		}
		else
		if(xmlStrcasecmp(node->name,(unsigned char*)"list")== 0)
			res = process_list_and_exec(node, username, domain, function, param);
	}
	return res;
}

char* generate_string(int length)
{
	static char buf[128];
	int r,i;

	if(length>= 128)
	{
		LM_ERR("requested length exceeds buffer size\n");
		return NULL;
	}
		
	for(i=0; i<length; i++) 
	{
		r= rand() % ('z'- 'A') + 'A';
		if(r>'Z' && r< 'a')
		r= '0'+ (r- 'Z');

		sprintf(buf+i, "%c", r);
	}
	buf[length]= '\0';

	return buf;
}

char* generate_cid(char* uri, int uri_len)
{
	static char cid[512];
	int len;

	len= sprintf(cid, "%d.%.*s.%d", (int)time(NULL), uri_len, uri, rand());
	cid[len]= '\0';
	
	return cid;
}

char* get_auth_string(int flag)
{
	switch(flag)
	{
		case ACTIVE_STATE:     return "active";
		case PENDING_STATE:    return "pending";
		case TERMINATED_STATE: return "terminated";
	}
	return NULL;
}

#define HTTP_PREFIX		"http://"
#define HTTP_PREFIX_LEN		7
#define HTTP_PORT_DEFAULT	80
#define HTTPS_PREFIX		"https://"
#define HTTPS_PREFIX_LEN	8
#define HTTPS_PORT_DEFAULT	443
#define LOCAL_PREFIX		"/"
#define LOCAL_PREFIX_LEN	1
#define CHAR_PORT_LEN		5

int parse_xcap_uri(char *uri, str *host, unsigned short *port, str *path)
{
	host->s = NULL;
	host->len = 0;
	*port = 0;
	path->s = NULL;
	path->len = 0;		
	
	if(strncmp(uri, HTTP_PREFIX, HTTP_PREFIX_LEN) == 0)
	{
		host->s = &uri[HTTP_PREFIX_LEN];
		*port = HTTP_PORT_DEFAULT;
		LM_DBG("resource list is on http server\n");
	}
	else
	if(strncmp(uri, HTTPS_PREFIX, HTTPS_PREFIX_LEN) == 0)
	{
		host->s = &uri[HTTPS_PREFIX_LEN];
		*port = HTTPS_PORT_DEFAULT;
		LM_DBG("resource list is on https server\n");
	}
	else
	if(strncmp(uri, LOCAL_PREFIX, LOCAL_PREFIX_LEN) == 0)
	{
		path->s = &uri[0];
		LM_DBG("resource list is local\n");
	}
	else
	{
		LM_ERR("resource list is unidentifiable\n");
		return -1;
	}

	if (host->s != NULL)
	{
		while(host->s[host->len] != '\0' && host->s[host->len] != ':' && host->s[host->len] != '/') host->len++;

		if (host->s[host->len] == ':')
		{
			char char_port[CHAR_PORT_LEN + 1];
			unsigned cur_pos = host->len + 1;
			memset(char_port, '\0', CHAR_PORT_LEN + 1);

			while(host->s[cur_pos] != '\0' && host->s[cur_pos] != '/') cur_pos++;
			strncpy(char_port, &host->s[host->len + 1], MIN(cur_pos - host->len - 1, CHAR_PORT_LEN));
			*port = atoi(char_port);

			path->s = &host->s[cur_pos];
		}
		else
		{
			path->s = &host->s[host->len];
		}
	}

	while(path->s[path->len] != '\0') path->len++;

	return 1;
}

#define MAX_PATH_LEN	127
int rls_get_resource_list(str *rl_uri, str *username, str *domain,
		xmlNodePtr *rl_node, xmlDocPtr *xmldoc)
{
	db_key_t query_cols[4];
	db_val_t query_vals[4];
	int n_query_cols = 0;
	db_key_t result_cols[1];
	int n_result_cols = 0;
	db1_res_t *result = 0;
	db_row_t *row;
	db_val_t *row_vals;
	int xcap_col;
	str body;
	int checked = 0;
	str root, path = {0, 0};
	char path_str[MAX_PATH_LEN + 1];
	xmlXPathContextPtr xpathCtx = NULL;
	xmlXPathObjectPtr xpathObj = NULL;


	if (rl_uri==NULL || username==NULL || domain==NULL)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	LM_DBG("rl_uri: %.*s", rl_uri->len, rl_uri->s);

	root.s = rl_uri->s;
	root.len = rl_uri->len;
	while (checked < rl_uri->len)
	{
		if (checked < rl_uri->len - 3 && strncmp(rl_uri->s + checked, "/~~", 3) == 0)
		{
			root.len = checked;
			checked += 3;
			break;
		}
		checked++;
	}
	LM_DBG("doc: %.*s", root.len, root.s);

	memset (path_str, '\0', MAX_PATH_LEN + 1);
	path.s = path_str;
	path.len = 0;
	while (checked < rl_uri->len && path.len <= MAX_PATH_LEN)
	{
		if (rl_uri->s[checked] == '/')
		{
			strcat(path.s, "/xmlns:");
			path.len += 7;
			checked++;
		}
		else
		{
			path.s[path.len++] = rl_uri->s[checked];
			checked++;
		}
	}
	LM_DBG("path: %.*s", path.len, path.s);

	query_cols[n_query_cols] = &str_username_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *username;
	n_query_cols++;

	query_cols[n_query_cols] = &str_domain_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *domain;
	n_query_cols++;

	query_cols[n_query_cols] = &str_doc_type_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val = RESOURCE_LIST;
	n_query_cols++;

	query_cols[n_query_cols] = &str_doc_uri_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = root;
	n_query_cols++;

	if(rls_xcap_dbf.use_table(rls_xcap_db, &rls_xcap_table) < 0)
	{
		LM_ERR("in use_table-[table]=%.*s\n",
			rls_xcap_table.len, rls_xcap_table.s);
		return -1;
	}

	result_cols[xcap_col= n_result_cols++] = &str_doc_col;

	if(rls_xcap_dbf.query(rls_xcap_db, query_cols, 0, query_vals, result_cols,
				n_query_cols, n_result_cols, 0, &result)<0)
	{
		LM_ERR("failed querying table xcap for document: %.*s\n",
				root.len, root.s);
		if(result)
			rls_xcap_dbf.free_result(rls_xcap_db, result);
		return -1;
	}

	if(result == NULL)
	{
		LM_ERR("bad result\n");
		return -1;
	}

	if(result->n<=0)
	{
		LM_DBG("No rl document found\n");
		rls_xcap_dbf.free_result(rls_xcap_db, result);
		return -1;
	}

	row = &result->rows[0];
	row_vals = ROW_VALUES(row);

	body.s = (char*)row_vals[xcap_col].val.string_val;
	if(body.s==NULL)
	{
		LM_ERR("xcap doc is null\n");
		goto error;
	}
	body.len = strlen(body.s);
	if(body.len==0)
	{
		LM_ERR("xcap doc is empty\n");
		goto error;
	}

	LM_DBG("rl document:\n%.*s", body.len, body.s);
	*xmldoc = xmlParseMemory(body.s, body.len);
	if(*xmldoc==NULL)
	{
		LM_ERR("while parsing XML memory. len = %d\n", body.len);
		goto error;
	}

	if (path.len == 0)
	{
		/* No path specified - use all resource-lists. */
		*rl_node = XMLDocGetNodeByName(*xmldoc,"resource-lists", NULL);
		if(*rl_node==NULL)
		{
			LM_ERR("no resource-lists node in XML document\n");
			goto error;
		}
	}
	else if (path.s != NULL)
	{
		xpathCtx = xmlXPathNewContext(*xmldoc);
		if (xpathCtx == NULL)
		{
			LM_ERR("unable to create new XPath context");
			goto error;
		}

		if (xmlXPathRegisterNs(xpathCtx, BAD_CAST "xmlns", BAD_CAST "urn:ietf:params:xml:ns:resource-lists") != 0)
		{
			LM_ERR("unable to register xmlns\n");
			goto error;
		}

		xpathObj = xmlXPathEvalExpression(BAD_CAST path.s, xpathCtx);
		if (xpathObj == NULL)
		{
			LM_ERR("unable to evaluate path\n");
			goto error;
		}

		if (xpathObj->nodesetval == NULL || xpathObj->nodesetval->nodeNr <= 0)
		{
			LM_ERR("no nodes found\n");
			goto error;
		}
		if (xpathObj->nodesetval->nodeTab[0] != NULL && xpathObj->nodesetval->nodeTab[0]->type != XML_ELEMENT_NODE)
		{
			LM_ERR("no nodes of the correct type found\n");
			goto error;

		}

		*rl_node = xpathObj->nodesetval->nodeTab[0];

		xmlXPathFreeObject(xpathObj);
		xmlXPathFreeContext(xpathCtx);
	}
	
	rls_xcap_dbf.free_result(rls_xcap_db, result);
	return 1;

error:
	if(result!=NULL)
		rls_xcap_dbf.free_result(rls_xcap_db, result);
	if(xpathObj!=NULL)
		xmlXPathFreeObject(xpathObj);
	
	if(xpathCtx!=NULL)
		xmlXPathFreeContext(xpathCtx);
	if(xmldoc!=NULL)
		xmlFreeDoc(*xmldoc);

	return -1;
}

/*
 * $Id: resource_notify.c 2230 2007-06-06 07:13:20Z anca_vamanu $
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2007-09-11  initial version (anca)
 */

#include <stdio.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../lib/kcore/hash_func.h"
#include "../../trim.h"
#include "../pua/hash.h"
#include "rls.h"
#include "notify.h"
#include "resource_notify.h"

/* how to relate resource oriented dialogs to list_uri */
/* sol1: use the same callid in Subscribe requests 
 * sol2: include an extra header
 * sol3: put the list_uri as the id of the record stored in
 * pua and write a function to return that id
 * winner: sol3
 * */
static str su_200_rpl     = str_init("OK");

#define CONT_COPY(buf, dest, source)\
	dest.s= (char*)buf+ size;\
	memcpy(dest.s, source.s, source.len);\
	dest.len= source.len;\
	size+= source.len;


#define CONT_COPY_1 (buf, dest_s, dest_len, source_s, source_len)\
	dest_s= (char*)buf+ size;\
	memcpy(dest_s, source_s, source_len);\
	dest_len= source_len;\
	size+= source_len;

int parse_rlsubs_did(char* str_did, str* callid, str* from_tag, str* to_tag)
{
	char* smc= NULL;

	smc= strstr(str_did, RLS_DID_SEP);
    if(smc== NULL)
    {
        LM_ERR("bad format for resource list Subscribe dialog"
            " indentifier[rlsubs did]= %s\n", str_did);
        return -1;
    }
	callid->s= str_did;
	callid->len= smc- str_did;
			
	from_tag->s= smc+ RLS_DID_SEP_LEN;
	smc= strstr(from_tag->s, RLS_DID_SEP);
	if(smc== NULL)
    {
        LM_ERR("bad format for resource list Subscribe dialog"
            " indentifier(rlsubs did)= %s\n", str_did);
        return -1;
    }
	from_tag->len= smc- from_tag->s;
		
	to_tag->s= smc+ RLS_DID_SEP_LEN;
	to_tag->len= strlen(str_did)- 2* RLS_DID_SEP_LEN- callid->len- from_tag->len;

	return 0;
}


void get_dialog_from_did(char* did, subs_t **dialog, unsigned int *hash_code)
{
    str callid, to_tag, from_tag;
    subs_t* s;
    
    *dialog= NULL;

    /* search the subscription in rlsubs_table*/		
    if( parse_rlsubs_did(did, &callid, &from_tag, &to_tag)< 0)
	{
        LM_ERR("bad format for "
            "resource list Subscribe dialog indentifier(rlsubs did)\n");
        return;
	}
    *hash_code= core_hash(&callid, &to_tag, hash_size);
    
    lock_get(&rls_table[*hash_code].lock);
    s= pres_search_shtable(rls_table,callid,to_tag,from_tag,*hash_code);
    if(s== NULL)
	{
        LM_ERR("record not found in hash_table [rlsubs_did]= %s\n",
                did);
        lock_release(&rls_table[*hash_code].lock);
        return;
	}

    /* save dialog info */
    *dialog= pres_copy_subs(s, PKG_MEM_TYPE);
    if(*dialog== NULL)
	{
        LM_ERR("while copying subs_t structure\n");
	}
    lock_release(&rls_table[*hash_code].lock);
	
}

int send_notify(xmlDocPtr * rlmi_doc, char * buf, int buf_len, 
                 const str bstr, subs_t * dialog, unsigned int hash_code)
{
    int result = 0;
    str rlmi_cont= {0, 0}, multi_cont;

    xmlDocDumpFormatMemory(*rlmi_doc,(xmlChar**)(void*)&rlmi_cont.s,
				&rlmi_cont.len, 0);
		
    multi_cont.s= buf;
    multi_cont.len= buf_len;

    result =agg_body_sendn_update(&dialog->pres_uri, bstr.s, &rlmi_cont, 
                 (buf_len==0)?NULL:&multi_cont, dialog, hash_code);
    xmlFree(rlmi_cont.s);
    xmlFreeDoc(*rlmi_doc);
    *rlmi_doc= NULL;
    return result;
}


void send_notifies(db1_res_t *result, int did_col, int resource_uri_col, int auth_state_col, int reason_col,
                   int pres_state_col, int content_type_col, subs_t* subscription, int external_hash)
{
    int i;
	char* prev_did= NULL, * curr_did= NULL;
	db_row_t *row;	
	db_val_t *row_vals;
	char* resource_uri;
	str pres_state = {0, 0};
	xmlDocPtr rlmi_doc= NULL;
	xmlNodePtr list_node= NULL, instance_node= NULL, resource_node;
	unsigned int hash_code= 0;
	int size= BUF_REALLOC_SIZE, buf_len= 0;	
	char* buf= NULL, *auth_state= NULL, *boundary_string= NULL;
	str cid = {0,0};
	str content_type= {0, 0};
	int contor= 0, auth_state_flag;
	int chunk_len=0;
	str bstr= {0, 0};
	subs_t* dialog= NULL;
    int len_est = 0;
    int resource_added = 0; /* Flag to indicate that we have added at least one resource */

	/* generate the boundary string */
    boundary_string= generate_string((int)time(NULL), BOUNDARY_STRING_LEN);
	bstr.len= strlen(boundary_string);
	bstr.s= (char*)pkg_malloc((bstr.len+ 1)* sizeof(char));
	if(bstr.s== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memcpy(bstr.s, boundary_string, bstr.len);
	bstr.s[bstr.len]= '\0';

	/* Allocate an initial buffer for the multipart body.
	 * This buffer will be reallocated if neccessary */
	buf= pkg_malloc(size* sizeof(char));
	if(buf== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

    if (subscription == NULL)
	{
        dialog = subscription;
        /* If we have been given a subscription to use
           We should also have been given a hash value. */
        hash_code = external_hash;
	}

	LM_DBG("found %d records with updated state\n", result->n);
	for(i= 0; i< result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);
		
		curr_did=     (char*)row_vals[did_col].val.string_val;
        resource_uri= (char*)row_vals[resource_uri_col].val.string_val;
		auth_state_flag=     row_vals[auth_state_col].val.int_val;
		pres_state.s=   (char*)row_vals[pres_state_col].val.string_val;
		pres_state.len = strlen(pres_state.s);
		trim(&pres_state);
		
        /* If we have moved onto a new resource list Subscribe dialog indentifier, 
           Send a NOTIFY for the previous ID and then drop the existing documents. */
		if(prev_did!= NULL && strcmp(prev_did, curr_did)) 
		{
            if (send_notify(&rlmi_doc, buf, buf_len, bstr, dialog, hash_code))
            {  
                LM_ERR("in send_notify\n");
                goto error;
            }
            len_est = 0;
            
            if (subscription == NULL)
            {
                pkg_free(dialog);
                dialog= NULL;
            }
        }

		/*if first or different*/
		if(prev_did==NULL || strcmp(prev_did, curr_did)!=0)
        {
            if (subscription == NULL)
            {
                /* We have not been given a subscription
                   Work it out from the did. */
                get_dialog_from_did(curr_did, &dialog, &hash_code);
                if(dialog== NULL)
                {
                    prev_did = NULL;
                    LM_ERR("Dialog is NULL\n");
                    continue;
                }
            }
		
            len_est = create_empty_rlmi_doc(&rlmi_doc, &list_node, &dialog->pres_uri, dialog->version, 0);
            len_est += 2*strlen(boundary_string)+4+102+2+50+strlen(resource_uri)+20;
			buf_len= 0;
            resource_added = 0;

			/* !!!! for now I will include the auth state without checking if 
			 * it has changed - > in future chech if it works */		
        }

		/* add a node in rlmi_doc and if any presence state registered add 
		 * it in the buffer */
		
		resource_node= xmlNewChild(list_node,NULL,BAD_CAST "resource", NULL);
		if(resource_node== NULL)
        {
			LM_ERR("when adding resource child\n");
			goto done;
        }
		xmlNewProp(resource_node, BAD_CAST "uri", BAD_CAST resource_uri);
        len_est += strlen (resource_uri) + 35; /* <resource uri="[uri]"></resource>/r/n */
        resource_added = 1;

		/* there might be more records with the same uri- more instances-
		 * search and add them all */
		
		contor= 0;
		while(1)
		{
			contor++;
			cid.s= NULL;
			cid.len= 0;
			
			auth_state= get_auth_string(auth_state_flag);
			if(auth_state== NULL)
            {
				LM_ERR("bad authorization status flag\n");
                goto error;
            }	
			len_est += strlen(auth_state) + 38; /* <instance id="12345678" state="[auth_state]" />r/n */

			if(auth_state_flag & ACTIVE_STATE)
            {
				cid.s= generate_cid(resource_uri, strlen(resource_uri));
                cid.len = strlen(cid.s);
                len_est += cid.len + 8; /* cid="[cid]" */
				content_type.s = (char*)row_vals[content_type_col].val.string_val;
				content_type.len = strlen(content_type.s);
				chunk_len = 4 + bstr.len
							+ 35
							+ 16 + cid.len
							+ 18 + content_type.len
							+ 4 + pres_state.len + 8;
                len_est += chunk_len;
            }
			else
			if(auth_state_flag & TERMINATED_STATE)
            {
			    len_est += strlen(row_vals[resource_uri_col].val.string_val) + 10; /* reason="[resaon]" */
            }
            
            if (rls_max_notify_body_len > 0 && len_est > rls_max_notify_body_len)
            {
                /* We have a limit on body length set, and we were about to exceed it */
                if (resource_added == 1)
                {
                    /* We added at least one resource. */
                    LM_ERR("timer_send_notify hit the size limit. len_est = %d\n", len_est);
                    if (send_notify(&rlmi_doc, buf, buf_len, bstr, dialog, hash_code))
                    {
                        LM_ERR("in send_notify\n");
                        goto error;
                    }
                    i --;
                }
                else
                {
                    LM_ERR("timer_send_notify hit the size limit. NO RESOURCE ADDED len_est = %d\n", len_est);
                }
                len_est = 0;

                if (subscription == NULL)
                {
                    pkg_free(dialog);
                    dialog= NULL;
                }
                
                curr_did=NULL;
                break;
            }

            /* OK, we are happy this will fit */
            instance_node= xmlNewChild(resource_node, NULL, BAD_CAST "instance", NULL);
            if(instance_node== NULL)
            {
				LM_ERR("while adding instance child\n");
				goto error;
            }	

            xmlNewProp(instance_node, BAD_CAST "id", 
					BAD_CAST generate_string(contor, 8));
            if(auth_state_flag & ACTIVE_STATE)
            {
                xmlNewProp(instance_node, BAD_CAST "state", BAD_CAST auth_state);
            }
			else
			if(auth_state_flag & TERMINATED_STATE)
            {
            	xmlNewProp(instance_node, BAD_CAST "reason",
					BAD_CAST row_vals[resource_uri_col].val.string_val);
            }
            xmlNewProp(instance_node, BAD_CAST "cid", BAD_CAST cid.s);

			/* add in the multipart buffer */
			if(cid.s)
			{
	
				if(buf_len + chunk_len >= size)
				{
					REALLOC_BUF
				}
				buf_len+= sprintf(buf+ buf_len, "--%.*s\r\n", bstr.len,
						bstr.s);
				buf_len+= sprintf(buf+ buf_len,
						"Content-Transfer-Encoding: binary\r\n");
				buf_len+= sprintf(buf+ buf_len, "Content-ID: <%.*s>\r\n",
						cid.len, cid.s);
				buf_len+= sprintf(buf+ buf_len, "Content-Type: %.*s\r\n\r\n",
						content_type.len, content_type.s);
				buf_len+= sprintf(buf+buf_len,"%.*s\r\n\r\n", pres_state.len,
						pres_state.s);
			}

			i++;
			if(i== result->n)
			{
				i--;
				break;
			}
	
			row = &result->rows[i];
			row_vals = ROW_VALUES(row);

			if(strncmp(resource_uri, row_vals[resource_uri_col].val.string_val,
					strlen(resource_uri))
				|| strncmp(curr_did, row_vals[did_col].val.string_val,
					strlen(curr_did)))
			{
				i--;
				break;
			}
			resource_uri= (char*)row_vals[resource_uri_col].val.string_val;
			auth_state_flag=     row_vals[auth_state_col].val.int_val;
			pres_state.s=   (char*)row_vals[pres_state_col].val.string_val;
			pres_state.len= strlen(pres_state.s);
			trim(&pres_state);
		}

		prev_did= curr_did;
	}

	if(rlmi_doc)
	{
        LM_DBG("timer_send_notify at end len_est = %d resource_added = %d\n", len_est, resource_added);
        if (resource_added == 1)
        {
            send_notify(&rlmi_doc, buf, buf_len, bstr, dialog, hash_code);
        }
        if (subscription == NULL)
        {
            /* We are using a derived subscription */
            if(dialog)
            {
                pkg_free(dialog);
            }
            dialog= NULL;
        }
	}

	
error:
done:
	if(bstr.s)
		pkg_free(bstr.s);

	if(buf)
		pkg_free(buf);
    if (subscription == NULL)
	{
        /* We are using a derived subscription */
        if(dialog)
            pkg_free(dialog);
    }
	return;
}


int parse_subs_state(str auth_state, str** reason, int* expires)
{
	str str_exp;
	str* res= NULL;
	char* smc= NULL;
	int len, flag= -1;


	if( strncmp(auth_state.s, "active", 6)== 0)
		flag= ACTIVE_STATE;

	if( strncmp(auth_state.s, "pending", 7)== 0)
		flag= PENDING_STATE; 

	if( strncmp(auth_state.s, "terminated", 10)== 0)
	{
		smc= strchr(auth_state.s, ';');
		if(smc== NULL)
		{
			LM_ERR("terminated state and no reason found");
			return -1;
		}
		if(strncmp(smc+1, "reason=", 7))
		{
			LM_ERR("terminated state and no reason found");
			return -1;
        }
		res= (str*)pkg_malloc(sizeof(str));
		if(res== NULL)
        {
			ERR_MEM(PKG_MEM_STR);
		}
		len=  auth_state.len- 10- 1- 7;
		res->s= (char*)pkg_malloc(len* sizeof(char));
		if(res->s== NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(res->s, smc+ 8, len);
		res->len= len;
		return TERMINATED_STATE;
	}
	
	if(flag> 0)
	{
		smc= strchr(auth_state.s, ';');
		if(smc== NULL)
		{
			LM_ERR("active or pending state and no expires parameter found");
			return -1;
        }	
		if(strncmp(smc+1, "expires=", 8))
		{
			LM_ERR("active or pending state and no expires parameter found");
			return -1;
		}
		str_exp.s= smc+ 9;
		str_exp.len= auth_state.s+ auth_state.len- smc- 9;

		if( str2int(&str_exp, (unsigned int*)expires)< 0)
        {
			LM_ERR("while getting int from str\n");
			return -1;
        }
		return flag;
	
	}
	return -1;

error:
	if(res)
	{
		if(res->s)
			pkg_free(res->s);
		pkg_free(res);
	}
	return -1;
}

int rls_handle_notify(struct sip_msg* msg, char* c1, char* c2)
{
	struct to_body *pto, TO, *pfrom= NULL;
	str body= {0, 0};
	ua_pres_t dialog;
	str* res_id= NULL;
	db_key_t query_cols[9], result_cols[1];
	db_val_t query_vals[9];
	db1_res_t* result= NULL;
	int n_query_cols= 0;
	str auth_state= {0, 0};
	int found= 0;
	str* reason= NULL;
	int auth_flag;
	struct hdr_field* hdr= NULL;
	int n, expires= -1;
	str content_type= {0, 0};


	LM_DBG("start\n");
	/* extract the dialog information and check if an existing dialog*/	
	if( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		return -1;
	}
	if((!msg->event ) ||(msg->event->body.len<=0))
	{
		LM_ERR("Missing event header field value\n");
		return -1;
	}
	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		return -1;
    }
	if(msg->to->parsed != NULL)
    {
		pto = (struct to_body*)msg->to->parsed;
		LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",
				pto->uri.len, pto->uri.s );	
	}
	else
	{
		memset( &TO , 0, sizeof(TO) );
		parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO);
		if(TO.uri.len <= 0) 
		{
			LM_ERR(" 'To' header NOT parsed\n");
			return -1;
	}
		pto = &TO;
	}
	memset(&dialog, 0, sizeof(ua_pres_t));
	dialog.watcher_uri= &pto->uri;
    if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
	{
		LM_ERR("to tag value not parsed\n");
		goto error;
	}
	dialog.from_tag= pto->tag_value;
	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LM_ERR("cannot parse callid header\n");
		goto error;
	}
	dialog.call_id = msg->callid->body;

	if (!msg->from || !msg->from->body.s)
	{
		LM_ERR("cannot find 'from' header!\n");
		goto error;
	}
	if (msg->from->parsed == NULL)
	{
		LM_DBG("'From' header not parsed\n");
		/* parsing from header */
		if ( parse_from_header( msg )<0 ) 
		{
			LM_ERR("cannot parse From header\n");
            goto error;
        }
	}
	pfrom = (struct to_body*)msg->from->parsed;
	dialog.pres_uri= &pfrom->uri;

	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LM_ERR("no from tag value present\n");
		goto error;
	}
	dialog.to_tag= pfrom->tag_value;
	dialog.flag|= RLS_SUBSCRIBE;

	dialog.event= get_event_flag(&msg->event->body);
	if(dialog.event< 0)
	{
		LM_ERR("unrecognized event package\n");
		goto error;
	}

	/* extract the subscription state */
	hdr = msg->headers;
	while (hdr!= NULL)
	{
		if(cmp_hdrname_strzn(&hdr->name, "Subscription-State", 18)==0)  
		{
			found = 1;
			break;
		}
		hdr = hdr->next;
	}
	if(found==0 )
	{
		LM_ERR("'Subscription-State' header not found\n");
		goto error;
	}
	auth_state = hdr->body;

	/* extract state and reason */
	auth_flag= parse_subs_state(auth_state, &reason, &expires);
	if(auth_flag< 0)
	{
		LM_ERR("while parsing 'Subscription-State' header\n");
		goto error;
	}
	if(pua_get_record_id(&dialog, &res_id)< 0) /* verify if within a stored dialog */
	{
		LM_ERR("occured when trying to get record id\n");
		goto error;
	}
	if(res_id==0)
	{
		LM_DBG("presence dialog record not found\n");
		/* if it is a NOTIFY for a terminated SUBSCRIBE dialog in RLS, then
		 * the module might not have the dialog structure anymore
		 * - just send 200ok, it is harmless
		 */
		if(auth_flag==TERMINATED_STATE)
			goto done;
        LM_ERR("no presence dialog record for non-TERMINATED state uri pres_uri = %.*s watcher_uri = %.*s\n",
                dialog.pres_uri->len, dialog.pres_uri->s, dialog.watcher_uri->len, dialog.watcher_uri->s);
		goto error;
	}
		
	if(msg->content_type== NULL || msg->content_type->body.s== NULL)
	{
		LM_DBG("cannot find content type header header\n");
	}
	else
		content_type= msg->content_type->body;
					
	/*constructing the xml body*/
	if(get_content_length(msg) == 0 )
    {	
        goto done;
    }	
	else
	{
		if(content_type.s== 0)
		{
			LM_ERR("content length != 0 and no content type header found\n");
			goto error;
		}
		body.s=get_body(msg);
		if (body.s== NULL) 
		{
			LM_ERR("cannot extract body from msg\n");
			goto error;
		}
		body.len = get_content_length( msg );

	}
	/* update in rlpres_table where rlsusb_did= res_id and resource_uri= from_uri*/

	LM_DBG("body= %.*s\n", body.len, body.s);

	query_cols[n_query_cols]= &str_rlsubs_did_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *res_id; 
	n_query_cols++;

	query_cols[n_query_cols]= &str_resource_uri_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= *dialog.pres_uri; 
	n_query_cols++;

	query_cols[n_query_cols]= &str_updated_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= UPDATED_TYPE; 
	n_query_cols++;
		
	query_cols[n_query_cols]= &str_auth_state_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= auth_flag; 
	n_query_cols++;

	if(reason)
	{
		query_cols[n_query_cols]= &str_reason_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val= *reason;
		n_query_cols++;
	}
	query_cols[n_query_cols]= &str_content_type_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= content_type;
	n_query_cols++;
			
	query_cols[n_query_cols]= &str_presence_state_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val= body;
	n_query_cols++;
		
	query_cols[n_query_cols]= &str_expires_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= expires+ (int)time(NULL);
	n_query_cols++;

	if (rls_dbf.use_table(rls_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}
	/* query-> if not present insert // else update */
	result_cols[0]= &str_updated_col;
			
	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols,
					2, 1, 0, &result)< 0)
	{
		LM_ERR("in sql query\n");
		if(result)
			rls_dbf.free_result(rls_db, result);
		goto error;
	}
	if(result== NULL)
		goto error;
	n= result->n;
	rls_dbf.free_result(rls_db, result);
		
	if(n<= 0)
	{
		if(rls_dbf.insert(rls_db, query_cols, query_vals, n_query_cols)< 0)
		{
			LM_ERR("in sql insert\n");
			goto error;
		}
		LM_DBG("Inserted in database table new record\n");
	}
	else
	{
		LM_DBG("Updated in db table already existing record\n");
		if(rls_dbf.update(rls_db, query_cols, 0, query_vals, query_cols+2,
						query_vals+2, 2, n_query_cols-2)< 0)
		{
			LM_ERR("in sql update\n");
			goto error;
		}
	}

	LM_DBG("Updated rlpres_table\n");	
	/* reply 200OK */
done:
	if(slb.freply(msg, 200, &su_200_rpl) < 0)
	{
		LM_ERR("while sending reply\n");
		goto error;
	}

	if(res_id!=NULL)
	{
		pkg_free(res_id->s);
		pkg_free(res_id);
	}
	return 1;

error:
	if(res_id!=NULL)
	{
		pkg_free(res_id->s);
		pkg_free(res_id);
	}
	return -1;
	}
/* callid, from_tag, to_tag parameters must be allocated */

void timer_send_notify(unsigned int ticks,void *param)
{
	db_key_t query_cols[2], update_cols[1], result_cols[7];
	db_val_t query_vals[2], update_vals[1];
	int did_col, resource_uri_col, auth_state_col, reason_col,
		pres_state_col, content_type_col;
	int n_result_cols= 0;
	db1_res_t *result= NULL;
		
	query_cols[0]= &str_updated_col;
	query_vals[0].type = DB1_INT;
	query_vals[0].nul = 0;
	query_vals[0].val.int_val= UPDATED_TYPE; 

	result_cols[did_col= n_result_cols++]= &str_rlsubs_did_col;
	result_cols[resource_uri_col= n_result_cols++]= &str_resource_uri_col;
	result_cols[auth_state_col= n_result_cols++]= &str_auth_state_col;
	result_cols[content_type_col= n_result_cols++]= &str_content_type_col;
	result_cols[reason_col= n_result_cols++]= &str_reason_col;
	result_cols[pres_state_col= n_result_cols++]= &str_presence_state_col;

	update_cols[0]= &str_updated_col;
	update_vals[0].type = DB1_INT;
	update_vals[0].nul = 0;
	update_vals[0].val.int_val= NO_UPDATE_TYPE; 

	/* query in alphabetical order after rlsusbs_did 
	 * (resource list Subscribe dialog indentifier)*/
	
	if (rls_dbf.use_table(rls_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto done;
	}

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols,
					1, n_result_cols, &str_rlsubs_did_col, &result)< 0)
	{
		LM_ERR("in sql query\n");
		goto done;
	}
	if(result== NULL || result->n<= 0)
		goto done;

	/* update the rlpres table */
	if(rls_dbf.update(rls_db, query_cols, 0, query_vals, update_cols,
					update_vals, 1, 1)< 0)
	{
		LM_ERR("in sql update\n");
		goto error;
	}

    send_notifies(result, did_col, resource_uri_col, auth_state_col, reason_col,
                  pres_state_col, content_type_col,
                  NULL, 0);  /* Work out the subscription */
error:
done:
	if(result)
		rls_dbf.free_result(rls_db, result);
}


/* function to periodicaly clean the rls_presentity table */

void rls_presentity_clean(unsigned int ticks,void *param)
{
	db_key_t query_cols[2];
	db_op_t query_ops[2];
	db_val_t query_vals[2];

	query_cols[0]= &str_expires_col;
	query_ops[0]= OP_LT;
	query_vals[0].nul= 0;
	query_vals[0].type= DB1_INT;
	query_vals[0].val.int_val= (int)time(NULL) - 10;

	if (rls_dbf.use_table(rls_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		return ;
	}

	if(rls_dbf.delete(rls_db, query_cols, query_ops, query_vals, 1)< 0)
	{
		LM_ERR("in sql delete\n");
		return ;
	}

}

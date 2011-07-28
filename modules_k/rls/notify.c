/*
 * $Id: notify.c 2230 2007-06-06 07:13:20Z anca_vamanu $
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
#include "../../lib/kcore/hash_func.h"
#include "rls.h"
#include "notify.h"

typedef struct res_param
{
	xmlNodePtr list_node;
	db1_res_t* db_result;
	char** cid_array;
}res_param_t;

int resource_uri_col=0, content_type_col, pres_state_col= 0,
	auth_state_col= 0, reason_col= 0;

str* constr_rlmi_doc(db1_res_t* result, str* rl_uri, int version,
		xmlNodePtr rl_node, char*** cid_array,
		str username, str domain);
str* constr_multipart_body(db1_res_t* result,char** cid_array,
		char* boundary_string);

dlg_t* rls_notify_dlg(subs_t* subs);

void rls_notify_callback( struct cell *t, int type, struct tmcb_params *ps);

int parse_xcap_uri(char *uri, str *host, unsigned short *port, str *path);
int rls_get_resource_list(str *rl_uri, str *username, str *domain,
		xmlNodePtr *rl_node, xmlDocPtr *xmldoc);

int send_full_notify(subs_t* subs, xmlNodePtr rl_node, int version, str* rl_uri,
		unsigned int hash_code)
{
	str* rlmi_body= NULL;
	str* multipart_body= NULL;
	db_key_t query_cols[2], update_cols[2], result_cols[7];
	db_val_t query_vals[2], update_vals[2];
	db1_res_t *result= NULL;
	int n_result_cols= 0, i;
	char* boundary_string;
	char** cid_array= NULL;
	str rlsubs_did= {0, 0};

	LM_DBG("start\n");
	/* query in alfabetical order */
	
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
	
	if (rls_dbf.use_table(rls_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols,
					1, n_result_cols, &str_resource_uri_col, &result )< 0)
	{
		LM_ERR("in sql query\n");
		goto error;
	}
	if(result== NULL)
		goto error;

	rlmi_body= constr_rlmi_doc(result, rl_uri, version, rl_node, &cid_array, subs->from_user, subs->from_domain);
	if(rlmi_body== NULL)
	{
		LM_ERR("while constructing rlmi doc\n");
		goto error;
	}

	boundary_string= generate_string((int)time(NULL), BOUNDARY_STRING_LEN);
	
	if(result->n> 0)
	{
		multipart_body= constr_multipart_body(result, cid_array, 
				boundary_string);
		if(multipart_body== NULL)
		{
			LM_ERR("while constructing multipart body\n");
			goto error;
		}
		for(i = 0; i<result->n; i++)
		{
			if(cid_array[i])
				pkg_free(cid_array[i]);
		}
	}
	pkg_free(cid_array);
	cid_array= NULL;
	rls_dbf.free_result(rls_db, result);
	result= NULL;

	if(agg_body_sendn_update(rl_uri, boundary_string, rlmi_body,
		multipart_body, subs, hash_code)< 0)
	{
		LM_ERR("in function agg_body_sendn_update\n");
		goto error;
	}

	/* update updated col in rlpres_table*/
	update_cols[0]= &str_updated_col;
	update_vals[0].type = DB1_INT;
	update_vals[0].nul = 0;
	update_vals[0].val.int_val= NO_UPDATE_TYPE; 
	
	if (rls_dbf.use_table(rls_db, &rlpres_table) < 0) 
	{
		LM_ERR("in use_table\n");
		goto error;
	}
	if(rls_dbf.update(rls_db, query_cols, 0, query_vals, update_cols,
					update_vals, 1, 1)< 0)
	{
		LM_ERR("in sql update\n");
		goto error;
	}

	xmlFree(rlmi_body->s);
	pkg_free(rlmi_body);

	if(multipart_body)			
	{
		pkg_free(multipart_body->s);
		pkg_free(multipart_body);
	}
	pkg_free(rlsubs_did.s);

	return 0;
error:

	if(rlmi_body)
	{
		if(rlmi_body->s)
			xmlFree(rlmi_body->s);
		pkg_free(rlmi_body);
	}
	if(multipart_body)
	{
		if(multipart_body->s)
			pkg_free(multipart_body->s);
		pkg_free(multipart_body);
	}
	
	if(cid_array)
	{
		for(i= 0; i< result->n ; i++)
			if(cid_array[i])
				pkg_free(cid_array[i]);
		pkg_free(cid_array);
	}
	if(result)
		rls_dbf.free_result(rls_db, result);
	if(rlsubs_did.s)
		pkg_free(rlsubs_did.s);
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

	if(pres_update_shtable(rls_table, hash_code,subs, LOCAL_TYPE)< 0)
	{
		LM_ERR("updating in hash table\n");
		goto error;
	}
	return 0;

error:
	if(body.s)
		pkg_free(body.s);

	return -1;
}


int add_resource_instance(char* uri, xmlNodePtr resource_node,
		db1_res_t* result, char** cid_array)
{
	xmlNodePtr instance_node= NULL;
	db_row_t *row;	
	db_val_t *row_vals;
	int i, cmp_code;
	char* auth_state= NULL;
	int contor= 0;
	str cid;
	int auth_state_flag;

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
			contor++;
			instance_node= xmlNewChild(resource_node, NULL, 
					BAD_CAST "instance", NULL);
			if(instance_node== NULL)
			{
				LM_ERR("while adding instance child\n");
				goto error;
			}
		
			xmlNewProp(instance_node, BAD_CAST "id",
					BAD_CAST generate_string(contor, 8));
			auth_state_flag= row_vals[auth_state_col].val.int_val;
			auth_state= get_auth_string(auth_state_flag );
			if(auth_state== NULL)
			{
				LM_ERR("bad authorization status flag\n");
				goto error;
			}
			xmlNewProp(instance_node, BAD_CAST "state", BAD_CAST auth_state);

			if(auth_state_flag & ACTIVE_STATE)
			{
				cid.s= generate_cid(uri, strlen(uri));
				cid.len= strlen(cid.s);

				cid_array[i]= (char*)pkg_malloc((1+ cid.len)*sizeof(char));
				if(cid_array[i]== NULL)
				{
					ERR_MEM(PKG_MEM_STR);
				}	
				memcpy(cid_array[i], cid.s, cid.len);
				cid_array[i][cid.len]= '\0';

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

int add_resource(char* uri, void* param)
{
	char** cid_array= ((res_param_t*)param)->cid_array;
	xmlNodePtr list_node= ((res_param_t*)param)->list_node;
	xmlNodePtr resource_node= NULL;
	db1_res_t *result= ((res_param_t*)param)->db_result;

	LM_DBG("uri= %s\n", uri);
	resource_node= xmlNewChild(list_node, NULL, BAD_CAST "resource", NULL);
	if(resource_node== NULL)
	{
		LM_ERR("while adding new rsource_node\n");
		goto error;
	}
	xmlNewProp(resource_node, BAD_CAST "uri", BAD_CAST uri);

	if(add_resource_instance(uri, resource_node, result, cid_array)< 0)
	{
		LM_ERR("while adding resource instance node\n");
		goto error;
	}

	return 0;
error:
	return -1;
}

str* constr_rlmi_doc(db1_res_t *result, str* rl_uri, int version,
		xmlNodePtr rl_node, char*** rlmi_cid_array,
		str username, str domain)
{
	xmlDocPtr doc= NULL;
	xmlNodePtr list_node= NULL;
	str* rlmi_cont= NULL;
	int len; 
	char* uri;
	res_param_t param;
	char** cid_array= NULL;
	int n= result->n;

	LM_DBG("start\n");
	cid_array= (char**)pkg_malloc(n* sizeof(char*));
	if(cid_array== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memset(cid_array, 0, n* sizeof(char*));

	doc= xmlNewDoc(BAD_CAST "1.0");
	if(doc== NULL)
	{
		LM_ERR("while constructing new xml doc\n");
		goto error;
	}
	list_node= xmlNewNode(NULL, BAD_CAST "list");
	if(list_node== NULL)
	{
		LM_ERR("while creating new xml node\n");
		goto error;
	}
	uri= (char*)pkg_malloc(rl_uri->len+ 1);
	if(uri== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memcpy(uri, rl_uri->s, rl_uri->len);
	uri[rl_uri->len]= '\0';
	xmlNewProp(list_node, BAD_CAST "uri", BAD_CAST uri);
	pkg_free(uri);

	xmlNewProp(list_node, BAD_CAST "xmlns",
			BAD_CAST "urn:ietf:params:xml:ns:rlmi");
	xmlNewProp(list_node, BAD_CAST "version", BAD_CAST int2str(version, &len));
	xmlNewProp(list_node, BAD_CAST "fullState", BAD_CAST "true");

	xmlDocSetRootElement(doc, list_node);
	
	/* go through the list -- and add the appropriate 'resource' nodes*/
	
	param.list_node= list_node;
	param.db_result= result;
	param.cid_array= cid_array;

	if(process_list_and_exec(rl_node, username, domain, add_resource,(void*)(&param))< 0)
	{
		LM_ERR("in process_list_and_exec function\n");
		goto error;
	}
	rlmi_cont= (str*)pkg_malloc(sizeof(str));
	if(rlmi_cont== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	xmlDocDumpFormatMemory(doc,(xmlChar**)(void*)&rlmi_cont->s,
			&rlmi_cont->len, 1);

	*rlmi_cid_array= cid_array;
	
	xmlFreeDoc(doc);

	return rlmi_cont;

error:
	if(doc)
		xmlFreeDoc(doc);
	return NULL;	
}


str* constr_multipart_body(db1_res_t* result, char** cid_array, 
		char* boundary_string)
{
	char* buf= NULL;
	int size= BUF_REALLOC_SIZE;
	int i, length= 0;
	db_row_t *row;	
	db_val_t *row_vals;
	str content_id = {0, 0};
	str body= {0, 0};
	str content_type= {0, 0};
	int chunk_len;
	str* multi_body= NULL;
	str bstr = {0, 0};
	
	LM_DBG("start\n");
	buf= pkg_malloc(size* sizeof(char));
	if(buf== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

	bstr.s = boundary_string;
	bstr.len = strlen(bstr.s);

	for(i= 0; i< result->n; i++)
	{
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);
	
		if(row_vals[auth_state_col].val.int_val!= ACTIVE_STATE)
			continue;
	
		body.s= (char*)row_vals[pres_state_col].val.string_val;
		body.len= strlen(body.s);
		trim(&body);
		content_type.s = (char*)row_vals[content_type_col].val.string_val;
		content_type.len = strlen(content_type.s);
		content_id.s= cid_array[i];
		if(content_id.s== NULL)
		{
			LM_ERR("No cid found in array for uri= %s\n",
					row_vals[resource_uri_col].val.string_val);
			goto error;
		}
		content_id.len = strlen(content_id.s);


		chunk_len = 4 + bstr.len
					+ 35
					+ 16 + content_id.len
					+ 18 + content_type.len
					+ 4 + body.len + 8;
		if(length + chunk_len >= size)
		{
			REALLOC_BUF
		}

		length+= sprintf(buf+ length, "--%.*s\r\n",
				bstr.len, bstr.s);
		length+= sprintf(buf+ length, "Content-Transfer-Encoding: binary\r\n");
		length+= sprintf(buf+ length, "Content-ID: <%.*s>\r\n",
				content_id.len, content_id.s);
		length+= sprintf(buf+ length, "Content-Type: %.*s\r\n\r\n",
				content_type.len, content_type.s);
		length+= sprintf(buf+length,"%.*s\r\n\r\n",
				body.len, body.s);
	}

	if(length+ strlen( boundary_string)+ 7> size )
	{
		REALLOC_BUF
	}
	buf[length]= '\0';
	
	multi_body= (str*)pkg_malloc(sizeof(str));
	if(multi_body== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}

	multi_body->s= buf;
	multi_body->len= length;

	return multi_body;

error:

	if(buf)
		pkg_free(buf);
	return NULL;
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
		pkg_free(td->loc_uri.s);
		pkg_free(td->rem_uri.s);
		pkg_free(td);
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
	if(td)
	{
		if(td->loc_uri.s)
			pkg_free(td->loc_uri.s);
	
		if(td->rem_uri.s)
			pkg_free(td->rem_uri.s);
		pkg_free(td);
	}	

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
		/* delete from database table */
		db_key_t db_keys[2];
		db_val_t db_vals[2];
		unsigned int hash_code;
		subs_t subs;
		
		memset(&subs, 0, sizeof(subs_t));

		subs.to_tag= ((dialog_id_t*)(*ps->param))->to_tag;
		subs.from_tag= ((dialog_id_t*)(*ps->param))->from_tag;
		subs.callid= ((dialog_id_t*)(*ps->param))->callid;

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

		/* delete from cache table */
		hash_code= core_hash(&subs.callid, &subs.to_tag , hash_size);

		if(pres_delete_shtable(rls_table,hash_code, subs.to_tag)< 0)
		{
			LM_ERR("record not found in hash table\n");
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
	char* uri = NULL;
	int res = 0;

	LM_DBG("start\n");
	for(node= list_node->children; node; node= node->next)
	{
		if(xmlStrcasecmp(node->name,(unsigned char*)"resource-list")==0)
		{
			str hostname, rl_uri;
			unsigned short port = 0;
			xmlNodePtr rl_node = NULL;
			xmlDocPtr rl_doc = NULL;

			uri= XMLNodeGetNodeContentByName(node, "resource-list", NULL);

			if (uri == NULL)
			{
				LM_ERR("when extracting URI from node\n");
				return -1;
			}

			if(parse_xcap_uri(uri, &hostname, &port, &rl_uri)>0)
			{
				if (rls_integrated_xcap_server == 1
					&& (hostname.len == 0
						|| check_self(&hostname, port, PROTO_NONE) == 1))
				{
					LM_DBG("fetching local <resource-list/>\n");
					if (rls_get_resource_list(&rl_uri, &username, &domain, &rl_node, &rl_doc)>0)
					{
						LM_DBG("calling myself for rl_node\n");
						res = process_list_and_exec(rl_node, username, domain, function, param);
						xmlFree(uri);
						xmlFree(rl_doc);
					}
					else
					{
						LM_ERR("<resource-list/> not found\n");
						xmlFree(uri);
						return -1;
					}
					
				}
				else
				{
					LM_ERR("<resource-list/> is not local - unsupported at this time\n");
					xmlFree(uri);
					return -1;
				}
			}
			else
			{
				LM_ERR("unable to parse URI for <resource-list/>\n");
				return -1;
			}
		}
		else
                if(xmlStrcasecmp(node->name,(unsigned char*)"entry")== 0)
		{
			uri= XMLNodeGetAttrContentByName(node, "uri");
			if(uri== NULL)
			{
				LM_ERR("when extracting entry uri attribute\n");
				return -1;
			}
			LM_DBG("uri= %s\n", uri);
			if(function(uri, param)< 0)
			{
				LM_ERR("in function given as a parameter\n");
				xmlFree(uri);
				return -1;
			}
			xmlFree(uri);
		}
		else
		if(xmlStrcasecmp(node->name,(unsigned char*)"list")== 0)
			res = process_list_and_exec(node, username, domain, function, param);
	}
	return res;
}

char* generate_string(int seed, int length)
{
	static char buf[128];
    int r,i;

    if(length>= 128)
	{
		LM_ERR("requested length exceeds buffer size\n");
		return NULL;
	}
	srand(seed); 
		
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

int rls_get_resource_list(str *rl_uri, str *username, str *domain,
		xmlNodePtr *rl_node, xmlDocPtr *xmldoc)
{
	db_key_t query_cols[5];
	db_val_t query_vals[5];
	int n_query_cols = 0;
	db_key_t result_cols[3];
	int n_result_cols = 0;
	db1_res_t *result = 0;
	db_row_t *row;
	db_val_t *row_vals;
	int xcap_col;
	str body;

	if (rl_uri==NULL || username==NULL || domain==NULL)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

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
	query_vals[n_query_cols].val.str_val = *rl_uri;
	n_query_cols++;

	if(rls_dbf.use_table(rls_db, &rls_xcap_table) < 0)
	{
		LM_ERR("in use_table-[table]=%.*s\n",
			rls_xcap_table.len, rls_xcap_table.s);
		return -1;
	}

	result_cols[xcap_col= n_result_cols++] = &str_doc_col;

	if(rls_dbf.query(rls_db, query_cols, 0, query_vals, result_cols,
				n_query_cols, n_result_cols, 0, &result)<0)
	{
		LM_ERR("failed querying table xcap for document [rl_uri]=%.*s\n",
				rl_uri->len, rl_uri->s);
		if(result)
			rls_dbf.free_result(rls_db, result);
		return -1;
	}

	if(result->n<=0)
	{
		LM_DBG("No rl document found\n");
		rls_dbf.free_result(rls_db, result);
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
		LM_ERR("while parsing XML memory\n");
		goto error;
	}

	*rl_node = XMLDocGetNodeByName(*xmldoc,"resource-lists", NULL);
	if(rl_node==NULL)
	{
		LM_ERR("no resource-lists node in XML document\n");
		goto error;
	}

	rls_dbf.free_result(rls_db, result);
	return 1;

error:
	if(result!=NULL)
		rls_dbf.free_result(rls_db, result);
	if(xmldoc!=NULL)
		xmlFreeDoc(*xmldoc);

	return -1;
}

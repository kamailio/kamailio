/*
 * $Id: subscribe.c 2230 2007-06-06 07:13:20Z anca_vamanu $
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
#include "../../dprint.h"
#include "../../data_lump_rpl.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../lib/kcore/hash_func.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_event.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_cseq.h"
#include "../../parser/parse_from.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_rr.h"
#include "../presence/subscribe.h"
#include "../presence/utils_func.h"
#include "../presence/hash.h"
#include "subscribe.h"
#include "notify.h"
#include "rls.h"

int counter= 0;

static str su_200_rpl     = str_init("OK");
static str pu_421_rpl     = str_init("Extension Required");
static str pu_400_rpl     = str_init("Bad request");
static str stale_cseq_rpl = str_init("Stale Cseq Value");
static str pu_489_rpl     = str_init("Bad Event");

#define Stale_cseq_code 401

subs_t* constr_new_subs(struct sip_msg* msg, struct to_body *pto, 
		pres_ev_t* event);
int resource_subscriptions(subs_t* subs, xmlNodePtr rl_node);

int update_rlsubs( subs_t* subs,unsigned int hash_code);

int get_resource_list(str* pres_uri, char** list)
{
	db_key_t query_cols[5];
	db_val_t query_vals[5];
	db_key_t result_cols[3];
	int n_query_cols = 0;
	db1_res_t *result = 0;
	db_row_t *row ;	
	db_val_t *row_vals ;
	str body ;
	struct sip_uri uri;
	int n_result_cols= 0;
	int etag_col, xcap_col;
	char* etag= NULL;
	xcap_get_req_t req;

	xcap_doc_sel_t doc_sel;
	char* rls_list;

	if(parse_uri(pres_uri->s, pres_uri->len, &uri)< 0)
	{
		LM_ERR("while parsing uri\n");
		return -1;
	}
	/* first search in database */
	query_cols[n_query_cols] = &str_username_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = uri.user;
	n_query_cols++;
	
	query_cols[n_query_cols] = &str_domain_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = uri.host;
	n_query_cols++;
	
	query_cols[n_query_cols] = &str_doc_type_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= RESOURCE_LIST;
	n_query_cols++;

	if (rls_dbf.use_table(rls_db, &rls_xcap_table) < 0)
	{
		LM_ERR("in use_table-[table]= %.*s\n", rls_xcap_table.len, rls_xcap_table.s);
		return -1;
	}

	result_cols[xcap_col= n_result_cols++] = &str_doc_col;
	result_cols[etag_col= n_result_cols++]= &str_etag_col;

	if(rls_dbf.query(rls_db, query_cols, 0 , query_vals, result_cols,
				n_query_cols, n_result_cols, 0, &result)<0)
	{
		LM_ERR("while querying table xcap for [uri]=%.*s\n",
				pres_uri->len, pres_uri->s);
		if(result)
			rls_dbf.free_result(rls_db, result);
		return -1;
	}

	if(result->n<= 0)
	{
		LM_DBG("No xcap document for [uri]=%.*s\n",pres_uri->len,pres_uri->s);
		
		if(rls_integrated_xcap_server)
		{
			rls_dbf.free_result(rls_db, result);
			*list= 0;
			return 0;		
		}
		
		/* make an initial request to xcap_client module */
		doc_sel.auid.s= "resource-lists";
		doc_sel.auid.len= strlen("resource-lists");
		doc_sel.doc_type= RESOURCE_LIST;
		doc_sel.type= USERS_TYPE;
		doc_sel.xid= *pres_uri;
		doc_sel.filename.s= "index";
		doc_sel.filename.len= 5;

		memset(&req, 0, sizeof(xcap_get_req_t));
		req.xcap_root= xcap_root;
		req.port= xcap_port;
		req.doc_sel= doc_sel;
		req.etag= etag;
		req.match_type= IF_NONE_MATCH;

		rls_list= xcap_GetNewDoc(req, uri.user, uri.host);
		if(rls_list== NULL)
		{
			LM_ERR("while fetching data from xcap server\n");
			goto error;	
		}
		
		*list= rls_list;
		return 0;		
	}

	row = &result->rows[0];
	row_vals = ROW_VALUES(row);

	body.s = (char*)row_vals[xcap_col].val.string_val;
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
	LM_DBG("xcap body:\n%.*s", body.len,body.s);
	rls_list= (char*)pkg_malloc((body.len+ 1)* sizeof(char));
	if(rls_list== NULL)
	{
		rls_dbf.free_result(rls_db, result);
		ERR_MEM(PKG_MEM_STR);
	}
	memcpy(rls_list, body.s, body.len);
	rls_list[body.len]= '\0';
	rls_dbf.free_result(rls_db, result);
	*list= rls_list;

	return 0;

error:
	if(result)
		rls_dbf.free_result(rls_db, result);
	return -1;

}


int reply_421(struct sip_msg* msg)
{
	str hdr_append;
	char buffer[256];
	
	hdr_append.s = buffer;
	hdr_append.s[0]='\0';
	hdr_append.len = sprintf(hdr_append.s, "Require: eventlist\r\n");
	if(hdr_append.len < 0)
	{
		LM_ERR("unsuccessful sprintf\n");
		return -1;
	}
	hdr_append.s[hdr_append.len]= '\0';

	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		return -1;
	}

	if (slb.freply(msg, 421, &pu_421_rpl) < 0)
	{
		LM_ERR("while sending reply\n");
		return -1;
	}
	return 0;

}

int reply_200(struct sip_msg* msg, str* contact, int expires)
{
	str hdr_append;
	int len;
	
	hdr_append.s = (char *)pkg_malloc( sizeof(char)*(contact->len+ 70));
	if(hdr_append.s == NULL)
	{
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	hdr_append.len = sprintf(hdr_append.s, "Expires: %d\r\n", expires);	
	if(hdr_append.len< 0)
	{
		LM_ERR("unsuccessful sprintf\n");
		goto error;
	}
	strncpy(hdr_append.s+hdr_append.len ,"Contact: <", 10);
	hdr_append.len += 10;
	strncpy(hdr_append.s+hdr_append.len, contact->s, contact->len);
	hdr_append.len+= contact->len;
	strncpy(hdr_append.s+hdr_append.len, ">", 1);
	hdr_append.len += 1;
	strncpy(hdr_append.s+hdr_append.len, CRLF, CRLF_LEN);
	hdr_append.len += CRLF_LEN;

	len = sprintf(hdr_append.s+ hdr_append.len, "Require: eventlist\r\n");
	if(len < 0)
	{
		LM_ERR("unsuccessful sprintf\n");
		goto error;
	}
	hdr_append.len+= len;
	hdr_append.s[hdr_append.len]= '\0';
	
	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	if(slb.freply(msg, 200, &su_200_rpl) < 0)
	{
		LM_ERR("while sending reply\n");
		goto error;
	}	
	pkg_free(hdr_append.s);
	return 0;

error:
	pkg_free(hdr_append.s);
	return -1;
}	

int reply_489(struct sip_msg * msg)
{
	str hdr_append;
	char buffer[256];
	str* ev_list;

	hdr_append.s = buffer;
	hdr_append.s[0]='\0';
	hdr_append.len = sprintf(hdr_append.s, "Allow-Events: ");
	if(hdr_append.len < 0)
	{
		LM_ERR("unsuccessful sprintf\n");
		return -1;
	}

	if(pres_get_ev_list(&ev_list)< 0)
	{
		LM_ERR("while getting ev_list\n");
		return -1;
	}	
	memcpy(hdr_append.s+ hdr_append.len, ev_list->s, ev_list->len);
	hdr_append.len+= ev_list->len;
	pkg_free(ev_list->s);
	pkg_free(ev_list);
	memcpy(hdr_append.s+ hdr_append.len, CRLF, CRLF_LEN);
	hdr_append.len+=  CRLF_LEN;
	hdr_append.s[hdr_append.len]= '\0';
		
	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		return -1;
	}
	if (slb.freply(msg, 489, &pu_489_rpl) < 0)
	{
		LM_ERR("while sending reply\n");
		return -1;
	}
	return 0;
}
	

int rls_handle_subscribe(struct sip_msg* msg, char* s1, char* s2)
{
	struct to_body *pto, *pfrom = NULL, TO;
	int found= 0;
	struct hdr_field* hdr;
	subs_t subs;
	str resource_list= {0, 0};
	int i, len= 0;
	pres_ev_t* event= NULL;
	int err_ret= -1;
	str* contact= NULL;
	xmlDocPtr doc= NULL;
	xmlNodePtr rl_node= NULL;
	unsigned int hash_code;
	int to_tag_gen= 0;
	event_t* parsed_event;
	param_t* ev_param= NULL;

/*** filter: 'For me or for presence server?' */	

	memset(&subs, 0, sizeof(subs_t));

	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		if (slb.freply(msg, 400, &pu_400_rpl) < 0)
		{
			LM_ERR("while sending 400 reply\n");
			return -1;
		}
		return 0;
	}

	/* inspecting the Event header field */
	if(msg->event && msg->event->body.len > 0)
	{
		if (!msg->event->parsed && (parse_event(msg->event) < 0))
		{
			LM_ERR("cannot parse Event header\n");
			goto error;
		}
		if(! ( ((event_t*)msg->event->parsed)->type & rls_events) )
		{	
			return to_presence_code;
		}
	}
	else
		goto bad_event;

	/* search event in the list */
	parsed_event= (event_t*)msg->event->parsed;
	event= pres_search_event(parsed_event);
	if(event== NULL)
	{
		goto bad_event;
	}
	subs.event= event;
	
	/* extract the id if any*/
	ev_param= parsed_event->params.list;
	while(ev_param)
	{
		if(ev_param->name.len== 2 && strncmp(ev_param->name.s, "id", 2)== 0)
		{
			subs.event_id= ev_param->body;
			break;
		}
		ev_param= ev_param->next;
	}		


	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",pto->uri.len,pto->uri.s);
	}
	else
	{
		memset( &TO , 0, sizeof(TO) );
		parse_to(msg->to->body.s,msg->to->body.s+msg->to->body.len+1,&TO);
		if( TO.uri.len <= 0 )
		{
			LM_DBG("'To' header NOT parsed\n");
			goto error;
		}
		pto = &TO;
	}

	if(pto->tag_value.s== NULL || pto->tag_value.len==0)
		/* if an initial Subscribe */
	{
		/*verify if Request URI represents a list by asking xcap server*/	
		if(uandd_to_uri(msg->parsed_uri.user, msg->parsed_uri.host,
					&subs.pres_uri)< 0)
		{
			LM_ERR("while constructing uri from user and domain\n");
			goto error;
		}
		if( get_resource_list(&subs.pres_uri, &resource_list.s)< 0)
		{
			LM_ERR("while attepmting to get a resource list\n");
			goto error;
		}
		if( resource_list.s== NULL )    
		{
			/* if not a resource list subscribe , return  to_presence_code
			 * so that presence will handle the subscription*/
			return to_presence_code; 
		}
		resource_list.len= strlen(resource_list.s);

	}
	else
	{
		if( msg->callid==NULL || msg->callid->body.s==NULL)
		{
			LM_ERR("cannot parse callid header\n");
			goto error;
		}
		if (msg->from->parsed == NULL)
		{
			LM_DBG("'From' header not parsed\n");
			/* parsing from header */
			if ( parse_from_header( msg )<0 ) 
			{
				LM_DBG("ERROR cannot parse From header\n");
				goto error;
			}
		}
		pfrom = (struct to_body*)msg->from->parsed;
		if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
		{
			LM_ERR("no from tag value present\n");
			goto error;
		}

		/* search if a stored dialog */
		hash_code= core_hash(&msg->callid->body, &pto->tag_value, hash_size);
		lock_get(&rls_table[hash_code].lock);

		if(pres_search_shtable(rls_table,msg->callid->body,
					pto->tag_value,	pfrom->tag_value, hash_code)== NULL)
		{
			lock_release(&rls_table[hash_code].lock);
			return to_presence_code;	
		}
		lock_release(&rls_table[hash_code].lock);
	}

/*** verify if it contains the 'Supported: eventlist' header
 * and if not - reply with '421 (Extension Required)' */

/*
	hdr = msg->supported;
	if(hdr== NULL || hdr->body.s== 0 || hdr->body.len== 0)
	{
		LM_DBG("msg->supported header NULL\n");
		goto found_support;
	}
*/
	hdr= msg->headers;
	while(hdr)
	{
		if(cmp_hdrname_strzn(&hdr->name, "Supported", 9)== 0)
			break;
		hdr= hdr->next;
	}
	while(hdr!= NULL )
	{
		len= hdr->body.len- 8;
		for(i= 0; i< len; i++)
		{	
			if(strncmp(hdr->body.s+ i, "eventlist", 9)== 0)
			{
				found= 1;
				goto found_support;
			}
		}
		hdr = next_sibling_hdr(hdr);
	}

found_support:	
	if(found== 0)	
	{
		LM_ERR("No 'Support: eventlist' header found\n");
		if(reply_421(msg)< 0)
			return -1;
		return 0;
	}
/*** examine the event header */

	/* extract dialog information from message headers */
	if(pres_extract_sdialog_info(&subs, msg, rls_max_expires,
				&to_tag_gen, server_address)< 0)
	{
		LM_ERR("bad Subscribe request\n");
		goto error;
	}

	hash_code= core_hash(&subs.callid, &subs.to_tag, hash_size);

	if(pto->tag_value.s== NULL || pto->tag_value.len==0) 
		/* if an initial subscribe */
	{
		subs.local_cseq= 0;

		if(subs.expires!= 0)
		{
			subs.version= 1;
			if(pres_insert_shtable(rls_table, hash_code, &subs)< 0)
			{
				LM_ERR("while adding new subscription\n");
				goto error;
			}
		}
	}
	else
	{
		str reason;
		int rt;

		rt= update_rlsubs(&subs, hash_code);
		if(rt< 0)
		{
			LM_ERR("while updating resource list subscription\n");
			goto error;
		}
	
		if(rt>= 400)
		{
			reason= (rt==400)?pu_400_rpl:stale_cseq_rpl;
		
			if (slb.freply(msg, 400, &reason) < 0)
			{
				LM_ERR("while sending reply\n");
				goto error;
			}
			return 0;
		}	

		if(get_resource_list(&subs.pres_uri, &resource_list.s)< 0)
		{
			LM_ERR("when getting resource list\n");
			goto error;
		}
		resource_list.len= strlen(resource_list.s);
	}
	
	doc= xmlParseMemory(resource_list.s, resource_list.len);
	if(doc== NULL)
	{
		LM_ERR("while parsing XML memory\n");
		goto error;
	}
	rl_node= XMLDocGetNodeByName(doc, "resource-lists", NULL);
	if(rl_node== NULL)
	{
		LM_ERR("while extracting resource-lists node\n");
		goto error;
	}

/*** send Subscribe requests for all in the list */
	
	if(resource_subscriptions(&subs, rl_node)< 0)
	{
		LM_ERR("while sending Subscribe requests to resources in a list\n");
		goto error;
	}

/*** if correct reply with 200 OK*/
	if(reply_200(msg, &subs.contact, subs.expires)< 0)
		goto error;

	/* call sending Notify with full state */
	if(send_full_notify(&subs,rl_node,subs.version,&subs.pres_uri,hash_code)< 0)
	{
		LM_ERR("while sending full state Notify\n");
		goto error;
	}
	if(contact)
	{	
		if(contact->s)
			pkg_free(contact->s);
		pkg_free(contact);
	}
		
	pkg_free(resource_list.s);
	pkg_free(subs.pres_uri.s);

	if(subs.record_route.s)
		pkg_free(subs.record_route.s);
	xmlFreeDoc(doc);
	return 1;

bad_event:
	if(reply_489(msg)< 0)
	{
		LM_ERR("while sending 489 reply\n");
		err_ret= -1;
	}
	err_ret= 0;

error:
	LM_ERR("occured in rls_handle_subscribe\n");

	if(contact)
	{	
		if(contact->s)
			pkg_free(contact->s);
		pkg_free(contact);
	}
	if(subs.pres_uri.s)
		pkg_free(subs.pres_uri.s);
		
	if(subs.record_route.s)
			pkg_free(subs.record_route.s);	
	
	if(doc)
		xmlFreeDoc(doc);
	if(resource_list.s)
		pkg_free(resource_list.s);
	return err_ret;
}

int update_rlsubs( subs_t* subs, unsigned int hash_code)
{
	subs_t* s, *ps;

	/* search the record in hash table */
	lock_get(&rls_table[hash_code].lock);

	s= pres_search_shtable(rls_table, subs->callid,
			subs->to_tag, subs->from_tag, hash_code);
	if(s== NULL)
	{
		LM_DBG("record not found in hash table\n");
		lock_release(&rls_table[hash_code].lock);
		return -1;
	}

	s->expires= subs->expires+ (int)time(NULL);
	s->remote_cseq= subs->remote_cseq;
	
	if(s->db_flag & NO_UPDATEDB_FLAG)
		s->db_flag= UPDATEDB_FLAG;
	
	if(	s->remote_cseq>= subs->remote_cseq)
	{
		lock_release(&rls_table[hash_code].lock);
		LM_DBG("stored cseq= %d\n", s->remote_cseq);
		return Stale_cseq_code;
	}

	subs->pres_uri.s= (char*)pkg_malloc(s->pres_uri.len* sizeof(char));
	if(subs->pres_uri.s== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memcpy(subs->pres_uri.s, s->pres_uri.s, s->pres_uri.len);
	subs->pres_uri.len= s->pres_uri.len;

	if(s->record_route.s!=NULL && s->record_route.len>0)
	{
		subs->record_route.s =
				(char*)pkg_malloc(s->record_route.len* sizeof(char));
		if(subs->record_route.s==NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->record_route.s, s->record_route.s, s->record_route.len);
		subs->record_route.len= s->record_route.len;
	}

	subs->local_cseq= s->local_cseq;
	subs->version= s->version;

	if(subs->expires== 0)
	{
		/* delete record from hash table */
		ps= rls_table[hash_code].entries;
		int found= 0;
		while(ps->next)
		{
			if(ps->next== s)
			{
				found= 1;
				break;
			}
			ps= ps->next;
		}
		if(found== 0)
		{
			LM_ERR("record not found\n");
			goto error;
		}
		ps->next= s->next;
		shm_free(s);
	}
	
	lock_release(&rls_table[hash_code].lock);

	return 0;

error:
	lock_release(&rls_table[hash_code].lock);
	return -1;
}

int send_resource_subs(char* uri, void* param)
{
	str pres_uri;

	pres_uri.s= uri;
	pres_uri.len= strlen(uri);

	((subs_info_t*)param)->pres_uri= &pres_uri;

	return pua_send_subscribe((subs_info_t*)param);
}

int resource_subscriptions(subs_t* subs, xmlNodePtr rl_node)
{
	char* uri= NULL;
	subs_info_t s;
	str wuri= {0, 0};
	static char buf[256];
	str extra_headers;
	str did_str= {0, 0};
		
	/* if is initial send an initial Subscribe 
	 * else search in hash table for a previous subscription */

	CONSTR_RLSUBS_DID(subs, &did_str);
	
	memset(&s, 0, sizeof(subs_info_t));

	if( uandd_to_uri(subs->from_user, subs->from_domain, &wuri)< 0)
	{
		LM_ERR("while constructing uri from user and domain\n");
		goto error;
	}
	s.id= did_str;
	s.watcher_uri= &wuri;
	s.contact= &subs->local_contact;
	s.event= get_event_flag(&subs->event->name);
	if(s.event< 0)
	{
		LM_ERR("not recognized event\n");
		goto error;
	}
	s.expires= subs->expires;
	s.source_flag= RLS_SUBSCRIBE;
	if(outbound_proxy.s)
		s.outbound_proxy= &outbound_proxy;
	extra_headers.s= buf;
	extra_headers.len= sprintf(extra_headers.s,
			"Max-Forwards: 70\r\nSupport: eventlist\r\n");
	s.extra_headers= &extra_headers;
	
	if(process_list_and_exec(rl_node, send_resource_subs,(void*)(&s))< 0)
	{
		LM_ERR("while processing list\n");
		goto error;
	}

	pkg_free(wuri.s);
	pkg_free(did_str.s);

	return 0;

error:
	if(wuri.s)
		pkg_free(wuri.s);
	if(uri)
		xmlFree(uri);
	if(did_str.s)
		pkg_free(did_str.s);
	return -1;

}


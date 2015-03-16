/*
 * pua module - presence user agent module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../parser/parse_expires.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../hashes.h"
#include "../../modules/tm/tm_load.h"
#include "pua.h"
#include "hash.h"
#include "send_publish.h"
#include "pua_callback.h"
#include "event_list.h"
#include "pua_db.h"

extern db_locking_t db_table_lock;

str* publ_build_hdr(int expires, pua_event_t* ev, str* content_type, str* etag,
		str* extra_headers, int is_body)
{
	static char buf[3000];
	str* str_hdr = NULL;	
	char* expires_s = NULL;
	int len = 0;
	int t= 0;
	str ctype;

	str_hdr =(str*)pkg_malloc(sizeof(str));
	if(str_hdr== NULL)
	{
		LM_ERR("no more memory\n");
		return NULL;
	}
	memset(str_hdr, 0 , sizeof(str));
	memset(buf, 0, 2999);
	str_hdr->s = buf;
	str_hdr->len= 0;

	memcpy(str_hdr->s ,"Max-Forwards: ", 14);
	str_hdr->len = 14;
	str_hdr->len+= sprintf(str_hdr->s+ str_hdr->len,"%d", MAX_FORWARD);
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	memcpy(str_hdr->s+ str_hdr->len ,"Event: ", 7);
	str_hdr->len+= 7;
	memcpy(str_hdr->s+ str_hdr->len, ev->name.s, ev->name.len);
	str_hdr->len+= ev->name.len;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	

	memcpy(str_hdr->s+str_hdr->len ,"Expires: ", 9);
	str_hdr->len += 9;

	t= expires; 

	if( t<=0 )
	{
		t= min_expires;
	}
	else
	{
		t++;
	}
	expires_s = int2str(t, &len);

	memcpy(str_hdr->s+str_hdr->len, expires_s, len);
	str_hdr->len+= len;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	
	if(etag)
	{
		LM_DBG("UPDATE_TYPE [etag]= %.*s\n", etag->len, etag->s);
		memcpy(str_hdr->s+str_hdr->len,"SIP-If-Match: ", 14);
		str_hdr->len += 14;
		memcpy(str_hdr->s+str_hdr->len, etag->s, etag->len);
		str_hdr->len += etag->len;
		memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len += CRLF_LEN;
	}
	if(is_body)
	{	
		if(content_type== NULL || content_type->s== NULL || content_type->len== 0)
		{
			ctype= ev->content_type; /* use event default value */ 
		}
		else
		{	
			ctype.s=   content_type->s;
			ctype.len= content_type->len;
		}

		memcpy(str_hdr->s+str_hdr->len,"Content-Type: ", 14);
		str_hdr->len += 14;
		memcpy(str_hdr->s+str_hdr->len, ctype.s, ctype.len);
		str_hdr->len += ctype.len;
		memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
		str_hdr->len += CRLF_LEN;
	}

	if(extra_headers && extra_headers->s && extra_headers->len)
	{
		memcpy(str_hdr->s+str_hdr->len,extra_headers->s , extra_headers->len);
		str_hdr->len += extra_headers->len;
	}	
	str_hdr->s[str_hdr->len] = '\0';
	
	return str_hdr;

}

static void find_and_delete_record(ua_pres_t *dialog, int hash_code)
{
	ua_pres_t *presentity;

	if (dbmode == PUA_DB_ONLY)
	{
		delete_record_puadb(dialog);
	}
	else
	{
		lock_get(&HashT->p_records[hash_code].lock);
		presentity = search_htable(dialog, hash_code);
		if (presentity == NULL)
		{
			LM_DBG("Record found in table and deleted\n");
			lock_release(&HashT->p_records[hash_code].lock);
			return;
		}
		delete_htable(presentity, hash_code);
		lock_release(&HashT->p_records[hash_code].lock);
	}
}

static int find_and_update_record(ua_pres_t *dialog, int hash_code, int lexpire, str *etag)
{
	ua_pres_t *presentity;

	if(dbmode==PUA_DB_ONLY)
	{
		return update_record_puadb(dialog, lexpire, etag);
	}
	else
	{
		lock_get(&HashT->p_records[hash_code].lock);
		presentity = search_htable(dialog, hash_code);
		if (presentity == NULL)
		{
			LM_DBG("Record found in table and deleted\n");
			lock_release(&HashT->p_records[hash_code].lock);
			return 0;
		}
		update_htable(presentity, dialog->desired_expires, lexpire, etag, hash_code, NULL);
		lock_release(&HashT->p_records[hash_code].lock);
		return 1;
	}
}

void publ_cback_func(struct cell *t, int type, struct tmcb_params *ps)
{
	struct hdr_field* hdr= NULL;
	struct sip_msg* msg= NULL;
	ua_pres_t* presentity= NULL;
	ua_pres_t* db_presentity= NULL; 
	ua_pres_t* hentity= NULL;
	int found = 0;
	int size= 0;
	unsigned int lexpire= 0;
	str etag;
	unsigned int hash_code;
	db1_res_t *res=NULL;
	ua_pres_t dbpres;
	str pres_uri={0,0}, watcher_uri={0,0}, extra_headers={0,0};
	int end_transaction = 1;

	memset(&dbpres, 0, sizeof(dbpres));
	dbpres.pres_uri = &pres_uri;
	dbpres.watcher_uri = &watcher_uri;
	dbpres.extra_headers = &extra_headers;

	if (dbmode == PUA_DB_ONLY && pua_dbf.start_transaction)
	{
		if (pua_dbf.start_transaction(pua_db, db_table_lock) < 0)
		{
			LM_ERR("in start_transaction\n");
			goto error;
		}
	}

	if(ps->param== NULL|| *ps->param== NULL)
	{
		LM_ERR("NULL callback parameter\n");
		goto error;
	}
	hentity= (ua_pres_t*)(*ps->param);

	msg= ps->rpl;
	if(msg == NULL)
	{
		LM_ERR("no reply message found\n ");
		goto error;
	}
	

	if(msg== FAKED_REPLY)
	{
		LM_DBG("FAKED_REPLY\n");
		goto done;
	}

	hash_code= core_hash(hentity->pres_uri, NULL, HASH_SIZE);

	if( ps->code>= 300 )
	{
		find_and_delete_record(hentity, hash_code);

		if(ps->code== 412 && hentity->body && hentity->flag!= MI_PUBLISH
				&& hentity->flag!= MI_ASYN_PUBLISH)
		{
			/* sent a PUBLISH within a dialog that no longer exists
			 * send again an intial PUBLISH */
			LM_DBG("received a 412 reply- try again to send PUBLISH\n");
			publ_info_t publ;
			memset(&publ, 0, sizeof(publ_info_t));
			publ.pres_uri= hentity->pres_uri; 
			publ.body= hentity->body;
			
			if(hentity->desired_expires== 0)
				publ.expires= -1;
			else
			if(hentity->desired_expires<= (int)time(NULL))
				publ.expires= 0;
			else
				publ.expires= hentity->desired_expires- (int)time(NULL)+ 3;

			publ.source_flag|= hentity->flag;
			publ.event|= hentity->event;
			publ.content_type= hentity->content_type;	
			publ.id= hentity->id;
			publ.extra_headers= hentity->extra_headers;
			publ.outbound_proxy = hentity->outbound_proxy;
			publ.cb_param= hentity->cb_param;

			if (dbmode == PUA_DB_ONLY && pua_dbf.end_transaction)
			{
				if (pua_dbf.end_transaction(pua_db) < 0)
				{
					LM_ERR("in end_transaction\n");
					goto error;
				}
			}

			end_transaction = 0;

			if(send_publish(&publ)< 0)
			{
				LM_ERR("when trying to send PUBLISH\n");
				goto error;
			}
		}
		goto done;
	} /* code >= 300 */
	
	if( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("parsing headers\n");
		goto error;
	}	
	if(msg->expires== NULL || msg->expires->body.len<= 0)
	{
			LM_ERR("No Expires header found\n");
			goto error;
	}	
	
	if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
	{
		LM_ERR("cannot parse Expires header\n");
		goto error;
	}
	lexpire = ((exp_body_t*)msg->expires->parsed)->val;
	LM_DBG("lexpire= %u\n", lexpire);
		
	hdr = msg->headers;
	while (hdr!= NULL)
	{
		if(cmp_hdrname_strzn(&hdr->name, "SIP-ETag",8)==0 )
		{
			found = 1;
			break;
		}
		hdr = hdr->next;
	}
	if(found== 0) /* must find SIP-Etag header field in 200 OK msg*/
	{	
		LM_ERR("no SIP-ETag header field found\n");
		goto error;
	}
	etag= hdr->body;
		
	LM_DBG("completed with status %d [contact:%.*s]\n",
	       ps->code, hentity->pres_uri->len, hentity->pres_uri->s);

	if (lexpire == 0)
	{
		find_and_delete_record(hentity, hash_code);
		goto done;
	}

	if (hentity->etag.s) {
		if (pua_dbf.affected_rows != NULL || dbmode != PUA_DB_ONLY) {
			if (find_and_update_record(hentity, hash_code,
						   lexpire, &etag) > 0)
				goto done;
		}
		else if ((db_presentity =
			  get_record_puadb(hentity->id, &hentity->etag,
					   &dbpres, &res)) != NULL)
		{
			update_record_puadb(hentity, lexpire, &etag);
			goto done;
		}
	}

	size= sizeof(ua_pres_t)+ sizeof(str)+ 
		(hentity->pres_uri->len+ hentity->tuple_id.len + 
		 hentity->id.len)* sizeof(char);
	if(hentity->extra_headers)
		size+= sizeof(str)+ hentity->extra_headers->len* sizeof(char);

	presentity= (ua_pres_t*)shm_malloc(size);
	if(presentity== NULL)
	{
		LM_ERR("no more share memory\n");
		goto error;
	}	
	memset(presentity, 0, size);

	size= sizeof(ua_pres_t);
	presentity->pres_uri= (str*)((char*)presentity+ size);
	size+= sizeof(str);

	presentity->pres_uri->s= (char*)presentity+ size;
	memcpy(presentity->pres_uri->s, hentity->pres_uri->s, 
			hentity->pres_uri->len);
	presentity->pres_uri->len= hentity->pres_uri->len;
	size+= hentity->pres_uri->len;
	
	presentity->tuple_id.s= (char*)presentity+ size;
	memcpy(presentity->tuple_id.s, hentity->tuple_id.s,
			hentity->tuple_id.len);
	presentity->tuple_id.len= hentity->tuple_id.len;
	size+= presentity->tuple_id.len;

	presentity->id.s=(char*)presentity+ size;
	memcpy(presentity->id.s, hentity->id.s, 
			hentity->id.len);
	presentity->id.len= hentity->id.len; 
	size+= presentity->id.len;
		
	if(hentity->extra_headers)
	{
		presentity->extra_headers= (str*)((char*)presentity+ size);
		size+= sizeof(str);
		presentity->extra_headers->s= (char*)presentity+ size;
		memcpy(presentity->extra_headers->s, hentity->extra_headers->s, 
				hentity->extra_headers->len);
		presentity->extra_headers->len= hentity->extra_headers->len;
		size+= hentity->extra_headers->len;
	}

	presentity->desired_expires= hentity->desired_expires;
	presentity->expires= lexpire+ (int)time(NULL);
	presentity->flag|= hentity->flag;
	presentity->event|= hentity->event;

	presentity->etag.s= (char*)shm_malloc(etag.len* sizeof(char));
	if(presentity->etag.s== NULL)
	{
		LM_ERR("No more share memory\n");
		goto error;
	}
	memcpy(presentity->etag.s, etag.s, etag.len);
	presentity->etag.len= etag.len;

	if (dbmode==PUA_DB_ONLY)
	{
		insert_record_puadb(presentity);
	}
	else
 	{
		lock_get(&HashT->p_records[hash_code].lock);
		insert_htable(presentity, hash_code);
		lock_release(&HashT->p_records[hash_code].lock);
	}
	LM_DBG("Inserted record\n");

done:
	if(hentity->ua_flag == REQ_OTHER)
	{
		run_pua_callbacks(hentity, msg);
	}
	if(*ps->param)
	{
		shm_free(*ps->param);
		*ps->param= NULL;
	}
	if(dbmode==PUA_DB_ONLY && presentity)
	{
		shm_free(presentity->etag.s);
		shm_free(presentity);
	}

	if (res) free_results_puadb(res);

	if (dbmode == PUA_DB_ONLY && pua_dbf.end_transaction && end_transaction)
	{
		if (pua_dbf.end_transaction(pua_db) < 0)
		{
			LM_ERR("in end_transaction\n");
			goto error;
		}
	}

	return;

error:
	if(*ps->param)
	{
		shm_free(*ps->param);
		*ps->param= NULL;
	}
	if(presentity) shm_free(presentity);

	if (res) free_results_puadb(res);

	if (dbmode == PUA_DB_ONLY && pua_dbf.abort_transaction)
	{
		if (pua_dbf.abort_transaction(pua_db) < 0)
			LM_ERR("in abort_transaction\n");
	}

	return;
}	

int send_publish( publ_info_t* publ )
{
	str met = {"PUBLISH", 7};
	str* str_hdr = NULL;
	ua_pres_t* presentity= NULL;
	str* body= NULL;
	str* tuple_id= NULL;
	ua_pres_t* cb_param= NULL;
	unsigned int hash_code=0;
	str etag= {0, 0};
	int ver= 0;
	int result;
	int ret_code= 0;
	pua_event_t* ev= NULL;
	uac_req_t uac_r;
	db1_res_t *res=NULL;
	ua_pres_t dbpres; 
	str pres_uri={0,0}, watcher_uri={0,0}, extra_headers={0,0};
	int ret = -1;

	LM_DBG("pres_uri=%.*s\n", publ->pres_uri->len, publ->pres_uri->s );
	
	if (dbmode == PUA_DB_ONLY && pua_dbf.start_transaction)
	{
		if (pua_dbf.start_transaction(pua_db, db_table_lock) < 0)
		{
			LM_ERR("in start_transaction\n");
			goto error;
		}
	}

	/* get event from list */
	ev= get_event(publ->event);
	if(ev== NULL)
	{
		LM_ERR("event not found in list\n");
		goto error;
	}	

	if (dbmode==PUA_DB_ONLY)
	{
		if (publ->etag) {
			memset(&dbpres, 0, sizeof(dbpres));
			dbpres.pres_uri = &pres_uri;
			dbpres.watcher_uri = &watcher_uri;
			dbpres.extra_headers = &extra_headers;
			presentity = get_record_puadb(publ->id, publ->etag,
						      &dbpres, &res);
		}
	}
	else
	{
		ua_pres_t pres;

		memset(&pres, 0, sizeof(ua_pres_t));
		pres.pres_uri = publ->pres_uri;
		pres.flag = publ->source_flag;
		pres.id = publ->id;
		pres.event = publ->event;
		if(publ->etag)
			pres.etag = *publ->etag;

		hash_code= core_hash(publ->pres_uri, NULL, HASH_SIZE);
		lock_get(&HashT->p_records[hash_code].lock);
		presentity= search_htable(&pres, hash_code);
	}

	if(publ->etag && presentity== NULL)
	{
		if (dbmode!=PUA_DB_ONLY) 
			lock_release(&HashT->p_records[hash_code].lock);
		ret = 418;
		goto error;
	}

	if(publ->flag & INSERT_TYPE)
	{
		LM_DBG("Insert flag set\n");
		goto insert;
	}
	
	if(presentity== NULL)
	{
insert:	
		if (dbmode!=PUA_DB_ONLY) 
			lock_release(&HashT->p_records[hash_code].lock);
		LM_DBG("insert type\n"); 
		
		if(publ->flag & UPDATE_TYPE )
		{
			LM_DBG("UPDATE_TYPE and no record found \n");
			publ->flag= INSERT_TYPE;
		}
		if(publ->expires== 0)
		{
			LM_DBG("request for a publish with expires 0 and"
					" no record found\n");
			goto done;
			
		}
		if(publ->body== NULL)
		{
			LM_ERR("New PUBLISH and no body found- invalid request\n");
			ret = ERR_PUBLISH_NO_BODY;
			goto error;
		}
	}
	else
	{
		LM_DBG("record found\n");
		publ->flag= UPDATE_TYPE;
		etag.s= (char*)pkg_malloc(presentity->etag.len* sizeof(char));
		if(etag.s== NULL)
		{
			LM_ERR("while allocating memory\n");
			if (dbmode!=PUA_DB_ONLY) 
				lock_release(&HashT->p_records[hash_code].lock);
			goto error;
		}
		memcpy(etag.s, presentity->etag.s, presentity->etag.len);
		etag.len= presentity->etag.len;

		if(presentity->tuple_id.s && presentity->tuple_id.len)
		{	
			/* get tuple_id*/
			tuple_id=(str*)pkg_malloc(sizeof(str));
			if(tuple_id== NULL)
			{
				LM_ERR("No more memory\n");
				if (dbmode!=PUA_DB_ONLY) 
					lock_release(&HashT->p_records[hash_code].lock);
				goto error;
			}	
			tuple_id->s= (char*)pkg_malloc(presentity->tuple_id.len* sizeof(char));
			if(tuple_id->s== NULL)
			{
				LM_ERR("No more memory\n");
				if (dbmode!=PUA_DB_ONLY) 
					lock_release(&HashT->p_records[hash_code].lock);
				goto error;
			}	
			memcpy(tuple_id->s, presentity->tuple_id.s, presentity->tuple_id.len);
			tuple_id->len= presentity->tuple_id.len;
		}

		if(publ->expires== 0)
		{
			LM_DBG("expires= 0- delete from hash table\n");
			if (dbmode!=PUA_DB_ONLY) 
				lock_release(&HashT->p_records[hash_code].lock);
			goto send_publish;
		}

		presentity->version++; 
		ver= presentity->version;

		if (dbmode==PUA_DB_ONLY)
		{ 
			update_version_puadb(presentity);
		}
		else
		{
			lock_release(&HashT->p_records[hash_code].lock);
		}
	}

	/* handle body */
	if(publ->body && publ->body->s)
	{
		ret_code= ev->process_body(publ, &body, ver, &tuple_id );
		if( ret_code< 0 || body== NULL)
		{
			LM_ERR("while processing body\n");
			if(body== NULL)
				LM_ERR("NULL body\n");
			goto error;
		}
	}
	if(tuple_id)
		LM_DBG("tuple_id= %.*s\n", tuple_id->len, tuple_id->s  );
	
send_publish:
	
	/* construct the callback parameter */
	if(etag.s && etag.len)
		publ->etag = &etag;

	cb_param= publish_cbparam(publ, body, tuple_id, REQ_OTHER);
	if(cb_param== NULL)
	{
		LM_ERR("constructing callback parameter\n");
		goto error;
	}

	if(publ->flag & UPDATE_TYPE)
		LM_DBG("etag:%.*s\n", etag.len, etag.s);
	str_hdr = publ_build_hdr((publ->expires< 0)?3600:publ->expires, ev, &publ->content_type, 
				(publ->flag & UPDATE_TYPE)?&etag:NULL, publ->extra_headers, (body)?1:0);

	if(str_hdr == NULL)
	{
		LM_ERR("while building extra_headers\n");
		goto error;
	}

	LM_DBG("publ->pres_uri:\n%.*s\n ", publ->pres_uri->len, publ->pres_uri->s);
	LM_DBG("str_hdr:\n%.*s %d\n ", str_hdr->len, str_hdr->s, str_hdr->len);
	if(body && body->len && body->s )
		LM_DBG("body:\n%.*s\n ", body->len, body->s);

	set_uac_req(&uac_r, &met, str_hdr, body, 0, TMCB_LOCAL_COMPLETED,
			publ_cback_func, (void*)cb_param);
	result= tmb.t_request(&uac_r,
			publ->pres_uri,			/*! Request-URI */
			publ->pres_uri,			/*! To */
 		        publ->pres_uri,			/*! From */
		        publ->outbound_proxy?
			      publ->outbound_proxy:&outbound_proxy /*! Outbound proxy*/
			);

	if(result< 0)
	{
		LM_ERR("in t_request tm module function\n");
		goto error;
	}

done:
	ret = 0;

	if (dbmode == PUA_DB_ONLY && pua_dbf.end_transaction)
	{
		if (pua_dbf.end_transaction(pua_db) < 0)
		{
			LM_ERR("in end_transaction\n");
			goto error;
		}
	}
	goto finish;

error:
	if(cb_param)
		shm_free(cb_param);

	if (dbmode == PUA_DB_ONLY && pua_dbf.abort_transaction)
	{
		if (pua_dbf.abort_transaction(pua_db) < 0)
			LM_ERR("in abort_transaction\n");
	}

finish:
	if(etag.s)
		pkg_free(etag.s);

	if(body && ret_code)
	{
		if(body->s)
			xmlFree(body->s);
		pkg_free(body);
	}	
	if(str_hdr)
		pkg_free(str_hdr);
	if(tuple_id)
	{
		if(tuple_id->s)
			pkg_free(tuple_id->s);
		pkg_free(tuple_id);
	}
	free_results_puadb(res);

	return ret;
}

ua_pres_t* publish_cbparam(publ_info_t* publ,str* body,str* tuple_id,
		int ua_flag)
{
	int size;
	ua_pres_t* cb_param= NULL;

	size= sizeof(ua_pres_t)+ sizeof(str)+ (publ->pres_uri->len+ 
		+ publ->content_type.len+ publ->id.len+ 1)*sizeof(char);

	if(publ->outbound_proxy)
		size+= sizeof(str)+ publ->outbound_proxy->len* sizeof(char);
	if(body && body->s && body->len)
		size+= sizeof(str)+ body->len* sizeof(char);
	if(publ->etag)
		size+= publ->etag->len* sizeof(char);
	if(publ->extra_headers)
		size+= sizeof(str)+ publ->extra_headers->len* sizeof(char);
	if(tuple_id )
		size+= tuple_id->len* sizeof(char);

	cb_param= (ua_pres_t*)shm_malloc(size);
	if(cb_param== NULL)
	{
		LM_ERR("ERROR no more share memory while allocating cb_param"
				" - size= %d\n", size);
		return NULL;
	}
	memset(cb_param, 0, size);
	
	size =  sizeof(ua_pres_t);

	cb_param->pres_uri = (str*)((char*)cb_param + size);
	size+= sizeof(str);
	cb_param->pres_uri->s = (char*)cb_param + size;
	memcpy(cb_param->pres_uri->s, publ->pres_uri->s ,
			publ->pres_uri->len ) ;
	cb_param->pres_uri->len= publ->pres_uri->len;
	size+= publ->pres_uri->len;

	if(publ->id.s && publ->id.len)
	{	
		cb_param->id.s = ((char*)cb_param+ size);
		memcpy(cb_param->id.s, publ->id.s, publ->id.len);
		cb_param->id.len= publ->id.len;
		size+= publ->id.len;
	}

	if(body && body->s && body->len)
	{
		cb_param->body = (str*)((char*)cb_param  + size);
		size+= sizeof(str);
		
		cb_param->body->s = (char*)cb_param + size;
		memcpy(cb_param->body->s, body->s ,
			body->len ) ;
		cb_param->body->len= body->len;
		size+= body->len;
	}
	if(publ->etag)
	{
		cb_param->etag.s = (char*)cb_param + size;
		memcpy(cb_param->etag.s, publ->etag->s ,
			publ->etag->len ) ;
		cb_param->etag.len= publ->etag->len;
		size+= publ->etag->len;
	}
	if(publ->extra_headers)
	{
		cb_param->extra_headers = (str*)((char*)cb_param  + size);
		size+= sizeof(str);
		cb_param->extra_headers->s = (char*)cb_param + size;
		memcpy(cb_param->extra_headers->s, publ->extra_headers->s ,
			publ->extra_headers->len ) ;
		cb_param->extra_headers->len= publ->extra_headers->len;
		size+= publ->extra_headers->len;
	}	
	if(publ->outbound_proxy)
	{
		cb_param->outbound_proxy = (str*)((char*)cb_param + size);
		size += sizeof(str);
		cb_param->outbound_proxy->s = (char*)cb_param + size;
		memcpy(cb_param->outbound_proxy->s, publ->outbound_proxy->s,
		       publ->outbound_proxy->len);
		cb_param->outbound_proxy->len = publ->outbound_proxy->len;
		size+= publ->outbound_proxy->len;
	}	

	if(publ->content_type.s && publ->content_type.len)
	{
		cb_param->content_type.s= (char*)cb_param + size;
		memcpy(cb_param->content_type.s, publ->content_type.s, publ->content_type.len);
		cb_param->content_type.len= publ->content_type.len;
		size+=  publ->content_type.len;
	}	
	if(tuple_id)
	{	
		cb_param->tuple_id.s = (char*)cb_param+ size;
		memcpy(cb_param->tuple_id.s, tuple_id->s ,tuple_id->len);
		cb_param->tuple_id.len= tuple_id->len;
		size+= tuple_id->len;
	}

	cb_param->event= publ->event;
	cb_param->flag|= publ->source_flag;
	cb_param->cb_param= publ->cb_param;
	cb_param->ua_flag= ua_flag;

	if(publ->expires< 0)
		cb_param->desired_expires= 0;
	else
		cb_param->desired_expires=publ->expires+ (int)time(NULL);

	return cb_param;
}

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
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libxml/parser.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/tm/dlg.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_expires.h"
#include "../presence/hash.h"
#include "../../hashes.h"
#include "hash.h"
#include "pua.h"
#include "send_subscribe.h"
#include "pua_callback.h"
#include "event_list.h"
#include "pua_db.h"
#include "../presence/subscribe.h"

extern db_locking_t db_table_lock;

void print_subs(subs_info_t* subs)
{
	LM_DBG("pres_uri= %.*s - len: %d\n", 
			subs->pres_uri->len,  subs->pres_uri->s, subs->pres_uri->len );
	LM_DBG("watcher_uri= %.*s - len: %d\n",
			subs->watcher_uri->len,  subs->watcher_uri->s,
			subs->watcher_uri->len);

}

str* subs_build_hdr(str* contact, int expires, int event, str* extra_headers)
{
	str* str_hdr= NULL;
	static char buf[3000];
	char* subs_expires= NULL;
	int len= 1;
	pua_event_t* ev;	

	str_hdr= (str*)pkg_malloc(sizeof(str));
	if(str_hdr== NULL)
	{
		LM_ERR("no more memory\n");
		return NULL;
	}
	memset(str_hdr, 0, sizeof(str));
	str_hdr->s= buf;
	
	ev= get_event(event);	
	if(ev== NULL)
	{
		LM_ERR("getting event from list\n");
		goto error;
	}

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
	
	memcpy(str_hdr->s+ str_hdr->len ,"Contact: <", 10);
	str_hdr->len += 10;
	memcpy(str_hdr->s +str_hdr->len, contact->s, 
			contact->len);
	str_hdr->len+= contact->len;
	memcpy(str_hdr->s+ str_hdr->len, ">", 1);
	str_hdr->len+= 1;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	memcpy(str_hdr->s+ str_hdr->len ,"Expires: ", 9);
	str_hdr->len += 9;

	if( expires<= min_expires)
		subs_expires= int2str(min_expires, &len);  
	else
		subs_expires= int2str(expires+ 10, &len);
		
	if(subs_expires == NULL || len == 0)
	{
		LM_ERR("while converting int to str\n");
		pkg_free(str_hdr);
		return NULL;
	}
	memcpy(str_hdr->s+str_hdr->len, subs_expires, len);
	str_hdr->len += len;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	if(extra_headers && extra_headers->len)
	{
		memcpy(str_hdr->s+str_hdr->len, extra_headers->s, extra_headers->len);
		str_hdr->len += extra_headers->len;
	}

	str_hdr->s[str_hdr->len]= '\0';

	return str_hdr;

error:
	if(str_hdr)
		pkg_free(str_hdr);
	return NULL;
}	

dlg_t* pua_build_dlg_t(ua_pres_t* presentity)	
{
	dlg_t* td =NULL;
	int size;

	size= sizeof(dlg_t)+ presentity->call_id.len+ presentity->to_tag.len+
		presentity->from_tag.len+ presentity->watcher_uri->len+ 
		presentity->pres_uri->len+ presentity->remote_contact.len;

	td = (dlg_t*)pkg_malloc(size);
	if(td == NULL)
	{
		LM_ERR("No memory left\n");
		return NULL;
	}
	memset(td, 0, size);
	size= sizeof(dlg_t);

	td->id.call_id.s = (char*)td+ size;
	memcpy(td->id.call_id.s, presentity->call_id.s, presentity->call_id.len);
	td->id.call_id.len= presentity->call_id.len;
	size+= presentity->call_id.len;

	td->id.rem_tag.s = (char*)td+ size;
	memcpy(td->id.rem_tag.s, presentity->to_tag.s, presentity->to_tag.len);
	td->id.rem_tag.len = presentity->to_tag.len;
	size+= presentity->to_tag.len;
	
	td->id.loc_tag.s = (char*)td+ size;
	memcpy(td->id.loc_tag.s, presentity->from_tag.s, presentity->from_tag.len);
	td->id.loc_tag.len =presentity->from_tag.len;
	size+= presentity->from_tag.len;
	
	td->loc_uri.s = (char*)td+ size;
	memcpy(td->loc_uri.s, presentity->watcher_uri->s,
			presentity->watcher_uri->len) ;
	td->loc_uri.len = presentity->watcher_uri->len;
	size+= td->loc_uri.len;
	
	td->rem_uri.s = (char*)td+ size;
	memcpy(td->rem_uri.s, presentity->pres_uri->s, presentity->pres_uri->len) ;
	td->rem_uri.len = presentity->pres_uri->len;
	size+= td->rem_uri.len;

	td->rem_target.s = (char*)td+ size;
	memcpy(td->rem_target.s, presentity->remote_contact.s,
			presentity->remote_contact.len) ;
	td->rem_target.len = presentity->remote_contact.len;
	size+= td->rem_target.len;
	
	if(presentity->record_route.s && presentity->record_route.len)
	{
		if(parse_rr_body(presentity->record_route.s, presentity->record_route.len,
				&td->route_set)< 0)
		{
			LM_ERR("ERROR in function parse_rr_body\n");
			pkg_free(td);
			return NULL;
		}
	}	

	td->loc_seq.value = presentity->cseq;
	td->loc_seq.is_set = 1;
	td->state= DLG_CONFIRMED ;
	
	LM_DBG("size = %d\n", size);

	return td;
}

/**
 * free the field in dlg_t as filled/allocated by this module
 */
static int pua_free_tm_dlg(dlg_t *td)
{
	if(td)
	{
		if(td->route_set)
			free_rr(&td->route_set);
		pkg_free(td);
	}
	return 0;
}

static void find_and_delete_dialog(ua_pres_t *dialog, int hash_code)
{
	ua_pres_t *presentity;

	if (dbmode == PUA_DB_ONLY)
	{
		delete_dialog_puadb(dialog);
	}
	else
	{
		lock_get(&HashT->p_records[hash_code].lock);
 		presentity= get_dialog(dialog, hash_code);
		if (presentity == NULL)
		{
			presentity = get_temporary_dialog(dialog, hash_code);
			if(presentity== NULL)
			{
				LM_ERR("no record found\n");
				lock_release(&HashT->p_records[hash_code].lock);
				return;
			}
		}

		delete_htable(presentity, hash_code);
		lock_release(&HashT->p_records[hash_code].lock);
	}
}

static void find_and_update_dialog(ua_pres_t *dialog, int hash_code, int lexpire, str *contact)
{
	ua_pres_t *presentity;

	if (dbmode == PUA_DB_ONLY)
	{
		update_dialog_puadb(dialog, lexpire, contact);
	}
	else
	{
		lock_get(&HashT->p_records[hash_code].lock);
  		presentity= get_dialog(dialog, hash_code);
		if (presentity == NULL)
		{
			LM_ERR("no record found\n");
			lock_release(&HashT->p_records[hash_code].lock);
			return;
		}

		update_htable(presentity, dialog->desired_expires, lexpire, NULL, hash_code, contact);
		lock_release(&HashT->p_records[hash_code].lock);
	}
}

void subs_cback_func(struct cell *t, int cb_type, struct tmcb_params *ps)
{
	struct sip_msg* msg= NULL;
	int lexpire= 0;
	unsigned int cseq;
	ua_pres_t* presentity= NULL, *hentity= NULL;
	struct to_body *pto = NULL, TO = {0}, *pfrom = NULL;
	int size= 0;
	unsigned int hash_code;
	int flag ;
	str record_route= {0, 0};
	int rt;
	str contact;
	int initial_request = 0;
	int end_transaction = 1;

	if( ps->param== NULL || *ps->param== NULL )
	{
		LM_ERR("null callback parameter\n");
		return;
	}

	if (dbmode == PUA_DB_ONLY && pua_dbf.start_transaction)
	{
		if (pua_dbf.start_transaction(pua_db, db_table_lock) < 0)
		{
			LM_ERR("in start_transaction\n");
			goto error;
		}
	}

	LM_DBG("completed with status %d\n",ps->code) ;
	hentity= (ua_pres_t*)(*ps->param);
	hash_code= core_hash(hentity->pres_uri,hentity->watcher_uri,
				HASH_SIZE);
	flag= hentity->flag;
	if(hentity->flag & XMPP_INITIAL_SUBS)
		hentity->flag= XMPP_SUBSCRIBE;

	/* get dialog information from reply message: callid, to_tag, from_tag */
	msg= ps->rpl;
	if(msg == NULL)
	{
		LM_ERR("no reply message found\n ");
		goto error;
	}

	if(msg== FAKED_REPLY)
	{
		struct hdr_field *callid = NULL, *from = NULL;
		struct to_body FROM = {0};

		callid = (struct hdr_field *) pkg_malloc(sizeof(struct hdr_field));
		if (callid == NULL)
		{
			LM_ERR("Out of memory\n");
			goto faked_error;
		}
		memset(callid, 0, sizeof(struct hdr_field));
		get_hdr_field(t->callid.s, t->callid.s + t->callid.len, callid);
		hentity->call_id = callid->body;

		from = (struct hdr_field *) pkg_malloc(sizeof(struct hdr_field));
		if (from == NULL)
		{
			LM_ERR("Out of memory\n");
			goto faked_error;
		}
		memset(from, 0, sizeof(struct hdr_field));
		get_hdr_field(t->from.s, t->from.s + t->from.len, from);
		parse_to(from->body.s, from->body.s + from->body.len + 1, &FROM);
		if(FROM.uri.len <= 0) 
		{
			LM_ERR("'From' header NOT parsed\n");
			goto faked_error;
		}
	
		hentity->call_id = callid->body;
		hentity->from_tag = (&FROM)->tag_value;
		hentity->to_tag.s = NULL;
		hentity->to_tag.len = 0;

		find_and_delete_dialog(hentity, hash_code);
faked_error:
		if (callid) pkg_free(callid);
		free_to_params(&FROM);
		if (from) pkg_free(from);
		goto done;
	}
	
	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("when parsing headers\n");
		goto error;
	}

	if(ps->rpl->expires && msg->expires->body.len > 0)
	{
		if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
		{
			LM_ERR("cannot parse Expires header\n");
			goto error;
		}
		lexpire = ((exp_body_t*)msg->expires->parsed)->val;
		LM_DBG("lexpire= %d\n", lexpire);
	}

	/*if initial request */
	if(hentity->call_id.s== NULL)
	{
		initial_request = 1;

		if( msg->callid==NULL || msg->callid->body.s==NULL)
		{
			LM_ERR("cannot parse callid header\n");
			goto error;
		}
		
		if (!msg->from || !msg->from->body.s)
		{
			LM_ERR("cannot find 'from' header!\n");
			goto error;
		}
		if (msg->from->parsed == NULL)
		{
			if ( parse_from_header( msg )<0 ) 
			{
				LM_ERR("cannot parse From header\n");
				goto error;
			}
		}
		pfrom = (struct to_body*)msg->from->parsed;
	
		if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
		{
			LM_ERR("no from tag value present\n");
			goto error;
		}

		hentity->call_id=  msg->callid->body;
		hentity->from_tag= pfrom->tag_value;

		if(ps->code >= 300 || lexpire == 0)
		{
			hentity->to_tag.s = NULL;
			hentity->to_tag.len = 0;
			find_and_delete_dialog(hentity, hash_code);
			goto done;
		}

		if( msg->to==NULL || msg->to->body.s==NULL)
		{
			LM_ERR("cannot parse TO header\n");
			goto error;
		}			
		if(msg->to->parsed != NULL)
		{
			pto = (struct to_body*)msg->to->parsed;
			LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",pto->uri.len,pto->uri.s);
		}
		else
		{
			parse_to(msg->to->body.s,msg->to->body.s +
				msg->to->body.len + 1, &TO);
			if(TO.uri.len <= 0) 
			{
				LM_ERR("'To' header NOT parsed\n");
				goto error;
			}
			pto = &TO;
		}			
		if( pto->tag_value.s ==NULL || pto->tag_value.len == 0)
		{
			LM_ERR("no to tag value present\n");
			goto error;
		}
		hentity->to_tag= pto->tag_value;
	}

	if(ps->code >= 300 )
	{	/* if an error code and a stored dialog delete it and try to send 
		   a subscription with type= INSERT_TYPE, else return*/	
		
		subs_info_t subs;

		hentity->to_tag.s = NULL;
		hentity->to_tag.len = 0;
		find_and_delete_dialog(hentity, hash_code);

		if (dbmode == PUA_DB_ONLY && pua_dbf.end_transaction)
		{
			if (pua_dbf.end_transaction(pua_db) < 0)
			{
				LM_ERR("in end_transaction\n");
				goto error;
			}
		}

		end_transaction = 0;

		/* Redirect if the response 3XX */
		memset(&subs, 0, sizeof(subs_info_t));
		subs.pres_uri= hentity->pres_uri; 
		subs.watcher_uri= hentity->watcher_uri;
		subs.contact= &hentity->contact;

		if(hentity->remote_contact.s)
			subs.remote_target= &hentity->remote_contact;

		if(hentity->desired_expires== 0)
			subs.expires= -1;
		else
		if(hentity->desired_expires< (int)time(NULL))
			subs.expires= 0;
		else
			subs.expires= hentity->desired_expires- (int)time(NULL)+ 3;

		subs.flag= INSERT_TYPE;
		subs.source_flag= flag;
		subs.event= hentity->event;
		subs.id= hentity->id;
		subs.outbound_proxy= hentity->outbound_proxy;
		subs.extra_headers= hentity->extra_headers;
		subs.cb_param= hentity->cb_param;
	
		if(send_subscribe(&subs)< 0)
		{
			LM_ERR("when trying to send SUBSCRIBE\n");
			goto error;
		}
		goto done;
	}

	if(lexpire== 0 )
	{
		LM_DBG("lexpire= 0 Delete from hash table");
		find_and_delete_dialog(hentity, hash_code);
		goto done;
	}

	/* extract the contact */
	if(msg->contact== NULL || msg->contact->body.s== NULL)
	{
		LM_ERR("no contact header found");
		goto error;
	}
	if( parse_contact(msg->contact) <0 )
	{
		LM_ERR(" cannot parse contact header\n");
		goto error;
	}
	if(msg->contact->parsed == NULL)
	{
		LM_ERR("cannot parse contact header\n");
		goto error;
	}
	contact = ((contact_body_t* )msg->contact->parsed)->contacts->uri;

	if( msg->cseq==NULL || msg->cseq->body.s==NULL)
	{
		LM_ERR("cannot parse cseq header\n");
		goto error;
	}

	if( str2int( &(get_cseq(msg)->number), &cseq)< 0)
	{
		LM_ERR("while converting str to int\n");
		goto error;
	}

	if(initial_request == 0)
	{
		hentity->cseq = cseq;
		find_and_update_dialog(hentity, hash_code, lexpire, &contact);
		goto done;
	}

	/*process record route and add it to a string*/
	if (msg->record_route!=NULL)
	{
		rt = print_rr_body(msg->record_route, &record_route, 1, 0);
		if(rt != 0)
		{
			LM_ERR("parsing record route [%d]\n", rt);	
			record_route.s=NULL;
			record_route.len=0;
		}
	}

	size= sizeof(ua_pres_t)+ 2*sizeof(str)+( pto->uri.len+
		pfrom->uri.len+ pto->tag_value.len+ pfrom->tag_value.len
		+msg->callid->body.len+ record_route.len+ hentity->contact.len+
		hentity->id.len )*sizeof(char);

	if(hentity->extra_headers)
		size+= sizeof(str)+ hentity->extra_headers->len*sizeof(char);

	presentity= (ua_pres_t*)shm_malloc(size);
	if(presentity== NULL)
	{
		LM_ERR("no more share memory\n");
		goto error;
	}
	
	memset(presentity, 0, size);
	size= sizeof(ua_pres_t);

	presentity->pres_uri= (str*)( (char*)presentity+ size);
	size+= sizeof(str);
	presentity->pres_uri->s= (char*)presentity+ size;
	memcpy(presentity->pres_uri->s, pto->uri.s, pto->uri.len);
	presentity->pres_uri->len= pto->uri.len;
	size+= pto->uri.len;

	presentity->watcher_uri= (str*)( (char*)presentity+ size);
	size+= sizeof(str);
	presentity->watcher_uri->s= (char*)presentity+ size;
	memcpy(presentity->watcher_uri->s, pfrom->uri.s, pfrom->uri.len);
	presentity->watcher_uri->len= pfrom->uri.len;
	size+= pfrom->uri.len;

	presentity->call_id.s= (char*)presentity + size;
	memcpy(presentity->call_id.s,msg->callid->body.s, 
		msg->callid->body.len);
	presentity->call_id.len= msg->callid->body.len;
	size+= presentity->call_id.len;

	presentity->to_tag.s= (char*)presentity + size;
	memcpy(presentity->to_tag.s,pto->tag_value.s, 
			pto->tag_value.len);
	presentity->to_tag.len= pto->tag_value.len;
	size+= pto->tag_value.len;

	presentity->from_tag.s= (char*)presentity + size;
	memcpy(presentity->from_tag.s,pfrom->tag_value.s, 
			pfrom->tag_value.len);
	presentity->from_tag.len= pfrom->tag_value.len;
	size+= pfrom->tag_value.len;

	if(record_route.len && record_route.s)
	{
		presentity->record_route.s= (char*)presentity + size;
		memcpy(presentity->record_route.s, record_route.s, record_route.len);
		presentity->record_route.len= record_route.len;
		size+= record_route.len;
		pkg_free(record_route.s);
		record_route.s = NULL;
	}

	presentity->contact.s= (char*)presentity + size;
	memcpy(presentity->contact.s, hentity->contact.s, hentity->contact.len);
	presentity->contact.len= hentity->contact.len;
	size+= hentity->contact.len;

	if(hentity->id.s)
	{
		presentity->id.s=(char*)presentity+ size;
		memcpy(presentity->id.s, hentity->id.s, 
			hentity->id.len);
		presentity->id.len= hentity->id.len; 
		size+= presentity->id.len;
	}

	if(hentity->extra_headers)
	{
		presentity->extra_headers= (str*)((char*)presentity+ size);
		size+= sizeof(str);
		presentity->extra_headers->s=(char*)presentity+ size;
		memcpy(presentity->extra_headers->s, hentity->extra_headers->s, 
			hentity->extra_headers->len);
		presentity->extra_headers->len= hentity->extra_headers->len; 
		size+= hentity->extra_headers->len;
	}

	/* write the remote contact filed */
	presentity->remote_contact.s= (char*)shm_malloc(contact.len* sizeof(char));
	if(presentity->remote_contact.s==NULL)
	{
		ERR_MEM(SHARE_MEM);
	}
	memcpy(presentity->remote_contact.s, contact.s, contact.len);
	presentity->remote_contact.len= contact.len;

	presentity->event|= hentity->event;
	presentity->flag= hentity->flag;
	presentity->etag.s= NULL;
	presentity->cseq= cseq;
	presentity->desired_expires= hentity->desired_expires;
	presentity->expires= lexpire+ (int)time(NULL);
	if(BLA_SUBSCRIBE & presentity->flag)
	{
		LM_DBG("BLA_SUBSCRIBE FLAG inserted\n");
	}	
	LM_DBG("record for subscribe from %.*s to %.*s inserted in datatbase\n",
			presentity->watcher_uri->len, presentity->watcher_uri->s,
			presentity->pres_uri->len, presentity->pres_uri->s);

	if (dbmode==PUA_DB_ONLY)
	{
		if (pua_dbf.end_transaction)
		{
			if (pua_dbf.end_transaction(pua_db) < 0)
			{
				LM_ERR("in end_transaction\n");
				goto error;
			}
		}

		if (pua_dbf.start_transaction)
		{
			if (pua_dbf.start_transaction(pua_db, db_table_lock) < 0)
			{
				LM_ERR("in start_transaction\n");
				goto error;
			}
		}

		if (convert_temporary_dialog_puadb(presentity) < 0)
		{
			LM_ERR("Could not convert temporary dialog into a dialog\n");
			goto error;
		}
	}
	else
	{
		if (convert_temporary_dialog(presentity) < 0)
		{
			LM_ERR("Could not convert temporary dialog into a dialog\n");
			goto error;
		}
	}

done:
	if(hentity->ua_flag == REQ_OTHER)
	{
		hentity->flag= flag;
		run_pua_callbacks( hentity, msg);
	}

	if (dbmode == PUA_DB_ONLY && pua_dbf.end_transaction && end_transaction)
	{
		if (pua_dbf.end_transaction(pua_db) < 0)
		{
			LM_ERR("in end_transaction\n");
			goto error;
		}
	}

	goto end;

error:	
        if (presentity)
	{
		if (presentity->remote_contact.s) shm_free(presentity->remote_contact.s);
	 	shm_free(presentity);
	}

	if(record_route.s)
		pkg_free(record_route.s);

	if (dbmode == PUA_DB_ONLY && pua_dbf.abort_transaction)
	{
		if (pua_dbf.abort_transaction(pua_db) < 0)
			LM_ERR("in abort_transaction\n");
	}

end:

	if(hentity)
	{	
		shm_free(hentity);
		hentity= NULL;
	}

	free_to_params(&TO);
	return;
}

ua_pres_t* subscribe_cbparam(subs_info_t* subs, int ua_flag)
{	
	ua_pres_t* hentity= NULL;
	int size;

	size= sizeof(ua_pres_t)+ 2*sizeof(str)+(subs->pres_uri->len+
		subs->watcher_uri->len+ subs->contact->len+ subs->id.len+ 1)*
		sizeof(char);
	
	if(subs->outbound_proxy && subs->outbound_proxy->len && subs->outbound_proxy->s )
		size+= sizeof(str)+ subs->outbound_proxy->len* sizeof(char);

	if(subs->extra_headers && subs->extra_headers->s)
		size+= sizeof(str)+ subs->extra_headers->len* sizeof(char);

	hentity= (ua_pres_t*)shm_malloc(size);
	if(hentity== NULL)
	{
		LM_ERR("No more share memory\n");
		return NULL;
	}
	memset(hentity, 0, size);

	size= sizeof(ua_pres_t);

	hentity->pres_uri = (str*)((char*)hentity + size);
	size+= sizeof(str);

	hentity->pres_uri->s = (char*)hentity+ size;
	memcpy(hentity->pres_uri->s, subs->pres_uri->s ,
		subs->pres_uri->len ) ;
	hentity->pres_uri->len= subs->pres_uri->len;
	size+= subs->pres_uri->len;

	hentity->watcher_uri = (str*)((char*)hentity + size);
	size+= sizeof(str);

	hentity->watcher_uri->s = (char*)hentity+ size;
	memcpy(hentity->watcher_uri->s, subs->watcher_uri->s ,
		subs->watcher_uri->len ) ;
	hentity->watcher_uri->len= subs->watcher_uri->len;
	size+= subs->watcher_uri->len;

	hentity->contact.s = (char*)hentity+ size;
	memcpy(hentity->contact.s, subs->contact->s ,
		subs->contact->len );
	hentity->contact.len= subs->contact->len;
	size+= subs->contact->len;

	if(subs->outbound_proxy)
	{
		hentity->outbound_proxy= (str*)((char*)hentity+ size);
		size+= sizeof(str);
		hentity->outbound_proxy->s= (char*)hentity+ size;
		memcpy(hentity->outbound_proxy->s, subs->outbound_proxy->s, subs->outbound_proxy->len);
		hentity->outbound_proxy->len= subs->outbound_proxy->len;
		size+= subs->outbound_proxy->len;
	}	
	if(subs->expires< 0)
		hentity->desired_expires= 0;
	else
		hentity->desired_expires=subs->expires+ (int)time(NULL);

	if(subs->id.s)
	{
		CONT_COPY(hentity, hentity->id, subs->id)
	}
	if(subs->extra_headers)
	{
		hentity->extra_headers= (str*)((char*)hentity+ size);
		size+= sizeof(str);
		hentity->extra_headers->s= (char*)hentity+ size;
		memcpy(hentity->extra_headers->s, subs->extra_headers->s,
				subs->extra_headers->len);
		hentity->extra_headers->len= subs->extra_headers->len;
		size+= subs->extra_headers->len;
	}
	hentity->flag= subs->source_flag;
	hentity->event= subs->event;
	hentity->ua_flag= hentity->ua_flag;
	hentity->cb_param= subs->cb_param;
	return hentity;

}	

ua_pres_t* subs_cbparam_indlg(ua_pres_t* subs, int expires, int ua_flag)
{	
	ua_pres_t* hentity= NULL;
	int size;

	size= sizeof(ua_pres_t)+ 2*sizeof(str)+subs->pres_uri->len+
		subs->watcher_uri->len+ subs->contact.len+ subs->id.len+
		subs->to_tag.len+ subs->call_id.len+ subs->from_tag.len+ 1;
	
	if(subs->outbound_proxy && subs->outbound_proxy->len && subs->outbound_proxy->s )
		size+= sizeof(str)+ subs->outbound_proxy->len;

	if(subs->extra_headers && subs->extra_headers->s)
		size+= sizeof(str)+ subs->extra_headers->len;

	if(subs->remote_contact.s)
		size+= subs->remote_contact.len;

	hentity= (ua_pres_t*)shm_malloc(size);
	if(hentity== NULL)
	{
		LM_ERR("No more share memory\n");
		return NULL;
	}
	memset(hentity, 0, size);

	size= sizeof(ua_pres_t);

	hentity->pres_uri = (str*)((char*)hentity + size);
	size+= sizeof(str);

	hentity->pres_uri->s = (char*)hentity+ size;
	memcpy(hentity->pres_uri->s, subs->pres_uri->s ,
		subs->pres_uri->len ) ;
	hentity->pres_uri->len= subs->pres_uri->len;
	size+= subs->pres_uri->len;

	hentity->watcher_uri = (str*)((char*)hentity + size);
	size+= sizeof(str);

	hentity->watcher_uri->s = (char*)hentity+ size;
	memcpy(hentity->watcher_uri->s, subs->watcher_uri->s ,
		subs->watcher_uri->len ) ;
	hentity->watcher_uri->len= subs->watcher_uri->len;
	size+= subs->watcher_uri->len;

	CONT_COPY(hentity, hentity->contact, subs->contact)

	if(subs->outbound_proxy && subs->outbound_proxy->len && subs->outbound_proxy->s)
	{
		hentity->outbound_proxy= (str*)((char*)hentity+ size);
		size+= sizeof(str);
		hentity->outbound_proxy->s= (char*)hentity+ size;
		memcpy(hentity->outbound_proxy->s, subs->outbound_proxy->s, subs->outbound_proxy->len);
		hentity->outbound_proxy->len= subs->outbound_proxy->len;
		size+= subs->outbound_proxy->len;
	}	

	if(subs->id.s)
	{
		CONT_COPY(hentity, hentity->id, subs->id)
	}
	
	if(subs->remote_contact.s)
	{
		CONT_COPY(hentity, hentity->remote_contact, subs->remote_contact)
	}

	if(subs->extra_headers && subs->extra_headers->s)
	{
		hentity->extra_headers= (str*)((char*)hentity+ size);
		size+= sizeof(str);
		hentity->extra_headers->s= (char*)hentity+ size;
		memcpy(hentity->extra_headers->s, subs->extra_headers->s,
				subs->extra_headers->len);
		hentity->extra_headers->len= subs->extra_headers->len;
		size+= subs->extra_headers->len;
	}
	/* copy dialog information */
	
	CONT_COPY(hentity, hentity->to_tag, subs->to_tag)
	CONT_COPY(hentity, hentity->from_tag, subs->from_tag)
	CONT_COPY(hentity, hentity->call_id, subs->call_id)
	
	if(expires< 0)
		hentity->desired_expires= 0;
	else
		hentity->desired_expires=expires+ (int)time(NULL);

	hentity->flag= subs->flag;
	hentity->event= subs->event;
	hentity->ua_flag= hentity->ua_flag;
	hentity->cb_param= subs->cb_param;

	return hentity;

}	


/**
 *
 */
int send_subscribe(subs_info_t* subs)
{
	ua_pres_t* presentity= NULL;
	str met= {"SUBSCRIBE", 9};
	str* str_hdr= NULL;
	int ret= -1;
	unsigned int hash_code=0;
	ua_pres_t* hentity= NULL;
	int expires;
	int flag;
	int result;
	uac_req_t uac_r;
	db1_res_t *res=NULL;
	ua_pres_t dbpres;
	str pres_uri={0,0}, watcher_uri={0,0}, extra_headers={0,0};
	dlg_t* td= NULL;


	memset(&dbpres, 0, sizeof(dbpres));
	dbpres.pres_uri = &pres_uri;
	dbpres.watcher_uri = &watcher_uri;
	dbpres.extra_headers = &extra_headers; 

	print_subs(subs);

	flag= subs->source_flag;
	if(subs->source_flag & XMPP_INITIAL_SUBS)
		subs->source_flag= XMPP_SUBSCRIBE;

	if(subs->expires< 0)
		expires= 3600;
	else
		expires= subs->expires;

	str_hdr= subs_build_hdr(subs->contact, expires, subs->event, 
			subs->extra_headers);
	if(str_hdr== NULL || str_hdr->s== NULL)
	{
		LM_ERR("while building extra headers\n");
		return -1;
	}

	if (dbmode == PUA_DB_ONLY && pua_dbf.start_transaction)
	{
		if (pua_dbf.start_transaction(pua_db, db_table_lock) < 0)
		{
			LM_ERR("in start_transaction\n");
			goto error;
		}
	}

	/* generation of hash and getting lock moved from here to further down */

	if (dbmode==PUA_DB_ONLY)
	{
		presentity = get_dialog_puadb(subs->id, subs->pres_uri, &dbpres, &res);
	}
	else
	{
		ua_pres_t pres;

		memset(&pres, 0, sizeof(ua_pres_t));
		pres.pres_uri = subs->pres_uri;
		pres.watcher_uri = subs->watcher_uri;
		pres.flag = subs->source_flag;
		pres.id = subs->id;
		pres.event = subs->event;
		if (subs->remote_target)
			pres.remote_contact = *subs->remote_target;

		hash_code=core_hash(subs->pres_uri, subs->watcher_uri, HASH_SIZE);
		lock_get(&HashT->p_records[hash_code].lock);
		presentity= search_htable(&pres, hash_code);
	}

	/* if flag == INSERT_TYPE insert no matter what the search result is */
	if(subs->flag & INSERT_TYPE)
	{
		LM_DBG("A subscription request with insert type\n");
		goto insert;
	}
	
	if(presentity== NULL )
	{
		int size;
insert:
	
		if (subs->expires == 0)
		{
			/* Don't create a new dialog when expires == 0 */
       			if (dbmode != PUA_DB_ONLY)
				lock_release(&HashT->p_records[hash_code].lock);
			goto done;
		}

		if(subs->flag & UPDATE_TYPE)
		{
			LM_DBG("request for a subscription with update type"
					" and no record found\n");
			subs->flag= INSERT_TYPE;
		}	
		hentity= subscribe_cbparam(subs, REQ_OTHER);
		if(hentity== NULL)
		{
			LM_ERR("while building callback"
					" param\n");
       			if (dbmode != PUA_DB_ONLY)
				lock_release(&HashT->p_records[hash_code].lock);
			goto error;
		}
		hentity->flag= flag;

		set_uac_req(&uac_r, &met, str_hdr, 0, 0, TMCB_LOCAL_COMPLETED,
				subs_cback_func, (void*)hentity);
		result= tmb.t_request_outside
			(&uac_r,						  /* Type of the message */
		subs->remote_target?subs->remote_target:subs->pres_uri,/* Request-URI*/
			subs->pres_uri,				  /* To */
			subs->watcher_uri,			  /* From */
			subs->outbound_proxy		  /* Outbound_proxy */	
			);
		if(result< 0)
		{
			LM_ERR("while sending request with t_request\n");
			if (uac_r.dialog != NULL)
			{
				uac_r.dialog->rem_target.s = 0;
				uac_r.dialog->dst_uri.s = 0;
				tmb.free_dlg(uac_r.dialog);
				uac_r.dialog = 0;
			}
			shm_free(hentity);
       			if (dbmode != PUA_DB_ONLY)
				lock_release(&HashT->p_records[hash_code].lock);

			/* Although this is an error must not return -1 as the
			   calling function must continue processing. */
			ret = 0;
			goto error;
		}

		/* Now create a temporary hash table entry.
		   This is needed to deal with the race-hazard when NOTIFYs
		   arrive before the 2xx response to the SUBSCRIBE. */
		size = sizeof(ua_pres_t)+ 2 * sizeof(str) + (
			subs->pres_uri->len +
			subs->watcher_uri->len +
			uac_r.dialog->id.loc_tag.len +
			uac_r.dialog->id.call_id.len +
			subs->id.len) * sizeof(char);

		presentity= (ua_pres_t*)shm_malloc(size);
		if(presentity== NULL)
		{
			LM_ERR("no more share memory\n");
       			if (dbmode != PUA_DB_ONLY)
				lock_release(&HashT->p_records[hash_code].lock);
			goto error;
		}
		memset(presentity, 0, size);
		size= sizeof(ua_pres_t);

		presentity->pres_uri = (str *) ((char *) presentity + size);
		size += sizeof(str);
		presentity->pres_uri->s= (char *) presentity + size;
		memcpy(presentity->pres_uri->s, subs->pres_uri->s, subs->pres_uri->len);
		presentity->pres_uri->len= subs->pres_uri->len;
		size+= subs->pres_uri->len;

		presentity->watcher_uri= (str *) ((char *) presentity + size);
		size += sizeof(str);
		presentity->watcher_uri->s= (char *) presentity + size;
		memcpy(presentity->watcher_uri->s, subs->watcher_uri->s, subs->watcher_uri->len);
		presentity->watcher_uri->len = subs->watcher_uri->len;
		size += subs->watcher_uri->len;

		presentity->call_id.s = (char *) presentity + size;
		memcpy(presentity->call_id.s, uac_r.dialog->id.call_id.s, uac_r.dialog->id.call_id.len);
		presentity->call_id.len = uac_r.dialog->id.call_id.len;
		size += uac_r.dialog->id.call_id.len;

		presentity->from_tag.s = (char *) presentity + size;
		memcpy(presentity->from_tag.s, uac_r.dialog->id.loc_tag.s, uac_r.dialog->id.loc_tag.len);
		presentity->from_tag.len= uac_r.dialog->id.loc_tag.len;
		size += uac_r.dialog->id.loc_tag.len;

		presentity->id.s = (char *) presentity+ size;
		memcpy(presentity->id.s, subs->id.s, subs->id.len);
		presentity->id.len = subs->id.len;
		size += subs->id.len;

		presentity->event = subs->event;
		presentity->flag = subs->source_flag;
		presentity->cseq = uac_r.dialog->loc_seq.value;

		/* Set the temporary record expiry for 2 * 64T1 seconds from now */
		presentity->expires= (int)time(NULL) + 64;
		presentity->desired_expires= presentity->expires;

		if (dbmode==PUA_DB_ONLY)
		{
			insert_dialog_puadb(presentity);
			shm_free(presentity);
		}
		else
		{
			insert_htable(presentity, hash_code);
			lock_release(&HashT->p_records[hash_code].lock);
		}

		uac_r.dialog->rem_target.s = 0;
		uac_r.dialog->dst_uri.s = 0;
		tmb.free_dlg(uac_r.dialog);
		uac_r.dialog = 0;
	}
	else
	{
		if (subs->internal_update_flag == INTERNAL_UPDATE_TRUE)
		{
			LM_INFO("attempting to re-SUBSCRIBE on internal (rls_update_subs()) update - skipping\n");
			if (dbmode != PUA_DB_ONLY)
				lock_release(&HashT->p_records[hash_code].lock);
			goto done;
		}

		if (presentity->to_tag.len == 0)
		{
			if (subs->expires > 0)
				LM_WARN("attempting to re-SUBSCRIBE to a temporary (non-established) dialog - skipping\n");
			else
			{
				LM_WARN("attempting to un-SUBSCRIBE from a temporary (non-established) dialog - skipping and deleting dialog\n");
				if (dbmode==PUA_DB_ONLY)
					delete_dialog_puadb(presentity);
				else
					delete_htable(presentity, hash_code);
			}

			if (dbmode != PUA_DB_ONLY)
				lock_release(&HashT->p_records[hash_code].lock);
			goto done;
		}

		td= pua_build_dlg_t(presentity);
		if(td== NULL)
		{
			LM_ERR("while building tm dlg_t structure");
			if (dbmode!=PUA_DB_ONLY)
				lock_release(&HashT->p_records[hash_code].lock);
			goto error;
		}
				
		hentity= subs_cbparam_indlg(presentity, expires, REQ_OTHER);
		if(hentity== NULL)
		{
			LM_ERR("while building callback param\n");
			if (dbmode!=PUA_DB_ONLY)
				lock_release(&HashT->p_records[hash_code].lock);
			goto error;
		}
		if (dbmode!=PUA_DB_ONLY)
			lock_release(&HashT->p_records[hash_code].lock);

		LM_DBG("event parameter: %d\n", hentity->event);	

		set_uac_req(&uac_r, &met, str_hdr, 0, td, TMCB_LOCAL_COMPLETED,
				subs_cback_func, (void*)hentity);
		result= tmb.t_request_within(&uac_r);
		if(result< 0)
		{
			shm_free(hentity);
			hentity= NULL;
			LM_ERR("while sending request with t_request\n");
			goto error;
		}
	}


done:
	if (dbmode == PUA_DB_ONLY && pua_dbf.end_transaction)
	{
		if (pua_dbf.end_transaction(pua_db) < 0)
		{
			LM_ERR("in end_transaction\n");
			goto error;
		}
	}

	ret = 0;

error:
	pua_free_tm_dlg(td);
	pkg_free(str_hdr);
	free_results_puadb(res);

	if (dbmode == PUA_DB_ONLY && pua_dbf.abort_transaction)
	{
		if (pua_dbf.abort_transaction(pua_db) < 0)
			LM_ERR("in abort_transaction\n");
	}

	return ret;
}

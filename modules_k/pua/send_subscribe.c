/*
 * $Id$
 *
 * pua module - presence user agent module
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
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libxml/parser.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../tm/tm_load.h"
#include "../tm/dlg.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_expires.h"
#include "hash.h"
#include "pua.h"
#include "send_subscribe.h"
#include "pua_callback.h"

extern int default_expires;
extern int min_expires;

void print_subs(subs_info_t* subs)
{
	DBG("PUA:send_subscribe\tpres_uri= %.*s - len: %d\n", 
			subs->pres_uri->len,  subs->pres_uri->s, subs->pres_uri->len );
	DBG("PUA:send_subscribe\twatcher_uri= %.*s - len: %d\n",
			subs->watcher_uri->len,  subs->watcher_uri->s,
			subs->watcher_uri->len);

}

str* subs_build_hdr(str* contact, int expires, int event)
{
	str* str_hdr= NULL;
	static char buf[3000];
	char* subs_expires= NULL;
	int len= 1;

	str_hdr= (str*)pkg_malloc(sizeof(str));
	if(str_hdr== NULL)
	{
		LOG(L_ERR, "PUA:subs_build_hdr:ERROR while allocating memory\n");
		return NULL;
	}
	memset(str_hdr, 0, sizeof(str));
	str_hdr->s= buf;

	if(event& PRESENCE_EVENT)
	{	
		memcpy(str_hdr->s ,"Event: presence", 15);
		str_hdr->len = 15;
	}
	else
	if(event& PWINFO_EVENT)	
	{	
		memcpy(str_hdr->s ,"Event: presence.winfo", 21);
		str_hdr->len = 21;
	}
	else
	if(event& BLA_EVENT)	
	{	
		memcpy(str_hdr->s ,"Event: dialog;sla", 17);
		str_hdr->len = 17;
	}
	else
	{
		LOG(L_ERR, "PUA:subs_build_hdr:ERROR wrong event parameter\n");
		pkg_free(str_hdr);
		return NULL;
	}

	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	
	memcpy(str_hdr->s+ str_hdr->len ,"Contact: ", 9);
	str_hdr->len += 9;
	memcpy(str_hdr->s +str_hdr->len, contact->s, 
			contact->len);
	str_hdr->len+= contact->len;
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
		LOG(L_ERR, "PUA:subs_build_hdr: ERROR while converting int "
				" to str\n");
		pkg_free(str_hdr);
		return NULL;
	}
	memcpy(str_hdr->s+str_hdr->len, subs_expires, len);
	str_hdr->len += len;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	str_hdr->s[str_hdr->len]= '\0';

	return str_hdr;
}	

dlg_t* pua_build_dlg_t(ua_pres_t* presentity)	
{
	dlg_t* td =NULL;
	int size;

	size= sizeof(dlg_t)+ ( presentity->call_id.len+ presentity->to_tag.len+
		presentity->from_tag.len+ presentity->watcher_uri->len+
		presentity->pres_uri->len+ 1)* sizeof(char);

	if(presentity->outbound_proxy)
		size+= presentity->outbound_proxy->len* sizeof(char);
	else
		size+= presentity->pres_uri->len* sizeof(char);

	td = (dlg_t*)pkg_malloc(size);
	if(td == NULL)
	{
		LOG(L_ERR, "PUA:pua_build_dlg_t: No memory left\n");
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
	if(presentity->outbound_proxy)
	{
		memcpy(td->rem_target.s, presentity->outbound_proxy->s,
				presentity->outbound_proxy->len) ;
		td->rem_target.len = presentity->outbound_proxy->len;
		size+= td->rem_target.len;

	}
	else
	{	
		memcpy(td->rem_target.s, presentity->pres_uri->s,
				presentity->pres_uri->len) ;
		td->rem_target.len = presentity->pres_uri->len;
		size+= td->rem_target.len;
	}

	td->loc_seq.value = presentity->cseq;
	td->loc_seq.is_set = 1;
	td->state= DLG_CONFIRMED ;
	
	return td;
}

void subs_cback_func(struct cell *t, int cb_type, struct tmcb_params *ps)
{
	struct sip_msg* msg= NULL;
	int lexpire= 0;
	unsigned int cseq;
	ua_pres_t* presentity= NULL, *hentity= NULL;
	struct to_body *pto= NULL, *pfrom = NULL, TO;
	int size= 0;
	unsigned int hash_code;
	int flag ;

	if( ps->param== NULL || *ps->param== NULL )
	{
		LOG(L_ERR, "PUA:subs_cback_func:ERROR null callback parameter\n");
		return;
	}
	DBG("PUA:subs_cback_func: completed with status %d\n",ps->code) ;
	hentity= (ua_pres_t*)(*ps->param);
	hash_code= core_hash(hentity->pres_uri,hentity->watcher_uri,
				HASH_SIZE);
	flag= hentity->flag;
	if(hentity->flag & XMPP_INITIAL_SUBS)
		hentity->flag= XMPP_SUBSCRIBE;

	/* get dialog information from reply message: callid, to_tag, from_tag */
	msg= ps->rpl;
	if(msg == NULL || msg== FAKED_REPLY)
	{
		LOG(L_ERR, "PUA:subs_cback_func: no reply message found\n ");
		goto done;
	}

	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LOG(L_ERR, "PUA:subs_cback_func: error parsing headers\n");
		goto done;
	}
	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LOG(L_ERR, "PUA: subs_cback_func: ERROR cannot parse callid"
		" header\n");
		goto done;
	}		
	if (!msg->from || !msg->from->body.s)
	{
		DBG("PUA:subs_cback_func: ERROR cannot find 'from' header!\n");
		goto done;
	}
	if (msg->from->parsed == NULL)
	{
		if ( parse_from_header( msg )<0 ) 
		{
			DBG("PUA:subs_cback_func: ERROR cannot parse From header\n");
			goto done;
		}
	}
	pfrom = (struct to_body*)msg->from->parsed;
	
	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LOG(L_ERR, "PUA: subs_cback_func: ERROR no from tag value"
			" present\n");
		goto done;
	}	
	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LOG(L_ERR, "PUA: subs_cback_func: ERROR cannot parse TO"
				" header\n");
		goto done;
	}		
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		DBG("PUA: subs_cback_func: 'To' header ALREADY PARSED: <%.*s>\n",
				pto->uri.len, pto->uri.s );	
	}
	else
	{
		memset( &TO , 0, sizeof(TO) );
		parse_to(msg->to->body.s,msg->to->body.s +
				msg->to->body.len + 1, &TO);
		if(TO.uri.len <= 0) 
		{
			DBG("PUA: subs_cback_func: 'To' header NOT parsed\n");
			goto done;
		}
		pto = &TO;
	}		
	if( pto->tag_value.s ==NULL || pto->tag_value.len == 0)
	{
		LOG(L_ERR, "PUA: subs_cback_func: ERROR no from tag value"
			" present\n");
		goto done;
	}
	/* extract the other necesary information for inserting a new record */		
	if(ps->rpl->expires && msg->expires->body.len > 0)
	{
		if (!msg->expires->parsed && (parse_expires(msg->expires) < 0))
		{
			LOG(L_ERR, "PUA:subs_cback_func: ERROR cannot parse Expires"
					" header\n");
			goto done;
		}
		lexpire = ((exp_body_t*)msg->expires->parsed)->val;
		DBG("PUA:subs_cback_func: lexpire= %d\n", lexpire);
	}		


	/* complete the callback function with dialog information */
	
	hentity->call_id=  msg->callid->body;
	hentity->to_tag= pto->tag_value;
	hentity->from_tag= pfrom->tag_value;

	lock_get(&HashT->p_records[hash_code].lock);

	presentity= get_dialog(hentity, hash_code);

	if(ps->code >= 300 )
	{	/* if an error code and a stored dialog delete it and try to send 
		   a subscription with type= INSERT_TYPE, else return*/	
	/*must work on this!!! generates infinite requests */
		if(presentity)
		{	
			subs_info_t subs;
			delete_htable(presentity, hash_code);
			lock_release(&HashT->p_records[hash_code].lock);
			
			memset(&subs, 0, sizeof(subs_info_t));
			subs.pres_uri= hentity->pres_uri; 
			subs.watcher_uri= hentity->watcher_uri;
			subs.contact= hentity->watcher_uri;
			subs.expires= hentity->desired_expires - (int)time(NULL)+ 10;
			subs.flag|= INSERT_TYPE;
			subs.source_flag|= hentity->flag;
			subs.event|= hentity->event;
			subs.id= hentity->id;
			if(send_subscribe(&subs)< 0)
			{
				LOG(L_ERR, "PUA:subs_cback_func: ERROR when trying to send SUBSCRIBE\n");
				goto done;
			}
		}
		else 
		{
			DBG("PUA:subs_cback_func: No dialog found\n");			
		}
		lock_release(&HashT->p_records[hash_code].lock);
		goto done;
	}
	/*if a 2XX reply handle the two cases- an existing dialog and a new one*/

	if(presentity)
	{		
		if(lexpire== 0 )
		{
			DBG("PUA:subs_cback_func: lexpire= 0 Delete from hash table");
			delete_htable(presentity, hash_code);
			lock_release(&HashT->p_records[hash_code].lock);
			goto done;
		}
		DBG("PUA:subs_cback_func: *** Update expires\n");
		update_htable(presentity, hentity->desired_expires, lexpire, hash_code);
		lock_release(&HashT->p_records[hash_code].lock);
		goto done;
	}
	lock_release(&HashT->p_records[hash_code].lock);

	/* if a new dialog -> insert */
	if(lexpire== 0)
	{	
		LOG(L_ERR, "PUA: subs_cback_func:expires= 0: no not insert\n");
		goto done;
	}

	if( msg->cseq==NULL || msg->cseq->body.s==NULL)
	{
		LOG(L_ERR, "PUA: subs_cback_func: ERROR cannot parse cseq"
		" header\n");
		goto done;
	}

	if( str2int( &(get_cseq(msg)->number), &cseq)< 0)
	{
		LOG(L_ERR, "PUA: subs_cback_func: ERROR while converting str"
					" to int\n");
    }	
	
	size= sizeof(ua_pres_t)+ 2*sizeof(str)+( pto->uri.len+
		pfrom->uri.len+ pto->tag_value.len+ pfrom->tag_value.len
		+msg->callid->body.len+ 1 )*sizeof(char);

	if(hentity->outbound_proxy)
		size+= sizeof(str)+ hentity->outbound_proxy->len* sizeof(char);	
	presentity= (ua_pres_t*)shm_malloc(size);
	if(presentity== NULL)
	{
		LOG(L_ERR, "PUA: subs_cback_func: Error no more share memory");
		goto done;
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

	if(hentity->outbound_proxy)
	{
		presentity->outbound_proxy= (str*)( (char*)presentity+ size);
		size+= sizeof(str);
		presentity->outbound_proxy->s= (char*)presentity+ size;
		memcpy(presentity->outbound_proxy->s, hentity->outbound_proxy->s, hentity->outbound_proxy->len);
		presentity->outbound_proxy->len= hentity->outbound_proxy->len;
		size+= hentity->outbound_proxy->len;
	}	
	presentity->event|= hentity->event;
	presentity->flag= hentity->flag;
	presentity->db_flag|= INSERTDB_FLAG;
	presentity->etag.s= NULL;
	presentity->cseq= cseq;
	presentity->desired_expires= hentity->desired_expires;
	presentity->expires= lexpire+ (int)time(NULL);
	if(BLA_SUBSCRIBE & presentity->flag)
	{
		DBG("PUA: subs_cback_func:  BLA_SUBSCRIBE FLAG inserted\n");
	}	
	DBG("PUA: subs_cback_func: record for subscribe from %.*s to %.*s inserted in"
			" datatbase\n", presentity->watcher_uri->len, presentity->watcher_uri->s,
			presentity->pres_uri->len, presentity->pres_uri->s);
	insert_htable(presentity);

done:
	hentity->flag= flag;
	run_pua_callbacks( hentity, &msg->first_line);
	
	if(*ps->param)
	{	
		shm_free(*ps->param);
		*ps->param= NULL;
	}
	return;

}

ua_pres_t* build_cback_param(subs_info_t* subs)
{	
	ua_pres_t* hentity= NULL;
	int size;

	size= sizeof(ua_pres_t)+ 2*sizeof(str)+(subs->pres_uri->len+
		subs->watcher_uri->len+ 1)* sizeof(char);
	
	if(subs->outbound_proxy && subs->outbound_proxy->len && subs->outbound_proxy->s )
		size+= sizeof(str)+ subs->outbound_proxy->len* sizeof(char);

	hentity= (ua_pres_t*)shm_malloc(size);
	if(hentity== NULL)
	{
		LOG(L_ERR, "PUA: build_cback_param: No more share memory\n");
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

	hentity->flag= subs->source_flag;
	hentity->event|= subs->event;

	return hentity;

}	

int send_subscribe(subs_info_t* subs)
{
	ua_pres_t* presentity= NULL;
	str met= {"SUBSCRIBE", 9};
	str* str_hdr= NULL;
	int ret= 0;
	unsigned int hash_code;
	ua_pres_t* hentity= NULL;
	int expires;
	int flag;

	DBG("send_subscribe... \n");
	print_subs(subs);

	flag= subs->source_flag;
	if(subs->source_flag & XMPP_INITIAL_SUBS)
		subs->source_flag= XMPP_SUBSCRIBE;

	if(subs->expires< 0)
		expires= 3600;
	else
		expires= subs->expires;

	str_hdr= subs_build_hdr(subs->contact, expires, subs->event);
	if(str_hdr== NULL || str_hdr->s== NULL)
	{
		LOG(L_ERR, "PUA:send_subscribe: Error while building extra headers\n");
		return -1;
	}

	hash_code=core_hash(subs->pres_uri, subs->watcher_uri, HASH_SIZE);

	lock_get(&HashT->p_records[hash_code].lock);

	presentity= search_htable(subs->pres_uri, subs->watcher_uri,
				 subs->source_flag, subs->id, hash_code);

	/* if flag == INSERT_TYPE insert no matter what the search result is */
	if(subs->flag & INSERT_TYPE)
	{
		DBG("PUA:send_subscribe: A subscription request with insert type\n");
		goto insert;
	}
	
	if(presentity== NULL )
	{
insert:
	
		lock_release(&HashT->p_records[hash_code].lock); 

		if(subs->flag & UPDATE_TYPE)
		{
			/*
			LOG(L_ERR, "PUA:send_subscribe:ERROR request for a subscription"
					" with update type and no record found\n");
			ret= -1;
			goto done;
			commented this because of the strange type parameter in usrloc callback functions
			*/
			
			DBG("PUA:send_subscribe:request for a subscription"
					" with update type and no record found\n");
			subs->flag= INSERT_TYPE;

		}	
		hentity= build_cback_param(subs);
		if(hentity== NULL)
		{
			LOG(L_ERR, "PUA:send_subscribe:ERROR while building callback"
					" param\n");
			ret= -1;
			goto done;
		}
		hentity->flag= flag;

		tmb.t_request
			(&met,						  /* Type of the message */
			subs->pres_uri,				  /* Request-URI */
			subs->pres_uri,				  /* To */
			subs->watcher_uri,			  /* From */
			str_hdr,					  /* Optional headers including CRLF */
			0,							  /* Message body */
			subs->outbound_proxy,		  /* Outbound_proxy */	
			subs_cback_func,		      /* Callback function */
			(void*)hentity			      /* Callback parameter */
			);
	}
	else
	{
		/* tackle the case in which the desired_expires== 0 and subs->expires< 0*/
		if(presentity->desired_expires== 0)
		{
			if(subs->expires< 0)
			{
				DBG("PUA:send_subscribe: Found previous request for"
						" unlimited subscribe- do not send subscribe\n");
				if (subs->event & PWINFO_EVENT)
				{	
					presentity->watcher_count++;
				}
				lock_release(&HashT->p_records[hash_code].lock);
				goto done;
			}
		
			if(subs->event & PWINFO_EVENT)
			{	
				if(subs->expires== 0) /* request for unsubscribe*/
				{
					presentity->watcher_count--;
					if(	presentity->watcher_count> 0)
					{
						lock_release(&HashT->p_records[hash_code].lock);
						goto done;
					}
				}
				else
				{
					presentity->watcher_count--;
					if(presentity->watcher_count> 1)
					{
						lock_release(&HashT->p_records[hash_code].lock);
						goto done;
					}
				}
			}	
			
		}	

		dlg_t* td= NULL;
		td= pua_build_dlg_t(presentity);
		if(td== NULL)
		{
			LOG(L_ERR, "PUA:send_subscribe: Error while building tm dlg_t"
					"structure");
			ret= -1;
			lock_release(&HashT->p_records[hash_code].lock);
			goto done;
		}
		lock_release(&HashT->p_records[hash_code].lock);
		
		hentity= build_cback_param(subs);
		if(hentity== NULL)
		{
			LOG(L_ERR, "PUA:send_subscribe:ERROR while building callback"
					" param\n");
			ret= -1;
			pkg_free(td);
			goto done;
		}
	//	hentity->flag= flag;
	
		tmb.t_request_within
		(&met,
		str_hdr,
		0,
		td,
		subs_cback_func,
		(void*)hentity
		);

		pkg_free(td);
		td= NULL;
	}

done:
	pkg_free(str_hdr);
	return ret;
}

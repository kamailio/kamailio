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

str* subs_build_hdr(str* watcher_uri, int expires, int event)
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
	str_hdr->s= buf;

	if(event& PRESENCE_EVENT)
	{	
		memcpy(str_hdr->s ,"Event: presence", 15);
		str_hdr->len = 15;
	}
	else
	{	
		memcpy(str_hdr->s ,"Event: presence.winfo", 21);
		str_hdr->len = 21;
	}

	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;
	
	memcpy(str_hdr->s+ str_hdr->len ,"Contact: ", 9);
	str_hdr->len += 9;
	memcpy(str_hdr->s +str_hdr->len, watcher_uri->s, 
			watcher_uri->len);
	str_hdr->len+= watcher_uri->len;
	memcpy(str_hdr->s+str_hdr->len, CRLF, CRLF_LEN);
	str_hdr->len += CRLF_LEN;

	memcpy(str_hdr->s+ str_hdr->len ,"Expires: ", 9);
	str_hdr->len += 9;

	if( expires<= min_expires)
		subs_expires= int2str(min_expires, &len);  
	else
		subs_expires= int2str(expires+ 1, &len);
		
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
		2* presentity->pres_uri->len+ 1)* sizeof(char);

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
	memcpy(td->rem_target.s, presentity->pres_uri->s,
			presentity->pres_uri->len) ;
	td->rem_target.len = presentity->pres_uri->len;
	size+= td->rem_target.len;

	td->loc_seq.value = presentity->cseq ;
	td->loc_seq.is_set = 1;
	td->state= DLG_CONFIRMED ;
	
	return td;
}

void subs_cback_func(struct cell *t, int type, struct tmcb_params *ps)
{
	struct sip_msg* msg= NULL;
	int lexpire= 0;
	unsigned int cseq;
	ua_pres_t* presentity= NULL;
	struct to_body *pto, *pfrom = NULL, TO;
	int size= 0;
	unsigned int hash_code;

	if( ps->param== NULL )
	{
		LOG(L_ERR, "PUA:subs_cback_func:ERROR null callback parameter\n");
		return;
	}
	DBG("PUA:subs_cback_func: completed with status %d\n",ps->code) ;
	
	if(ps->code >= 300 )
	{

		if( *ps->param== NULL )
		{
			LOG(L_ERR, "PUA:subs_cback_func: null callback parameter\n");
			return;
		}
		
		hash_code= core_hash(((hentity_t*)(*ps->param))->pres_uri, 
			((hentity_t*)(*ps->param))->watcher_uri, HASH_SIZE);

		lock_get(&HashT->p_records[hash_code].lock);

		presentity= search_htable(((hentity_t*)(*ps->param))->pres_uri, 
				((hentity_t*)(*ps->param))->watcher_uri,
				((hentity_t*)(*ps->param))->id	,
				((hentity_t*)(*ps->param))->flag, 
				((hentity_t*)(*ps->param))->event, hash_code);

		if(presentity)
			delete_htable(presentity);

		lock_release(&HashT->p_records[hash_code].lock);

		goto done;
	}

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
	if(lexpire== 0 && *ps->param== NULL )
	{
		DBG("PUA:subs_cback_func: Reply with expires= 0 and entity already"
				" deleted\n");
		return;
	}
	if( *ps->param== NULL )
	{
		LOG(L_ERR, "PUA:subs_cback_func:ERROR null callback parameter\n");
		return;
	}


	hash_code= core_hash(((hentity_t*)(*ps->param))->pres_uri, 
			((hentity_t*)(*ps->param))->watcher_uri, HASH_SIZE);
	
	lock_get(&HashT->p_records[hash_code].lock);

	presentity= search_htable(((hentity_t*)(*ps->param))->pres_uri, 
				((hentity_t*)(*ps->param))->watcher_uri,
				((hentity_t*)(*ps->param))->id,
				((hentity_t*)(*ps->param))->flag,
				((hentity_t*)(*ps->param))->event, hash_code);

	if(presentity)
	{
		if(lexpire == 0 )
		{
			DBG("PUA:subs_cback_func: lexpire= 0 Delete from hash table");
			delete_htable(presentity );
			lock_release(&HashT->p_records[hash_code].lock);
			goto done;
		}
		DBG("PUA:subs_cback_func: *** Update expires\n");
		update_htable(presentity, ((hentity_t*)(*ps->param))->desired_expires, lexpire, hash_code);
		
		lock_release(&HashT->p_records[hash_code].lock);
		goto done;
	}

	lock_release(&HashT->p_records[hash_code].lock);
	/* if a new subscribe -> insert */
	
	if(lexpire== 0)
	{	
		LOG(L_ERR, "PUA: subs_cback_func:expires= 0: no not insert\n");
		goto done;
	}
	
	/* get dialog information */
	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LOG(L_ERR, "PUA: subs_cback_func: ERROR cannot parse callid"
		" header\n");
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
		
	size= sizeof(ua_pres_t)+ 2*sizeof(str)+( pto->uri.len+
		pfrom->uri.len+ pto->tag_value.len+ pfrom->tag_value.len
		+msg->callid->body.len+ 1 )*sizeof(char);
	
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
	
	presentity->event|= ((hentity_t*)(*ps->param))->event;
	presentity->flag|= ((hentity_t*)(*ps->param))->flag;
	presentity->db_flag|= INSERTDB_FLAG;
	presentity->etag.s= NULL;
	presentity->cseq= cseq;
	presentity->desired_expires= ((hentity_t*)(*ps->param))->desired_expires;
	presentity->expires= lexpire+ (int)time(NULL);
	insert_htable(presentity);
	
	
	shm_free(*ps->param);
	(*ps->param)= NULL;

	return ;
	
done:
		if(*ps->param)
		{	
			shm_free(*ps->param);
			*ps->param= NULL;
		}
		return;

}

hentity_t* build_cback_param(subs_info_t* subs)
{	
	hentity_t* hentity= NULL;
	int size;

	size= sizeof(hentity_t)+ 2*sizeof(str)+(subs->pres_uri->len+
		subs->watcher_uri->len+ 1)* sizeof(char);
	
	hentity= (hentity_t*)shm_malloc(size);
	if(hentity== NULL)
	{
		LOG(L_ERR, "PUA: build_cback_param: No more share memory\n");
		return NULL;
	}
	memset(hentity, 0, size);

	size= sizeof(hentity_t);

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

	if(subs->expires< 0)
		hentity->desired_expires= 0;
	else
		hentity->desired_expires=subs->expires+ (int)time(NULL);

	hentity->desired_expires= subs->expires+ (int)time(NULL);
	hentity->flag|= subs->source_flag;
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
	hentity_t* hentity= NULL;
	int expires;

	DBG("send_subscribe... \n");
	print_subs(subs);

	if(subs->expires< 0)
		expires= 3600;
	else
		expires= subs->expires;

	str_hdr= subs_build_hdr(subs->watcher_uri, expires, subs->event);
	if(str_hdr== NULL || str_hdr->s== NULL)
	{
		LOG(L_ERR, "PUA:send_subscribe: Error while building extra headers\n");
		return -1;
	}

	hash_code=core_hash(subs->pres_uri, subs->watcher_uri, HASH_SIZE);

	lock_get(&HashT->p_records[hash_code].lock);

	presentity= search_htable(subs->pres_uri, subs->watcher_uri,
				subs->id, subs->source_flag, subs->event, hash_code);

	if(presentity== NULL )
	{
		lock_release(&HashT->p_records[hash_code].lock); 

		if(subs->flag & UPDATE_TYPE)
		{
			LOG(L_ERR,"PUA:send_subscribe: UNSUBS_FLAG and no record found\n");
			goto done;
		}

		hentity= build_cback_param(subs);
		if(hentity== NULL)
		{
			LOG(L_ERR, "PUA:send_subscribe:ERROR while building callback"
					" param\n");
			ret= -1;
			goto done;
		}

		tmb.t_request
			(&met,						  /* Type of the message */
			subs->pres_uri,				  /* Request-URI */
			subs->pres_uri,				  /* To */
			subs->watcher_uri,			  /* From */
			str_hdr,					  /* Optional headers including CRLF */
			0,							  /* Message body */
			subs_cback_func,		      /* Callback function */
			(void*)hentity			      /* Callback parameter */
			);
	}
	else
	{
		dlg_t* td= NULL;
		td= pua_build_dlg_t(presentity);
		if(td== NULL)
		{
			LOG(L_ERR, "PUA:send_subscribe: Error while building tm dlg_t"
					"structure");
			ret= -1;
			lock_release(&HashT->p_records[hash_code].lock);
			shm_free(hentity);
			goto done;
		}

		if(subs->expires== 0)
		{	
			delete_htable(presentity);
		}
		else
		{
			hentity= build_cback_param(subs);
			if(hentity== NULL)
			{
				LOG(L_ERR, "PUA:send_subscribe:ERROR while building callback"
						" param\n");
				lock_release(&HashT->p_records[hash_code].lock);
				ret= -1;
				goto done;
			}
		}
		lock_release(&HashT->p_records[hash_code].lock);
	
	
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

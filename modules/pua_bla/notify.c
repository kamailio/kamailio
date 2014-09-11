/*
 * $Id: notify.c 1666 2007-03-02 13:40:09Z anca_vamanu $
 *
 * pua_bla module - pua Bridged Line Appearance
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
 * History:
 * --------
 *  2007-03-30  initial version (anca)
 */
#include<stdio.h>
#include<stdlib.h>
#include<libxml/parser.h>

#include "../../parser/parse_content.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_from.h"
#include "../../lib/kcore/cmpapi.h"
#include "../pua/hash.h"
#include"pua_bla.h"

int bla_handle_notify(struct sip_msg* msg, char* s1, char* s2)
{
 	publ_info_t publ;
	struct to_body *pto = NULL, TO = {0}, *pfrom = NULL;
 	str body;
 	ua_pres_t dialog;
 	unsigned int expires= 0;
	struct hdr_field* hdr;
	str subs_state;
	int found= 0;
 	str extra_headers= {0, 0};
 	static char buf[255];
	str contact;

	memset(&publ, 0, sizeof(publ_info_t));
	memset(&dialog, 0, sizeof(ua_pres_t));
 
  	LM_DBG("start\n");
  
  	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
  	{
  		LM_ERR("parsing headers\n");
  		return -1;
  	}
  
  	if( msg->to==NULL || msg->to->body.s==NULL)
  	{
  		LM_ERR("cannot parse TO header\n");
  		goto error;
  	}
  	/* examine the to header */
  	if(msg->to->parsed != NULL)
  	{
  		pto = (struct to_body*)msg->to->parsed;
  		LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",
  				pto->uri.len, pto->uri.s );
  	}
 	else
  	{
  		parse_to(msg->to->body.s,msg->to->body.s + msg->to->body.len + 1, &TO);
  		if(TO.uri.len <= 0)
  		{
  			LM_DBG("'To' header NOT parsed\n");
  			goto error;
  		}
  		pto = &TO;
  	}
  	publ.pres_uri= &pto->uri;
  	dialog.watcher_uri= publ.pres_uri;
  
  	if (pto->tag_value.s==NULL || pto->tag_value.len==0 )
  	{
  		LM_ERR("NULL to_tag value\n");
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
  		LM_DBG(" 'From' header not parsed\n");
  		/* parsing from header */
  		if ( parse_from_header( msg )<0 )
  		{
  			LM_DBG(" ERROR cannot parse From header\n");
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
 	dialog.event= BLA_EVENT;
 	dialog.flag= BLA_SUBSCRIBE;
 	if(pua_is_dialog(&dialog)< 0)
 	{
 		LM_ERR("Notify in a non existing dialog\n");
 		goto error;
 	}
 	LM_DBG("found a matching dialog\n");
 
 	/* parse Subscription-State and extract expires if existing */
	hdr = msg->headers;
	while (hdr!= NULL)
	{
 		if(cmp_hdrname_strzn(&hdr->name, "Subscription-State",18)==0)
 		{
 			found = 1;
 			break;
 		}
		hdr = hdr->next;
  	}
	if(found==0 )
	{
		LM_ERR("No Subscription-State header found\n");
		goto error;
 	}
 	subs_state= hdr->body;
	if(strncmp(subs_state.s, "terminated", 10)== 0)
 		expires= 0;
 	else
 		if(strncmp(subs_state.s, "active", 6)== 0 ||
				strncmp(subs_state.s, "pending", 7)==0 )
 		{
   			char* sep= NULL;
   			str exp= {0, 0};
  			sep= strchr(subs_state.s, ';');
   			if(sep== NULL)
   			{
   				LM_ERR("No expires found in Notify\n");
   				goto error;
   			}
   			if(strncmp(sep+1, "expires=", 8)!= 0)
   			{
   				LM_ERR("No expires found in Notify\n");
   				goto error;
   			}
   			exp.s= sep+ 9;
   			sep= exp.s;
   			while((*sep)>='0' && (*sep)<='9')
   			{
   				sep++;
   				exp.len++;
   			}
   			if( str2int(&exp, &expires)< 0)
   			{
   				LM_ERR("while parsing int\n");
   				goto error;
   			}
   		}
   
   	if ( get_content_length(msg) == 0 )
   	{
   		LM_ERR("content length= 0\n");
   		goto error;
   	}
   	else
   	{
   		body.s=get_body(msg);
   		if (body.s== NULL)
   		{
   			LM_ERR("cannot extract body from msg\n");
   			goto error;
   		}
   		body.len = get_content_length( msg );
   	}
   	
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

	/* build extra_headers with Sender*/
   	extra_headers.s= buf;
   	memcpy(extra_headers.s, header_name.s, header_name.len);
   	extra_headers.len= header_name.len;
   	memcpy(extra_headers.s+extra_headers.len,": ",2);
   	extra_headers.len+= 2;
   	memcpy(extra_headers.s+ extra_headers.len, contact.s, contact.len);
   	extra_headers.len+= contact.len;
   	memcpy(extra_headers.s+ extra_headers.len, CRLF, CRLF_LEN);
   	extra_headers.len+= CRLF_LEN;
   
   	publ.body= &body;
   	publ.source_flag= BLA_PUBLISH;
   	publ.expires= expires;
   	publ.event= BLA_EVENT;
   	publ.extra_headers= &extra_headers;
	
   	if(pua_send_publish(&publ)< 0)
   	{
   		LM_ERR("while sending Publish\n");
   		goto error;
   	}
      
   	xmlCleanupParser();
   	xmlMemoryDump();

	free_to_params(&TO);
	return 1;
   
error:
	free_to_params(&TO);
   	return 0;
}


/*
 * $Id$
 *
 * pua_mi module - MI pua module
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

#include "../../parser/parse_expires.h"
#include "../../parser/parse_uri.h"
#include  "../../mem/mem.h"
#include "../../lib/kmi/mi.h"
#include "../../ut.h"
#include "../../lib/kcore/cmpapi.h"

#include "../pua/pua_bind.h"
#include "pua_mi.h"

/*
 * mi cmd: pua_publish
 *		<presentity_uri> 
 *		<expires>
 *		<event package>
 *		<content_type>     - body type if body of a type different from default
 *		                     event content-type or .
 *		<id>               - id for a series of related PUBLISHes to the same
 *		                     presentity-uri or .
 *		<ETag>             - ETag that publish should match or . if no ETag
 *		<outbound_proxy>   - outbound proxy or .
 *		<extra_headers>    - extra headers to be added to the request or .
 *		<publish_body>     - may not be present in case of update for expire
 */

struct mi_root* mi_pua_publish(struct mi_root* cmd, void* param)
{
	int exp;
	struct mi_node* node= NULL;
	str pres_uri, expires;
	str body= {0, 0};
	struct sip_uri uri;
	publ_info_t publ;
	str event;
	str content_type;
	str id;
	str etag;
	str outbound_proxy;
	str extra_headers;
	int result;
	int sign= 1;

	LM_DBG("start\n");

	node = cmd->node.kids;
	if(node == NULL)
		return 0;

	/* Get presentity URI */
	pres_uri = node->value;
	if(pres_uri.s == NULL || pres_uri.s== 0)
	{
		LM_ERR("empty uri\n");
		return init_mi_tree(404, "Empty presentity URI", 20);
	}
	if(parse_uri(pres_uri.s, pres_uri.len, &uri)<0 )
	{
		LM_ERR("bad uri\n");
		return init_mi_tree(404, "Bad presentity URI", 18);
	}
	LM_DBG("pres_uri '%.*s'\n", pres_uri.len, pres_uri.s);

	node = node->next;
	if(node == NULL)
		return 0;

	/* Get expires */
	expires= node->value;
	if(expires.s== NULL || expires.len== 0)
	{
		LM_ERR("empty expires parameter\n");
		return init_mi_tree(400, "Empty expires parameter", 23);
	}
	if(expires.s[0]== '-')
	{
		sign= -1;
		expires.s++;
		expires.len--;
	}
	if( str2int(&expires, (unsigned int*) &exp)< 0)
	{
		LM_ERR("invalid expires parameter\n" );
		goto error;
	}
	
	exp= exp* sign;

	LM_DBG("expires '%d'\n", exp);

	node = node->next;
	if(node == NULL)
		return 0;

	/* Get event */
	event= node->value;
	if(event.s== NULL || event.len== 0)
	{
		LM_ERR("empty event parameter\n");
		return init_mi_tree(400, "Empty event parameter", 21);
	}
	LM_DBG("event '%.*s'\n",
	    event.len, event.s);

	node = node->next;
	if(node == NULL)
		return 0;

	/* Get content type */
	content_type= node->value;
	if(content_type.s== NULL || content_type.len== 0)
	{
		LM_ERR("empty content type\n");
		return init_mi_tree(400, "Empty content type parameter", 28);
	}
	LM_DBG("content type '%.*s'\n",
	    content_type.len, content_type.s);

	node = node->next;
	if(node == NULL)
		return 0;

	/* Get id */
	id= node->value;
	if(id.s== NULL || id.len== 0)
	{
		LM_ERR("empty id parameter\n");
		return init_mi_tree(400, "Empty id parameter", 20);
	}
	LM_DBG("id '%.*s'\n", id.len, id.s);

	node = node->next;
	if(node == NULL)
		return 0;

	/* Get etag */
	etag= node->value;
	if(etag.s== NULL || etag.len== 0)
	{
		LM_ERR("empty etag parameter\n");
		return init_mi_tree(400, "Empty etag parameter", 20);
	}
	LM_DBG("etag '%.*s'\n", etag.len, etag.s);

	node = node->next;
	if(node == NULL)
		return 0;

	/* Get outbound_proxy */
	if (publish_with_ob_proxy) {

		outbound_proxy = node->value;
		if(outbound_proxy.s== NULL || outbound_proxy.len== 0) {
			LM_ERR("empty outbound proxy parameter\n");
			return init_mi_tree(400, "Empty outbound proxy", 20);
		}
		LM_DBG("outbound_proxy '%.*s'\n",
		       outbound_proxy.len, outbound_proxy.s);
	
		node = node->next;
		if(node == NULL) return 0;
	}

	/* Get extra_headers */
	extra_headers = node->value;
	if(extra_headers.s== NULL || extra_headers.len== 0)
	{
		LM_ERR("empty extra_headers parameter\n");
		return init_mi_tree(400, "Empty extra_headers", 19);
	}
	LM_DBG("extra_headers '%.*s'\n",
	    extra_headers.len, extra_headers.s);

	node = node->next;

	/* Get body */
	if(node == NULL )
	{
		body.s= NULL;
		body.len= 0;
	}
	else
	{
		if(node->next!=NULL)
			return init_mi_tree(400, "Too many parameters", 19);

		body= node->value;
		if(body.s == NULL || body.s== 0)
		{
			LM_ERR("empty body parameter\n");
			return init_mi_tree(400, "Empty body parameter", 20);
		}
	}
	LM_DBG("body '%.*s'\n", body.len, body.s);

	/* Check that body is NULL if content type is . */
	if(body.s== NULL && (content_type.len!= 1 || content_type.s[0]!= '.'))
	{
		LM_ERR("body is missing, but content type is not .\n");
		return init_mi_tree(400, "Body parameter is missing", 25);
	}

	/* Create the publ_info_t structure */
	memset(&publ, 0, sizeof(publ_info_t));
	
	publ.pres_uri= &pres_uri;
	if(body.s)
	{
		publ.body= &body;
	}
	
	publ.event= get_event_flag(&event);
	if(publ.event< 0)
	{
		LM_ERR("unknown event\n");
		return init_mi_tree(400, "Unknown event", 13);
	}
	if(content_type.len!= 1)
	{
		publ.content_type= content_type;
	}	

	if(! (id.len== 1 && id.s[0]== '.'))
	{
		publ.id= id;
	}

	if(! (etag.len== 1 && etag.s[0]== '.'))
	{
		publ.etag= &etag;
	}	
	publ.expires= exp;

	if (publish_with_ob_proxy) {
		if (!((outbound_proxy.len == 1) &&
		      (outbound_proxy.s[0] == '.')))
			publ.outbound_proxy = &outbound_proxy;
	}

	if (!(extra_headers.len == 1 && extra_headers.s[0] == '.')) {
	    publ.extra_headers = &extra_headers;
	}

	if (!(etag.len== 1 && etag.s[0]== '.'))
	{
		publ.etag= &etag;
	}	
	publ.expires= exp;

	if (cmd->async_hdl!=NULL)
	{
		publ.source_flag= MI_ASYN_PUBLISH;
		publ.cb_param= (void*)cmd->async_hdl;
	}	
	else
		publ.source_flag|= MI_PUBLISH;

	LM_DBG("send publish\n");

	result= pua_send_publish(&publ);

	if(result< 0)
	{
		LM_ERR("sending publish failed\n");
		return init_mi_tree(500, "MI/PUBLISH failed", 17);
	}	
	if(result== 418)
		return init_mi_tree(418, "Wrong ETag", 10);
	
	if (cmd->async_hdl==NULL)
			return init_mi_tree( 202, "Accepted", 8);
	else
			return MI_ROOT_ASYNC_RPL;

error:

	return 0;
}

int mi_publ_rpl_cback( ua_pres_t* hentity, struct sip_msg* reply)
{
	struct mi_root *rpl_tree= NULL;
	struct mi_handler* mi_hdl= NULL;
	struct hdr_field* hdr= NULL;
	int statuscode;
	int lexpire;
	int found;
	str etag;
	str reason= {0, 0};

	if(reply== NULL || hentity== NULL || hentity->cb_param== NULL)
	{
		LM_ERR("NULL parameter\n");
		return -1;
	}
	if(reply== FAKED_REPLY)
	{
		statuscode= 408;
		reason.s= "Request Timeout";
		reason.len= strlen(reason.s);
	}
	else
	{
		statuscode= reply->first_line.u.reply.statuscode;
		reason= reply->first_line.u.reply.reason;
	}

	mi_hdl = (struct mi_handler *)(hentity->cb_param);
	
	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		goto done;
	
	addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "%d %.*s",
		statuscode, reason.len, reason.s);
	
	
	if(statuscode== 200)
	{
		/* extract ETag and expires */
		lexpire = ((exp_body_t*)reply->expires->parsed)->val;
		LM_DBG("lexpire= %d\n", lexpire);
		
		hdr = reply->headers;
		found = 0;
		while (hdr!= NULL)
		{
			if(cmp_hdrname_strzn(&hdr->name, "SIP-ETag",8)==0)
			{
				found = 1;
				break;
			}
			hdr = hdr->next;
		}
		if(found== 0) /* must find SIP-Etag header field in 200 OK msg*/
		{
			LM_ERR("SIP-ETag header field not found\n");
			goto error;
		}
		etag= hdr->body;
			
		addf_mi_node_child( &rpl_tree->node, 0, "ETag", 4, "%.*s", etag.len, etag.s);
		
		addf_mi_node_child( &rpl_tree->node, 0, "Expires", 7, "%d", lexpire);
	}	

done:
	if ( statuscode >= 200) 
	{
		mi_hdl->handler_f( rpl_tree, mi_hdl, 1);
	}
	else 
	{
		mi_hdl->handler_f( rpl_tree, mi_hdl, 0 );
	}
	return 0;

error:
	return  -1;
}


/*Command parameters:
 * pua_subscribe
 *		<presentity_uri>
 *		<watcher_uri>
 *		<event_package>
 *		<expires>
 * */


struct mi_root* mi_pua_subscribe(struct mi_root* cmd, void* param)
{
	int exp= 0;
	str pres_uri, watcher_uri, expires;
	struct mi_node* node= NULL;
	struct mi_root* rpl= NULL;
	struct sip_uri uri;
	subs_info_t subs;
	int sign= 1;
	str event;

	node = cmd->node.kids;
	if(node == NULL)
		return 0;

	pres_uri= node->value;
	if(pres_uri.s == NULL || pres_uri.len== 0)
	{
		return init_mi_tree(400, "Bad uri", 7);
	}
	if(parse_uri(pres_uri.s, pres_uri.len, &uri)<0 )
	{
		LM_ERR("bad uri\n");	
		return init_mi_tree(400, "Bad uri", 7);
	}

	node = node->next;
	if(node == NULL)
		return 0;

	watcher_uri= node->value;
	if(watcher_uri.s == NULL || watcher_uri.len== 0)
	{
		return init_mi_tree(400, "Bad uri", 7);
	}
	if(parse_uri(watcher_uri.s, watcher_uri.len, &uri)<0 )
	{
		LM_ERR("bad uri\n");	
		return init_mi_tree(400, "Bad uri", 7);
	}

	/* Get event */
	node = node->next;
	if(node == NULL)
		return 0;

	event= node->value;
	if(event.s== NULL || event.len== 0)
	{
		LM_ERR("empty event parameter\n");
		return init_mi_tree(400, "Empty event parameter", 21);
	}
	LM_DBG("event '%.*s'\n", event.len, event.s);

	node = node->next;
	if(node == NULL || node->next!=NULL)
	{
		LM_ERR("Too much or too many parameters\n");
		return 0;
	}

	expires= node->value;
	if(expires.s== NULL || expires.len== 0)
	{
		LM_ERR("Bad expires parameter\n");
		return init_mi_tree(400, "Bad expires", 11);
	}		
	if(expires.s[0]== '-')
	{
		sign= -1;
		expires.s++;
		expires.len--;
	}
	if( str2int(&expires, (unsigned int*) &exp)< 0)
	{
		LM_ERR("invalid expires parameter\n" );
		goto error;
	}
	
	exp= exp* sign;

	LM_DBG("expires '%d'\n", exp);
	
	memset(&subs, 0, sizeof(subs_info_t));
	
	subs.pres_uri= &pres_uri;

	subs.watcher_uri= &watcher_uri;

	subs.contact= &watcher_uri;
	
	subs.expires= exp;
	subs.source_flag |= MI_SUBSCRIBE;
	subs.event= get_event_flag(&event);
	if(subs.event< 0)
	{
		LM_ERR("unknown event\n");
		return init_mi_tree(400, "Unknown event", 13);
	}

	if(pua_send_subscribe(&subs)< 0)
	{
		LM_ERR("while sending subscribe\n");
		goto error;
	}
	
	rpl= init_mi_tree(202, "accepted", 8);
	if(rpl == NULL)
		return 0;
	
	return rpl;

error:

	return 0;

}


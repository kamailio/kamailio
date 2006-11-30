/*
 * $Id$
 *
 * pua_mi module - MI pua module
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
#include <libxml/parser.h>

#include  "../../mem/mem.h"
#include "../../mi/mi.h"
#include "../../parser/parse_uri.h"
#include "../../ut.h"

#include "../pua/pua_bind.h"
#include "../pua/pua.h"
#include "pua_mi.h"

/*
 * cmd: mi_pua_publish
 *		<presentity_uri> 
 *		<expires>
 *		<xml_presence_body> - may not be present in case of update 
 *	 * */

struct mi_root* mi_pua_publish(struct mi_root* cmd, void* param)
{
	int len;
	struct mi_root* rpl= NULL;
	struct mi_node* node= NULL;
	str pres_uri, expires;
	str body= {0, 0};
	struct sip_uri uri;
	publ_info_t* publ= NULL;
	int size= 0;

	DBG("pua_mi: pua_mi_publish ..\n");
	node = cmd->node.kids;
	if(node == NULL)
		return 0;

	pres_uri = node->value;
	if(pres_uri.s == NULL || pres_uri.s== 0)
	{
		if(parse_uri(pres_uri.s, pres_uri.len, &uri)<0 )
		{
			LOG(L_ERR, "pua_mi: pua_mi_publish: bad uri\n");	
		}
		return init_mi_tree(404, "Bad uri", 7);
	}

	DBG("pua_mi_publish: pres_uri: '%.*s'\n", pres_uri.len, pres_uri.s);

	node = node->next;
	if(node == NULL)
		return 0;
	
	expires= node->value;
	if(expires.s== NULL || expires.len== 0)
	{
		LOG(L_ERR, "pua_mi: pua_mi_publish: Bad expires parameter\n");
		return init_mi_tree(400, "Bad expires", 11);
	}
	
	if( str2int(&expires, (unsigned int*) &len)< 0)
	{
		LOG(L_ERR,"pua_mi: pua_mi_publish:ERROR while getting int from str\n" );
		goto error;
	}

	DBG("pua_mi: pua_mi_publish: expires= %d\n", len);
	
	node = node->next;
	if(node == NULL )
	{
		body.s= NULL;
	}
	else
	{
		if(node->next!=NULL)
			return init_mi_tree(400, "Too many parameters", 19);

		body= node->value;
		if(body.s == NULL || body.s== 0)
		{
			if(xmlParseMemory(body.s, body.len)== 0)
			{
				LOG(L_ERR, "pua_mi: pua_mi_publish: bad body\n");	
			}
			return init_mi_tree(400, "Bad body", 8);
		}
	}

	/* creating the publ_info_t structure */
	
	size= sizeof(publ_info_t)+ sizeof(str)+ pres_uri.len *sizeof(char); 
	
	if(body.s)
		size+= body.len* sizeof(char) +sizeof(str);

	publ= (publ_info_t*)pkg_malloc(size);
	if(publ== NULL)
	{
		LOG(L_ERR, "pua_mi: pua_mi_publish: Error no more share memory\n");
		return 0;
	}
	memset(publ, 0, size);
	size= sizeof(publ_info_t);

	publ->pres_uri= (str*)( (char*)publ + size);
	size+= sizeof(str);
	publ->pres_uri->s= (char*)publ+ size;
	memcpy(publ->pres_uri->s, pres_uri.s, pres_uri.len);
	publ->pres_uri->len= pres_uri.len;
	size+= pres_uri.len;

	if(body.s)
	{
		publ->body= (str*)( (char*)publ + size);
		size+= sizeof(str);

		publ->body->s= (char*)publ + size;
		memcpy(publ->body->s, body.s, body.len);
		publ->body->len= body.len;
		size+= body.len;
		DBG("pua_mi: pua_mi_publish: body= %.*s\n",publ->body->len, 
				publ->body->s);
	}
	publ->expires= len;
	publ->flag|= INSERT_TYPE;
	publ->source_flag|= MI_PUBLISH;

	DBG("pua_mi: pua_mi_publish: send publish\n");

	if(pua_send_publish(publ)< 0)
	{
		LOG(L_ERR, "pua_mi: pua_mi_publish:ERROR while sending publish\n");
		goto error;
	}	

	pkg_free(publ);
	rpl = init_mi_tree( 202, "accepted", 8);
	if(rpl == NULL)
		return 0;
	
	return rpl;

error:

	pkg_free(publ);
	return 0;

}

/*
 * cmd: mi_pua_publish
 *		<presentity_uri> 
 *		<xml_presence_body>
 *		<expires>
 * */

struct mi_root* mi_pua_subscribe(struct mi_root* cmd, void* param)
{
	int exp= 0;
	str pres_uri, watcher_uri, expires;
	struct mi_node* node= NULL;
	struct mi_root* rpl= NULL;
	struct sip_uri uri;
	subs_info_t* subs= NULL;
	int size;

	node = cmd->node.kids;
	if(node == NULL)
		return 0;

	pres_uri= node->value;
	if(pres_uri.s == NULL || pres_uri.s== 0)
	{
		if(parse_uri(pres_uri.s, pres_uri.len, &uri)<0 )
		{
			LOG(L_ERR, "pua_mi:mi_pua_subscribe: ERROR bad uri\n");	
		}
		return init_mi_tree(400, "Bad uri", 7);
	}

	node = node->next;
	if(node == NULL)
		return 0;

	watcher_uri= node->value;
	if(watcher_uri.s == NULL || watcher_uri.s== 0)
	{
		if(parse_uri(watcher_uri.s, watcher_uri.len, &uri)<0 )
		{
			LOG(L_ERR, "pua_mi:pua_mi_subscribe: ERROR bad uri\n");	
		}
		return init_mi_tree(400, "Bad uri", 7);
	}

	node = node->next;
	if(node == NULL || node->next!=NULL)
	{
		LOG(L_ERR, "pua_mi:pua_mi_subscribe: Too much or too many"
				" parameters\n");
		return 0;
	}

	expires= node->value;
	if(expires.s== NULL || expires.len== 0)
	{
		LOG(L_ERR, "pua_mi:pua_mi_subscribe: Bad expires parameter\n");
		return init_mi_tree(400, "Bad expires", 11);
	}		

	if( str2int(&expires,(unsigned int*) &exp)< 0 )
	{
		LOG(L_ERR, "pua_mi:pua_mi_subscribe: Error while transforming str to"
				" int\n");
		return 0;
	}
	
	size= sizeof(subs_info_t)+ sizeof(str)* 2+ (pres_uri.len +
			watcher_uri.len)*sizeof(char); 
	
	subs= (subs_info_t*)pkg_malloc(size);
	if(subs== NULL)
	{
		LOG(L_ERR, "pua_mi: pua_mi_subscribe: Error no more share memory\n");
		return 0;
	}
	memset(subs, 0, size);
	size= sizeof(subs_info_t);

	subs->pres_uri= (str*)((char*)subs + size);
	size+= sizeof(str);
	subs->pres_uri->s= (char*)subs+ size;
	memcpy(subs->pres_uri->s, pres_uri.s, pres_uri.len);
	subs->pres_uri->len= pres_uri.len;
	size+= pres_uri.len;

	subs->watcher_uri= (str*)((char*)subs + size);
	size+= sizeof(str);
	subs->watcher_uri->s= (char*)subs+ size;
	memcpy(subs->watcher_uri->s, watcher_uri.s, watcher_uri.len);
	subs->watcher_uri->len= watcher_uri.len;
	size+= watcher_uri.len;

	subs->expires= exp;
	subs->source_flag |= MI_SUBSCRIBE;

	if(pua_send_subscribe(subs)< 0)
	{
		LOG(L_ERR, "pua_mi:pua_mi_subscribe: ERROR while sending subscribe\n");
		goto error;
	}
	
	pkg_free(subs);

	rpl= init_mi_tree(202, "accepted", 8);
	if(rpl == NULL)
		return 0;
	
	return rpl;

error:

	pkg_free(subs);
	return 0;

}

#if 0
struct mi_node* mi_send_reply(str* pres_uri, str* watcher_uri, int FLAG)
{
	ua_pres_t* presentity= NULL;
	struct mi_node* rpl= NULL;
	struct mi_node* node= NULL;
	int exp= 0;
	str expires;

	DBG("pua_mi:mi_send_reply ..\n");

	presentity= search_htable(pres_uri, watcher_uri, NULL, 
			FLAG, HashT);
	
	if(presentity == NULL)
	{
		LOG(L_ERR, "pua_mi:mi_send_reply: ERROR no record found for"
				" subscription\n");
		return 0;
	}

	rpl= init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);
	if(rpl == NULL)
		return 0;

	if(FLAG & MI_PUBLISH_FLAG)
	{
		node = add_mi_node_child(rpl, 0, "Etag", 4, presentity->etag.s, 
				presentity->etag.len);
		if( node== NULL)
			goto error;
	}

	exp= presentity->expires- (int)time(NULL); 
	expires.s= int2str(exp, &expires.len);
	
	node= add_mi_node_child(rpl, MI_DUP_VALUE, "EXPIRES", 7, expires.s, expires.len);
	if(node == NULL)
	{
		LOG(L_ERR, "pua_mi:mi_send_reply: ERROR while adding mi node\n");
		goto error;
	}

	DBG("pua_mi:mi_send_reply: send 200 OK reply\n");
	return rpl;

error:
	free_mi_tree(rpl);
	return 0;

}
#endif

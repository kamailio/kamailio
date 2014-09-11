/*
 * $Id$
 *
 * imc module - instant messaging conferencing implementation
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../parser/parse_uri.h"

#include "imc.h"
#include "imc_cmd.h"

#define IMC_BUF_SIZE	1024

static char imc_body_buf[IMC_BUF_SIZE];

static str imc_msg_type = { "MESSAGE", 7 };

int imc_send_message(str *src, str *dst, str *headers, str *body);
int imc_room_broadcast(imc_room_p room, str *ctype, str *body);
void imc_inv_callback( struct cell *t, int type, struct tmcb_params *ps);

/**
 * parse cmd
 */
int imc_parse_cmd(char *buf, int len, imc_cmd_p cmd)
{
	char *p;
	int i;
	if(buf==NULL || len<=0 || cmd==NULL)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	memset(cmd, 0, sizeof(imc_cmd_t));
	if(buf[0]!=imc_cmd_start_char)
	{
		LM_ERR("invalid command [%.*s]\n", len, buf);
		return -1;
	}
	p = &buf[1];
	cmd->name.s = p;
	while(*p && p<buf+len)
	{
		if(*p==' ' || *p=='\t' || *p=='\r' || *p=='\n')
			break;
		p++;
	}
	if(cmd->name.s == p)
	{
		LM_ERR("no command in [%.*s]\n", len, buf);
		return -1;
	}
	cmd->name.len = p - cmd->name.s;

	/* identify the command */
	if(cmd->name.len==(sizeof("create")-1)
			&& !strncasecmp(cmd->name.s, "create", cmd->name.len))
	{
		cmd->type = IMC_CMDID_CREATE;
	} else if(cmd->name.len==(sizeof("join")-1)
				&& !strncasecmp(cmd->name.s, "join", cmd->name.len)) {
		cmd->type = IMC_CMDID_JOIN;
	} else if(cmd->name.len==(sizeof("invite")-1)
				&& !strncasecmp(cmd->name.s, "invite", cmd->name.len)) {
		cmd->type = IMC_CMDID_INVITE;
	} else if(cmd->name.len==(sizeof("accept")-1)
				&& !strncasecmp(cmd->name.s, "accept", cmd->name.len)) {
		cmd->type = IMC_CMDID_ACCEPT;
	} else if(cmd->name.len==(sizeof("deny")-1)
				&& !strncasecmp(cmd->name.s, "deny", cmd->name.len)) {
		cmd->type = IMC_CMDID_DENY;
	} else if(cmd->name.len==(sizeof("remove")-1)
				&& !strncasecmp(cmd->name.s, "remove", cmd->name.len)) {
		cmd->type = IMC_CMDID_REMOVE;
	} else if(cmd->name.len==(sizeof("exit")-1)
				&& !strncasecmp(cmd->name.s, "exit", cmd->name.len)) {
		cmd->type = IMC_CMDID_EXIT;
	} else if(cmd->name.len==(sizeof("list")-1)
				&& !strncasecmp(cmd->name.s, "list", cmd->name.len)) {
		cmd->type = IMC_CMDID_LIST;
	} else if(cmd->name.len==(sizeof("destroy")-1)
				&& !strncasecmp(cmd->name.s, "destroy", cmd->name.len)) {
		cmd->type = IMC_CMDID_DESTROY;
	} else if(cmd->name.len==(sizeof("help")-1)
				&& !strncasecmp(cmd->name.s, "help", cmd->name.len)) {
		cmd->type = IMC_CMDID_HELP;
		goto done;
	} else {
		cmd->type = IMC_CMDID_UNKNOWN;
		goto done;
	}


	if(*p=='\0' || p>=buf+len)
		goto done;
	
	i=0;
	do {
		while(p<buf+len && (*p==' ' || *p=='\t'))
			p++;
		if(p>=buf+len || *p=='\0' || *p=='\r' || *p=='\n')
			goto done;
		cmd->param[i].s = p;
		while(p<buf+len)
		{
			if(*p=='\0' || *p==' ' || *p=='\t' || *p=='\r' || *p=='\n')
				break;
			p++;
		}
		cmd->param[i].len =  p - cmd->param[i].s;
		i++;
		if(i>=IMC_CMD_MAX_PARAM)
			break;
	} while(1);
	
done:
	LM_DBG("command: [%.*s]\n", cmd->name.len, cmd->name.s);
	for(i=0; i<IMC_CMD_MAX_PARAM; i++)
	{
		if(cmd->param[i].len<=0)
			break;
		LM_DBG("parameter %d=[%.*s]\n", i, cmd->param[i].len, cmd->param[i].s);
	}
	return 0;
}

/**
 *
 */
int imc_handle_create(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	int flag_room = 0;
	int flag_member = 0;
	str body;

	room = imc_get_room(&cmd->param[0], &dst->host);
	if(room== NULL)
	{
		LM_DBG("new room [%.*s]\n",	cmd->param[0].len, cmd->param[0].s);
		if(cmd->param[1].len==IMC_ROOM_PRIVATE_LEN
			&& !strncasecmp(cmd->param[1].s, IMC_ROOM_PRIVATE,
				cmd->param[1].len))
		{
			flag_room |= IMC_ROOM_PRIV;					
			LM_DBG("room with private flag on\n");
		}
			
		room= imc_add_room(&cmd->param[0], &dst->host, flag_room);
		if(room == NULL)
		{
			LM_ERR("failed to add new room\n");
			goto error;
		}	
		LM_DBG("added room uri= %.*s\n", room->uri.len, room->uri.s);
		flag_member |= IMC_MEMBER_OWNER;
		/* adding the owner as the forst member*/
		member= imc_add_member(room, &src->user, &src->host, flag_member);
		if(member == NULL)
		{
			LM_ERR("failed to add owner [%.*s]\n", src->user.len, src->user.s);
			goto error;
		}
		LM_DBG("added the owner as the first member "
				"[%.*s]\n",member->uri.len, member->uri.s);
	
		/* send info message */
		body.s = "*** room was created";
		body.len = sizeof("*** room was created")-1;
		imc_send_message(&room->uri, &member->uri, &all_hdrs, &body);
		goto done;
	}
	
	/* room already exists */

	LM_DBG("room [%.*s] already created\n",	cmd->param[0].len, cmd->param[0].s);
	if(!(room->flags & IMC_ROOM_PRIV)) 
	{
		LM_DBG("checking if the user [%.*s] is a member\n",
				src->user.len, src->user.s);
		member= imc_get_member(room, &src->user, &src->host);
		if(member== NULL)
		{					
			member= imc_add_member(room, &src->user, &src->host, flag_member);
			if(member == NULL)
			{
				LM_ERR("failed to add member [%.*s]\n",
					src->user.len, src->user.s);
				goto error;
			}
			LM_DBG("added as member [%.*s]\n",member->uri.len, member->uri.s);
			/* send info message */
			body.s = imc_body_buf;
			body.len = snprintf(body.s, IMC_BUF_SIZE,
				"*** <%.*s> has joined the room",
				member->uri.len, member->uri.s);
			if(body.len>0)
				imc_room_broadcast(room, &all_hdrs, &body);

			if(body.len>=IMC_BUF_SIZE)
				LM_ERR("member name %.*s truncated\n", member->uri.len, member->uri.s);
		}
	}

done:
	if(room!=NULL)
		imc_release_room(room);
	return 0;

error:
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}


/**
 *
 */
int imc_handle_join(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	int flag_room = 0;
	int flag_member = 0;
	str room_name;
	str body;

	room_name = cmd->param[0].s?cmd->param[0]:dst->user;
	room=imc_get_room(&room_name, &dst->host);
	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{			
		LM_DBG("could not find room [%.*s]- adding\n", 
				room_name.len, room_name.s);
		room= imc_add_room(&room_name, &dst->host, flag_room);
		if(room == NULL)
		{
			LM_ERR("failed to add new room [%.*s]\n",
				room_name.len, room_name.s);
			goto error;
		}
		LM_DBG("created a new room [%.*s]\n", room->name.len, room->name.s);

		flag_member |= IMC_MEMBER_OWNER;
		member= imc_add_member(room, &src->user, &src->host, flag_member);
		if(member == NULL)
		{
			LM_ERR("failed to add new member [%.*s]\n",
				src->user.len, src->user.s);
			goto error;
		}
		/* send info message */
		body.s = "*** room was created";
		body.len = sizeof("*** room was created")-1;
		imc_send_message(&room->uri, &member->uri, &all_hdrs, &body);
		goto done;
	}

	/* room exists */
	LM_DBG("found room [%.*s]\n", room_name.len, room_name.s);

	member= imc_get_member(room, &src->user, &src->host);
	if(!(room->flags & IMC_ROOM_PRIV))
	{
		LM_DBG("room [%.*s] is public\n", room_name.len, room_name.s);
		if(member== NULL)
		{					
			LM_DBG("adding new member [%.*s]\n", src->user.len, src->user.s);
			member= imc_add_member(room, &src->user,
									&src->host, flag_member);	
			if(member == NULL)
			{
				LM_ERR("adding new user [%.*s]\n", src->user.len, src->user.s);
				goto error;
			}	
			goto build_inform;
		} else {	
			LM_DBG("member [%.*s] is in room already\n",
				member->uri.len,member->uri.s );
		}
	} else {
		if(member==NULL)
		{
			LM_ERR("attept to join private room [%.*s] from user [%.*s]\n",
					room_name.len, room_name.s,	src->user.len, src->user.s);
			goto build_inform;

		}

		if(member->flags & IMC_MEMBER_INVITED) 
			member->flags &= ~IMC_MEMBER_INVITED;
	}

build_inform:
	/* send info message */
	body.s = imc_body_buf;
	if(member!=NULL)
	{
		body.len = snprintf(body.s, IMC_BUF_SIZE,
				"*** <%.*s@%.*s> has joined the room",
				src->user.len, src->user.s, src->host.len, src->host.s);

	} else {
		body.len = snprintf(body.s, IMC_BUF_SIZE,
				"*** <%.*s@%.*s> attempted to join the room",
				src->user.len, src->user.s, src->host.len, src->host.s);
	}
	if(body.len>0)
		imc_room_broadcast(room, &all_hdrs, &body);
	if(body.len>=IMC_BUF_SIZE)
		LM_ERR("member name %.*s@%.*s truncated\n",
				src->user.len, src->user.s, src->host.len, src->host.s);

done:
	if(room!=NULL)
		imc_release_room(room);
	return 0;

error:
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}

/**
 *
 */
int imc_handle_invite(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	int flag_member = 0;
	int size = 0;
	int i = 0;
	int add_domain = 0;
	int add_sip = 0;
	str uri = {0, 0};
	str body;
	str room_name;
	struct sip_uri inv_uri;
	del_member_t *cback_param = NULL;
	int result;
	uac_req_t uac_r;

	size = cmd->param[0].len+2 ;	
	add_domain = 1;
	add_sip = 0;
	while (i<size )
	{
		if(cmd->param[0].s[i]== '@')
		{	
			add_domain = 0;
			break;
		}
		i++;
	}

	if(add_domain)
		size += dst->host.len;
	if(cmd->param[0].len<4 || strncmp(cmd->param[0].s, "sip:", 4)!=0)
	{
		size += 4;
		add_sip = 1;
	}
		
	uri.s = (char*)pkg_malloc(size *sizeof(char));
	if(uri.s == NULL)
	{
		LM_ERR("no more pkg memory\n");
		goto error;
	}
	size= 0;
	if(add_sip)
	{	
		strcpy(uri.s, "sip:");
		size=4;
	}
		
	memcpy(uri.s+size, cmd->param[0].s, cmd->param[0].len);
	size += cmd->param[0].len;

	if(add_domain)
	{	
		uri.s[size] = '@';
		size++;
		memcpy(uri.s+ size, dst->host.s, dst->host.len);
		size+= dst->host.len;
	}
	uri.len = size;

	if(parse_uri(uri.s, uri.len, &inv_uri)!=0)
	{
		LM_ERR("bad uri [%.*s]!\n", uri.len, uri.s);
		goto error;
	}
					
	room_name = (cmd->param[1].s)?cmd->param[1]:dst->user;
	room = imc_get_room(&room_name, &dst->host);				
	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("the room does not exist [%.*s]!\n",
				room_name.len, room_name.s);
		goto error;
	}			
	member= imc_get_member(room, &src->user, &src->host);

	if(member==NULL)
	{
		LM_ERR("user [%.*s] is not member of[%.*s]!\n",
			src->user.len, src->user.s, room_name.len, room_name.s);
		goto error;
	}
	if(!(member->flags & IMC_MEMBER_OWNER) &&
			!(member->flags & IMC_MEMBER_ADMIN))
	{
		LM_ERR("user [%.*s] has no right to invite"
				" other users!\n", src->user.len, src->user.s);
		goto error;
	}
    
	member= imc_get_member(room, &inv_uri.user, &inv_uri.host);
	if(member!=NULL)
	{
		LM_ERR("user [%.*s] is already member"
				" of the room!\n", inv_uri.user.len, inv_uri.user.s);
		goto error;
	}
		
	flag_member |= IMC_MEMBER_INVITED;		
	member=imc_add_member(room, &inv_uri.user, &inv_uri.host, flag_member);
	if(member == NULL)
	{
		LM_ERR("adding member [%.*s]\n",
				inv_uri.user.len, inv_uri.user.s);
		goto error;	
	}
	
	body.len = 13 + member->uri.len - 4/* sip: */ + 28;	
	if(body.len>=IMC_BUF_SIZE || member->uri.len>=IMC_BUF_SIZE
			|| room->uri.len>=IMC_BUF_SIZE)
	{
		LM_ERR("buffer size overflow\n");
		goto error;	
	}

	body.s = imc_body_buf;
	memcpy(body.s, "INVITE from: ", 13);
	memcpy(body.s+13, member->uri.s + 4, member->uri.len - 4);
	memcpy(body.s+ 9 + member->uri.len, "(Type: '#accept' or '#deny')", 28);	
	body.s[body.len] = '\0';			

	LM_DBG("to=[%.*s]\nfrom=[%.*s]\nbody=[%.*s]\n", 
			member->uri.len,member->uri.s,room->uri.len, room->uri.s,
			body.len, body.s);
				
	cback_param = (del_member_t*)shm_malloc(sizeof(del_member_t));
	if(cback_param==NULL)
	{
		LM_ERR("no more shm\n");
		goto error;	
	}
	memset(cback_param, 0, sizeof(del_member_t));
	cback_param->room_name = room->name;
	cback_param->room_domain = room->domain;
	cback_param->member_name = member->user;
	cback_param->member_domain = member->domain;
	cback_param->inv_uri = member->uri;
	/*?!?! possible race with 'remove user' */

	set_uac_req(&uac_r, &imc_msg_type, &all_hdrs, &body, 0, TMCB_LOCAL_COMPLETED,
				imc_inv_callback, (void*)(cback_param));
	result= tmb.t_request(&uac_r,
				&member->uri,							/* Request-URI */
				&member->uri,							/* To */
				&room->uri,								/* From */
				(outbound_proxy.s)?&outbound_proxy:NULL/* outbound proxy*/
			);				
	if(result< 0)
	{
		LM_ERR("in tm send request\n");
		shm_free(cback_param);
		goto error;
	}
	if(uri.s!=NULL)
		pkg_free(uri.s);

	imc_release_room(room);

	return 0;

error:
	if(uri.s!=0)
		pkg_free(uri.s);
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}

/**
 *
 */
int imc_handle_accept(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	str room_name;
	str body;
	
	/* accepting the invitation */
	room_name = cmd->param[0].s?cmd->param[0]:dst->user;
	room=imc_get_room(&room_name, &dst->host);
	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s] is not created!\n",	room_name.len, room_name.s);
		goto error;
	}			
	/* if aready invited add as a member */
	member=imc_get_member(room, &src->user, &src->host);
	if(member==NULL || !(member->flags & IMC_MEMBER_INVITED))
	{
		LM_ERR("user [%.*s] not invited in the room!\n",
				src->user.len, src->user.s);
		goto error;
	}
			
	member->flags &= ~IMC_MEMBER_INVITED;
					
	/* send info message */
	body.s = imc_body_buf;
	body.len = snprintf(body.s, IMC_BUF_SIZE, "*** <%.*s> has joined the room",
					member->uri.len, member->uri.s);
	if(body.len>0)
		imc_room_broadcast(room, &all_hdrs, &body);

	if(body.len>=IMC_BUF_SIZE)
		LM_ERR("member name %.*s truncated\n", member->uri.len, member->uri.s);

	imc_release_room(room);
	return 0;

error:
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}

/**
 *
 */
int imc_handle_remove(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	str room_name;
	str body;
	str uri = {0, 0};
	int size =0;
	int i = 0;
	int add_domain = 0;
	int add_sip = 0;
	struct sip_uri inv_uri;

	size= cmd->param[0].len+2;
	add_domain = 1;
	while (i<size )
	{
		if(cmd->param[0].s[i]== '@')
		{	
			add_domain =0;
			break;
		}
		i++;
	}

	if(add_domain)
		size += dst->host.len;
	if(cmd->param[0].len<=4 || strncmp(cmd->param[0].s, "sip:", 4)!=0)
	{
		size+= 4;
		add_sip = 1;
	}

	uri.s = (char*)pkg_malloc(size*sizeof(char));
	if(uri.s == NULL)
	{
		LM_ERR("no more pkg memory\n");
		goto error;
	}

	size= 0;
	if(add_sip)
	{	
		strcpy(uri.s, "sip:");
		size = 4;
	}
					
	memcpy(uri.s+size, cmd->param[0].s, cmd->param[0].len);
	size+= cmd->param[0].len;

	if(add_domain)
	{	
		uri.s[size] = '@';
		size++;
		memcpy(uri.s+size, dst->host.s, dst->host.len);
		size+= dst->host.len;
	}
	uri.len = size;

	if(parse_uri(uri.s, uri.len, &inv_uri)<0)
	{
		LM_ERR("invalid uri [%.*s]\n", uri.len, uri.s);
		goto error;
	}
					
	room_name = cmd->param[1].s?cmd->param[1]:dst->user;
	room= imc_get_room(&room_name, &dst->host);
	if(room==NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s]does not exist!\n", room_name.len, room_name.s);
		goto error;
	}			

	/* verify if the user who sent the request is a member in the room
	 * and has the right to remove other users */
	member= imc_get_member(room, &src->user, &src->host);

	if(member== NULL)
	{
		LM_ERR("user [%.*s] is not member of room [%.*s]!\n", 
				src->user.len, src->user.s, room_name.len, room_name.s);
		goto error;
	}
	
	if(!(member->flags & IMC_MEMBER_OWNER) &&
		!(member->flags & IMC_MEMBER_ADMIN))
	{
			LM_ERR("user [%.*s] has no right to remove other users [%.*s]!\n",
					src->user.len, src->user.s,	uri.len, uri.s);
			goto error;
	}

	/* verify if the user that is to be removed is a member of the room */
	member= imc_get_member(room, &inv_uri.user, &inv_uri.host);
	if(member== NULL)
	{
		LM_ERR("user [%.*s] is not member of room [%.*s]!\n", 
				inv_uri.user.len, inv_uri.user.s, room_name.len, room_name.s);
		goto error;
	}
				
	if(member->flags & IMC_MEMBER_OWNER)
	{
		LM_ERR("user [%.*s] is owner of room [%.*s]"
			" -- cannot be removed!\n", inv_uri.user.len, inv_uri.user.s,
			room_name.len, room_name.s);
		goto error;
	}	

	/* send message to the removed person */
	body.s = "You have been removed from this room";
	body.len = strlen(body.s);

	LM_DBG("to: [%.*s]\nfrom: [%.*s]\nbody: [%.*s]\n",
			member->uri.len, member->uri.s , room->uri.len, room->uri.s,
			body.len, body.s);
	imc_send_message(&room->uri, &member->uri, &all_hdrs, &body);

	member->flags |= IMC_MEMBER_DELETED;
	imc_del_member(room, &inv_uri.user, &inv_uri.host);

	body.s = imc_body_buf;
	body.len = snprintf(body.s, IMC_BUF_SIZE, "*** <%.*s> has joined the room",
					member->uri.len, member->uri.s);
	if(body.len>0)
		imc_room_broadcast(room, &all_hdrs, &body);

	if(body.len>=IMC_BUF_SIZE)
		LM_ERR("member name %.*s truncated\n", member->uri.len, member->uri.s);

	if(uri.s!=0)
		pkg_free(uri.s);
	imc_release_room(room);
	return 0;

error:
	if(uri.s!=0)
		pkg_free(uri.s);
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}

/**
 *
 */
int imc_handle_deny(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	str room_name;
	// str body;

	/* denying an invitation */
	room_name = cmd->param[0].s?cmd->param[0]:dst->user;
	room= imc_get_room(&room_name, &dst->host);

	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s] does not exist!\n", room_name.len, room_name.s);
		goto error;
	}			
	/* If the user is an invited member, delete it froim the list */
	member= imc_get_member(room, &src->user, &src->host);
	if(member==NULL || !(member->flags & IMC_MEMBER_INVITED))
	{
		LM_ERR("user [%.*s] was not invited in room [%.*s]!\n", 
				src->user.len, src->user.s,	room_name.len, room_name.s);
		goto error;
	}		
	
#if 0
	/* send info message */
	body.s = imc_body_buf;
	body.len = snprintf(body.s, IMC_BUF_SIZE, 
			"The user [%.*s] has denied the invitation",
			src->user.len, src->user.s);
	if(body.len>0)
	    imc_send_message(&room->uri, &memeber->uri, &all_hdrs, &body);
#endif
	LM_ERR("user [%.*s] declined invitation in room [%.*s]!\n", 
			src->user.len, src->user.s,	room_name.len, room_name.s);

	imc_del_member(room, &src->user, &src->host);
	
	imc_release_room(room);

	return 0;
error:
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}

/**
 *
 */
int imc_handle_list(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	imc_member_p imp = 0;
	str room_name;
	str body;
	char *p;
	
	/* the user wants to leave the room */
	room_name = cmd->param[0].s?cmd->param[0]:dst->user;

	room= imc_get_room(&room_name, &dst->host);
	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s] does not exist!\n",	room_name.len, room_name.s);
		goto error;
	}		

	/* verify if the user is a member of the room */
	member = imc_get_member(room, &src->user, &src->host);

	if(member == NULL)
	{
		LM_ERR("user [%.*s] is not member of room [%.*s]!\n", 
				src->user.len, src->user.s,	room_name.len, room_name.s);
		goto error;
	}
	p = imc_body_buf;
	strncpy(p, "Members:\n", 9);
	p+=9;
	imp = room->members;

	while(imp)
	{
		if((imp->flags&IMC_MEMBER_INVITED)||(imp->flags&IMC_MEMBER_DELETED)
				|| (imp->flags&IMC_MEMBER_SKIP))
		{
			imp = imp->next;
			continue;
		}
		if(imp->flags & IMC_MEMBER_OWNER)
			*p++ = '*';
		else if(imp->flags & IMC_MEMBER_ADMIN)
			*p++ = '~';
		strncpy(p, imp->uri.s, imp->uri.len);
		p += imp->uri.len;
		*p++ = '\n';
		imp = imp->next;
	}
	
	imc_release_room(room);

	/* write over last '\n' */
	*(--p) = 0;
	body.s   = imc_body_buf;
	body.len = p-body.s;
	LM_DBG("members = [%.*s]\n", body.len, body.s);
	imc_send_message(&room->uri, &member->uri, &all_hdrs, &body);


	return 0;
error:
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}

/**
 *
 */
int imc_handle_exit(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	str room_name;
	str body;
	
	/* the user wants to leave the room */
	room_name = cmd->param[0].s?cmd->param[0]:dst->user;

	room= imc_get_room(&room_name, &dst->host);
	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s] does not exist!\n",	room_name.len, room_name.s);
		goto error;
	}		

	/* verify if the user is a member of the room */
	member= imc_get_member(room, &src->user, &src->host);

	if(member== NULL)
	{
		LM_ERR("user [%.*s] is not member of room [%.*s]!\n", 
				src->user.len, src->user.s,	room_name.len, room_name.s);
		goto error;
	}
			
	if(member->flags & IMC_MEMBER_OWNER)
	{
		/*If the user is the owner of the room, the room is distroyed */
		room->flags |=IMC_ROOM_DELETED;

		body.s = imc_body_buf;
		strcpy(body.s, "The room has been destroyed");
		body.len = strlen(body.s);
		imc_room_broadcast(room, &all_hdrs, &body);

		imc_release_room(room);
		
		imc_del_room(&room_name, &dst->host);
		room = NULL;
		goto done;
	} else {
		/* delete user */
		member->flags |= IMC_MEMBER_DELETED;
		imc_del_member(room, &src->user, &src->host);
		body.s = imc_body_buf;
		body.len = snprintf(body.s, IMC_BUF_SIZE, 
				"The user [%.*s] has left the room",
				src->user.len, src->user.s);
		if(body.len>0)
			imc_room_broadcast(room, &all_hdrs, &body);

		if(body.len>=IMC_BUF_SIZE)
			LM_ERR("user name %.*s truncated\n", src->user.len, src->user.s);
	}

done:
	if(room!=NULL)
		imc_release_room(room);
	return 0;

error:
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}

/**
 *
 */
int imc_handle_destroy(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	str room_name;
	str body;
	
	/* distrugere camera */
	room_name = cmd->param[0].s?cmd->param[0]:dst->user;

	room= imc_get_room(&room_name, &dst->host);
	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s] does not exist!\n",	room_name.len, room_name.s);
		goto error;
	}		

	/* verify is the user is a member of the room*/
	member= imc_get_member(room, &src->user, &src->host);

	if(member== NULL)
	{
		LM_ERR("user [%.*s] is not a member of room [%.*s]!\n", 
				src->user.len, src->user.s,	room_name.len, room_name.s);
		goto error;
	}
			
	if(!(member->flags & IMC_MEMBER_OWNER))
	{
		LM_ERR("user [%.*s] is not owner of room [%.*s] -- cannot destroy it"
				"!\n", src->user.len, src->user.s, room_name.len, room_name.s);
		goto error;
	}
	room->flags |= IMC_ROOM_DELETED;

	body.s = imc_body_buf;
	strcpy(body.s, "The room has been destroyed");
	body.len = strlen(body.s);

	/* braodcast message */
	imc_room_broadcast(room, &all_hdrs, &body);

	imc_release_room(room);

	LM_DBG("deleting room\n");
	imc_del_room(&room_name, &dst->host);

	return 0;

error:
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}

/**
 *
 */
int imc_handle_help(struct sip_msg* msg, imc_cmd_t *cmd, str *src, str *dst)
{
	str body;
	uac_req_t uac_r;

	body.s   = IMC_HELP_MSG;
	body.len = IMC_HELP_MSG_LEN;

	LM_DBG("to: [%.*s] from: [%.*s]\n", src->len, src->s, dst->len, dst->s);
	set_uac_req(&uac_r, &imc_msg_type, &all_hdrs, &body, 0, 0, 0, 0);
	tmb.t_request(&uac_r,
				NULL,									/* Request-URI */
				src,									/* To */
				dst,									/* From */
				(outbound_proxy.s)?&outbound_proxy:NULL/* outbound proxy */
				);
	return 0;
}

/**
 *
 */
int imc_handle_unknown(struct sip_msg* msg, imc_cmd_t *cmd, str *src, str *dst)
{
	str body;
	uac_req_t uac_r;

	body.s   = imc_body_buf;
	body.len = snprintf(body.s, IMC_BUF_SIZE,
		"invalid command '%.*s' - send ''%.*shelp' for details",
		cmd->name.len, cmd->name.s, imc_cmd_start_str.len, imc_cmd_start_str.s);

	if(body.len<0 || body.len>=IMC_BUF_SIZE)
	{
		LM_ERR("unable to print message\n");
		return -1;
	}

	LM_DBG("to: [%.*s] from: [%.*s]\n", src->len, src->s, dst->len, dst->s);
	set_uac_req(&uac_r, &imc_msg_type, &all_hdrs, &body, 0, 0, 0, 0);
	tmb.t_request(&uac_r,
				NULL,									/* Request-URI */
				src,									/* To */
				dst,									/* From */
				(outbound_proxy.s)?&outbound_proxy:NULL /* outbound proxy */
			);
	return 0;
}

/**
 *
 */
int imc_handle_message(struct sip_msg* msg, str *msgbody,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	str body;

	room = imc_get_room(&dst->user, &dst->host);		
	if(room==NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s] does not exist!\n",	dst->user.len, dst->user.s);
		goto error;
	}

	member= imc_get_member(room, &src->user, &src->host);
	if(member== NULL || (member->flags & IMC_MEMBER_INVITED))
	{
		LM_ERR("user [%.*s] has no rights to send messages in room [%.*s]!\n",
				src->user.len, src->user.s,	dst->user.len, dst->user.s);
		goto error;
	}
	
	LM_DBG("broadcast to room [%.*s]\n", room->uri.len, room->uri.s);

	body.s = imc_body_buf;
	body.len = msgbody->len + member->uri.len /* -4 (sip:) +4 (<>: ) */;
	if(body.len>=IMC_BUF_SIZE)
	{
		LM_ERR("buffer overflow [%.*s]\n", msgbody->len, msgbody->s);
		goto error;
	}
	body.s[0] = '<';
	memcpy(body.s + 1, member->uri.s + 4, member->uri.len - 4);
	memcpy(body.s + 1 + member->uri.len - 4, ">: ", 3);		
	memcpy(body.s + 1 + member->uri.len - 4 +3, msgbody->s, msgbody->len);
	body.s[body.len] = '\0';

	member->flags |= IMC_MEMBER_SKIP;
	imc_room_broadcast(room, &all_hdrs, &body);
	member->flags &= ~IMC_MEMBER_SKIP;

	imc_release_room(room);
	return 0;

error:
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}

/*
 *
 */
int imc_room_broadcast(imc_room_p room, str *ctype, str *body)
{
	imc_member_p imp;

	if(room==NULL || body==NULL)
		return -1;

	imp = room->members;

	LM_DBG("nr = %d\n", room->nr_of_members );

	while(imp)
	{
		LM_DBG("to uri = %.*s\n", imp->uri.len, imp->uri.s);
		if((imp->flags&IMC_MEMBER_INVITED)||(imp->flags&IMC_MEMBER_DELETED)
				|| (imp->flags&IMC_MEMBER_SKIP))
		{
			imp = imp->next;
			continue;
		}
		
		/* to-do: callbac to remove user fi delivery fails */
		imc_send_message(&room->uri, &imp->uri, ctype, body);
		
		imp = imp->next;
	}
	return 0;
}

/*
 *
 */
int imc_send_message(str *src, str *dst, str *headers, str *body)
{
	uac_req_t uac_r;
	if(src==NULL || dst==NULL || body==NULL)
		return -1;
	/* to-do: callbac to remove user fi delivery fails */
	set_uac_req(&uac_r, &imc_msg_type, headers, body, 0, 0, 0, 0);
	tmb.t_request(&uac_r,
			NULL,										/* Request-URI */
			dst,										/* To */
			src,										/* From */
			(outbound_proxy.s)?&outbound_proxy:NULL  	/* outbound proxy */
		);
	return 0;
}

/*
 *
 */
void imc_inv_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	str body_final;
	char from_uri_buf[256];
	char to_uri_buf[256];
	char body_buf[256];
	str from_uri_s, to_uri_s;
	imc_member_p member= NULL;
	imc_room_p room = NULL;
	uac_req_t uac_r;

	if(ps->param==NULL || *ps->param==NULL || 
			(del_member_t*)(*ps->param) == NULL)
	{
		LM_DBG("member not received\n");
		return;
	}
	
	LM_DBG("completed with status %d [member name domain:"
			"%p/%.*s/%.*s]\n",ps->code, ps->param, 
			((del_member_t *)(*ps->param))->member_name.len,
			((del_member_t *)(*ps->param))->member_name.s,
			((del_member_t *)(*ps->param))->member_domain.len, 
			((del_member_t *)(*ps->param))->member_domain.s);
	if(ps->code < 300)
		return;
	else
	{
		room= imc_get_room(&((del_member_t *)(*ps->param))->room_name,
						&((del_member_t *)(*ps->param))->room_domain );
		if(room==NULL)
		{
			LM_ERR("the room does not exist!\n");
			goto error;
		}			
		/*verify if the user who sent the request is a member in the room
		 * and has the right to remove other users */
		member= imc_get_member(room,
				&((del_member_t *)(*ps->param))->member_name,
				&((del_member_t *)(*ps->param))->member_domain);

		if(member== NULL)
		{
			LM_ERR("the user is not a member of the room!\n");
			goto error;
		}
		imc_del_member(room,
				&((del_member_t *)(*ps->param))->member_name,
				&((del_member_t *)(*ps->param))->member_domain);
		goto build_inform;

	}
	

build_inform:
		
	body_final.s = body_buf;
	body_final.len = member->uri.len - 4 /* sip: part of URI */ + 20;
	memcpy(body_final.s, member->uri.s + 4, member->uri.len - 4);
	memcpy(body_final.s+member->uri.len-4," is not registered.  ",21);
		
	goto send_message;

send_message:
	
	from_uri_s.s = from_uri_buf;
	from_uri_s.len = room->uri.len;
	strncpy(from_uri_s.s, room->uri.s, room->uri.len);

	LM_DBG("sending message\n");
	
	to_uri_s.s = to_uri_buf;
	to_uri_s.len = ((del_member_t *)(*ps->param))->inv_uri.len;
	strncpy(to_uri_s.s,((del_member_t *)(*ps->param))->inv_uri.s ,
			((del_member_t *)(*ps->param))->inv_uri.len);

	LM_DBG("to: %.*s\nfrom: %.*s\nbody: %.*s\n", to_uri_s.len, to_uri_s.s,
			from_uri_s.len, from_uri_s.s, body_final.len, body_final.s);
	set_uac_req(&uac_r, &imc_msg_type, &extra_hdrs, &body_final, 0, 0, 0, 0);
	tmb.t_request(&uac_r,
					NULL,									/* Request-URI */
					&to_uri_s,								/* To */
					&from_uri_s,							/* From */
					(outbound_proxy.s)?&outbound_proxy:NULL /* outbound proxy*/
				);
	if(room!=NULL)
	{
		imc_release_room(room);
	}

	if((del_member_t *)(*ps->param))
		shm_free(*ps->param);

	return;

error:
	if(room!=NULL)
	{
		imc_release_room(room);
	}
	if((del_member_t *)(*ps->param))
		shm_free(*ps->param);
	return; 
}


/*
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
#include "../../core/mem/shm_mem.h"
#include "../../core/mem/mem.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/parser/parse_uri.h"

#include "imc.h"
#include "imc_cmd.h"

#define IMC_BUF_SIZE	1024

#define PREFIX "*** "

static char imc_body_buf[IMC_BUF_SIZE];

static str imc_msg_type = { "MESSAGE", 7 };

static str msg_room_created    = STR_STATIC_INIT(PREFIX "Room was created");
static str msg_room_destroyed  = STR_STATIC_INIT(PREFIX "The room has been destroyed");
static str msg_room_not_found  = STR_STATIC_INIT(PREFIX "Room not found");
static str msg_room_exists     = STR_STATIC_INIT(PREFIX "Room already exists");
static str msg_user_joined     = STR_STATIC_INIT(PREFIX "<%.*s> has joined the room");
static str msg_user_joined2    = STR_STATIC_INIT(PREFIX "<%.*s@%.*s> has joined the room");
static str msg_user_left       = STR_STATIC_INIT(PREFIX "<%.*s> has left the room");
static str msg_join_attempt    = STR_STATIC_INIT(PREFIX "<%.*s@%.*s> attempted to join the room");
static str msg_invite          = STR_STATIC_INIT(PREFIX "Invite to join the room from: <%.*s> (send '%.*saccept' or '%.*sreject')");
static str msg_user_removed    = STR_STATIC_INIT(PREFIX "You have been removed from the room");
static str msg_invalid_command = STR_STATIC_INIT(PREFIX "Invalid command '%.*s' (send '%.*shelp' for help)");

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
	} else if(cmd->name.len==(sizeof("reject")-1)
				&& !strncasecmp(cmd->name.s, "reject", cmd->name.len)) {
		cmd->type = IMC_CMDID_REJECT;
	} else if(cmd->name.len==(sizeof("deny")-1)
				&& !strncasecmp(cmd->name.s, "deny", cmd->name.len)) {
		cmd->type = IMC_CMDID_REJECT;
	} else if(cmd->name.len==(sizeof("remove")-1)
				&& !strncasecmp(cmd->name.s, "remove", cmd->name.len)) {
		cmd->type = IMC_CMDID_REMOVE;
	} else if(cmd->name.len==(sizeof("leave")-1)
				&& !strncasecmp(cmd->name.s, "leave", cmd->name.len)) {
		cmd->type = IMC_CMDID_LEAVE;
	} else if(cmd->name.len==(sizeof("exit")-1)
				&& !strncasecmp(cmd->name.s, "exit", cmd->name.len)) {
		cmd->type = IMC_CMDID_LEAVE;
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
	LM_DBG("command: [%.*s]\n", STR_FMT(&cmd->name));
	for(i=0; i<IMC_CMD_MAX_PARAM; i++)
	{
		if(cmd->param[i].len<=0)
			break;
		LM_DBG("parameter %d=[%.*s]\n", i, STR_FMT(&cmd->param[i]));
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
		LM_DBG("new room [%.*s]\n",	STR_FMT(&cmd->param[0]));
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
		LM_DBG("added room uri= %.*s\n", STR_FMT(&room->uri));
		flag_member |= IMC_MEMBER_OWNER;
		/* adding the owner as the first member*/
		member= imc_add_member(room, &src->user, &src->host, flag_member);
		if(member == NULL)
		{
			LM_ERR("failed to add owner [%.*s]\n", STR_FMT(&src->user));
			goto error;
		}
		LM_DBG("added the owner as the first member "
				"[%.*s]\n", STR_FMT(&member->uri));
	
		/* send info message */
		imc_send_message(&room->uri, &member->uri, &all_hdrs, &msg_room_created);
		goto done;
	}

	/* room already exists */
	LM_DBG("room [%.*s] already exists\n", STR_FMT(&cmd->param[0]));

	if (imc_check_on_create)
	{
		imc_send_message(&room->uri, &member->uri, &all_hdrs, &msg_room_exists);
		goto done;
	}

	if(!(room->flags & IMC_ROOM_PRIV))
	{
		LM_DBG("checking if the user [%.*s] is a member\n",
				STR_FMT(&src->user));
		member= imc_get_member(room, &src->user, &src->host);
		if(member== NULL)
		{					
			member= imc_add_member(room, &src->user, &src->host, flag_member);
			if(member == NULL)
			{
				LM_ERR("failed to add member [%.*s]\n", STR_FMT(&src->user));
				goto error;
			}
			LM_DBG("added as member [%.*s]\n", STR_FMT(&member->uri));
			/* send info message */
			body.s = imc_body_buf;
			body.len = snprintf(body.s, IMC_BUF_SIZE, msg_user_joined.s, STR_FMT(&member->uri));
			if(body.len>0)
				imc_room_broadcast(room, &all_hdrs, &body);

			if(body.len>=IMC_BUF_SIZE)
				LM_ERR("member name %.*s truncated\n", STR_FMT(&member->uri));
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
		LM_DBG("could not find room [%.*s]\n", STR_FMT(&room_name));

		if (imc_create_on_join) {
			LM_DBG("Creating room [%.*s]\n", STR_FMT(&room_name));
			room= imc_add_room(&room_name, &dst->host, flag_room);
			if(room == NULL)
			{
				LM_ERR("failed to add new room [%.*s]\n", STR_FMT(&room_name));
				goto error;
			}
			LM_DBG("created a new room [%.*s]\n", STR_FMT(&room->name));

			flag_member |= IMC_MEMBER_OWNER;
			member= imc_add_member(room, &src->user, &src->host, flag_member);
			if(member == NULL)
			{
				LM_ERR("failed to add new member [%.*s]\n", STR_FMT(&src->user));
				goto error;
			}
			/* send info message */
			imc_send_message(&room->uri, &member->uri, &all_hdrs, &msg_room_created);
		} else {
			imc_send_message(&room->uri, &member->uri, &all_hdrs, &msg_room_not_found);
		}
		goto done;
	}

	/* room exists */
	LM_DBG("found room [%.*s]\n", STR_FMT(&room_name));

	member= imc_get_member(room, &src->user, &src->host);
	if(!(room->flags & IMC_ROOM_PRIV))
	{
		LM_DBG("room [%.*s] is public\n", STR_FMT(&room_name));
		if(member== NULL)
		{					
			LM_DBG("adding new member [%.*s]\n", STR_FMT(&src->user));
			member= imc_add_member(room, &src->user,
									&src->host, flag_member);	
			if(member == NULL)
			{
				LM_ERR("adding new user [%.*s]\n", STR_FMT(&src->user));
				goto error;
			}	
			goto build_inform;
		} else {	
			LM_DBG("member [%.*s] is in room already\n", STR_FMT(&member->uri));
		}
	} else {
		if(member==NULL)
		{
			LM_ERR("attept to join private room [%.*s] from user [%.*s]\n",
					STR_FMT(&room_name), STR_FMT(&src->user));
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
		body.len = snprintf(body.s, IMC_BUF_SIZE, msg_user_joined2.s, STR_FMT(&src->user), STR_FMT(&src->host));

	} else {
		body.len = snprintf(body.s, IMC_BUF_SIZE, msg_join_attempt.s, STR_FMT(&src->user), STR_FMT(&src->host));
	}
	if(body.len>0)
		imc_room_broadcast(room, &all_hdrs, &body);
	if(body.len>=IMC_BUF_SIZE)
		LM_ERR("member name %.*s@%.*s truncated\n", STR_FMT(&src->user), STR_FMT(&src->host));

done:
	if(room!=NULL)
		imc_release_room(room);
	return 0;

error:
	if(room!=NULL)
		imc_release_room(room);
	return -1;
}




static str
param2uri(struct sip_uri *parsed_uri, str param, struct sip_uri *dst)
{
	int size;
	int add_domain = 1;
	int add_sip = 0;
	int i = 0;
	str uri = {0, 0};

	size = param.len+2 ;
	while (i<size )
	{
		if(param.s[i]== '@')
		{	
			add_domain = 0;
			break;
		}
		i++;
	}

	if(add_domain)
		size += dst->host.len;
	if(param.len<4 || strncmp(param.s, "sip:", 4)!=0)
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
		
	memcpy(uri.s+size, param.s, param.len);
	size += param.len;

	if(add_domain)
	{	
		uri.s[size] = '@';
		size++;
		memcpy(uri.s+ size, dst->host.s, dst->host.len);
		size+= dst->host.len;
	}
	uri.len = size;

	if(parse_uri(uri.s, uri.len, parsed_uri)!=0)
	{
		LM_ERR("bad uri [%.*s]!\n", STR_FMT(&uri));
		goto error;
	}
        return uri;

error:
	if(uri.s!=0)
		pkg_free(uri.s);
        uri.s = 0;
        uri.len = 0;
        return uri;
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
	str uri = {0, 0};
	str body;
	str room_name;
	del_member_t *cback_param = NULL;
	int result;
	uac_req_t uac_r;
	struct sip_uri inv_uri;

	uri = param2uri(&inv_uri, cmd->param[0], dst);
	if (uri.s == 0) goto error;

	room_name = (cmd->param[1].s)?cmd->param[1]:dst->user;
	room = imc_get_room(&room_name, &dst->host);				
	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("the room does not exist [%.*s]!\n", STR_FMT(&room_name));
		goto error;
	}			
	member= imc_get_member(room, &src->user, &src->host);

	if(member==NULL)
	{
		LM_ERR("user [%.*s] is not member of[%.*s]!\n",
			STR_FMT(&src->user), STR_FMT(&room_name));
		goto error;
	}
	if(!(member->flags & IMC_MEMBER_OWNER) &&
			!(member->flags & IMC_MEMBER_ADMIN))
	{
		LM_ERR("user [%.*s] has no right to invite"
				" other users!\n", STR_FMT(&src->user));
		goto error;
	}
    
	member= imc_get_member(room, &inv_uri.user, &inv_uri.host);
	if(member!=NULL)
	{
		LM_ERR("user [%.*s] is already member"
				" of the room!\n", STR_FMT(&inv_uri.user));
		goto error;
	}
		
	flag_member |= IMC_MEMBER_INVITED;		
	member=imc_add_member(room, &inv_uri.user, &inv_uri.host, flag_member);
	if(member == NULL)
	{
		LM_ERR("adding member [%.*s]\n", STR_FMT(&inv_uri.user));
		goto error;
	}

	body.s = imc_body_buf;
        body.len = snprintf(body.s, IMC_BUF_SIZE, msg_invite.s, STR_FMT(&member->uri),
		STR_FMT(&imc_cmd_start_str), STR_FMT(&imc_cmd_start_str));

	LM_DBG("to=[%.*s]\nfrom=[%.*s]\nbody=[%.*s]\n",
	       STR_FMT(&member->uri), STR_FMT(&room->uri), STR_FMT(&body));

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
		LM_ERR("room [%.*s] does not exist!\n",	STR_FMT(&room_name));
		goto error;
	}			
	/* if aready invited add as a member */
	member=imc_get_member(room, &src->user, &src->host);
	if(member==NULL || !(member->flags & IMC_MEMBER_INVITED))
	{
		LM_ERR("user [%.*s] not invited to the room!\n", STR_FMT(&src->user));
		goto error;
	}
			
	member->flags &= ~IMC_MEMBER_INVITED;
					
	/* send info message */
	body.s = imc_body_buf;
	body.len = snprintf(body.s, IMC_BUF_SIZE, msg_user_joined.s, STR_FMT(&member->uri));
	if(body.len>0)
		imc_room_broadcast(room, &all_hdrs, &body);

	if(body.len>=IMC_BUF_SIZE)
		LM_ERR("member name %.*s truncated\n", STR_FMT(&member->uri));

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
        struct sip_uri parsed_uri;

        uri = param2uri(&parsed_uri, cmd->param[0], dst);
        if (uri.s == NULL) goto error;
					
	room_name = cmd->param[1].s?cmd->param[1]:dst->user;
	room= imc_get_room(&room_name, &dst->host);
	if(room==NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s]does not exist!\n", STR_FMT(&room_name));
		goto error;
	}			

	/* verify if the user who sent the request is a member in the room
	 * and has the right to remove other users */
	member= imc_get_member(room, &src->user, &src->host);

	if(member== NULL)
	{
		LM_ERR("user [%.*s] is not member of room [%.*s]!\n", 
				STR_FMT(&src->user), STR_FMT(&room_name));
		goto error;
	}
	
	if(!(member->flags & IMC_MEMBER_OWNER) &&
		!(member->flags & IMC_MEMBER_ADMIN))
	{
			LM_ERR("user [%.*s] has no right to remove other users [%.*s]!\n",
					STR_FMT(&src->user), STR_FMT(&uri));
			goto error;
	}

	/* verify if the user that is to be removed is a member of the room */
	member= imc_get_member(room, &parsed_uri.user, &parsed_uri.host);
	if(member== NULL)
	{
		LM_ERR("user [%.*s] is not member of room [%.*s]!\n", 
				STR_FMT(&parsed_uri.user), STR_FMT(&room_name));
		goto error;
	}
				
	if(member->flags & IMC_MEMBER_OWNER)
	{
		LM_ERR("user [%.*s] is owner of room [%.*s]"
			" -- cannot be removed!\n", STR_FMT(&parsed_uri.user),
			STR_FMT(&room_name));
		goto error;
	}	

	/* send message to the removed person */
	LM_DBG("to: [%.*s]\nfrom: [%.*s]\nbody: [%.*s]\n",
			STR_FMT(&member->uri) , STR_FMT(&room->uri),
			STR_FMT(&msg_user_removed));
	imc_send_message(&room->uri, &member->uri, &all_hdrs, &msg_user_removed);

	member->flags |= IMC_MEMBER_DELETED;
	imc_del_member(room, &parsed_uri.user, &parsed_uri.host);

	body.s = imc_body_buf;
	body.len = snprintf(body.s, IMC_BUF_SIZE, msg_user_left.s, STR_FMT(&member->uri));
	if(body.len>0)
		imc_room_broadcast(room, &all_hdrs, &body);

	if(body.len>=IMC_BUF_SIZE)
		LM_ERR("member name %.*s truncated\n", STR_FMT(&member->uri));

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
int imc_handle_reject(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst)
{
	imc_room_p room = 0;
	imc_member_p member = 0;
	str room_name;
	// str body;

	/* rejecting an invitation */
	room_name = cmd->param[0].s?cmd->param[0]:dst->user;
	room= imc_get_room(&room_name, &dst->host);

	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s] does not exist!\n", STR_FMT(&room_name));
		goto error;
	}			
	/* If the user is an invited member, delete it froim the list */
	member= imc_get_member(room, &src->user, &src->host);
	if(member==NULL || !(member->flags & IMC_MEMBER_INVITED))
	{
		LM_ERR("user [%.*s] was not invited to room [%.*s]!\n",
				STR_FMT(&src->user), STR_FMT(&room_name));
		goto error;
	}		
	
#if 0
	/* send info message */
	body.s = imc_body_buf;
	body.len = snprintf(body.s, IMC_BUF_SIZE, 
			"The user [%.*s] has rejected the invitation",
			STR_FMT(&src->user));
	if(body.len>0)
	    imc_send_message(&room->uri, &memeber->uri, &all_hdrs, &body);
#endif
	LM_ERR("user [%.*s] rejected invitation to room [%.*s]!\n",
			STR_FMT(&src->user), STR_FMT(&room_name));

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
	
	/* the user wants to list the room */
	room_name = cmd->param[0].s?cmd->param[0]:dst->user;

	room= imc_get_room(&room_name, &dst->host);
	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s] does not exist!\n",	STR_FMT(&room_name));
		goto error;
	}		

	/* verify if the user is a member of the room */
	member = imc_get_member(room, &src->user, &src->host);

	if(member == NULL)
	{
		LM_ERR("user [%.*s] is not member of room [%.*s]!\n", 
				STR_FMT(&src->user), STR_FMT(&room_name));
		goto error;
	}
	p = imc_body_buf;
	memcpy(p, "Members:\n", 9);
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
	LM_DBG("members = [%.*s]\n", STR_FMT(&body));
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
int imc_handle_leave(struct sip_msg* msg, imc_cmd_t *cmd,
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
		LM_ERR("room [%.*s] does not exist!\n",	STR_FMT(&room_name));
		goto error;
	}		

	/* verify if the user is a member of the room */
	member= imc_get_member(room, &src->user, &src->host);

	if(member== NULL)
	{
		LM_ERR("user [%.*s] is not member of room [%.*s]!\n", 
				STR_FMT(&src->user), STR_FMT(&room_name));
		goto error;
	}
			
	if(member->flags & IMC_MEMBER_OWNER)
	{
		/*If the user is the owner of the room, the room is distroyed */
		room->flags |=IMC_ROOM_DELETED;

		imc_room_broadcast(room, &all_hdrs, &msg_room_destroyed);

		imc_release_room(room);
		
		imc_del_room(&room_name, &dst->host);
		room = NULL;
		goto done;
	} else {
		/* delete user */
		member->flags |= IMC_MEMBER_DELETED;
		imc_del_member(room, &src->user, &src->host);
		body.s = imc_body_buf;
		body.len = snprintf(body.s, IMC_BUF_SIZE, msg_user_left.s, STR_FMT(&src->user));
		if(body.len>0)
			imc_room_broadcast(room, &all_hdrs, &body);

		if(body.len>=IMC_BUF_SIZE)
			LM_ERR("user name %.*s truncated\n", STR_FMT(&src->user));
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
	
	/* distrugere camera */
	room_name = cmd->param[0].s?cmd->param[0]:dst->user;

	room= imc_get_room(&room_name, &dst->host);
	if(room== NULL || (room->flags&IMC_ROOM_DELETED))
	{
		LM_ERR("room [%.*s] does not exist!\n",	STR_FMT(&room_name));
		goto error;
	}		

	/* verify is the user is a member of the room*/
	member= imc_get_member(room, &src->user, &src->host);

	if(member== NULL)
	{
		LM_ERR("user [%.*s] is not a member of room [%.*s]!\n", 
				STR_FMT(&src->user), STR_FMT(&room_name));
		goto error;
	}
			
	if(!(member->flags & IMC_MEMBER_OWNER))
	{
		LM_ERR("user [%.*s] is not owner of room [%.*s] -- cannot destroy it"
				"!\n", STR_FMT(&src->user), STR_FMT(&room_name));
		goto error;
	}
	room->flags |= IMC_ROOM_DELETED;

	/* braodcast message */
	imc_room_broadcast(room, &all_hdrs, &msg_room_destroyed);

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

	LM_DBG("to: [%.*s] from: [%.*s]\n", STR_FMT(src), STR_FMT(dst));
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
	body.len = snprintf(body.s, IMC_BUF_SIZE, msg_invalid_command.s,
		STR_FMT(&cmd->name), STR_FMT(&imc_cmd_start_str));

	if(body.len<0 || body.len>=IMC_BUF_SIZE)
	{
		LM_ERR("unable to print message\n");
		return -1;
	}

	LM_DBG("to: [%.*s] from: [%.*s]\n", STR_FMT(src), STR_FMT(dst));
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
		LM_ERR("room [%.*s] does not exist!\n",	STR_FMT(&dst->user));
		goto error;
	}

	member= imc_get_member(room, &src->user, &src->host);
	if(member== NULL || (member->flags & IMC_MEMBER_INVITED))
	{
		LM_ERR("user [%.*s] has no rights to send messages to room [%.*s]!\n",
				STR_FMT(&src->user), STR_FMT(&dst->user));
		goto error;
	}
	
	LM_DBG("broadcast to room [%.*s]\n", STR_FMT(&room->uri));

	body.s = imc_body_buf;
	body.len = msgbody->len + member->uri.len /* -4 (sip:) +4 (<>: ) */;
	if(body.len>=IMC_BUF_SIZE)
	{
		LM_ERR("buffer overflow [%.*s]\n", STR_FMT(msgbody));
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
		LM_DBG("to uri = %.*s\n", STR_FMT(&imp->uri));
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
			STR_FMT(&((del_member_t *)(*ps->param))->member_name),
			STR_FMT(&((del_member_t *)(*ps->param))->member_domain));
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

	LM_DBG("to: %.*s\nfrom: %.*s\nbody: %.*s\n", STR_FMT(&to_uri_s),
			STR_FMT(&from_uri_s), STR_FMT(&body_final));
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


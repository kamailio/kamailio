/*
 * $Id$
 *
 * imc module - instant messaging conferencing implementation
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
 *
 */


#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"

#include "imc_mng.h"
/* imc hash table */
extern imc_hentry_p _imc_htable;
extern int imc_hash_size;
extern char imc_cmd_start_char;
#define imc_get_hentry(_hid, _size) ((_hid)&(_size-1))

/**
 * hash thable init
 */
int imc_htable_init()
{
	int i;

	if(imc_hash_size<=0)
	{
		LOG(L_ERR, "imc:imc_htable_init: ERROR invalid hash table size\n");
		return -1;
	}
	_imc_htable = (imc_hentry_p)shm_malloc(imc_hash_size*sizeof(imc_hentry_t));
	if(_imc_htable == NULL)
	{
		LOG(L_ERR, "imc:imc_htable_init: ERROR no more shm memory\n");
		return -1;
	}
	memset(_imc_htable, 0, imc_hash_size*sizeof(imc_hentry_t));
	for(i=0; i<imc_hash_size; i++)
	{
		if (lock_init(&_imc_htable[i].lock)==0)
		{
			LOG(L_CRIT,
				"imc:imc_htable_init: ERROR initializing lock [%d]\n", i);
			goto error;
		}
	}
	
	return 0;

error:
	if(_imc_htable!=NULL)
	{
		shm_free(_imc_htable);
		_imc_htable = NULL;
	}

	return -1;
}

/**
 * destroy hash table
 */
int imc_htable_destroy()
{
	int i,room_deleted;
	imc_room_p irp = NULL, irp_temp=NULL;
	if(_imc_htable==NULL)
		return -1;
	
	for(i=0; i<imc_hash_size; i++)
	{
		lock_destroy(&_imc_htable[i].lock);
		if(_imc_htable[i].rooms==NULL)
			continue;
			irp = _imc_htable[i].rooms;
			while(irp){
				irp_temp = irp->next;
				room_deleted = imc_del_room(&irp->name, &irp->domain);
				irp = irp_temp;
			}
	}
	shm_free(_imc_htable);
	_imc_htable = NULL;
	return 0;
}

/**
 * add room
 */
imc_room_p imc_add_room(str* name, str* domain, int flags)
{
	imc_room_p irp = NULL;
	int size;
	int hidx;
	
	if(name == NULL || name->s==NULL || name->len<=0
			|| domain == NULL || domain->s==NULL || domain->len<=0)
	{
		LOG(L_ERR, "imc:imc_add_room: ERROR Invalid parameters\n");
		return NULL;
	}

	/* struct size + "sip:" + name len + "@" + domain len + '\0' */
	size = sizeof(imc_room_t) + (name->len+domain->len+6)*sizeof(char);
	irp = (imc_room_p)shm_malloc(size);
	if(irp==NULL)
	{
		LOG(L_ERR, "imc:imc_add_room:ERROR no more memory left\n");
		return NULL;
	}
	memset(irp, 0, size);
	
	irp->uri.len = 4 /*sip:*/ + name->len + 1 /*@*/ + domain->len;
	irp->uri.s = (char*)(((char*)irp)+sizeof(imc_room_t));
	memcpy(irp->uri.s, "sip:", 4);
	memcpy(irp->uri.s+4, name->s, name->len);
	irp->uri.s[4+name->len] = '@';
	memcpy(irp->uri.s+5+name->len, domain->s, domain->len);
	irp->uri.s[irp->uri.len] = '\0';

	irp->name.len = name->len;
	irp->name.s = irp->uri.s+4;
	irp->domain.len = domain->len;
	irp->domain.s = irp->uri.s+5+name->len;
	
	irp->flags  = flags;
	irp->hashid = core_case_hash(&irp->name, &irp->domain, 0);
	
	hidx = imc_get_hentry(irp->hashid, imc_hash_size);

	lock_get(&_imc_htable[hidx].lock);
	
	if(_imc_htable[hidx].rooms!=NULL)
	{
		irp->next = _imc_htable[hidx].rooms;
		_imc_htable[hidx].rooms->prev = irp;
		_imc_htable[hidx].rooms = irp;
	} else {
		_imc_htable[hidx].rooms = irp;
	}	
	
	return irp;
}

/**
 * release room
 */
int imc_release_room(imc_room_p room)
{
	unsigned int hidx;
	
	if(room==NULL)
	{
		LOG(L_ERR, "imc:imc_release_room: ERROR Invalid parameters\n");
		return -1;
	}
	
	hidx = imc_get_hentry(room->hashid, imc_hash_size);
	lock_release(&_imc_htable[hidx].lock);

	return 0;
}

/**
 * search room
 */
imc_room_p imc_get_room(str* name, str* domain)
{
	imc_room_p irp = NULL;
	unsigned int hashid;
	int hidx;
	
	if(name == NULL || name->s==NULL || name->len<=0
			|| domain == NULL || domain->s==NULL || domain->len<=0)
	{
		LOG(L_ERR, "imc:imc_get_room: ERROR Invalid parameters\n");
		return NULL;
	}
	
	hashid = core_case_hash(name, domain, 0);
	
	hidx = imc_get_hentry(hashid, imc_hash_size);

	lock_get(&_imc_htable[hidx].lock);
	irp = _imc_htable[hidx].rooms;

	while(irp)
	{
		if(irp->hashid==hashid && irp->name.len==name->len
				&& irp->domain.len==domain->len
				&& !strncasecmp(irp->name.s, name->s, name->len)
				&& !strncasecmp(irp->domain.s, domain->s, domain->len))
		{
			return irp;
		}
		irp = irp->next;
	}

	/* no room */
	lock_release(&_imc_htable[hidx].lock);

	return NULL;
}

/**
 * delete room
 */
int imc_del_room(str* name, str* domain)
{
	imc_room_p irp = NULL;
	imc_member_p imp=NULL, imp_temp=NULL;
	unsigned int hashid;
	int hidx;	
	
	if(name == NULL || name->s==NULL || name->len<=0
			|| domain == NULL || domain->s==NULL || domain->len<=0)
	{
		LOG(L_ERR, "imc:imc_del_room: ERROR Invalid parameters\n");
		return -1;
	}
	
	hashid = core_case_hash(name, domain, 0);
	
	hidx = imc_get_hentry(hashid, imc_hash_size);

	irp = _imc_htable[hidx].rooms;
	while(irp)
	{
		if(irp->hashid==hashid && irp->name.len==name->len
				&& irp->domain.len==domain->len
				&& !strncasecmp(irp->name.s, name->s, name->len)
				&& !strncasecmp(irp->domain.s, domain->s, domain->len))
		{
			if(irp->prev==NULL)
				_imc_htable[hidx].rooms = irp->next;
			else
				irp->prev->next = irp->next;
			if(irp->next!=NULL)
				irp->next->prev = irp->prev;

			/* delete members */
			imp = irp->members;
			while(imp){
				imp_temp = imp->next;
				shm_free(imp);
				imp = imp_temp;
			}		

			shm_free(irp);

			goto done;
		}
		irp = irp->next;
	}

done:	
	lock_release(&_imc_htable[hidx].lock);

	return 0;
}

/**
 * add member
 */
imc_member_p imc_add_member(imc_room_p room, str* user, str* domain, int flags)
{
	imc_member_p imp = NULL;
	int size;
	
	if(room==NULL || user == NULL || user->s==NULL || user->len<=0
			|| domain == NULL || domain->s==NULL || domain->len<=0)
	{
		LOG(L_ERR, "imc:imc_add_member:ERROR Invalid parameters\n");
		return NULL;
	}
	
	/* struct size + "sip:" + user name len + "@" + domain len + '\0' */
	size = sizeof(imc_member_t) + (user->len+domain->len+6)*sizeof(char);
	imp = (imc_member_p)shm_malloc(size);
	if(imp== NULL)
	{
		LOG(L_ERR, "imc:imc_add_member: ERROR while allocating memory\n");
		return NULL;
	}
	memset(imp, 0, size);
	
	imp->uri.len = 4 /*sip:*/ + user->len + 1 /*@*/ + domain->len;
	imp->uri.s = (char*)(((char*)imp)+sizeof(imc_member_t));
	memcpy(imp->uri.s, "sip:", 4);
	memcpy(imp->uri.s+4, user->s, user->len);
	imp->uri.s[4+user->len] = '@';
	memcpy(imp->uri.s+5+user->len, domain->s, domain->len);
	imp->uri.s[imp->uri.len] = '\0';
	
	DBG("imc:imc_add_member: [uri]= %.*s\n", imp->uri.len, imp->uri.s);
	imp->user.len = user->len;
	imp->user.s = imp->uri.s+4;
	
	DBG("imc:imc_add_member: [user]= %.*s\n", imp->user.len, imp->user.s);
	imp->domain.len = domain->len;
	imp->domain.s = imp->uri.s+5+user->len;

	imp->flags  = flags;
	imp->hashid = core_case_hash(&imp->user, &imp->domain, 0);

	room->nr_of_members++;
	
	if(room->members==NULL)
		room->members = imp;
	else {
		imp->next = room->members->next;
		if((room->members)->next!=NULL)
			((room->members)->next)->prev = imp;
		imp->prev = room->members;
		
		room->members->next=imp;
	}

	return imp;
}

/**
 * search memeber
 */
imc_member_p imc_get_member(imc_room_p room, str* user, str* domain)
{
	imc_member_p imp = NULL;
	unsigned int hashid;

	if(room==NULL || user == NULL || user->s==NULL || user->len<=0
			|| domain == NULL || domain->s==NULL || domain->len<=0)
	{
		LOG(L_ERR, "imc:imc_get_member:ERROR Invalid parameters\n");
		return NULL;
	}
	
	hashid = core_case_hash(user, domain, 0);
	imp = room->members;
	while(imp)
	{
		if(imp->hashid==hashid && imp->user.len==user->len
				&& imp->domain.len==domain->len
				&& !strncasecmp(imp->user.s, user->s, user->len)
				&& !strncasecmp(imp->domain.s, domain->s, domain->len))
		{
			DBG("imc:imc_get_member:found member\n");
			return imp;
		}
		imp = imp->next;
	}

	return NULL;
}

/**
 * delete member
 */
int imc_del_member(imc_room_p room, str* user, str* domain)
{
	imc_member_p imp = NULL;
	unsigned int hashid;
	
	if(room==NULL || user == NULL || user->s==NULL || user->len<=0
			|| domain == NULL || domain->s==NULL || domain->len<=0)
	{
		LOG(L_ERR, "imc:imc_del_member:ERROR Invalid parameters\n");
		return -1;
	}
	
	hashid = core_case_hash(user, domain, 0);
	imp = room->members;
	while(imp)
	{
		if(imp->hashid==hashid && imp->user.len==user->len
				&& imp->domain.len==domain->len
				&& !strncasecmp(imp->user.s, user->s, user->len)
				&& !strncasecmp(imp->domain.s, domain->s, domain->len))
		{
			if(imp->prev==NULL)
				room->members = imp->next;
			else
				imp->prev->next = imp->next;
			if(imp->next!=NULL)
				imp->next->prev = imp->prev;
			shm_free(imp);
			room->nr_of_members--;
			return 0;
		}
		imp = imp->next;
	}
	
	return 0;
}

/**
 * parse cmd
 */
int imc_parse_cmd(char *buf, int len, imc_cmd_p cmd)
{
	char *p;
	int i;
	if(buf==NULL || len<=0 || cmd==NULL)
	{
		LOG(L_ERR, "imc:imc_parse_cmd:ERROR Invalid parameters\n");
		return -1;
	}

	memset(cmd, 0, sizeof(imc_cmd_t));
	if(buf[0]!=imc_cmd_start_char)
	{
		LOG(L_ERR, "imc:imc_parse_cmd:ERROR Invalid command [%.*s]\n", len, buf);
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
		LOG(L_ERR, "imc:imc_parse_cmd:ERROR no command in [%.*s]\n", len, buf);
		return -1;
	}
	cmd->name.len = p - cmd->name.s;
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
	DBG("imc:imc_parse_cmd: command: [%.*s]\n", cmd->name.len, cmd->name.s);
	for(i=0; i<IMC_CMD_MAX_PARAM; i++)
	{
		if(cmd->param[i].len<=0)
			break;
		DBG("imc:imc_parse_cmd: parameter %d=[%.*s]\n", i, cmd->param[i].len,
				cmd->param[i].s);
	}
	return 0;
}


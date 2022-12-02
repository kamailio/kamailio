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


#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "../../lib/srdb1/db_res.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/dprint.h"
#include "../../core/hashes.h"

#include "imc_mng.h"
#include "imc.h"

/* imc hash table */
extern imc_hentry_p _imc_htable;
extern int imc_hash_size;
extern char imc_cmd_start_char;
#define imc_get_hentry(_hid, _size) ((_hid)&(_size-1))

/**
 * hash thable init
 */
int imc_htable_init(void)
{
	int i;

	if(imc_hash_size<=0)
	{
		LM_ERR("invalid hash table size\n");
		return -1;
	}
	_imc_htable = (imc_hentry_p)shm_malloc(imc_hash_size*sizeof(imc_hentry_t));
	if(_imc_htable == NULL)
	{
		LM_ERR("no more shm memory\n");
		return -1;
	}
	memset(_imc_htable, 0, imc_hash_size*sizeof(imc_hentry_t));
	for(i=0; i<imc_hash_size; i++)
	{
		if (lock_init(&_imc_htable[i].lock)==0)
		{
			LM_CRIT("failed to initialize lock [%d]\n", i);
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
int imc_htable_destroy(void)
{
	int i;
	imc_room_p irp = NULL, irp_temp=NULL;
	if(_imc_htable==NULL)
		return -1;
	
	for(i=0; i<imc_hash_size; i++)
	{
		lock_destroy(&_imc_htable[i].lock);
		if(_imc_htable[i].rooms==NULL) {
			continue;
		}
		irp = _imc_htable[i].rooms;
		while(irp){
			irp_temp = irp->next;
			imc_del_room(&irp->name, &irp->domain);
			irp = irp_temp;
		}
	}
	shm_free(_imc_htable);
	_imc_htable = NULL;
	return 0;
}

int load_rooms_from_db()
{
	imc_member_p member = NULL;
	int i, j, flag;
	db_key_t mq_result_cols[4], mquery_cols[2];
	db_key_t rq_result_cols[4];
	db_val_t mquery_vals[2];
	db1_res_t *r_res= NULL;
	db1_res_t *m_res= NULL;
	db_row_t *m_row = NULL, *r_row = NULL;
	db_val_t *m_row_vals, *r_row_vals = NULL;
	str name, domain;
	imc_room_p room = NULL;
	int er_ret = -1;

	rq_result_cols[0] = &imc_col_name;
	rq_result_cols[1] = &imc_col_domain;
	rq_result_cols[2] = &imc_col_flag;

	mq_result_cols[0] = &imc_col_username;
	mq_result_cols[1] = &imc_col_domain;
	mq_result_cols[2] = &imc_col_flag;

	mquery_cols[0] = &imc_col_room;
	mquery_vals[0].type = DB1_STR;
	mquery_vals[0].nul = 0;

	if(imc_dbf.use_table(imc_db, &rooms_table)< 0)
	{
		LM_ERR("use_table failed\n");
		return -1;
	}

	if(imc_dbf.query(imc_db,0, 0, 0, rq_result_cols,0, 3, 0,&r_res)< 0)
	{
		LM_ERR("failed to query table\n");
		return -1;
	}
	if(r_res==NULL || r_res->n<=0)
	{
		LM_INFO("the query returned no result\n");
		if(r_res) imc_dbf.free_result(imc_db, r_res);
		r_res = NULL;
		return 0;
	}

	LM_DBG("found %d rooms\n", r_res->n);

	for(i =0 ; i< r_res->n ; i++)
	{
		/*add rooms*/
		r_row = &r_res->rows[i];
		r_row_vals = ROW_VALUES(r_row);

		name.s = 	r_row_vals[0].val.str_val.s;
		name.len = strlen(name.s);

		domain.s = 	r_row_vals[1].val.str_val.s;
		domain.len = strlen(domain.s);

		flag = 	r_row_vals[2].val.int_val;

		room = imc_add_room(&name, &domain, flag);
		if(room == NULL)
		{
			LM_ERR("failed to add room\n ");
			goto error;
		}

		/* add members */
		if(imc_dbf.use_table(imc_db, &members_table)< 0)
		{
			LM_ERR("use_table failed\n ");
			goto error;
		}

		mquery_vals[0].val.str_val= room->uri;

		if(imc_dbf.query(imc_db, mquery_cols, 0, mquery_vals, mq_result_cols,
					1, 3, 0, &m_res)< 0)
		{
			LM_ERR("failed to query table\n");
			goto error;
		}

		if(m_res==NULL || m_res->n<=0)
		{
			LM_INFO("the query returned no result\n");
			er_ret = 0;
			goto error; /* each room must have at least one member*/
		}
		for(j =0; j< m_res->n; j++)
		{
			m_row = &m_res->rows[j];
			m_row_vals = ROW_VALUES(m_row);

			name.s = m_row_vals[0].val.str_val.s;
			name.len = strlen(name.s);

			domain.s = m_row_vals[1].val.str_val.s;
			domain.len = strlen(domain.s);

			flag = m_row_vals[2].val.int_val;

			LM_DBG("adding memeber: [name]=%.*s [domain]=%.*s"
					" in [room]= %.*s\n", STR_FMT(&name), STR_FMT(&domain),
					STR_FMT(&room->uri));

			member = imc_add_member(room, &name, &domain, flag);
			if(member == NULL)
			{
				LM_ERR("failed to adding member\n ");
				goto error;
			}
			imc_release_room(room);
		}

		if(m_res)
		{
			imc_dbf.free_result(imc_db, m_res);
			m_res = NULL;
		}
	}

	return 0;

error:
	if(r_res)
	{
		imc_dbf.free_result(imc_db, r_res);
		r_res = NULL;
	}
	if(m_res)
	{
		imc_dbf.free_result(imc_db, m_res);
		m_res = NULL;
	}
	if(room)
		imc_release_room(room);
	return er_ret;
}

int add_room_to_db(imc_room_p room) 
{
	db_key_t rkeys[3];
	db_val_t rvalues[3];
	
	rkeys[0] = &imc_col_name;
	rkeys[1] = &imc_col_domain;
	rkeys[2] = &imc_col_flag;

	rvalues[0].type = DB1_STR;
	rvalues[0].nul = 0;
	rvalues[0].val.str_val.s = room->name.s;
	rvalues[0].val.str_val.len = room->name.len;
	
	rvalues[1].type = DB1_STR;
	rvalues[1].nul = 0;
	rvalues[1].val.str_val.s = room->domain.s;
	rvalues[1].val.str_val.len = room->domain.len;

	rvalues[2].type = DB1_INT;
	rvalues[2].nul = 0;
	rvalues[2].val.int_val = 0;	

	if(imc_dbf.use_table(imc_db, &rooms_table)< 0)
	{
		LM_ERR("use_table failed on rooms_table\n");
		return -1;
	}

	if(imc_dbf.insert(imc_db, rkeys, rvalues, 3)< 0)
	{
		LM_ERR("failed to insert room\n");
		return -1;
	}	

	return 0;
}

int remove_room_from_db(imc_room_p room)
{
	db_key_t rkeys[2];
	db_val_t rvalues[2];
	db_key_t mkeys[1];
	db_val_t mvalues[1];
			
	mkeys[0] = &imc_col_room;

	mvalues[0].type = DB1_STR;
	mvalues[0].nul = 0;
	mvalues[0].val.str_val.s = room->uri.s;
	mvalues[0].val.str_val.len = room->uri.len;

	if(imc_dbf.use_table(imc_db, &members_table)< 0)
	{
		LM_ERR("use table failed\n ");
		return -1;
	}

	if(imc_dbf.delete(imc_db, mkeys, 0 , mvalues, 1) < 0)
	{
		LM_ERR("failed to delete room member from db\n");
		return -1;
	}

	rkeys[0] = &imc_col_name;
	rkeys[1] = &imc_col_domain;

	rvalues[0].type = DB1_STR;
	rvalues[0].nul = 0;
	rvalues[0].val.str_val.s = room->name.s;
	rvalues[0].val.str_val.len = room->name.len;
	
	rvalues[1].type = DB1_STR;
	rvalues[1].nul = 0;
	rvalues[1].val.str_val.s = room->domain.s;
	rvalues[1].val.str_val.len = room->domain.len;

	if(imc_dbf.use_table(imc_db, &rooms_table)< 0)
	{
		LM_ERR("use_table failed on rooms_table\n");
		return -1;
	}

	if(imc_dbf.delete(imc_db, rkeys, 0 , rvalues, 2) < 0)
	{
		LM_ERR("failed to delete room from db\n");
		return -1;
	}

	return 0;
}

int add_room_member_to_db(imc_member_p member, imc_room_p room, int flag) 
{
	db_key_t mkeys[4];
	db_val_t mvalues[4];
	
	mkeys[0] = &imc_col_username;
	mkeys[1] = &imc_col_domain;
	mkeys[2] = &imc_col_room;
	mkeys[3] = &imc_col_flag;

	mvalues[0].type = DB1_STR;
	mvalues[0].nul = 0;
	mvalues[0].val.str_val.s = member->user.s;
	mvalues[0].val.str_val.len = member->user.len;
	
	mvalues[1].type = DB1_STR;
	mvalues[1].nul = 0;
	mvalues[1].val.str_val.s = member->domain.s;
	mvalues[1].val.str_val.len = member->domain.len;

	mvalues[2].type = DB1_STR;
	mvalues[2].nul = 0;
	mvalues[2].val.str_val.s = room->uri.s;
	mvalues[2].val.str_val.len = room->uri.len;

	mvalues[3].type = DB1_INT;
	mvalues[3].nul = 0;
	mvalues[3].val.int_val = flag;		

	if(imc_dbf.use_table(imc_db, &members_table)< 0)
	{
		LM_ERR("use_table failed on members_table\n");
		return -1;
	}

	if(imc_dbf.insert(imc_db, mkeys, mvalues, 4)< 0)
	{
		LM_ERR("failed to insert member\n");
		return -1;
	}

	return 0;
}

int remove_room_member_from_db(imc_member_p member, imc_room_p room) {
	db_key_t mkeys[3];
	db_val_t mvalues[3];
	
	mkeys[0] = &imc_col_username;
	mkeys[1] = &imc_col_domain;
	mkeys[2] = &imc_col_room;

	mvalues[0].type = DB1_STR;
	mvalues[0].nul = 0;
	mvalues[0].val.str_val.s = member->user.s;
	mvalues[0].val.str_val.len = member->user.len;
	
	mvalues[1].type = DB1_STR;
	mvalues[1].nul = 0;
	mvalues[1].val.str_val.s = member->domain.s;
	mvalues[1].val.str_val.len = member->domain.len;

	mvalues[2].type = DB1_STR;
	mvalues[2].nul = 0;
	mvalues[2].val.str_val.s = room->uri.s;
	mvalues[2].val.str_val.len = room->uri.len;

	if(imc_dbf.use_table(imc_db, &members_table)< 0)
	{
		LM_ERR("use table failed\n ");
		return -1;
	}

	if(imc_dbf.delete(imc_db, mkeys, 0 , mvalues, 3) < 0)
	{
		LM_ERR("failed to delete room member from db\n");
		return -1;
	}

	return 0;
}

int modify_room_member_in_db(imc_member_p member, imc_room_p room, int flag)
{
	db_key_t mkeys[3];
	db_val_t mvalues[3];

	db_key_t mukeys[1];
	db_val_t muvalues[1];
	
	mkeys[0] = &imc_col_username;
	mkeys[1] = &imc_col_domain;
	mkeys[2] = &imc_col_room;	

	mvalues[0].type = DB1_STR;
	mvalues[0].nul = 0;
	mvalues[0].val.str_val.s = member->user.s;
	mvalues[0].val.str_val.len = member->user.len;
	
	mvalues[1].type = DB1_STR;
	mvalues[1].nul = 0;
	mvalues[1].val.str_val.s = member->domain.s;
	mvalues[1].val.str_val.len = member->domain.len;

	mvalues[2].type = DB1_STR;
	mvalues[2].nul = 0;
	mvalues[2].val.str_val.s = room->uri.s;
	mvalues[2].val.str_val.len = room->uri.len;	

	mukeys[0] = &imc_col_flag;
	
	muvalues[0].type = DB1_INT;
	muvalues[0].nul = 0;
	muvalues[0].val.int_val = flag;

	if(imc_dbf.use_table(imc_db, &members_table)< 0)
	{
		LM_ERR("use_table failed on members_table\n");
		return -1;
	}

	if(imc_dbf.update(imc_db, mkeys, 0, mvalues, mukeys, muvalues, 3, 1)< 0)
	{
		LM_ERR("failed to update member\n");
		return -1;
	}

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
		LM_ERR("invalid parameters\n");
		return NULL;
	}

	/* struct size + "sip:" + name len + "@" + domain len + '\0' */
	size = sizeof(imc_room_t) + (name->len+domain->len+6)*sizeof(char);
	irp = (imc_room_p)shm_malloc(size);
	if(irp==NULL)
	{
		LM_ERR("no more shm memory left\n");
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
		LM_ERR("invalid parameters\n");
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
		LM_ERR("invalid parameters\n");
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
		LM_ERR("invalid parameters\n");
		return -1;
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
			if(irp->prev==NULL) {
				_imc_htable[hidx].rooms = irp->next;
			} else {
				irp->prev->next = irp->next;
			}
			if(irp->next!=NULL) {
				irp->next->prev = irp->prev;
			}

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
		LM_ERR("invalid parameters\n");
		return NULL;
	}
	
	/* struct size + "sip:" + user name len + "@" + domain len + '\0' */
	size = sizeof(imc_member_t) + (user->len+domain->len+6)*sizeof(char);
	imp = (imc_member_p)shm_malloc(size);
	if(imp== NULL)
	{
		LM_ERR("out of shm memory\n");
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
	
	LM_DBG("[uri]= %.*s\n", STR_FMT(&imp->uri));
	imp->user.len = user->len;
	imp->user.s = imp->uri.s+4;
	
	LM_DBG("[user]= %.*s\n", STR_FMT(&imp->user));
	imp->domain.len = domain->len;
	imp->domain.s = imp->uri.s+5+user->len;

	imp->flags  = flags;
	imp->hashid = core_case_hash(&imp->user, &imp->domain, 0);

	room->nr_of_members++;
	
	if(room->members==NULL) {
		room->members = imp;
	} else {
		imp->next = room->members->next;
		if((room->members)->next!=NULL)
			((room->members)->next)->prev = imp;
		imp->prev = room->members;
		
		room->members->next=imp;
	}

	return imp;
}

int imc_modify_member(imc_room_p room, str* user, str* domain, int flags) {
	imc_member_p imp = NULL;
	unsigned int hashid;

	if(room==NULL || user == NULL || user->s==NULL || user->len<=0
			|| domain == NULL || domain->s==NULL || domain->len<=0)
	{
		LM_ERR("invalid parameters\n");
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
			LM_DBG("member found. modify flags\n");
			imp->flags = flags;
			imp->hashid = core_case_hash(&imp->user, &imp->domain, 0);			

			return 0;
		}
		imp = imp->next;
	}

	return -1;
}

/**
 * search member
 */
imc_member_p imc_get_member(imc_room_p room, str* user, str* domain)
{
	imc_member_p imp = NULL;
	unsigned int hashid;

	if(room==NULL || user == NULL || user->s==NULL || user->len<=0
			|| domain == NULL || domain->s==NULL || domain->len<=0)
	{
		LM_ERR("invalid parameters\n");
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
			LM_DBG("found member\n");
			return imp;
		}
		imp = imp->next;
	}

	return 0;
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
		LM_ERR("invalid parameters\n");
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

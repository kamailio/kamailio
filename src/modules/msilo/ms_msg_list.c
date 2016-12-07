/**
 * MSILO module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"

#include "ms_msg_list.h"

/**
 * create a new element
 */
msg_list_el msg_list_el_new(void)
{
	msg_list_el mle = NULL;
	mle = (msg_list_el)shm_malloc(sizeof(t_msg_list_el));
	if(mle == NULL)
		return NULL;

	mle->next = NULL;
	mle->prev = NULL;
	mle->msgid = 0;
	mle->flag = MS_MSG_NULL;

	return mle;
}

/**
 * free an element
 */
void msg_list_el_free(msg_list_el mle)
{
	if(mle)
		shm_free(mle);
}

/**
 * free a list of elements
 */
void msg_list_el_free_all(msg_list_el mle)
{
	msg_list_el p0, p1;	
	
	if(!mle)
		return;
	
	p0 = mle;
	while(p0)
	{
		p1 = p0;
		p0 = p0->next;
		msg_list_el_free(p1);
	}
}

/**
 * init a list
 */
msg_list msg_list_init(void)
{
	msg_list ml = NULL;
	
	ml = (msg_list)shm_malloc(sizeof(t_msg_list));
	if(ml == NULL)
		return NULL;
	/* init locks */
	if (lock_init(&ml->sem_sent)==0){
		LM_CRIT("could not initialize a lock\n");
		goto clean;
	};
	if (lock_init(&ml->sem_done)==0){
		LM_CRIT("could not initialize a lock\n");
		lock_destroy(&ml->sem_sent);
		goto clean;
	};
	ml->nrsent = 0;
	ml->nrdone = 0;
	ml->lsent = NULL;
	ml->ldone = NULL;
	
	return ml;

clean:
	shm_free(ml);
	return NULL;
}

/**
 * free a list
 */
void msg_list_free(msg_list ml)
{
	msg_list_el p0, p1;

	if(!ml)
		return;

	lock_destroy(&ml->sem_sent);
	lock_destroy(&ml->sem_done);

	if(ml->nrsent>0 && ml->lsent)
	{ // free sent list
		p0 = ml->lsent;
		ml->lsent = NULL;
		ml->nrsent = 0;
		while(p0)
		{
			p1 = p0->next;
			msg_list_el_free(p0);
			p0 = p1;
		}		
	}

	if(ml->nrdone>0 && ml->ldone)
	{ // free done list
		p0 = ml->ldone;
		ml->ldone = NULL;
		ml->nrdone = 0;
		while(p0)
		{
			p1 = p0->next;
			msg_list_el_free(p0);
			p0 = p1;
		}
	}
	
	shm_free(ml);	
}

/**
 * check if a message is in list
 */
int msg_list_check_msg(msg_list ml, int mid)
{
	msg_list_el p0, p1;	
	
	if(!ml || mid==0)
		goto errorx;

	LM_DBG("checking msgid=%d\n", mid);
	
	lock_get(&ml->sem_sent);

	p0 = p1 = ml->lsent;
	while(p0)
	{
		if(p0->msgid==mid)
			goto exist;
		p1 = p0;
		p0 = p0->next;
	}

	p0 = msg_list_el_new();
	if(!p0)
	{
		LM_ERR("failed to create new msg elem.\n");
		goto error;
	}
	p0->msgid = mid;
	p0->flag |= MS_MSG_SENT;

	if(p1)
	{
		p1->next = p0;
		p0->prev = p1;
		goto done;
	}
	
	ml->lsent = p0;
		
done:
	ml->nrsent++;
	lock_release(&ml->sem_sent);
	LM_DBG("msg added to sent list.\n");
	return MSG_LIST_OK;
exist:
	lock_release(&ml->sem_sent);
	LM_DBG("msg already in sent list.\n");
	return MSG_LIST_EXIST;	
error:
	lock_release(&ml->sem_sent);
errorx:
	return MSG_LIST_ERR;
}

/**
 * set flag for message with mid
 */
int msg_list_set_flag(msg_list ml, int mid, int fl)
{
	msg_list_el p0;	
	
	if(ml==0 || mid==0)
	{
		LM_ERR("bad param %p / %d\n", ml, fl);
		goto errorx;
	}
	
	lock_get(&ml->sem_sent);

	p0 = ml->lsent;
	while(p0)
	{
		if(p0->msgid==mid)
		{
			p0->flag |= fl;
			LM_DBG("mid:%d fl:%d\n", p0->msgid, fl);
			goto done;
		}
		p0 = p0->next;
	}

done:
	lock_release(&ml->sem_sent);
	return MSG_LIST_OK;
errorx:
	return MSG_LIST_ERR;
}

/**
 * check if the messages from list were sent
 */
int msg_list_check(msg_list ml)
{
	msg_list_el p0;
	msg_list_el p1;
	
	if(!ml)
		goto errorx;
	
	lock_get(&ml->sem_sent);
	if(ml->nrsent<=0)
		goto done;
	
	lock_get(&ml->sem_done);
	
	p0 = ml->lsent;
	while(p0)
	{
		p1 = p0->next;
		if(p0->flag & MS_MSG_DONE || p0->flag & MS_MSG_ERRO)
		{
			LM_DBG("mid:%d got reply\n", p0->msgid);
			if(p0->prev)
				(p0->prev)->next = p0->next;
			else
			    ml->lsent = p0->next;
			if(p0->next)
				(p0->next)->prev = p0->prev;
			ml->nrsent--;
			if(!ml->nrsent)
				ml->lsent = NULL;

			if(ml->ldone)
				(ml->ldone)->prev = p0;
			p0->next = ml->ldone;
			
			p0->prev = NULL;

			ml->ldone = p0;
			ml->nrdone++;
		}
		p0 = p1;
	}

	lock_release(&ml->sem_done);

done:
	lock_release(&ml->sem_sent);
	return MSG_LIST_OK;
errorx:
	return MSG_LIST_ERR;
}

/**
 * reset a list
 * return old list
 */
msg_list_el msg_list_reset(msg_list ml)
{
	msg_list_el p0;	
	
	if(!ml)
		return NULL;
	
	lock_get(&ml->sem_done);
	p0 = ml->ldone;
	ml->ldone = NULL;
	ml->nrdone = 0;
	lock_release(&ml->sem_done);
	
	return p0;
}


/**
 * MSILO module
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"

#include "ms_msg_list.h"

/**
 *
 */
msg_list_el msg_list_el_new()
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
 *
 */
void msg_list_el_free(msg_list_el mle)
{
	if(mle)
	{
		shm_free(mle);
		mle = NULL;
	}
}

/**
 *
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
 *
 */
msg_list msg_list_init()
{
	msg_list ml = NULL;
	
	ml = (msg_list)shm_malloc(sizeof(t_msg_list));
	if(ml == NULL)
		return NULL;
	if((ml->sems = create_semaphores(2)) == NULL)
		goto clean;
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
 *
 */
void msg_list_free(msg_list ml)
{
	msg_list_el p0, p1;

	if(!ml)
		return;

	if(ml->sems != NULL)
		destroy_semaphores(ml->sems);

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
 *
 */
int msg_list_check_msg(msg_list ml, int mid)
{
	msg_list_el p0, p1;	
	
	if(!ml || mid==0)
		goto errorx;

	DBG("MSILO:msg_list_check_msg: checking msgid=%d\n", mid);
	
	s_lock_at(ml->sems, MS_SEM_SENT);

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
		DBG("MSILO:msg_list_check_msg: Error creating new msg elem.\n");
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
	goto done;
		
done:
	ml->nrsent++;
	s_unlock_at(ml->sems, MS_SEM_SENT);
	DBG("MSILO:msg_list_check_msg: msg added to sent list.\n");
	return MSG_LIST_OK;
exist:
	s_unlock_at(ml->sems, MS_SEM_SENT);
	DBG("MSILO:msg_list_check_msg: msg already in sent list.\n");
	return MSG_LIST_EXIST;	
error:
	s_unlock_at(ml->sems, MS_SEM_SENT);
errorx:
	return MSG_LIST_ERR;
}

/**
 *
 */
int msg_list_set_flag(msg_list ml, int mid, int fl)
{
	msg_list_el p0;	
	
	if(!ml || mid==0)
		goto errorx;
	
	s_lock_at(ml->sems, MS_SEM_SENT);

	p0 = ml->lsent;
	while(p0)
	{
		if(p0->msgid==mid)
		{
			p0->flag |= fl;
			DBG("MSILO: msg_list_set_flag: mid:%d fl:%d\n", p0->msgid, fl);
			goto done;
		}
		p0 = p0->next;
	}

done:
	s_unlock_at(ml->sems, MS_SEM_SENT);
	return MSG_LIST_OK;
errorx:
	return MSG_LIST_ERR;
}

/**
 *
 */
int msg_list_check(msg_list ml)
{
	msg_list_el p0, p1;	
	
	if(!ml)
		goto errorx;
	
	s_lock_at(ml->sems, MS_SEM_SENT);
	if(ml->nrsent<=0)
		goto done;
	
	s_lock_at(ml->sems, MS_SEM_DONE);
	
	p0 = ml->lsent;
	while(p0)
	{
		p1 = p0->next;
		if(p0->flag & MS_MSG_DONE)
		{
			DBG("MSILO: msg_list_check: mid:%d is done\n", p0->msgid);
			if(p0->prev)
				p0->prev->next = p1;
			if(p0->next)
				p0->next->prev = p0->prev;
			ml->nrsent--;
			if(!ml->nrsent)
				ml->lsent = NULL;

			if(ml->ldone)
			{
				ml->ldone->prev = p0;
				p0->next = ml->ldone;
			}
			else
				p0->next = p0->prev = NULL;

			ml->ldone = p0;
			ml->nrdone++;
		}
		p0 = p1;
	}

	s_unlock_at(ml->sems, MS_SEM_DONE);

done:
	s_unlock_at(ml->sems, MS_SEM_SENT);
	return MSG_LIST_OK;
errorx:
	return MSG_LIST_ERR;
}

/**
 *
 */
msg_list_el msg_list_reset(msg_list ml)
{
	msg_list_el p0;	
	
	if(!ml)
		return NULL;
	
	s_lock_at(ml->sems, MS_SEM_DONE);
	p0 = ml->ldone;
	ml->ldone = NULL;
	ml->nrdone = 0;
	s_unlock_at(ml->sems, MS_SEM_DONE);
	
	return p0;
}


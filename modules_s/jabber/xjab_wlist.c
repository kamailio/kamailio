/*
 * eXtended JABber module - worker implemetation
 *
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

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#include "xjab_worker.h"
#include "mdefines.h"

/**
 * init a workers list
 * - pipes : communication pipes
 * - size : size of list - number of workers
 * - max : maximum number of jobs per worker
 * #return : pointer to workers list or NULL on error
 */
xj_wlist xj_wlist_init(int **pipes, int size, int max, int cache_time,
		int sleep_time, int delay_time)
{
	int i;
	xj_wlist jwl = NULL;

	if(pipes == NULL || size <= 0 || max <= 0)
		return NULL;

	DBG("XJAB:xj_wlist_init: -----START-----\n");
	
	jwl = (xj_wlist)_M_SHM_MALLOC(sizeof(t_xj_wlist));
	if(jwl == NULL)
		return NULL;
	jwl->len = size;
	jwl->maxj = max;
	
	jwl->cachet = cache_time;
	jwl->delayt = delay_time;
	jwl->sleept = sleep_time;

	jwl->contact_h = NULL;
	jwl->aliases = NULL;
	jwl->sems = NULL;
	i = 0;
	if((jwl->sems = create_semaphores(size)) == NULL)
		goto clean;
	jwl->workers = (xj_worker)_M_SHM_MALLOC(size*sizeof(t_xj_worker));
	if(jwl->workers == NULL)
		goto clean;

	for(i = 0; i < size; i++)
	{
		jwl->workers[i].nr = 0;
		jwl->workers[i].pid = 0;
		jwl->workers[i].wpipe = pipes[i][1];
		jwl->workers[i].rpipe = pipes[i][0];
		if((jwl->workers[i].sip_ids = newtree234(xj_jkey_cmp)) == NULL)
			goto clean;
	}	

	return jwl;

clean:
	DBG("XJAB:xj_wlist_init: error ocurred -> cleaning\n");
	if(jwl->sems != NULL)
		destroy_semaphores(jwl->sems);
	if(jwl->workers != NULL)
	{
		while(i>=0)
		{
			if(jwl->workers[i].sip_ids == NULL)
				free2tree234(jwl->workers[i].sip_ids, xj_jkey_free_p);
			i--;
		}
		_M_SHM_FREE(jwl->workers);
	}
	_M_SHM_FREE(jwl);
	return NULL;

}

/**
 * init contact address for SIP messages that will be sent by workers
 * - jwl - pointer to workers list
 * - ch - string representation of the contact, e.g. 'sip:100.100.100.100:5060'
 * #return : 0 on success or <0 on error
 * info: still has 0 at the end of string
 */
int xj_wlist_init_contact(xj_wlist jwl, char *ch)
{
	int f = 0; // correction flag: 1 -> must be converted to <sip: ... > 
	if(ch == NULL)
		return -1;
	if((jwl->contact_h = (str*)_M_SHM_MALLOC(sizeof(str))) == NULL)
		return -1;
	jwl->contact_h->len = strlen(ch);

	if(jwl->contact_h->len > 2 && strstr(ch, "sip:") == NULL)
	{
		// contact correction
		jwl->contact_h->len += 6;
		f = 1;
	}

	if((jwl->contact_h->s=(char*)_M_SHM_MALLOC(jwl->contact_h->len+1))==NULL)
	{
		_M_SHM_FREE(jwl->contact_h);
		return -2;
	}

	if(f)
	{
		strncpy(jwl->contact_h->s, "<sip:", 5);
		strcpy(jwl->contact_h->s+5, ch);
		jwl->contact_h->s[jwl->contact_h->len-1] = '>';
		jwl->contact_h->s[jwl->contact_h->len] = 0;
	}
	else
		strcpy(jwl->contact_h->s, ch);

	return 0;
}


/**
 * set the p.id's of the workers
 * - jwl : pointer to the workers list
 * - pids : p.id's array
 * - size : number of pids
 * #return : 0 on success or <0 on error
 */
int xj_wlist_set_pid(xj_wlist jwl, int pid, int idx)
{
	if(jwl == NULL || pid <= 0 || idx < 0 || idx >= jwl->len)
		return -1;
	s_lock_at(jwl->sems, idx);
	jwl->workers[idx].pid = pid;
	s_unlock_at(jwl->sems, idx);
	return 0;
}

/**
 * free jab_wlist
 * - jwl : pointer to the workers list
 */
void xj_wlist_free(xj_wlist jwl)
{
	int i;
	DBG("XJAB:xj_wlist_free: freeing 'xj_wlist' memory ...\n");
	if(jwl == NULL)
		return;

	if(jwl->contact_h != NULL && jwl->contact_h->s != NULL)
		_M_SHM_FREE(jwl->contact_h->s);
	if(jwl->contact_h != NULL)
		_M_SHM_FREE(jwl->contact_h);

	if(jwl->workers != NULL)
	{
		for(i=0; i<jwl->len; i++)
			free2tree234(jwl->workers[i].sip_ids, xj_jkey_free_p);
		_M_SHM_FREE(jwl->workers);
	}

	if(jwl->aliases != NULL)
	{
		if(jwl->aliases->jdm != NULL)
		{
			_M_SHM_FREE(jwl->aliases->jdm->s);
			_M_SHM_FREE(jwl->aliases->jdm);
		}
		if(jwl->aliases->size > 0)
		{
			for(i=0; i<jwl->aliases->size; i++)
				_M_SHM_FREE(jwl->aliases->a[i].s);
			_M_SHM_FREE(jwl->aliases->a);
		}
		_M_SHM_FREE(jwl->aliases);
		jwl->aliases = NULL;
	}
	
	//rm_sem(jwl->semid);
	if(jwl->sems != NULL)
		destroy_semaphores(jwl->sems);
	
	_M_SHM_FREE(jwl);
}

/**
 * return communication pipe with the worker that will process the message for
 * 		the id 'sid' only if it exsists, or -1 if error
 * - jwl : pointer to the workers list
 * - sid : id of the entity (connection to Jabber - usually SHOULD be FROM
 *   header of the incomming SIP message)
 * - p : will point to the SHM location of the 'sid' in jwl
 */
int xj_wlist_check(xj_wlist jwl, xj_jkey jkey, xj_jkey *p)
{
	int i;
	if(jwl==NULL || jkey==NULL || jkey->id==NULL || jkey->id->s==NULL)
		return -1;
	
	i = 0;
	*p = NULL;
	while(i < jwl->len)
	{
		s_lock_at(jwl->sems, i);
		if(jwl->workers[i].pid <= 0)
		{
			s_unlock_at(jwl->sems, i);
			i++;
			continue;
		}
		if((*p = find234(jwl->workers[i].sip_ids, (void*)jkey, NULL)) != NULL)
		{
			s_unlock_at(jwl->sems, i);
			DBG("XJAB:xj_wlist_check: entry exists for <%.*s> in the"
				" pool of <%d> [%d]\n",jkey->id->len, jkey->id->s,
				jwl->workers[i].pid,i);
			return jwl->workers[i].wpipe;
		}
		s_unlock_at(jwl->sems, i);
		i++;
	}
	DBG("XJAB:xj_wlist_check: entry does not exist for <%.*s>\n",
			jkey->id->len, jkey->id->s);	
	return -1;
}

/**
 * return communication pipe with the worker that will process the message for
 * 		the id 'sid', or -1 if error
 * - jwl : pointer to the workers list
 * - sid : id of the entity (connection to Jabber - usually SHOULD be FROM
 *   header of the incomming SIP message)
 * - p : will point to the SHM location of the 'sid' in jwl
 */
int xj_wlist_get(xj_wlist jwl, xj_jkey jkey, xj_jkey *p)
{
	int i = 0, pos = -1, min = 100000;
	xj_jkey msid = NULL;
	
	if(jwl==NULL || jkey==NULL || jkey->id==NULL || jkey->id->s==NULL)
		return -1;

	*p = NULL;
	while(i < jwl->len)
	{
		s_lock_at(jwl->sems, i);
		if(jwl->workers[i].pid <= 0)
		{
			s_unlock_at(jwl->sems, i);
			i++;
			continue;
		}
		if((*p = find234(jwl->workers[i].sip_ids, (void*)jkey, NULL))!=NULL)
		{
			if(pos >= 0)
				s_unlock_at(jwl->sems, pos);
			s_unlock_at(jwl->sems, i);
			DBG("XJAB:xj_wlist_get: entry already exists for <%.*s> in the"
				" pool of <%d> [%d]\n",jkey->id->len, jkey->id->s,
				jwl->workers[i].pid,i);
			return jwl->workers[i].wpipe;
		}
		if(min > jwl->workers[i].nr)
		{
			if(pos >= 0)
				s_unlock_at(jwl->sems, pos);
			pos = i;
			min = jwl->workers[i].nr;
		}
		else
			s_unlock_at(jwl->sems, i);
		i++;
	}
	if(pos >= 0 && jwl->workers[pos].nr < jwl->maxj)
	{
		jwl->workers[pos].nr++;

		msid = (xj_jkey)_M_SHM_MALLOC(sizeof(t_xj_jkey));
		if(msid == NULL)
			goto error;
		msid->id = (str*)_M_SHM_MALLOC(sizeof(str));
		if(msid->id == NULL)
		{
			_M_SHM_FREE(msid);
			goto error;
		}
		
		msid->id->s = (char*)_M_SHM_MALLOC(jkey->id->len);
		if(msid->id == NULL)
		{
			_M_SHM_FREE(msid->id);
			_M_SHM_FREE(msid);
			goto error;
		}
		
		if((*p = add234(jwl->workers[pos].sip_ids, msid)) != NULL)
		{
			msid->id->len = jkey->id->len;
			memcpy(msid->id->s, jkey->id->s, jkey->id->len);
			msid->hash = jkey->hash;
			msid->flag = 0;
			s_unlock_at(jwl->sems, pos);
			DBG("XJAB:xj_wlist_get: new entry for <%.*s> in the pool of"
				" <%d> - [%d]\n", jkey->id->len, jkey->id->s,
				jwl->workers[pos].pid, pos);
			return jwl->workers[pos].wpipe;
		}
		_M_SHM_FREE(msid->id->s);
		_M_SHM_FREE(msid->id);
		_M_SHM_FREE(msid);
	}

error:
	if(pos >= 0)
		s_unlock_at(jwl->sems, pos);

	DBG("XJAB:xj_wlist_get: can not create a new entry for <%.*s>\n",
				jkey->id->len, jkey->id->s);
	return -1;
}

int  xj_wlist_set_aliases(xj_wlist jwl, char *als, char *jd)
{
	char *p, *p0;
	int i;
	
	DBG("XJAB:xj_wlist_set_aliases\n");
	if(jwl == NULL)
		return -1;
	if(!jd) // || !als || strlen(als)<2)
		return 0;
	
	if((jwl->aliases = (xj_jalias)_M_SHM_MALLOC(sizeof(t_xj_jalias)))==NULL)
	{
		DBG("XJAB:xj_wlist_set_aliases: not enough SHMemory.\n");
		return -1;
	}
	
	jwl->aliases->jdm = NULL;
	jwl->aliases->size = 0;
	jwl->aliases->a = NULL;
	
	if(jd != NULL && strlen(jd)>2)
	{
		if((jwl->aliases->jdm = (str*)_M_SHM_MALLOC(sizeof(str)))== NULL)
		{
			DBG("XJAB:xj_wlist_set_aliases: not enough SHMemory!?\n");
			_M_SHM_FREE(jwl->aliases);
			jwl->aliases = NULL;
			return -1;		
		}
		jwl->aliases->jdm->len = strlen(jd);
		if((jwl->aliases->jdm->s=(char*)_M_SHM_MALLOC(jwl->aliases->jdm->len))
				== NULL)
		{
			DBG("XJAB:xj_wlist_set_aliases: not enough SHMemory!?!\n");
			_M_SHM_FREE(jwl->aliases->jdm);
			_M_SHM_FREE(jwl->aliases);
			jwl->aliases = NULL;
		}
		strncpy(jwl->aliases->jdm->s, jd, jwl->aliases->jdm->len);
	}
	
	if(!als || strlen(als)<2)
		return 0;
	
	if((p = strchr(als, ';')) == NULL)
	{
		DBG("XJAB:xj_wlist_set_aliases: bad parameter value\n");
		return -1;
	}
	
	if((jwl->aliases->size = atoi(als)) <= 0)
	{
		DBG("XJAB:xj_wlist_set_aliases: wrong number of aliases\n");
		return 0;
	}
	
	if((jwl->aliases->a = (str*)_M_SHM_MALLOC(jwl->aliases->size*sizeof(str)))
			== NULL)
	{
		DBG("XJAB:xj_wlist_set_aliases: not enough SHMemory..\n");
		if(jwl->aliases->jdm)
		{
			_M_SHM_FREE(jwl->aliases->jdm->s);
			_M_SHM_FREE(jwl->aliases->jdm);
		}
		_M_SHM_FREE(jwl->aliases);
		jwl->aliases = NULL;
		return -1;
	}
	
	p++;
	for(i=0; i<jwl->aliases->size; i++)
	{
		if((p0 = strchr(p, ';'))==NULL)
		{
			DBG("XJAB:xj_wlist_set_aliases: bad parameter value format\n");
			goto clean;
		}
		jwl->aliases->a[i].len = p0 - p;
		if((jwl->aliases->a[i].s = (char*)_M_SHM_MALLOC(jwl->aliases->a[i].len))
				== NULL)
		{
			DBG("XJAB:xj_wlist_set_aliases: not enough SHMemory!\n");
			goto clean;
		}
			
		strncpy(jwl->aliases->a[i].s, p, jwl->aliases->a[i].len);
		DBG("XJAB:xj_wlist_set_aliases: alias[%d/%d]=%.*s\n", 
				i+1, jwl->aliases->size, jwl->aliases->a[i].len, 
				jwl->aliases->a[i].s);
		p = p0 + 1;
	}
	return 0;

clean:
	if(jwl->aliases->jdm)
	{
		_M_SHM_FREE(jwl->aliases->jdm->s);
		_M_SHM_FREE(jwl->aliases->jdm);
	}
	while(i>0)
	{
		_M_SHM_FREE(jwl->aliases->a[i-1].s);
		i--;
	}
	_M_SHM_FREE(jwl->aliases->a);
	_M_SHM_FREE(jwl->aliases);
	jwl->aliases = NULL;
	return -1;
}

/**
 * delete an entity from working list of a worker
 * - jwl : pointer to the workers list
 * - sid : id of the entity (connection to Jabber - usually SHOULD be FROM
 *   header of the incomming SIP message
 * - _pid : process id of the worker
 */
void xj_wlist_del(xj_wlist jwl, xj_jkey jkey, int _pid)
{
	int i;
	void *p;
	if(jwl==NULL || jkey==NULL || jkey->id==NULL || jkey->id->s==NULL)
		return;
	for(i=0; i < jwl->len; i++)
		if(jwl->workers[i].pid == _pid)
			break;
	if(i >= jwl->len)
	{
		DBG("XJAB:xj_wlist_del:%d: key <%.*s> not found in [%d]...\n",
			_pid, jkey->id->len, jkey->id->s, i);
		return;
	}
	DBG("XJAB:xj_wlist_del:%d: trying to delete entry for <%.*s>...\n",
		_pid, jkey->id->len, jkey->id->s);

	s_lock_at(jwl->sems, i);
	p = del234(jwl->workers[i].sip_ids, (void*)jkey);

	if(p != NULL)
	{
		jwl->workers[i].nr--;

		DBG("XJAB:xj_wlist_del:%d: sip id <%.*s> deleted\n", _pid,
			jkey->id->len, jkey->id->s);
		xj_jkey_free_p(p);
	}

	s_unlock_at(jwl->sems, i);
}


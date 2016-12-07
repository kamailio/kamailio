/*
 * $Id$
 *
 * eXtended JABber module - worker implementation
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
 * ---
 *
 * History
 * -------
 * 2003-02-24  added 'xj_wlist_set_flag' function (dcm)
 * 2003-03-11  major locking changes - uses locking.h (andrei)
 *
 */


#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#include "xjab_worker.h"
#include "mdefines.h"

#define XJ_DEF_JDELIM '*'

/**
 * init a workers list
 * - pipes : communication pipes
 * - size : size of list - number of workers
 * - max : maximum number of jobs per worker
 * return : pointer to workers list or NULL on error
 */
xj_wlist xj_wlist_init(int **pipes, int size, int max, int cache_time,
		int sleep_time, int delay_time)
{
	int i;
	xj_wlist jwl = NULL;

	if(pipes == NULL || size <= 0 || max <= 0)
		return NULL;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("-----START-----\n");
#endif	
	jwl = (xj_wlist)_M_SHM_MALLOC(sizeof(t_xj_wlist));
	if(jwl == NULL)
		return NULL;
	jwl->len = size;
	jwl->maxj = max;
	
	jwl->cachet = cache_time;
	jwl->delayt = delay_time;
	jwl->sleept = sleep_time;

	jwl->aliases = NULL;
	jwl->sems = NULL;
	i = 0;
	/* alloc locks*/
	if((jwl->sems = lock_set_alloc(size)) == NULL){
		LM_CRIT("failed to alloc lock set\n");
		goto clean;
	};
	/* init the locks*/
	if (lock_set_init(jwl->sems)==0){
		LM_CRIT("failed to initialize the locks\n");
		goto clean;
	};
	jwl->workers = (xj_worker)_M_SHM_MALLOC(size*sizeof(t_xj_worker));
	if(jwl->workers == NULL){
		lock_set_destroy(jwl->sems);
		goto clean;
	}

	for(i = 0; i < size; i++)
	{
		jwl->workers[i].nr = 0;
		jwl->workers[i].pid = 0;
		jwl->workers[i].wpipe = pipes[i][1];
		jwl->workers[i].rpipe = pipes[i][0];
		if((jwl->workers[i].sip_ids = newtree234(xj_jkey_cmp)) == NULL){
			lock_set_destroy(jwl->sems);
			goto clean;
		}
	}	

	return jwl;

clean:
	LM_DBG("error occurred -> cleaning\n");
	if(jwl->sems != NULL)
		lock_set_dealloc(jwl->sems);
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
 * set the p.id's of the workers
 * - jwl : pointer to the workers list
 * - pids : p.id's array
 * - size : number of pids
 * return : 0 on success or <0 on error
 */
int xj_wlist_set_pid(xj_wlist jwl, int pid, int idx)
{
	if(jwl == NULL || pid <= 0 || idx < 0 || idx >= jwl->len)
		return -1;
	lock_set_get(jwl->sems, idx);
	jwl->workers[idx].pid = pid;
	lock_set_release(jwl->sems, idx);
	return 0;
}

/**
 * free jab_wlist
 * - jwl : pointer to the workers list
 */
void xj_wlist_free(xj_wlist jwl)
{
	int i;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("freeing 'xj_wlist' memory ...\n");
#endif
	if(jwl == NULL)
		return;

	if(jwl->workers != NULL)
	{
		for(i=0; i<jwl->len; i++)
			free2tree234(jwl->workers[i].sip_ids, xj_jkey_free_p);
		_M_SHM_FREE(jwl->workers);
	}

	if(jwl->aliases != NULL)
	{
		if(jwl->aliases->d)
			_M_SHM_FREE(jwl->aliases->d);

		if(jwl->aliases->jdm != NULL)
		{
			_M_SHM_FREE(jwl->aliases->jdm->s);
			_M_SHM_FREE(jwl->aliases->jdm);
		}
		if(jwl->aliases->proxy != NULL)
		{
			_M_SHM_FREE(jwl->aliases->proxy->s);
			_M_SHM_FREE(jwl->aliases->proxy);
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
	
	if(jwl->sems != NULL){
		lock_set_destroy(jwl->sems);
		lock_set_dealloc(jwl->sems);
	}
	
	_M_SHM_FREE(jwl);
}

/**
 * return communication pipe with the worker that will process the message for
 * 		the id 'sid' only if it exists, or -1 if error
 * - jwl : pointer to the workers list
 * - sid : id of the entity (connection to Jabber - usually SHOULD be FROM
 *   header of the incoming SIP message)
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
		lock_set_get(jwl->sems, i);
		if(jwl->workers[i].pid <= 0)
		{
			lock_set_release(jwl->sems, i);
			i++;
			continue;
		}
		if((*p = find234(jwl->workers[i].sip_ids, (void*)jkey, NULL)) != NULL)
		{
			lock_set_release(jwl->sems, i);
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("entry exists for <%.*s> in the"
				" pool of <%d> [%d]\n",jkey->id->len, jkey->id->s,
				jwl->workers[i].pid,i);
#endif
			return jwl->workers[i].wpipe;
		}
		lock_set_release(jwl->sems, i);
		i++;
	}
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("entry does not exist for <%.*s>\n",
			jkey->id->len, jkey->id->s);
#endif
	return -1;
}

/**
 * return communication pipe with the worker that will process the message for
 * 		the id 'sid', or -1 if error
 * - jwl : pointer to the workers list
 * - sid : id of the entity (connection to Jabber - usually SHOULD be FROM
 *   header of the incoming SIP message)
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
		lock_set_get(jwl->sems, i);
		if(jwl->workers[i].pid <= 0)
		{
			lock_set_release(jwl->sems, i);
			i++;
			continue;
		}
		if((*p = find234(jwl->workers[i].sip_ids, (void*)jkey, NULL))!=NULL)
		{
			if(pos >= 0)
				lock_set_release(jwl->sems, pos);
				lock_set_release(jwl->sems, i);
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("entry already exists for <%.*s> in the"
				" pool of <%d> [%d]\n",jkey->id->len, jkey->id->s,
				jwl->workers[i].pid,i);
#endif
			return jwl->workers[i].wpipe;
		}
		if(min > jwl->workers[i].nr)
		{
			if(pos >= 0)
				lock_set_release(jwl->sems, pos);
			pos = i;
			min = jwl->workers[i].nr;
		}
		else
			lock_set_release(jwl->sems, i);
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
			msid->flag = XJ_FLAG_OPEN;
			lock_set_release(jwl->sems, pos);
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("new entry for <%.*s> in the pool of"
				" <%d> - [%d]\n", jkey->id->len, jkey->id->s,
				jwl->workers[pos].pid, pos);
#endif
			return jwl->workers[pos].wpipe;
		}
		_M_SHM_FREE(msid->id->s);
		_M_SHM_FREE(msid->id);
		_M_SHM_FREE(msid);
	}

error:
	if(pos >= 0)
		lock_set_release(jwl->sems, pos);
	LM_DBG("cannot create a new entry for <%.*s>\n",
				jkey->id->len, jkey->id->s);
	return -1;
}

/**
 * set the flag of the connection identified by 'jkey'
 *
 */
int xj_wlist_set_flag(xj_wlist jwl, xj_jkey jkey, int fl)
{
	int i;
	xj_jkey p = NULL;
	if(jwl==NULL || jkey==NULL || jkey->id==NULL || jkey->id->s==NULL)
		return -1;
	
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("looking for <%.*s>"
		" having id=%d\n", jkey->id->len, jkey->id->s, jkey->hash);
#endif
			
	i = 0;
	while(i < jwl->len)
	{
		lock_set_get(jwl->sems, i);
		if(jwl->workers[i].pid <= 0)
		{
			lock_set_release(jwl->sems, i);
			i++;
			continue;
		}
		if((p=find234(jwl->workers[i].sip_ids, (void*)jkey, NULL)) != NULL)
		{
			p->flag = fl;
			lock_set_release(jwl->sems, i);
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("the connection for <%.*s>"
				" marked with flag=%d", jkey->id->len, jkey->id->s, fl);
#endif
			return jwl->workers[i].wpipe;
		}
		lock_set_release(jwl->sems, i);
		i++;
	}
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("entry does not exist for <%.*s>\n",
			jkey->id->len, jkey->id->s);
#endif
	return -1;
}


/**
 * set IM aliases, jdomain and outbound proxy
 *
 * return 0 if OK
 */
int  xj_wlist_set_aliases(xj_wlist jwl, char *als, char *jd, char *pa)
{
	char *p, *p0, *p1;
	int i, n;
	
	if(jwl == NULL)
		return -1;
	if(!jd) // || !als || strlen(als)<2)
		return 0;
	
	if((jwl->aliases = (xj_jalias)_M_SHM_MALLOC(sizeof(t_xj_jalias)))==NULL)
	{
		LM_DBG("not enough SHMemory.\n");
		return -1;
	}
	
	jwl->aliases->jdm = NULL;
	jwl->aliases->proxy = NULL;
	jwl->aliases->dlm = XJ_DEF_JDELIM; // default user part delimiter
	jwl->aliases->size = 0;
	jwl->aliases->a = NULL;
	jwl->aliases->d = NULL;

	// set the jdomain
	if(jd != NULL && (n=strlen(jd))>2)
	{
		p = jd;
		while(p < jd+n && *p!='=')
			p++;
		if(p<jd+n-1)
		{
			jwl->aliases->dlm = *(p+1);
			n = p - jd;
		}
		if((jwl->aliases->jdm = (str*)_M_SHM_MALLOC(sizeof(str)))== NULL)
		{
			LM_DBG("not enough SHMemory!?\n");
			_M_SHM_FREE(jwl->aliases);
			jwl->aliases = NULL;
			return -1;		
		}
		jwl->aliases->jdm->len = n;
		if((jwl->aliases->jdm->s=(char*)_M_SHM_MALLOC(jwl->aliases->jdm->len))
				== NULL)
		{
			LM_DBG("not enough SHMemory!?!\n");
			_M_SHM_FREE(jwl->aliases->jdm);
			_M_SHM_FREE(jwl->aliases);
			jwl->aliases = NULL;
		}
		strncpy(jwl->aliases->jdm->s, jd, jwl->aliases->jdm->len);
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("jdomain=%.*s delim=%c\n",
			jwl->aliases->jdm->len, jwl->aliases->jdm->s, jwl->aliases->dlm);
#endif
	}
	
	// set the proxy address
	if(pa && strlen(pa)>0)
	{
		if((jwl->aliases->proxy = (str*)_M_SHM_MALLOC(sizeof(str)))==NULL)
		{
			LM_DBG(" not enough SHMemory!!\n");
			goto clean3;		
		}
		i = jwl->aliases->proxy->len = strlen(pa);
		// check if proxy address has sip: prefix
		if(i < 4 || pa[0]!='s' || pa[1]!='i' || pa[2]!='p' || pa[3]!=':')
			jwl->aliases->proxy->len += 4;
		if((jwl->aliases->proxy->s=
					(char*)_M_SHM_MALLOC(jwl->aliases->proxy->len))
				== NULL)
		{
			LM_DBG("not enough SHMemory!!!\n");
			_M_SHM_FREE(jwl->aliases->proxy);
			goto clean3;
		}
		p0 = jwl->aliases->proxy->s;
		if(jwl->aliases->proxy->len != i)
		{
			strncpy(p0, "sip:", 4);
			p0 += 4;
		}
		strncpy(p0, pa, i);
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("outbound proxy=[%.*s]\n",
			jwl->aliases->proxy->len, jwl->aliases->proxy->s);
#endif
	}
	
	// set the IM aliases
	if(!als || strlen(als)<2)
		return 0;
	
	if((p = strchr(als, ';')) == NULL)
	{
		LM_DBG("bad parameter value\n");
		return -1;
	}
	
	if((jwl->aliases->size = atoi(als)) <= 0)
	{
		LM_DBG("wrong number of aliases\n");
		return 0;
	}
	
	jwl->aliases->d = (char*)_M_SHM_MALLOC(jwl->aliases->size*sizeof(char));
	if(jwl->aliases->d == NULL)
	{
		LM_DBG("not enough SHMemory..\n");
		goto clean2;
	}
	memset(jwl->aliases->d, 0, jwl->aliases->size);
	
	jwl->aliases->a = (str*)_M_SHM_MALLOC(jwl->aliases->size*sizeof(str));
	if(jwl->aliases->a == NULL)
	{
		LM_DBG("not enough SHMemory..\n");
		goto clean1;
	}
	
	p++;
	for(i=0; i<jwl->aliases->size; i++)
	{
		if((p0 = strchr(p, ';'))==NULL)
		{
			LM_DBG("bad parameter value format\n");
			goto clean;
		}
		n = p0 - p;
		p1 = strchr(p, '=');
		if(p1 && p1<p0-1)
		{
			jwl->aliases->d[i] = *(p1+1);
			n = p1 - p;
		}
		jwl->aliases->a[i].len = n;
		if((jwl->aliases->a[i].s = (char*)_M_SHM_MALLOC(jwl->aliases->a[i].len))
				== NULL)
		{
			LM_DBG("not enough SHMemory!\n");
			goto clean;
		}
			
		strncpy(jwl->aliases->a[i].s, p, jwl->aliases->a[i].len);
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("alias[%d/%d]=%.*s delim=%c\n", 
			i+1, jwl->aliases->size, jwl->aliases->a[i].len, 
			jwl->aliases->a[i].s, jwl->aliases->d[i]?jwl->aliases->d[i]:'X');
#endif
		p = p0 + 1;
	}
	return 0;

clean:
	while(i>0)
	{
		_M_SHM_FREE(jwl->aliases->a[i-1].s);
		i--;
	}
	_M_SHM_FREE(jwl->aliases->a);

clean1:
	if(jwl->aliases->d)
		_M_SHM_FREE(jwl->aliases->d);

clean2:
	if(jwl->aliases->proxy)
	{
		_M_SHM_FREE(jwl->aliases->proxy->s);
		_M_SHM_FREE(jwl->aliases->proxy);
	}
clean3:
	if(jwl->aliases->jdm)
	{
		_M_SHM_FREE(jwl->aliases->jdm->s);
		_M_SHM_FREE(jwl->aliases->jdm);
	}
	_M_SHM_FREE(jwl->aliases);
	jwl->aliases = NULL;
	return -1;
}


/**
 * check if the addr contains jdomain or an alias
 * - jwl : pointer to the workers list
 * - addr: the address to check against jdomain and aliases
 * returns 0 - if contains or !=0 if not
 */
int  xj_wlist_check_aliases(xj_wlist jwl, str *addr)
{
	char *p, *p0;
	int ll, i;
	if(!jwl || !jwl->aliases || !addr || !addr->s || addr->len<=0)
		return -1;

	// find '@'
	p = addr->s;
	while(p < addr->s + addr->len && *p != '@')
		p++;
	if(p >= addr->s + addr->len)
		return -1;
	
	p++;
	ll = addr->s + addr->len - p;
	
	// check parameters
	p0 = p;
	while(p0 < p + ll && *p0 != ';')
		p0++;
	if(p0 < p + ll)
		ll = p0 - p;
	
	ll = addr->s + addr->len - p;
	if(jwl->aliases->jdm && jwl->aliases->jdm->len == ll && 
			!strncasecmp(jwl->aliases->jdm->s, p, ll))
		return 0;

	if(jwl->aliases->size <= 0)
		return 1;
	
	for(i = 0; i < jwl->aliases->size; i++)
		if(jwl->aliases->a[i].len == ll && 
			!strncasecmp(p, jwl->aliases->a[i].s, ll))
				return 0;
	return 1;
}

/**
 * delete an entity from working list of a worker
 * - jwl : pointer to the workers list
 * - sid : id of the entity (connection to Jabber - usually SHOULD be FROM
 *   header of the incoming SIP message
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
		LM_DBG("%d: key <%.*s> not found in [%d]...\n",
			_pid, jkey->id->len, jkey->id->s, i);
		return;
	}
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("%d: trying to delete entry for <%.*s>...\n",
		_pid, jkey->id->len, jkey->id->s);
#endif
	lock_set_get(jwl->sems, i);
	p = del234(jwl->workers[i].sip_ids, (void*)jkey);

	if(p != NULL)
	{
		jwl->workers[i].nr--;
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("%d: sip id <%.*s> deleted\n", _pid,
			jkey->id->len, jkey->id->s);
#endif
		xj_jkey_free_p(p);
	}

	lock_set_release(jwl->sems, i);
}


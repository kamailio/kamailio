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
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "../../dprint.h"
#include "../../timer.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../tm/t_funcs.h"
#include "../tm/uac.h"

#include "xjab_worker.h"
#include "xjab_util.h"
#include "xjab_jcon.h"
#include "xode.h"

#include "mdefines.h"

#define XJAB_RESOURCE "serXjab"

/** TM bind */
extern struct tm_binds tmb;

/** debug info */
int _xj_pid = 0;

/**
 * function used to compare two elements in B-Tree
 */
int k_cmp(void *a, void *b)
{
	int n;
	if(a == NULL)
	    return -1;
	if(a == NULL)
	    return 1;
	// DBG("JABBER: k_kmp: comparing <%.*s> / <%.*s>\n", ((str *)a)->len,
	// 		((str *)a)->s, ((str *)b)->len, ((str *)b)->s);
	if(((str *)a)->len != ((str *)b)->len)
		return (((str *)a)->len < ((str *)b)->len)?-1:1;
	n = strncmp(((str *)a)->s, ((str *)b)->s, ((str *)a)->len);
	if(n!=0)
		return (n<0)?-1:1;
	return 0;
}

/**
 * free the information from a B-Tree node
 */
void free_str_p(void *p)
{
	if(p == NULL)
		return;
	if(((str*)p)->s != NULL)
		_M_SHM_FREE(((str*)p)->s);
	_M_SHM_FREE(p);
}

/**
 * free a pointer to a t_jab_sipmsg structure
 * > element where points 'from' MUST be eliberated separated
 */
void xj_sipmsg_free(xj_sipmsg jsmsg)
{
	if(jsmsg == NULL)
		return;
	if(jsmsg->to.s != NULL)
		_M_SHM_FREE(jsmsg->to.s);
//	if(jsmsg->from.s != NULL)
//		_M_SHM_FREE(jsmsg->from.s);
	if(jsmsg->msg.s != NULL)
		_M_SHM_FREE(jsmsg->msg.s);
	_M_SHM_FREE(jsmsg);
}

/**
 * init a workers list
 * - pipes : communication pipes
 * - size : size of list - number of workers
 * - max : maximum number of jobs per worker
 * #return : pointer to workers list or NULL on error
 */
xj_wlist xj_wlist_init(int **pipes, int size, int max)
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
	jwl->contact_h = NULL;
	jwl->aliases = NULL;
	if((jwl->sems = create_semaphores(size)) == NULL)
	{
		_M_SHM_FREE(jwl);
		return NULL;
	}
	jwl->workers = (xj_worker)_M_SHM_MALLOC(size*sizeof(t_xj_worker));
	if(jwl->workers == NULL)
	{
		_M_SHM_FREE(jwl);
		return NULL;
	}

	for(i = 0; i < size; i++)
	{
		jwl->workers[i].nr = 0;
		jwl->workers[i].pipe = pipes[i][1];
		if((jwl->workers[i].sip_ids = newtree234(k_cmp)) == NULL)
		{
			_M_SHM_FREE(jwl);
			return NULL;
		}
	}	

	return jwl;
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
int xj_wlist_set_pids(xj_wlist jwl, int *pids, int size)
{
	int i;

	if(jwl == NULL || pids == NULL || size <= 0)
		return -1;
	for(i = 0; i < size; i++)
		jwl->workers[i].pid = pids[i];
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
			free2tree234(jwl->workers[i].sip_ids, free_str_p);
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
 * 		the id 'sid', or -1 if error
 * - jwl : pointer to the workers list
 * - sid : id of the entity (connection to Jabber - usually SHOULD be FROM
 *   header of the incomming SIP message)
 * - p : will point to the SHM location of the 'sid' in jwl
 */
int xj_wlist_get(xj_wlist jwl, str *sid, str **p)
{
	int i = 0, pos = -1, min = 100000;
	str *msid = NULL;
	
	if(jwl == NULL)
		return -1;
	DBG("XJAB:xj_wlist_get: -----START-----\n");
	//mutex_lock(jwl->semid);

	*p = NULL;
	while(i < jwl->len)
	{
		s_lock_at(jwl->sems, i);
		if((*p = find234(jwl->workers[i].sip_ids, (void*)sid, NULL)) != NULL)
		{
			//mutex_unlock(jwl->semid);
			if(pos >= 0)
				s_unlock_at(jwl->sems, pos);
			s_unlock_at(jwl->sems, i);
			DBG("XJAB:xj_wlist_get: entry already exists for <%.*s> in the"
				" pool of <%d> [%d]\n",sid->len, sid->s,jwl->workers[i].pid,i);
			return jwl->workers[i].pipe;
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

		msid = (str*)_M_SHM_MALLOC(sizeof(str));
		if((msid != NULL) &&
			(*p = add234(jwl->workers[pos].sip_ids, msid)) != NULL)
		{
			msid->s = (char*)_M_SHM_MALLOC(sid->len);
			msid->len = sid->len;
			memcpy(msid->s, sid->s, sid->len);
			s_unlock_at(jwl->sems, pos);
			//mutex_unlock(jwl->semid);
			DBG("XJAB:xj_wlist_get: new entry for <%.*s> in the pool of"
				" <%d> - [%d]\n", sid->len, sid->s,
				jwl->workers[pos].pid, pos);
			return jwl->workers[pos].pipe;
		}
	}

	if(pos >= 0)
		s_unlock_at(jwl->sems, pos);
	//mutex_unlock(jwl->semid);

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
void xj_wlist_del(xj_wlist jwl, str *sid, int _pid)
{
	int i;
	void *p;
	if(jwl == NULL || sid == NULL)
		return;
	for(i=0; i < jwl->len; i++)
		if(jwl->workers[i].pid == _pid)
			break;
	if(i >= jwl->len)
	{
		DBG("XJAB:xj_wlist_del:%d: key <%.*s> not found in [%d]...\n",
			_pid, sid->len, sid->s, i);
		return;
	}
	DBG("XJAB:xj_wlist_del:%d: trying to delete entry for <%.*s>...\n",
		_pid, sid->len, sid->s);

	s_lock_at(jwl->sems, i);
	p = del234(jwl->workers[i].sip_ids, (void*)sid);

	if(p != NULL)
	{
		jwl->workers[i].nr--;

		DBG("XJAB:xj_wlist_del:%d: sip id <%.*s> deleted\n", _pid,
			sid->len, sid->s);
		free_str_p(p);
	}

	s_unlock_at(jwl->sems, i);
}

/**
 * send a SIP MESSAGE message
 * - to : destination
 * - from : origin
 * - contact : contact header
 * - msg : body of the message
 * #return : 0 on success or <0 on error
 */
int xj_send_sip_msg(str *to, str *from, str *contact, str *msg)
{
	str  msg_type = { "MESSAGE", 7};
	char buf[512];
	str  tfrom;
	str  str_hdr;
	char buf1[1024];

	// from correction
	strcpy(buf, "<sip:");
	strncat(buf, from->s, from->len);
	tfrom.len = from->len;
	if(strstr(buf+5, "sip:") == NULL)
	{
		tfrom.len += 5;
		buf[tfrom.len++] = '>';
		tfrom.s = buf;
	} else
		tfrom.s = buf+5;
	// building Contact and Content-Type
	strcpy(buf1,"Content-Type: text/plain"CRLF"Contact: ");
	str_hdr.len = 24 + CRLF_LEN + 9;
	if(contact != NULL && contact->len > 2) {
		strncat(buf1,contact->s,contact->len);
		str_hdr.len += contact->len;
	} else {
		strncat(buf1,tfrom.s,tfrom.len);
		str_hdr.len += tfrom.len;
	}
	strcat(buf1, CRLF);
	str_hdr.len += CRLF_LEN;
	str_hdr.s = buf1;

	return tmb.t_uac( &msg_type, to, &str_hdr , msg, &tfrom, 0 , 0, 0);
}

/**
 * send a SIP MESSAGE message
 * - to : destination
 * - from : origin
 * - contact : contact header
 * - msg : body of the message, string terminated by zero
 * #return : 0 on success or <0 on error
 */
int xj_send_sip_msgz(str *to, str *from, str *contact, char *msg)
{
	str tstr;
	int n;
	tstr.s = msg;
	tstr.len = strlen(msg);
	if((n = xj_send_sip_msg(to, from, contact, &tstr)) < 0)
		DBG("JABBER: jab_send_sip_msgz: ERROR SIP MESSAGE wasn't sent to"
			" [%.*s]...\n", tstr.len, tstr.s);
	else
		DBG("JABBER: jab_send_sip_msgz: SIP MESSAGE was sent to [%.*s]...\n",
			tstr.len, tstr.s);
	return n;
}

/**
 * address corection
 * alias A~B: flag == 0 => A->B, otherwise B->A
 */
int xj_address_translation(str *src, str *dst, xj_jalias als, int flag)
{
	char *p, *p0;
	int i;
	
	if(!src || !dst || !src->s || !dst->s )
		return -1; 
	
	if(!als || !als->jdm || !als->jdm->s || als->jdm->len <= 0)
		goto done;
	
	dst->len = 0;
	DBG("XJAB:xj_address_translation:%d: - checking aliases\n", 
			_xj_pid);
	p = src->s;

	while(p<(src->s + src->len)	&& *p != '@') 
		p++;
	if(*p != '@')
		goto done;

	p++;
	
	/*** checking aliases */
	if(als->size > 0)
		for(i=0; i<als->size; i++)
			if(!strncasecmp(p, als->a[i].s, als->a[i].len))
				goto done;
	
	DBG("XJAB:xj_address_translation:%d: - doing address corection\n", 
			_xj_pid);	

	if(!flag)
	{
		if(!strncasecmp(p, als->jdm->s, als->jdm->len))
		{
			DBG("XJAB:xj_address_translation:%d: - that is for"
				" Jabber network\n", _xj_pid);
			dst->len = p - src->s - 1;
			strncpy(dst->s, src->s, dst->len);
			dst->s[dst->len]=0;
			if((p = strchr(dst->s, '%')) != NULL)
				*p = '@';
			else
			{
				DBG("XJAB:xj_address_translation:%d: - wrong Jabber"
				" destination\n", _xj_pid);
				return -1;
			}
			return 0;
		}
		DBG("XJAB:xj_address_translation:%d: - wrong Jabber"
			" destination!\n", _xj_pid);
		return -1;		
	}
	else
	{
		*(p-1) = '%';
		p0 = src->s + src->len;
		while(p0 > p)
		{
			if(*p0 == '/')
			{
				src->len = p0 - src->s;
				*p0 = 0;
			}
			p0--;
		}
		strncpy(dst->s, src->s, src->len);
		dst->s[src->len] = '@';
		dst->s[src->len+1] = 0;
		strncat(dst->s, als->jdm->s, als->jdm->len);
		dst->len = strlen(dst->s);
		return 0;
	}

done:
	dst->s = src->s;
	dst->len = src->len;
	return 0;	
}

/**
 * worker implementation 
 * - jwl : pointer to the workers list
 * - jaddress : address of the jabber server
 * - jport : port of the jabber server
 * - pipe : communication pipe with SER
 * - size : maximun number of jobs - open connections to Jabber server
 * - ctime : cache time for a connection to Jabber
 * - wtime : wait time between cache checking
 * - dtime : delay time for first message
 * - db_con : connection to database
 * #return : 0 on success or <0 on error
 */
int xj_worker_process(xj_wlist jwl, char* jaddress, int jport, int pipe,
		int size, int ctime, int wtime, int dtime, db_con_t* db_con)
{
	int ret, i, pos, maxfd;
	xj_jcon_pool jcp;
	struct timeval tmv;
	fd_set set, mset;
	xj_sipmsg jsmsg;
	xj_jcon jbc;
	char *p, buff[1024], recv_buff[4096];
	int flags, nr, ltime = 0;
	str sto;
	
	db_key_t keys[] = {"sip_id"};
	db_val_t vals[] = {{DB_STRING, 0, {.string_val = buff}}};
	db_key_t col[] = {"jab_id", "jab_passwd"};
	db_res_t* res = NULL;

	_xj_pid = getpid();
	
	DBG("XJAB:xj_worker:%d: started - pipe=<%d> : 1st message delay"
		" <%d>\n", _xj_pid, pipe, dtime);

	if((jcp = xj_jcon_pool_init(size, 10, dtime)) == NULL)
	{
		DBG("XJAB:xj_worker: cannot allocate the pool\n");
		return -1;
	}

	maxfd = pipe;
	tmv.tv_sec = wtime;
	tmv.tv_usec = 0;

	FD_ZERO(&set);
	FD_SET(pipe, &set);
	while(1)
	{
		mset = set;

		tmv.tv_sec = (jcp->jmqueue.size == 0)?wtime:1;
		DBG("XJAB:xj_worker:%d: select waiting %ds - queue=%d\n",_xj_pid,
				(int)tmv.tv_sec, jcp->jmqueue.size);
		tmv.tv_usec = 0;

		ret = select(maxfd+1, &mset, NULL, NULL, &tmv);
		/** check the queue AND conecction of head element is ready */
		for(i = 0; i<jcp->jmqueue.size; i++)
		{
			if(jcp->jmqueue.jsm[i]==NULL || jcp->jmqueue.ojc[i]==NULL)
				continue;
			if(jcp->jmqueue.expire[i] < get_ticks())
			{
				DBG("XJAB:xj_worker:%d: message to %.*s is expired\n",
					_xj_pid, jcp->jmqueue.jsm[i]->to.len, 
					jcp->jmqueue.jsm[i]->to.s);
				xj_send_sip_msgz(jcp->jmqueue.jsm[i]->from, 
					&jcp->jmqueue.jsm[i]->to, jwl->contact_h, 
					"ERROR: Your message was not sent. Conection to IM"
					" network failed.");
				xj_sipmsg_free(jcp->jmqueue.jsm[i]);
				/** delete message from queue */
				xj_jcon_pool_del_jmsg(jcp, i);
				continue;
			}

			if(xj_jcon_is_ready(jcp->jmqueue.ojc[i], 
					jcp->jmqueue.jsm[i]->to.s, jcp->jmqueue.jsm[i]->to.len))
				continue;

			/*** address corection ***/
			sto.s = buff; 
			sto.len = 0;
			if(xj_address_translation(&jcp->jmqueue.jsm[i]->to,
				&sto, jwl->aliases, 0) == 0)
			{
		
				/** send message from queue */
				DBG("XJAB:xj_worker:%d: SENDING AS JABBER MESSAGE FROM "
					" LOCAL QUEUE ...\n", _xj_pid);
				xj_jcon_send_msg(jcp->jmqueue.ojc[i],
					sto.s, sto.len,
					jcp->jmqueue.jsm[i]->msg.s,
					jcp->jmqueue.jsm[i]->msg.len);
			}
			else
				DBG("XJAB:xj_worker:%d: ERROR SENDING AS JABBER MESSAGE FROM "
				" LOCAL QUEUE ...\n", _xj_pid);
				
			xj_sipmsg_free(jcp->jmqueue.jsm[i]);
			/** delete message from queue */
			xj_jcon_pool_del_jmsg(jcp, i);
		}
		
		if(ret <= 0)
			goto step_x;
		
		DBG("XJAB:xj_worker:%d: something is coming\n", _xj_pid);
		if(!FD_ISSET(pipe, &mset))
			goto step_y;
		
		read(pipe, &jsmsg, sizeof(jsmsg));

		DBG("XJAB:xj_worker:%d: job <%p> from SER\n", _xj_pid, jsmsg);
		if( jsmsg == NULL || jsmsg->from == NULL)
			goto step_y;

		strncpy(buff, jsmsg->from->s, jsmsg->from->len);
		buff[jsmsg->from->len] = 0;

		jbc = xj_jcon_pool_get(jcp, jsmsg->from);
		if(jbc != NULL)
		{
			DBG("XJAB:xj_worker:%d: connection already exists"
				" for <%s> ...\n", _xj_pid, buff);
			xj_jcon_update(jbc, ctime);
			goto step_z;
		}
		
		// NO OPEN CONNECTION FOR THIS SIP ID
		DBG("XJAB:xj_worker:%d: new connection for <%s>.\n",
			_xj_pid, buff);
		
		if(db_query(db_con, keys, vals, col, 1, 2, NULL, &res) != 0 ||
			RES_ROW_N(res) <= 0)
		{
			DBG("XJAB:xj_worker:%d: no database result\n", _xj_pid);
			xj_send_sip_msgz(jsmsg->from, &jsmsg->to, 
				jwl->contact_h, "ERROR: Your message was not"
				" sent. You do not have permision to use the"
				" gateway.");
			
			goto step_v;
		}
		
		jbc = xj_jcon_init(jaddress, jport);
		
		if(xj_jcon_connect(jbc))
		{
			DBG("XJAB:xj_worker:%d: Cannot connect"
				" to the Jabber server ...\n", _xj_pid);
			xj_send_sip_msgz(jsmsg->from, &jsmsg->to, 
				jwl->contact_h, "ERROR: Your message was"
				" not sent. Cannot connect to the Jabber"
				" server.");

			goto step_v;
		}
		
		DBG("XJAB:xj_worker: auth to jabber as: [%s] / [%s]\n",
			(char*)(ROW_VALUES(RES_ROWS(res))[0].val.string_val), 
			(char*)(ROW_VALUES(RES_ROWS(res))[1].val.string_val));
		
		if(xj_jcon_user_auth(jbc,
			(char*)(ROW_VALUES(RES_ROWS(res))[0].val.string_val),
			(char*)(ROW_VALUES(RES_ROWS(res))[1].val.string_val),
			XJAB_RESOURCE) < 0)
		{
			DBG("XJAB:xj_worker:%d: Authentication to the Jabber server"
				" failed ...\n", _xj_pid);
			xj_jcon_disconnect(jbc);
			xj_jcon_free(jbc);
			
			xj_send_sip_msgz(jsmsg->from, &jsmsg->to,
				jwl->contact_h, "ERROR: Your message"
				" was not sent. Authentication to the"
				" Jabber server failed.");

			goto step_v;
		}
		
		if(xj_jcon_set_attrs(jbc, jsmsg->from, ctime, dtime)
			|| xj_jcon_pool_add( jcp, jbc))
		{
			DBG("XJAB:xj_worker:%d: Keeping connection to Jabber server"
				" failed! Not enough memory ...\n", _xj_pid);
			xj_jcon_disconnect(jbc);
			xj_jcon_free(jbc);
			xj_send_sip_msgz(jsmsg->from,
				&jsmsg->to,	jwl->contact_h,	
				"ERROR:Your message was	not"
				" sent. SIP-2-JABBER"
				" gateway is full.");
	
			goto step_v;
		}
								
		/** add socket descriptor to select */
		DBG("XJAB:xj_worker:%d: add connection on <%d> \n", _xj_pid, jbc->sock);
		if(jbc->sock > maxfd)
			maxfd = jbc->sock;
		FD_SET(jbc->sock, &set);
										
		xj_jcon_get_roster(jbc);
		xj_jcon_send_presence(jbc, NULL, "Online", "9");
		
		/** wait for a while - SER is tired */
		//sleep(3);
		
		if ((res != NULL) && (db_free_query(db_con,res) < 0))
		{
			DBG("XJAB:xj_worker:%d:Error while freeing"
				" SQL result - worker terminated\n", _xj_pid);
			return -1;
		}
		else
			res = NULL;
		
step_z:
		switch(xj_jcon_is_ready(jbc, jsmsg->to.s, jsmsg->to.len))
		{
			case 0:
				DBG("XJAB:xj_worker:%d: SENDING AS JABBER"
					" MESSAGE ...\n", _xj_pid);
				/*** address corection ***/
				sto.s = buff; 
				sto.len = 0;
				if(xj_address_translation(&jsmsg->to, &sto, jwl->aliases, 0) == 0)
				{
					if(xj_jcon_send_msg(jbc, sto.s, sto.len,
							jsmsg->msg.s, jsmsg->msg.len)<0)
						xj_send_sip_msgz(jsmsg->from, &jsmsg->to,
							jwl->contact_h, "ERROR: Your message was not"
							" sent. Something wrong during transmition to"
							" Jabber.");
				}
				else
					DBG("XJAB:xj_worker:%d: ERROR SENDING AS JABBER"
						" MESSAGE ...\n", _xj_pid);
						
				goto step_w;
		
			case 1:
				DBG("XJAB:xj_worker:%d:SCHEDULING THE MESSAGE."
					"\n", _xj_pid);
				if(xj_jcon_pool_add_jmsg(jcp, jsmsg, jbc) < 0)
				{
					DBG("XJAB:xj_worker:%d: SCHEDULING THE"
						" MESSAGE FAILED. Message was droped.\n",_xj_pid);
					goto step_w;
				}
				else // skip freeing the SIP message - now is in queue
					goto step_y;
	
			case 2:
				xj_send_sip_msgz(jsmsg->from, &jsmsg->to,
						jwl->contact_h, "ERROR: Your message was not"
						" sent. You are not registered with this transport.");
				goto step_w;
			
			default:
				xj_send_sip_msgz(jsmsg->from, &jsmsg->to,
						jwl->contact_h, "ERROR: Your message was not"
						" sent. Something wrong during transmition to"
						" Jabber.");
				goto step_w;
		}

step_v:
		if ((res != NULL) && (db_free_query(db_con,res) < 0))
		{
			DBG("XJAB:xj_worker:%d:Error while freeing"
				" SQL result - worker terminated\n", _xj_pid);
			return -1;
		}
		else
			res = NULL;

step_w:
		xj_sipmsg_free(jsmsg);

step_y:			 
		// check for new message from ... JABBER
		for(i = 0; i < jcp->len; i++)
		{
			if(jcp->ojc[i] == NULL)
				continue;
			
			DBG("XJAB:xj_worker:%d: checking socket <%d>"
				" ...\n", _xj_pid, jcp->ojc[i]->sock);
			
			if(!FD_ISSET(jcp->ojc[i]->sock, &mset))
				continue;
			pos = nr = 0;
			do
			{
				p = recv_buff;
				if(pos != 0)
				{
					while(pos < nr)
					{
						*p = recv_buff[pos];
						pos++;
						p++;
					}
					*p = 0;
					/**
					 * flush out the socket - set it to nonblocking 
					 */
 					flags = fcntl(jcp->ojc[i]->sock, F_GETFL, 0);
					if(flags!=-1 && !(flags & O_NONBLOCK))
   					{
    					/* set NONBLOCK bit to enable non-blocking */
    					fcntl(jcp->ojc[i]->sock, F_SETFL, flags | O_NONBLOCK);
   					}
				}
				
				if((nr = read(jcp->ojc[i]->sock, p,	
						sizeof(recv_buff)-(p-recv_buff))) == 0
					||(nr < 0 && errno != EAGAIN))
				{
					DBG("XJAB:xj_worker:%d: ERROR -"
						" connection to jabber lost on socket <%d> ...\n",
						_xj_pid, jcp->ojc[i]->sock);
					jcp->ojc[i]->sock = -1;
					jcp->ojc[i]->expire = ltime = 0;
					break;
				}
				DBG("XJAB:xj_worker: received: %dbytes err:%d/EA:%d\n", nr, 
						errno, EAGAIN);		
				xj_jcon_update(jcp->ojc[i], ctime);

				if(nr>0)
					p[nr] = 0;
				nr = strlen(recv_buff);
				pos = 0;

				DBG("XJAB:xj_worker: JMSG START ----------\n%.*s\n"
					" JABBER: JMSGL:%d END ----------\n", nr, recv_buff, nr);

			
			} while(xj_manage_jab(recv_buff, nr, &pos, jcp->ojc[i]->id, 
						jwl->contact_h, jwl->aliases, jcp->ojc[i]) == 9);
	
			/**
			 * flush out the socket - set it back to blocking 
			 */
 			flags = fcntl(jcp->ojc[i]->sock, F_GETFL, 0);
			if(flags!=-1 && (flags & O_NONBLOCK))
   			{
    			/* reset NONBLOCK bit to enable blocking */
    			fcntl(jcp->ojc[i]->sock, F_SETFL, flags & ~O_NONBLOCK);
   			}
	
			DBG("XJAB:xj_worker:%d: msgs from socket <%d> parsed"
				" ...\n", _xj_pid, jcp->ojc[i]->sock);	
		} // end FOR(i = 0; i < jcp->len; i++)
step_x:

		if(ret < 0)
		{
			DBG("XJAB:xj_worker:%d: SIGNAL received!!!!!!!!\n", _xj_pid);
			maxfd = pipe;
			FD_ZERO(&set);
			FD_SET(pipe, &set);
			for(i = 0; i < jcp->len; i++)
			{
				if(jcp->ojc[i] != NULL)
				{
					FD_SET(jcp->ojc[i]->sock, &set);
					if( jcp->ojc[i]->sock > maxfd )
						maxfd = jcp->ojc[i]->sock;
				}
			}
		}

		if(ltime + wtime <= get_ticks())
		{
			ltime = get_ticks();
			DBG("XJAB:xj_worker:%d: scanning for expired connection\n",
				_xj_pid);
			for(i = 0; i < jcp->len; i++)
			{
				if((jcp->ojc[i] != NULL) && (jcp->ojc[i]->expire <= ltime))
				{
					DBG("XJAB:xj_worker:%d: connection expired for"
						" <%.*s>\n", _xj_pid, jcp->ojc[i]->id->len,
						jcp->ojc[i]->id->s);

					// CLEAN JAB_WLIST
					xj_wlist_del(jwl, jcp->ojc[i]->id, _xj_pid);

					FD_CLR(jcp->ojc[i]->sock, &set);
					xj_jcon_disconnect(jcp->ojc[i]);
					xj_jcon_free(jcp->ojc[i]);
					jcp->ojc[i] = NULL;
				}
			}
		}

	} // END while

	return 0;
} // end xj_worker_process

/**
 * parse incoming message from Jabber server
 */
int xj_manage_jab(char *buf, int len, int *pos, str *sid, 
		str *sct, xj_jalias als, xj_jcon jbc)
{
	int i, err=0;
	char *to, *from, *msg, *type, *emsg, lbuf[4096], fbuf[128];
	str ts, tf;
	xode x, y, z;
	
	x = xode_from_strx(buf, len, &err, &i);
	DBG("XJAB:xj_parse_jab: XODE ret:%d pos:%d\n", err, i);
	
	if(err && pos != NULL)
		*pos= i;
	if(x == NULL)
		return -1;
	
	lbuf[0] = 0;
	
	if(!strncasecmp(xode_get_name(x), "message", 7))
	{
		DBG("XJAB:xj_manage_jab: jabber [message] received\n");
		if((to = xode_get_attrib(x, "to")) == NULL)
		{
			DBG("XJAB:xj_manage_jab: missing 'to' attribute\n");
			err = -1;
			goto ready;
		}
		if((from = xode_get_attrib(x, "from")) == NULL)
		{
			DBG("XJAB:xj_manage_jab: missing 'from' attribute\n");
			err = -1;
			goto ready;
		}
		if((y = xode_get_tag(x, "body")) == NULL
				|| (msg = xode_get_data(y)) == NULL)
		{
			DBG("XJAB:xj_manage_jab: missing 'body' of message\n");
			err = -1;
			goto ready;
		}
		type = xode_get_attrib(x, "type");
		if(type != NULL && !strncasecmp(type, "error", 5))
		{
			if((y = xode_get_tag(x, "error")) == NULL
					|| (emsg = xode_get_data(y)) == NULL)
			{
				strcpy(lbuf, "{Error sending following message} - ");
			}
			else
			{
				if(xode_get_attrib(y, "code") != NULL)
					sprintf(lbuf, 
						"{Error (%s/%s) when trying to send following messge}",
						xode_get_attrib(y, "code"), emsg);
				else
					sprintf(lbuf, 
						"{Error (%s) when trying to send following messge}",
						emsg);
			}

		}
		
		strcat(lbuf, msg);
		ts.s = from;
		ts.len = strlen(from);
		tf.s = fbuf;
		tf.len = 0;
		if(xj_address_translation(&ts, &tf, als, 1) == 0)
		{
			ts.s = lbuf;
			ts.len = strlen(lbuf);
	
			if(xj_send_sip_msg(sid, &tf, sct, &ts) < 0)
				DBG("XJAB:xj_manage_jab: ERROR SIP MESSAGE was not sent ...\n");
			else
				DBG("XJAB:xj_manage_jab: SIP MESSAGE was sent.\n");
		}
		goto ready;
	} // end MESSAGE
	
	if(!strncasecmp(xode_get_name(x), "presence", 8))
	{
		DBG("XJAB:xj_manage_jab: jabber [presence] received\n");
		if(!jbc)
			goto ready;
		type = xode_get_attrib(x, "type");
		if(type!=NULL && !strncasecmp(type, "error", 5))
			goto ready;
		
		if(type == NULL || !strncasecmp(type, "online", 6))
		{
			from = xode_get_attrib(x, "from");
			if(from != NULL && strchr(from, '@') == NULL)
			{
				if(!strncasecmp(from, XJ_AIM_NAME, XJ_AIM_LEN))
				{
					jbc->ready |= XJ_NET_AIM;
					DBG("XJAB:xj_manage_jab: AIM network ready\n");
				}
				else if(!strncasecmp(from, XJ_ICQ_NAME, XJ_ICQ_LEN))
				{
					jbc->ready |= XJ_NET_ICQ;
					DBG("XJAB:xj_manage_jab: ICQ network ready\n");
				}
				else if(!strncasecmp(from, XJ_MSN_NAME, XJ_MSN_LEN))
				{
					jbc->ready |= XJ_NET_MSN;
					DBG("XJAB:xj_manage_jab: MSN network ready\n");
				}
				else if(!strncasecmp(from, XJ_YAH_NAME, XJ_YAH_LEN))
				{
					jbc->ready |= XJ_NET_YAH;
					DBG("XJAB:xj_manage_jab: YAHOO network ready\n");
				} 
			}
		}
		
		goto ready;
	} // end PRESENCE
	
	if(!strncasecmp(xode_get_name(x), "iq", 2))
	{
		DBG("XJAB:xj_manage_jab: jabber [iq] received\n");
		if(!jbc)
			goto ready;
		if(!strncasecmp(xode_get_attrib(x, "type"), "result", 6))
		{
			if((y = xode_get_tag(x, "query?xmlns=jabber:iq:roster")) == NULL)
				goto ready;
			z = xode_get_firstchild(y);
			while(z)
			{
				if(!strncasecmp(xode_get_name(z), "item", 5)
					&& (type = xode_get_attrib(z, "jid")) != NULL
					&& strchr(type, '@') == NULL)
				{
					if(!strncasecmp(type, XJ_AIM_NAME, XJ_AIM_LEN))
					{
						jbc->allowed |= XJ_NET_AIM;
						DBG("XJAB:xj_manage_jab: AIM network available\n");
					}
					else if(!strncasecmp(type, XJ_ICQ_NAME, XJ_ICQ_LEN))
					{
						jbc->allowed |= XJ_NET_ICQ;
						DBG("XJAB:xj_manage_jab: ICQ network available\n");
					}
					else if(!strncasecmp(type, XJ_MSN_NAME, XJ_MSN_LEN))
					{
						jbc->allowed |= XJ_NET_MSN;
						DBG("XJAB:xj_manage_jab: MSN network available\n");
					}
					else if(!strncasecmp(type, XJ_YAH_NAME, XJ_YAH_LEN))
					{
						jbc->allowed |= XJ_NET_YAH;
						DBG("XJAB:xj_manage_jab: YAHOO network available\n");
					} 
				}
				z = xode_get_nextsibling(z);
			}
		}
		
		goto ready;
	} // end IQ

ready:
	xode_free(x);
	return err;
}
/*****************************     ****************************************/


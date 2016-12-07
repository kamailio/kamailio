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
 * History
 * -------
 * 2003-01-20  xj_worker_precess function cleaning - some part of it moved to
 *             xj_worker_check_jcons function, (dcm)
 * 2003-02-28  send NOTIFYs even the connection is closed by user, (dcm)
 * 2003-03-11  major locking changes - uses locking.h, (andrei)
 * 2003-05-07  added new presence status - 'terminated' - when connection
 *             with Jabber server is lost or closed, (dcm)
 * 2003-05-09  added new presence status - 'refused' - when the presence
 *             subscription request is refused by target, (dcm)
 * 2003-05-09  new xj_worker_precess function cleaning - some part of it moved
 *             to xj_worker_check_qmsg and xj_worker_check_watcher functions,
 *             (dcm)
 * 2004-06-07  new DB api => xj_worker_process takes another parameter: dbf
 *              (andrei)
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "../../dprint.h"
#include "../../timer.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../modules/tm/tm_load.h"

#include "xjab_worker.h"
#include "xjab_util.h"
#include "xjab_jcon.h"
#include "xjab_dmsg.h"
#include "xode.h"
#include "xjab_presence.h"

#include "mdefines.h"

#define XJAB_RESOURCE "serXjab"

#define XJ_ADDRTR_NUL	0
#define XJ_ADDRTR_S2J	1
#define XJ_ADDRTR_J2S	2
#define XJ_ADDRTR_CON	4

#define XJ_MSG_POOL_SIZE	10

// proxy address
#define _PADDR(a)	((a)->aliases->proxy)

/** TM bind */
extern struct tm_binds tmb;

/** debug info */
int _xj_pid = 0;
int main_loop = 1;

/** **/
extern char *registrar;
static str jab_gw_name = {"jabber_gateway@127.0.0.1", 24};

/**
 * address correction
 * alias A~B: flag == 0 => A->B, otherwise B->A
 */
int xj_address_translation(str *src, str *dst, xj_jalias als, int flag)
{
	char *p, *p0;
	int i, ll;
	
	if(!src || !dst || !src->s || !dst->s )
		return -1; 
	
	if(!als || !als->jdm || !als->jdm->s || als->jdm->len <= 0)
		goto done;
	
	dst->len = 0;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("%d: - checking aliases\n", _xj_pid);
#endif
	p = src->s;

	while(p<(src->s + src->len)	&& *p != '@') 
		p++;
	if(*p != '@')
		goto done;

	p++;
	ll = src->s + src->len - p;

#ifdef XJ_EXTRA_DEBUG
	LM_DBG("%d: - domain is [%.*s]\n",_xj_pid,ll,p);
#endif
	
	/*** checking aliases */
	if(als->size > 0)
	{
		for(i=0; i<als->size; i++)
			if(als->a[i].len == ll && 
				!strncasecmp(p, als->a[i].s, als->a[i].len))
			{
				if(als->d[i])
				{
					if(flag & XJ_ADDRTR_S2J)
					{
						strncpy(dst->s, src->s, src->len);
						p0 = dst->s;
						while(p0 < dst->s + (p-src->s)) 
						{
							if(*p0 == als->dlm)
								*p0 = als->d[i];
							p0++;
						}
						return 0;
					}
					if(flag & XJ_ADDRTR_J2S)
					{
						strncpy(dst->s, src->s, src->len);
						p0 = dst->s;
						while(p0 < dst->s + (p-src->s)) 
						{
							if(*p0 == als->d[i])
								*p0 = als->dlm;
							p0++;
						}						
						return 0;
					}
				}
				goto done;
			}
	}

#ifdef XJ_EXTRA_DEBUG
	LM_DBG("%d: - doing address correction\n",
			_xj_pid);	
#endif
	
	if(flag & XJ_ADDRTR_S2J)
	{
		if(als->jdm->len != ll || strncasecmp(p, als->jdm->s, als->jdm->len))
		{
			LM_DBG("%d: - wrong Jabber"
				" destination <%.*s>!\n", _xj_pid, src->len, src->s);
			return -1;
		}
		if(flag & XJ_ADDRTR_CON)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("%d: - that is for"
				" Jabber conference\n", _xj_pid);
#endif
			p0 = p-1;
			while(p0 > src->s && *p0 != als->dlm)
				p0--;
			if(p0 <= src->s)
				return -1;
			p0--;
			while(p0 > src->s && *p0 != als->dlm)
				p0--;
			if(*p0 != als->dlm)
				return -1;
			dst->len = p - p0 - 2;
			strncpy(dst->s, p0+1, dst->len);
			dst->s[dst->len]=0;
			p = dst->s;
			while(p < (dst->s + dst->len) && *p!=als->dlm)
				p++;
			if(*p==als->dlm)
				*p = '@';
			return 0;
		}
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("%d: - that is for Jabber network\n", _xj_pid);
#endif
		dst->len = p - src->s - 1;
		strncpy(dst->s, src->s, dst->len);
		dst->s[dst->len]=0;
		if((p = strchr(dst->s, als->dlm)) != NULL)
			*p = '@';
		else
		{
			LM_DBG("%d: - wrong Jabber"
				" destination <%.*s>!!!\n", _xj_pid, src->len, src->s);
			return -1;
		}
		return 0;
	}
	if(flag & XJ_ADDRTR_J2S)
	{
		*(p-1) = als->dlm;
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
 * - rank : worker's rank
 * - db_con : connection to database
 * - priority: jabber's priority
 *   dbf: database module callbacks structure
 * return : 0 on success or <0 on error
 */
int xj_worker_process(xj_wlist jwl, char* jaddress, int jport, char* priority,
		int rank, db1_con_t* db_con, db_func_t* dbf)
{
	int pipe, ret, i, pos, maxfd, flag;
	xj_jcon_pool jcp;
	struct timeval tmv;
	fd_set set, mset;
	xj_sipmsg jsmsg;
	str sto;
	xj_jcon jbc = NULL;
	xj_jconf jcf = NULL;
	char *p, buff[1024], recv_buff[4096];
	int flags, nr, ltime = 0;

	static str tmp1 = str_init("sip_id");
	static str tmp2 = str_init("type");
	static str tmp3 = str_init("jab_id");
	static str tmp4 = str_init("jab_passwd");
	
	db_key_t keys[] = {&tmp1, &tmp2};
	db_val_t vals[2];
	db_key_t col[] = {&tmp3, &tmp4};
	db1_res_t* res = NULL;

	vals[0].type=DB1_STRING;
	vals[0].nul=0;
	vals[0].val.string_val=buff;
	vals[1].type=DB1_INT;
	vals[1].nul=0;
	vals[1].val.int_val=0;
		
	_xj_pid = getpid();
	
	//signal(SIGTERM, xj_sig_handler);
	//signal(SIGINT, xj_sig_handler);
	//signal(SIGQUIT, xj_sig_handler);
	signal(SIGSEGV, xj_sig_handler);

	if(registrar)
	{
		jab_gw_name.s = registrar;
		jab_gw_name.len = strlen(registrar);
		if(registrar[0]== 's' && registrar[1]== 'i' &&
			registrar[2]== 'p' && registrar[3]== ':')
		{
			jab_gw_name.s += 4;
			jab_gw_name.len -= 4;
		}
	}

	if(!jwl || !jwl->aliases || !jwl->aliases->jdm 
			|| !jaddress || rank >= jwl->len)
	{
		LM_DBG("[%d]:%d: exiting - wrong parameters\n", rank, _xj_pid);
		return -1;
	}

	pipe = jwl->workers[rank].rpipe;
	LM_DBG("[%d]:%d: started - pipe=<%d> : 1st message delay"
		" <%d>\n", rank, _xj_pid, pipe, jwl->delayt);
	if((jcp=xj_jcon_pool_init(jwl->maxj,XJ_MSG_POOL_SIZE,jwl->delayt))==NULL)
	{
		LM_DBG("cannot allocate the pool\n");
		return -1;
	}

	maxfd = pipe;
	tmv.tv_sec = jwl->sleept;
	tmv.tv_usec = 0;

	FD_ZERO(&set);
	FD_SET(pipe, &set);
	while(main_loop)
	{
		mset = set;

		tmv.tv_sec = (jcp->jmqueue.size == 0)?jwl->sleept:1;
#ifdef XJ_EXTRA_DEBUG
		//LM_DBG("XJAB:xj_worker[%d]:%d: select waiting %ds - queue=%d\n",rank,
		//		_xj_pid, (int)tmv.tv_sec, jcp->jmqueue.size);
#endif
		tmv.tv_usec = 0;

		ret = select(maxfd+1, &mset, NULL, NULL, &tmv);
		
		// check the msg queue
		xj_worker_check_qmsg(jwl, jcp);
		
		if(ret <= 0)
			goto step_x;

#ifdef XJ_EXTRA_DEBUG
		LM_DBG("%d: something is coming\n", _xj_pid);
#endif
		if(!FD_ISSET(pipe, &mset))
			goto step_y;
		
		if(read(pipe, &jsmsg, sizeof(jsmsg)) < (int)sizeof(jsmsg))
		{
			LM_DBG("%d: BROKEN PIPE - exiting\n", _xj_pid);
			break;
		}

#ifdef XJ_EXTRA_DEBUG
		LM_DBG("%d: job <%p> from SER\n", _xj_pid, jsmsg);
#endif

		if(jsmsg == NULL || jsmsg->jkey==NULL || jsmsg->jkey->id==NULL)
			goto step_w;

		strncpy(buff, jsmsg->jkey->id->s, jsmsg->jkey->id->len);
		buff[jsmsg->jkey->id->len] = 0;

		jbc = xj_jcon_pool_get(jcp, jsmsg->jkey);
		
		switch(jsmsg->type)
		{
			case XJ_SEND_MESSAGE:
				if(!xj_jconf_check_addr(&jsmsg->to, jwl->aliases->dlm) &&
				(!jbc||!xj_jcon_get_jconf(jbc,&jsmsg->to,jwl->aliases->dlm)))
				{
					xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_NOTJCONF, NULL);
					goto step_w;
				}
				break;
			case XJ_REG_WATCHER:
			case XJ_JOIN_JCONF:
			case XJ_GO_ONLINE:
				break;
			case XJ_EXIT_JCONF:
				if(jbc == NULL)
					goto step_w;
				// close the conference session here
				if(jbc->nrjconf <= 0)
					goto step_w;
				if(!xj_jconf_check_addr(&jsmsg->to, jwl->aliases->dlm))
					xj_jcon_del_jconf(jbc, &jsmsg->to, jwl->aliases->dlm,
						XJ_JCMD_UNSUBSCRIBE);
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
					XJ_DMSG_INF_JCONFEXIT, NULL);
				goto step_w;
			case XJ_GO_OFFLINE:
				if(jbc != NULL)
					jbc->expire = ltime = -1;
				goto step_w;
			case XJ_DEL_WATCHER:
			default:
				goto step_w;
		}
		
		if(jbc != NULL)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("%d: connection already exists"
				" for <%s> ...\n", _xj_pid, buff);
#endif
			xj_jcon_update(jbc, jwl->cachet);
			goto step_z;
		}
		
		// NO OPEN CONNECTION FOR THIS SIP ID
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("%d: new connection for <%s>.\n", _xj_pid, buff);
#endif		
		if(dbf->query(db_con, keys, 0, vals, col, 2, 2, NULL, &res) != 0 ||
			RES_ROW_N(res) <= 0)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("%d: no database result when looking"
				" for associated Jabber account\n", _xj_pid);
#endif
			xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to, 
				XJ_DMSG_ERR_JGWFORB, NULL);
			
			goto step_v;
		}
		
		jbc = xj_jcon_init(jaddress, jport);
		
		if(xj_jcon_connect(jbc))
		{
			LM_DBG("%d: Cannot connect"
				" to the Jabber server ...\n", _xj_pid);
			xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to, 
				XJ_DMSG_ERR_NOJSRV, NULL);

			goto step_v;
		}
		
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("auth to jabber as: [%s] / [xxx]\n",
			(char*)(ROW_VALUES(RES_ROWS(res))[0].val.string_val));
//			(char*)(ROW_VALUES(RES_ROWS(res))[1].val.string_val));
#endif		
		if(xj_jcon_user_auth(jbc,
			(char*)(ROW_VALUES(RES_ROWS(res))[0].val.string_val),
			(char*)(ROW_VALUES(RES_ROWS(res))[1].val.string_val),
			XJAB_RESOURCE) < 0)
		{
			LM_DBG("athentication to the Jabber server failed ...\n");
			xj_jcon_disconnect(jbc);
			
			xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to, 
					XJ_DMSG_ERR_JAUTH, NULL);
			
			xj_jcon_free(jbc);
			goto step_v;
		}
		
		if(xj_jcon_set_attrs(jbc, jsmsg->jkey, jwl->cachet, jwl->delayt)
			|| xj_jcon_pool_add(jcp, jbc))
		{
			LM_DBG("keeping connection to Jabber server"
				" failed! Not enough memory ...\n");
			xj_jcon_disconnect(jbc);
			xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,	
					XJ_DMSG_ERR_JGWFULL, NULL);
			xj_jcon_free(jbc);
			goto step_v;
		}
								
		/** add socket descriptor to select */
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("add connection on <%d> \n",jbc->sock);
#endif
		if(jbc->sock > maxfd)
			maxfd = jbc->sock;
		FD_SET(jbc->sock, &set);
										
		xj_jcon_get_roster(jbc);
		xj_jcon_send_presence(jbc, NULL, NULL, "Online", priority);
		
		/** wait for a while - the worker is tired */
		//sleep(3);
		
		if ((res != NULL) && (dbf->free_result(db_con,res) < 0))
		{
			LM_DBG("failed to free SQL result - worker terminated\n");
			return -1;
		}
		else
			res = NULL;

step_z:
		if(jsmsg->type == XJ_GO_ONLINE)
			goto step_w;
		
		if(jsmsg->type == XJ_REG_WATCHER)
		{ // update or register a presence watcher
			xj_worker_check_watcher(jwl, jcp, jbc, jsmsg);
			goto step_w;
		}
		
		flag = 0;
		if(!xj_jconf_check_addr(&jsmsg->to, jwl->aliases->dlm))
		{
			if((jcf = xj_jcon_get_jconf(jbc, &jsmsg->to, jwl->aliases->dlm))
					!= NULL)
			{
				if((jsmsg->type == XJ_JOIN_JCONF) &&
					!(jcf->status & XJ_JCONF_READY || 
						jcf->status & XJ_JCONF_WAITING))
				{
					if(!xj_jcon_jconf_presence(jbc,jcf,NULL,"online"))
						jcf->status = XJ_JCONF_WAITING;
					else
					{
						// unable to join the conference 
						// --- send back to SIP user a msg
						xj_send_sip_msgz(_PADDR(jwl),jsmsg->jkey->id,&jsmsg->to,
							XJ_DMSG_ERR_JOINJCONF, &jbc->jkey->flag);
						goto step_w;
					}
				}
				flag |= XJ_ADDRTR_CON;
			}
			else
			{
				// unable to get the conference 
				// --- send back to SIP user a msg
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_NEWJCONF, &jbc->jkey->flag);
				goto step_w;
			}
		}
		if(jsmsg->type != XJ_SEND_MESSAGE)
			goto step_w;
		
		// here will come only XJ_SEND_MESSAGE
		switch(xj_jcon_is_ready(jbc,jsmsg->to.s,jsmsg->to.len,jwl->aliases->dlm))
		{
			case 0:
#ifdef XJ_EXTRA_DEBUG
				LM_DBG("sending the message to Jabber network ...\n");
#endif
				/*** address correction ***/
				sto.s = buff; 
				sto.len = 0;
				flag |= XJ_ADDRTR_S2J;
				if(xj_address_translation(&jsmsg->to, &sto, jwl->aliases, 
							flag) == 0)
				{
					if(xj_jcon_send_msg(jbc, sto.s, sto.len,
						jsmsg->msg.s, jsmsg->msg.len,
						(flag&XJ_ADDRTR_CON)?XJ_JMSG_GROUPCHAT:XJ_JMSG_CHAT)<0)
							
						xj_send_sip_msgz(_PADDR(jwl),jsmsg->jkey->id,&jsmsg->to,
							XJ_DMSG_ERR_SENDJMSG, &jbc->jkey->flag);
				}
				else
					LM_ERR("sending as Jabber message ...\n");
						
				goto step_w;
		
			case 1:
#ifdef XJ_EXTRA_DEBUG
				LM_DBG("scheduling the message.\n");
#endif
				if(xj_jcon_pool_add_jmsg(jcp, jsmsg, jbc) < 0)
				{
					LM_DBG("scheduling the message FAILED."
							"Message was dropped.\n");
					xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_STOREJMSG, &jbc->jkey->flag);
					goto step_w;
				}
				else // skip freeing the SIP message - now is in queue
					goto step_y;
	
			case 2:
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_NOREGIM, &jbc->jkey->flag);
				goto step_w;
			case 3: // not joined to Jabber conference
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_NOTJCONF, &jbc->jkey->flag);
				goto step_w;
				
			default:
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_SENDJMSG, &jbc->jkey->flag);
				goto step_w;
		}

step_v: // error connecting to Jabber server
		
		// cleaning jab_wlist
		xj_wlist_del(jwl, jsmsg->jkey, _xj_pid);

		// cleaning db_query
		if ((res != NULL) && (dbf->free_result(db_con,res) < 0))
		{
			LM_DBG("failed to free the SQL result - worker terminated\n");
			return -1;
		}
		else
			res = NULL;

step_w:
		if(jsmsg!=NULL)
		{
			xj_sipmsg_free(jsmsg);
			jsmsg = NULL;
		}			

step_y:			 
		// check for new message from ... JABBER
		for(i = 0; i < jcp->len && main_loop; i++)
		{
			if(jcp->ojc[i] == NULL)
				continue;
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("checking socket <%d> ...\n", jcp->ojc[i]->sock);
#endif
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
    					fcntl(jcp->ojc[i]->sock, F_SETFL, flags|O_NONBLOCK);
   					}
				}
				
				if((nr = read(jcp->ojc[i]->sock, p,	
						sizeof(recv_buff)-(p-recv_buff))) == 0
					||(nr < 0 && errno != EAGAIN))
				{
					LM_DBG("connection to jabber lost on socket <%d> ...\n",
						jcp->ojc[i]->sock);
					xj_send_sip_msgz(_PADDR(jwl), jcp->ojc[i]->jkey->id,
						&jab_gw_name,XJ_DMSG_ERR_DISCONNECTED,&jbc->jkey->flag);
					// make sure that will ckeck expired connections
					ltime = jcp->ojc[i]->expire = -1;
					FD_CLR(jcp->ojc[i]->sock, &set);
					goto step_xx;
				}
#ifdef XJ_EXTRA_DEBUG
				LM_DBG("received: %dbytes Err:%d/EA:%d\n", nr, errno, EAGAIN);
#endif
				xj_jcon_update(jcp->ojc[i], jwl->cachet);

				if(nr>0)
					p[nr] = 0;
				nr = strlen(recv_buff);
				pos = 0;
#ifdef XJ_EXTRA_DEBUG
				LM_DBG("JMSG START ----------\n%.*s\n"
					" JABBER: JMSGL:%d END ----------\n", nr, recv_buff, nr);
#endif
			} while(xj_manage_jab(recv_buff, nr, &pos, jwl->aliases,
							jcp->ojc[i]) == 9	&& main_loop);
	
			/**
			 * flush out the socket - set it back to blocking 
			 */
 			flags = fcntl(jcp->ojc[i]->sock, F_GETFL, 0);
			if(flags!=-1 && (flags & O_NONBLOCK))
   			{
    			/* reset NONBLOCK bit to enable blocking */
    			fcntl(jcp->ojc[i]->sock, F_SETFL, flags & ~O_NONBLOCK);
   			}
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("msgs from socket <%d> parsed ...\n", jcp->ojc[i]->sock);	
#endif
		} // end FOR(i = 0; i < jcp->len; i++)

step_x:
		if(ret < 0)
		{
			LM_DBG("signal received!!!!!!!!\n");
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
step_xx:
		if(ltime < 0 || ltime + jwl->sleept <= get_ticks())
		{
			ltime = get_ticks();
#ifdef XJ_EXTRA_DEBUG
			//LM_DBG("XJAB:xj_worker:%d: scanning for expired connection\n",
			//	_xj_pid);
#endif
			xj_worker_check_jcons(jwl, jcp, ltime, &set);
		}
	} // END while

	LM_DBG("cleaning procedure\n");

	return 0;
} // end xj_worker_process


/**
 * parse incoming message from Jabber server
 */
int xj_manage_jab(char *buf, int len, int *pos, xj_jalias als, xj_jcon jbc)
{
	int j, err=0;
	char *p, *to, *from, *msg, *type, *emsg, *ecode, lbuf[4096], fbuf[128];
	xj_jconf jcf = NULL;
	str ts, tf;
	xode x, y, z;
	str *sid;
	xj_pres_cell prc = NULL;

	if(!jbc)
		return -1;

	sid = jbc->jkey->id;	
	x = xode_from_strx(buf, len, &err, &j);
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("xode ret:%d pos:%d\n", err, j);
#endif	
	if(err && pos != NULL)
		*pos= j;
	if(x == NULL)
		return -1;
	lbuf[0] = 0;
	ecode = NULL;

/******************** XMPP 'MESSAGE' HANDLING **********************/
	
	if(!strncasecmp(xode_get_name(x), "message", 7))
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("jabber [message] received\n");
#endif
		if((to = xode_get_attrib(x, "to")) == NULL)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("missing 'to' attribute\n");
#endif
			err = -1;
			goto ready;
		}
		if((from = xode_get_attrib(x, "from")) == NULL)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("missing 'from' attribute\n");
#endif
			err = -1;
			goto ready;
		}
		if((y = xode_get_tag(x, "body")) == NULL
				|| (msg = xode_get_data(y)) == NULL)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("missing 'body' of message\n");
#endif
			err = -1;
			goto ready;
		}
		type = xode_get_attrib(x, "type");
		if(type != NULL && !strncasecmp(type, "error", 5))
		{
			if((y = xode_get_tag(x, "error")) == NULL
					|| (emsg = xode_get_data(y)) == NULL)
				strcpy(lbuf, "{Error sending following message} - ");
			else
			{
				ecode = xode_get_attrib(y, "code");
				strcpy(lbuf, "{Error (");
				if(ecode != NULL)
				{
					strcat(lbuf, ecode);
					strcat(lbuf, " - ");
				}
				strcat(lbuf, emsg);
				strcat(lbuf, ") when trying to send following message}");
			}

		}

		// is from a conference?!?!
		if((jcf=xj_jcon_check_jconf(jbc, from))!=NULL)
		{
			if(lbuf[0] == 0)
			{
				p = from + strlen(from);
				while(p>from && *p != '/')
					p--;
				if(*p == '/')
				{
					if(jcf->nick.len>0 
						&& strlen(p+1) == jcf->nick.len
						&& !strncasecmp(p+1, jcf->nick.s, jcf->nick.len))
					{
#ifdef XJ_EXTRA_DEBUG
						LM_DBG("message sent by myself\n");
#endif
						goto ready;
					}
					lbuf[0] = '[';
					lbuf[1] = 0;
					strcat(lbuf, p+1);
					strcat(lbuf, "] ");
				}
			}
			else
			{
				jcf->status = XJ_JCONF_NULL;
				xj_jcon_jconf_presence(jbc,jcf,NULL,"online");
			}
			strcat(lbuf, msg);
			ts.s = lbuf;
			ts.len = strlen(lbuf);
	
			if(xj_send_sip_msg(als->proxy, sid, &jcf->uri, &ts,
						&jbc->jkey->flag)<0)
				LM_ERR("sip message was not sent!\n");
#ifdef XJ_EXTRA_DEBUG
			else
				LM_DBG("sip message was sent!\n");
#endif
			goto ready;
		}

		strcat(lbuf, msg);
		ts.s = from;
		ts.len = strlen(from);
		tf.s = fbuf;
		tf.len = 0;
		if(xj_address_translation(&ts, &tf, als, XJ_ADDRTR_J2S) == 0)
		{
			ts.s = lbuf;
			ts.len = strlen(lbuf);
	
			if(xj_send_sip_msg(als->proxy, sid, &tf, &ts, &jbc->jkey->flag)<0)
				LM_ERR("sip message was not sent!\n");
#ifdef XJ_EXTRA_DEBUG
			else
				LM_DBG("sip message was sent!\n");
#endif
		}
		goto ready;
	}
/*------------------- END 'MESSAGE' HANDLING ----------------------*/
	
/******************** XMPP 'PRESENCE' HANDLING *********************/
	if(!strncasecmp(xode_get_name(x), "presence", 8))
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("jabber [presence] received\n");
#endif
		type = xode_get_attrib(x, "type");
		from = xode_get_attrib(x, "from");
		if(from == NULL)
			goto ready;
		ts.s = from;
		p = from;
		while(p<from + strlen(from) && *p != '/')
					p++;
		if(*p == '/')
			ts.len = p - from;
		else
			ts.len = strlen(from);

		if(type == NULL || !strncasecmp(type, "online", 6)
			|| !strncasecmp(type, "available", 9))
		{
			if(strchr(from, '@') == NULL)
			{
				if(!strncasecmp(from, XJ_AIM_NAME, XJ_AIM_LEN))
				{
					jbc->ready |= XJ_NET_AIM;
#ifdef XJ_EXTRA_DEBUG
					LM_DBG("AIM network ready\n");
#endif
				}
				else if(!strncasecmp(from, XJ_ICQ_NAME, XJ_ICQ_LEN))
				{
					jbc->ready |= XJ_NET_ICQ;
#ifdef XJ_EXTRA_DEBUG
					LM_DBG("ICQ network ready\n");
#endif
				}
				else if(!strncasecmp(from, XJ_MSN_NAME, XJ_MSN_LEN))
				{
					jbc->ready |= XJ_NET_MSN;
#ifdef XJ_EXTRA_DEBUG
					LM_DBG("MSN network ready\n");
#endif
				}
				else if(!strncasecmp(from, XJ_YAH_NAME, XJ_YAH_LEN))
				{
					jbc->ready |= XJ_NET_YAH;
#ifdef XJ_EXTRA_DEBUG
					LM_DBG("YAHOO network ready\n");
#endif
				}
			}
			else if((jcf=xj_jcon_check_jconf(jbc, from))!=NULL)
			{
				jcf->status = XJ_JCONF_READY;
#ifdef XJ_EXTRA_DEBUG
				LM_DBG(" %s conference ready\n", from);
#endif
			}
			else
			{
#ifdef XJ_EXTRA_DEBUG
				LM_DBG("user <%.*s> is online\n",ts.len,ts.s);
#endif
				prc = xj_pres_list_check(jbc->plist, &ts);
				if(prc)
				{
					if(prc->state != XJ_PS_ONLINE)
					{
						prc->state = XJ_PS_ONLINE;
						goto call_pa_cbf;
					}
				}
				else
				{
#ifdef XJ_EXTRA_DEBUG
					LM_DBG("user state received - creating"
						" presence cell for [%.*s]\n", ts.len, ts.s);
#endif
					prc = xj_pres_cell_new();
					if(prc == NULL)
					{
						LM_DBG("cannot create presence"
							" cell for [%s]\n", from);
						goto ready;
					}
					if(xj_pres_cell_init(prc, &ts, NULL, NULL)<0)
					{
						LM_DBG("cannot init presence"
							" cell for [%s]\n", from);
						xj_pres_cell_free(prc);
						goto ready;
					}
					prc = xj_pres_list_add(jbc->plist, prc);
					if(prc)
					{
						prc->state = XJ_PS_ONLINE;
						goto call_pa_cbf;
					}
				}
			}
			goto ready;
		}
		
		if(strchr(from, '@') == NULL)
			goto ready;
	
		
		if(!strncasecmp(type, "error", 5))
		{
			if((jcf=xj_jcon_check_jconf(jbc, from))!=NULL)
			{
				tf.s = from;
				tf.len = strlen(from);
				if((y = xode_get_tag(x, "error")) == NULL)
					goto ready;
				if ((p = xode_get_attrib(y, "code")) != NULL
						&& atoi(p) == 409)
				{
					xj_send_sip_msgz(als->proxy, sid, &tf,
							XJ_DMSG_ERR_JCONFNICK, &jbc->jkey->flag);
					goto ready;
				}
				xj_send_sip_msgz(als->proxy,sid,&tf,XJ_DMSG_ERR_JCONFREFUSED,
						&jbc->jkey->flag);
			}
			goto ready;
		}
		if(type!=NULL && !strncasecmp(type, "subscribe", 9))
		{
			xj_jcon_send_presence(jbc, from, "subscribed", NULL, NULL);
			goto ready;
		}

		prc = xj_pres_list_check(jbc->plist, &ts);
		if(!prc)
			goto ready;

		if(!strncasecmp(type, "unavailable", 11))
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("user <%s> is offline\n", from);
#endif
			if(prc->state != XJ_PS_OFFLINE)
			{
				prc->state = XJ_PS_OFFLINE;
				goto call_pa_cbf;
			}
			goto ready;
		}
		
		if(!strncasecmp(type, "unsubscribed", 12))
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("user <%s> does not allow to see his"
				" presence status\n", from);
#endif
			if(prc->state != XJ_PS_REFUSED)
			{
				prc->state = XJ_PS_REFUSED;
				goto call_pa_cbf;
			}
		}
	
		// ignoring unknown types
		goto ready;
	}
/*------------------- END XMPP 'PRESENCE' HANDLING ----------------*/
	
/******************** XMPP 'IQ' HANDLING ***************************/
	if(!strncasecmp(xode_get_name(x), "iq", 2))
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("jabber [iq] received\n");
#endif
		if(!strncasecmp(xode_get_attrib(x, "type"), "result", 6))
		{
			if((y = xode_get_tag(x, "query?xmlns=jabber:iq:roster")) == NULL)
				goto ready;
			z = xode_get_firstchild(y);
			while(z)
			{
				if(!strncasecmp(xode_get_name(z), "item", 5)
					&& (from = xode_get_attrib(z, "jid")) != NULL)
				{
					if(strchr(from, '@') == NULL)
					{ // transports
						if(!strncasecmp(from, XJ_AIM_NAME, XJ_AIM_LEN))
						{
							jbc->allowed |= XJ_NET_AIM;
#ifdef XJ_EXTRA_DEBUG
							LM_DBG("AIM network available\n");
#endif
						}
						else if(!strncasecmp(from, XJ_ICQ_NAME, XJ_ICQ_LEN))
						{
							jbc->allowed |= XJ_NET_ICQ;
#ifdef XJ_EXTRA_DEBUG
							LM_DBG("ICQ network available\n");
#endif
						}
						else if(!strncasecmp(from, XJ_MSN_NAME, XJ_MSN_LEN))
						{
							jbc->allowed |= XJ_NET_MSN;
#ifdef XJ_EXTRA_DEBUG
							LM_DBG("MSN network available\n");
#endif
						}
						else if(!strncasecmp(from, XJ_YAH_NAME, XJ_YAH_LEN))
						{
							jbc->allowed |= XJ_NET_YAH;
#ifdef XJ_EXTRA_DEBUG
							LM_DBG("YAHOO network available\n");
#endif
						}
						goto next_sibling;
					}
				}
next_sibling:
				z = xode_get_nextsibling(z);
			}
		}
		
		goto ready;
	}
/*------------------- END XMPP 'IQ' HANDLING ----------------------*/

call_pa_cbf:
	if(prc && prc->cbf)
	{
		// call the PA callback function
		tf.s = fbuf;
		tf.len = 0;
		if(xj_address_translation(&ts,&tf,als,XJ_ADDRTR_J2S)==0)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("calling CBF(%.*s,%d)\n", tf.len, tf.s, prc->state);
#endif
			(*(prc->cbf))(&tf, &tf, prc->state, prc->cbp);
		}
	}
ready:
	xode_free(x);
	return err;
}

/**
 *
 */
void xj_sig_handler(int s) 
{
	//signal(SIGTERM, xj_sig_handler);
	//signal(SIGINT, xj_sig_handler);
	//signal(SIGQUIT, xj_sig_handler);
	signal(SIGSEGV, xj_sig_handler);
	main_loop = 0;
	LM_DBG("%d: SIGNAL received=%d\n **************", _xj_pid, s);
}

/*****************************     ****************************************/

/**
 * send a SIP MESSAGE message
 * - to : destination
 * - from : origin
 * - contact : contact header
 * - msg : body of the message
 * return : 0 on success or <0 on error
 */
int xj_send_sip_msg(str *proxy, str *to, str *from, str *msg, int *cbp)
{
	str  msg_type = { "MESSAGE", 7};
	char buf[512];
	str  tfrom;
	str  str_hdr;
	char buf1[1024];
	uac_req_t uac_r;

	if( !to || !to->s || to->len <= 0 
			|| !from || !from->s || from->len <= 0 
			|| !msg || !msg->s || msg->len <= 0
			|| (cbp && *cbp!=0) )
		return -1;

	// from correction
	tfrom.len = 0;
	strncpy(buf+tfrom.len, "<sip:", 5);
	tfrom.len += 5;
	strncpy(buf+tfrom.len, from->s, from->len);
	tfrom.len += from->len;
	buf[tfrom.len++] = '>';
		
	tfrom.s = buf;
	
	// building Contact and Content-Type
	strcpy(buf1,"Content-Type: text/plain"CRLF"Contact: ");
	str_hdr.len = 24 + CRLF_LEN + 9;
	
	strncat(buf1,tfrom.s,tfrom.len);
	str_hdr.len += tfrom.len;
	
	strcat(buf1, CRLF);
	str_hdr.len += CRLF_LEN;
	str_hdr.s = buf1;
	if(cbp)
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("uac callback parameter [%p==%d]\n", cbp, *cbp);
#endif
		set_uac_req(&uac_r, &msg_type, &str_hdr, msg, 0, TMCB_LOCAL_COMPLETED,
				xj_tuac_callback, (void*)cbp);
	} else {
		set_uac_req(&uac_r, &msg_type, &str_hdr, msg, 0, 0, 0, 0);		
	}
	return tmb.t_request(&uac_r, 0, to, &tfrom, 0);
}

/**
 * send a SIP MESSAGE message
 * - to : destination
 * - from : origin
 * - contact : contact header
 * - msg : body of the message, string terminated by zero
 * return : 0 on success or <0 on error
 */
int xj_send_sip_msgz(str *proxy, str *to, str *from, char *msg, int *cbp)
{
	str tstr;
	int n;

	if(!to || !from || !msg || (cbp && *cbp!=0))
		return -1;

	tstr.s = msg;
	tstr.len = strlen(msg);
	if((n = xj_send_sip_msg(proxy, to, from, &tstr, cbp)) < 0)
		LM_ERR("sip message wasn't sent to [%.*s]...\n", to->len, to->s);
#ifdef XJ_EXTRA_DEBUG
	else
		LM_DBG("sip message was sent to [%.*s]...\n", to->len, to->s);
#endif
	return n;
}

/**
 * send disconnected info to all SIP users associated with worker idx
 * and clean the entries from wlist
 */
int xj_wlist_clean_jobs(xj_wlist jwl, int idx, int fl)
{
	xj_jkey p;
	if(jwl==NULL || idx < 0 || idx >= jwl->len || !jwl->workers[idx].sip_ids)
		return -1;
	lock_set_get(jwl->sems, idx);
	while((p=(xj_jkey)delpos234(jwl->workers[idx].sip_ids, 0))!=NULL)
	{
		if(fl)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("sending disconnect message"
				" to <%.*s>\n",	p->id->len, p->id->s);
#endif
			xj_send_sip_msgz(_PADDR(jwl), p->id, &jab_gw_name,
					XJ_DMSG_INF_DISCONNECTED, NULL);
		}
		jwl->workers[idx].nr--;
		xj_jkey_free_p(p);
	}
	lock_set_release(jwl->sems, idx);
	return 0;
}


/**
 * callback function for TM
 */
void xj_tuac_callback( struct cell *t, int type, struct tmcb_params *ps)
{
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("completed with status %d\n", ps->code);
#endif
	if(!ps->param)
	{
		LM_DBG("parameter not received\n");
		return;
	}
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("parameter [%p : ex-value=%d]\n", ps->param,*((int*)ps->param) );
#endif
	if(ps->code < 200 || ps->code >= 300)
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("no 2XX return code - connection set as expired \n");
#endif
		*((int*)ps->param) = XJ_FLAG_CLOSE;
	}
}

/**
 * check for expired connections
 */
void xj_worker_check_jcons(xj_wlist jwl, xj_jcon_pool jcp, int ltime, fd_set *pset)
{
	int i;
	xj_jconf jcf;
	
	for(i = 0; i < jcp->len && main_loop; i++)
	{
		if(jcp->ojc[i] == NULL)
			continue;
		if(jcp->ojc[i]->jkey->flag==XJ_FLAG_OPEN &&
			jcp->ojc[i]->expire > ltime)
			continue;
			
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("connection expired for <%.*s> \n",
			jcp->ojc[i]->jkey->id->len, jcp->ojc[i]->jkey->id->s);
#endif
		xj_send_sip_msgz(_PADDR(jwl), jcp->ojc[i]->jkey->id, &jab_gw_name,
				XJ_DMSG_INF_JOFFLINE, NULL);
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("connection's close flag =%d\n",
			jcp->ojc[i]->jkey->flag);
#endif
		// CLEAN JAB_WLIST
		xj_wlist_del(jwl, jcp->ojc[i]->jkey, _xj_pid);

		// looking for open conference rooms
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("having %d open conferences\n", 
				jcp->ojc[i]->nrjconf);
#endif
		while(jcp->ojc[i]->nrjconf > 0)
		{
			if((jcf=delpos234(jcp->ojc[i]->jconf,0))!=NULL)
			{
				// get out of room
				xj_jcon_jconf_presence(jcp->ojc[i],jcf, "unavailable", NULL);
				xj_jconf_free(jcf);
			}
			jcp->ojc[i]->nrjconf--;
		}

		// send offline presence to all subscribers
		if(jcp->ojc[i]->plist)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("sending 'terminated' status to SIP subscriber\n");
#endif
			xj_pres_list_notifyall(jcp->ojc[i]->plist,
					XJ_PS_TERMINATED);
		}
		FD_CLR(jcp->ojc[i]->sock, pset);
		xj_jcon_disconnect(jcp->ojc[i]);
		xj_jcon_free(jcp->ojc[i]);
		jcp->ojc[i] = NULL;
	}
}

/**
 * check if there are msg to send or delete from queue
 */
void xj_worker_check_qmsg(xj_wlist jwl, xj_jcon_pool jcp)
{
	int i, flag;
	str sto;
	char buff[1024];

	if(!jwl || !jcp)
		return;

	/** check the msg queue AND if the target connection is ready */
	for(i = 0; i<jcp->jmqueue.size && main_loop; i++)
	{
		if(jcp->jmqueue.jsm[i]==NULL || jcp->jmqueue.ojc[i]==NULL)
		{
			if(jcp->jmqueue.jsm[i]!=NULL)
			{
				xj_sipmsg_free(jcp->jmqueue.jsm[i]);
				jcp->jmqueue.jsm[i] = NULL;
				xj_jcon_pool_del_jmsg(jcp, i);
			}
			if(jcp->jmqueue.ojc[i]!=NULL)
				xj_jcon_pool_del_jmsg(jcp, i);
			continue;
		}
		if(jcp->jmqueue.expire[i] < get_ticks())
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("message to %.*s is expired\n",
				jcp->jmqueue.jsm[i]->to.len, 
				jcp->jmqueue.jsm[i]->to.s);
#endif
			xj_send_sip_msgz(_PADDR(jwl), jcp->jmqueue.jsm[i]->jkey->id, 
					&jcp->jmqueue.jsm[i]->to, XJ_DMSG_ERR_SENDIM,
					&jcp->jmqueue.ojc[i]->jkey->flag);
			if(jcp->jmqueue.jsm[i]!=NULL)
			{
				xj_sipmsg_free(jcp->jmqueue.jsm[i]);
				jcp->jmqueue.jsm[i] = NULL;
			}
			/** delete message from queue */
			xj_jcon_pool_del_jmsg(jcp, i);
			continue;
		}
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("%d: QUEUE: message[%d] from [%.*s]"
				"/to [%.*s]/body[%.*s] expires at %d\n",
				get_ticks(), i, 
				jcp->jmqueue.jsm[i]->jkey->id->len,
				jcp->jmqueue.jsm[i]->jkey->id->s,
				jcp->jmqueue.jsm[i]->to.len,jcp->jmqueue.jsm[i]->to.s,
				jcp->jmqueue.jsm[i]->msg.len,jcp->jmqueue.jsm[i]->msg.s,
				jcp->jmqueue.expire[i]);
#endif
		if(xj_jcon_is_ready(jcp->jmqueue.ojc[i], jcp->jmqueue.jsm[i]->to.s,
				jcp->jmqueue.jsm[i]->to.len, jwl->aliases->dlm))
			continue;
		
		/*** address correction ***/
		flag = XJ_ADDRTR_S2J;
		if(!xj_jconf_check_addr(&jcp->jmqueue.jsm[i]->to,jwl->aliases->dlm))
		flag |= XJ_ADDRTR_CON;
		
		sto.s = buff; 
		sto.len = 0;
		if(xj_address_translation(&jcp->jmqueue.jsm[i]->to,
			&sto, jwl->aliases, flag) == 0)
		{
			/** send message from queue */
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("sending the message from"
				" local queue to Jabber network ...\n");
#endif
			xj_jcon_send_msg(jcp->jmqueue.ojc[i],
				sto.s, sto.len,
				jcp->jmqueue.jsm[i]->msg.s,
				jcp->jmqueue.jsm[i]->msg.len,
				(flag&XJ_ADDRTR_CON)?XJ_JMSG_GROUPCHAT:XJ_JMSG_CHAT);
		}
		else
			LM_ERR("sending the message from"
				" local queue to Jabber network ...\n");
		
		if(jcp->jmqueue.jsm[i]!=NULL)
		{
			xj_sipmsg_free(jcp->jmqueue.jsm[i]);
			jcp->jmqueue.jsm[i] = NULL;
		}
		/** delete message from queue */
		xj_jcon_pool_del_jmsg(jcp, i);
	}
}


/**
 * update or register a presence watcher
 */
void xj_worker_check_watcher(xj_wlist jwl, xj_jcon_pool jcp,
				xj_jcon jbc, xj_sipmsg jsmsg)
{
	str sto;
	char buff[1024];
	xj_pres_cell prc = NULL;

	if(!jwl || !jcp || !jbc || !jsmsg)
		return;

	if(!jsmsg->cbf)
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("null PA callback function\n");
#endif
		return;
	}

	if(!xj_jconf_check_addr(&jsmsg->to, jwl->aliases->dlm))
	{ // is for a conference - ignore?!?!
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("presence request for a conference.\n");
#endif
		// set as offline
		(*(jsmsg->cbf))(&jsmsg->to, &jsmsg->to, XJ_PS_OFFLINE, jsmsg->p);
		return;
	}
			
	sto.s = buff; 
	sto.len = 0;

	if(xj_address_translation(&jsmsg->to, &sto, jwl->aliases, 
		XJ_ADDRTR_S2J) == 0)
	{
		prc = xj_pres_list_check(jbc->plist, &sto);
		if(!prc)
		{
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("new presence cell for %.*s.\n", sto.len, sto.s);
#endif
			prc = xj_pres_cell_new();
			if(!prc)
			{
				LM_DBG("cannot create a presence cell for %.*s.\n",sto.len, sto.s);
				return;
			}
			if(xj_pres_cell_init(prc, &sto, jsmsg->cbf, jsmsg->p)<0)
			{
				LM_DBG("cannot init the presence"
					" cell for %.*s.\n", sto.len, sto.s);
				xj_pres_cell_free(prc);
				return;
			}
			if((prc = xj_pres_list_add(jbc->plist, prc))==NULL)
			{
				LM_DBG("cannot add the presence"
					" cell for %.*s.\n", sto.len, sto.s);
				return;
			}
			sto.s[sto.len] = 0;
			if(!xj_jcon_send_subscribe(jbc, sto.s, NULL, "subscribe"))
				prc->status = XJ_PRES_STATUS_WAIT; 
		}
		else
		{
			xj_pres_cell_update(prc, jsmsg->cbf, jsmsg->p);
#ifdef XJ_EXTRA_DEBUG
			LM_DBG("calling CBF(%.*s,%d)\n",
				jsmsg->to.len, jsmsg->to.s, prc->state);
#endif
			// send presence info to SIP subscriber
			(*(prc->cbf))(&jsmsg->to, &jsmsg->to, prc->state, prc->cbp);
		}
	}
}

/*****************************     ****************************************/


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
#include <signal.h>

#include "../../dprint.h"
#include "../../timer.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../tm/tm_load.h"

#include "xjab_worker.h"
#include "xjab_util.h"
#include "xjab_jcon.h"
#include "xjab_dmsg.h"
#include "xode.h"

#include "mdefines.h"

#define XJAB_RESOURCE "serXjab"

#define XJ_ADDRTR_NUL	0
#define XJ_ADDRTR_A2B	1
#define XJ_ADDRTR_B2A	2
#define XJ_ADDRTR_CON	4

#define XJ_MSG_POOL_SIZE	10

/** TM bind */
extern struct tm_binds tmb;

/** debug info */
int _xj_pid = 0;
int main_loop = 1;

/** **/

static str jab_gw_name = {"sip_to_jabber_gateway", 21};

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
	{
		for(i=0; i<als->size; i++)
			if(!strncasecmp(p, als->a[i].s, als->a[i].len))
			{
				if(als->d[i])
				{
					if(flag & XJ_ADDRTR_A2B)
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
					if(flag & XJ_ADDRTR_B2A)
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

	DBG("XJAB:xj_address_translation:%d: - doing address corection\n", 
			_xj_pid);	

	if(flag & XJ_ADDRTR_A2B)
	{
		if(strncasecmp(p, als->jdm->s, als->jdm->len))
		{
			DBG("XJA:xj_address_translation:%d: - wrong Jabber"
				" destination!\n", _xj_pid);
			return -1;
		}
		if(flag & XJ_ADDRTR_CON)
		{
			DBG("XJAB:xj_address_translation:%d: - that is for"
				" Jabber conference\n", _xj_pid);
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
		
		DBG("XJAB:xj_address_translation:%d: - that is for"
			" Jabber network\n", _xj_pid);
		dst->len = p - src->s - 1;
		strncpy(dst->s, src->s, dst->len);
		dst->s[dst->len]=0;
		if((p = strchr(dst->s, als->dlm)) != NULL)
			*p = '@';
		else
		{
			DBG("XJAB:xj_address_translation:%d: - wrong Jabber"
			" destination\n", _xj_pid);
			return -1;
		}
		return 0;
	}
	if(flag & XJ_ADDRTR_B2A)
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
 * #return : 0 on success or <0 on error
 */
int xj_worker_process(xj_wlist jwl, char* jaddress, int jport, int rank,
		db_con_t* db_con)
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
	
	db_key_t keys[] = {"sip_id", "type"};
	db_val_t vals[] = { {DB_STRING, 0, {.string_val = buff}},
						{DB_INT, 0, {.int_val = 0}} };
	db_key_t col[] = {"jab_id", "jab_passwd"};
	db_res_t* res = NULL;

	_xj_pid = getpid();
	
	//signal(SIGTERM, xj_sig_handler);
	//signal(SIGINT, xj_sig_handler);
	//signal(SIGQUIT, xj_sig_handler);
	signal(SIGSEGV, xj_sig_handler);

	if(!jwl || !jwl->aliases || !jwl->aliases->jdm 
			|| !jaddress || rank >= jwl->len)
	{
		DBG("XJAB:xj_worker[%d]:%d: exiting - wrong parameters\n",
				rank, _xj_pid);
		return -1;
	}

	pipe = jwl->workers[rank].rpipe;

	DBG("XJAB:xj_worker[%d]:%d: started - pipe=<%d> : 1st message delay"
		" <%d>\n", rank, _xj_pid, pipe, jwl->delayt);

	if((jcp=xj_jcon_pool_init(jwl->maxj,XJ_MSG_POOL_SIZE,jwl->delayt))==NULL)
	{
		DBG("XJAB:xj_worker: cannot allocate the pool\n");
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
		DBG("XJAB:xj_worker[%d]:%d: select waiting %ds - queue=%d\n",rank,
				_xj_pid, (int)tmv.tv_sec, jcp->jmqueue.size);
		tmv.tv_usec = 0;

		ret = select(maxfd+1, &mset, NULL, NULL, &tmv);
		/** check the queue AND conecction of head element is ready */
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
				DBG("XJAB:xj_worker:%d: message to %.*s is expired\n",
					_xj_pid, jcp->jmqueue.jsm[i]->to.len, 
					jcp->jmqueue.jsm[i]->to.s);
				xj_send_sip_msgz(jcp->jmqueue.jsm[i]->jkey->id, 
						&jcp->jmqueue.jsm[i]->to, jwl->contact_h, 
						XJ_DMSG_ERR_SENDIM, &jcp->jmqueue.ojc[i]->jkey->flag);
				if(jcp->jmqueue.jsm[i]!=NULL)
				{
					xj_sipmsg_free(jcp->jmqueue.jsm[i]);
					jcp->jmqueue.jsm[i] = NULL;
				}
				/** delete message from queue */
				xj_jcon_pool_del_jmsg(jcp, i);
				continue;
			}

			DBG("XJAB:xj_worker:%d:%d: QUEUE: message[%d] from [%.*s]/to [%.*s]/"
					"body[%.*s] expires at %d\n",
					_xj_pid, get_ticks(), i, 
					jcp->jmqueue.jsm[i]->jkey->id->len,
					jcp->jmqueue.jsm[i]->jkey->id->s,
					jcp->jmqueue.jsm[i]->to.len,jcp->jmqueue.jsm[i]->to.s,
					jcp->jmqueue.jsm[i]->msg.len,jcp->jmqueue.jsm[i]->msg.s,
					jcp->jmqueue.expire[i]);
			if(xj_jcon_is_ready(jcp->jmqueue.ojc[i], jcp->jmqueue.jsm[i]->to.s,
					jcp->jmqueue.jsm[i]->to.len, jwl->aliases->dlm))
				continue;

			/*** address corection ***/
			flag = XJ_ADDRTR_A2B;
			if(!xj_jconf_check_addr(&jcp->jmqueue.jsm[i]->to,jwl->aliases->dlm))
				flag |= XJ_ADDRTR_CON;
			
			sto.s = buff; 
			sto.len = 0;
			if(xj_address_translation(&jcp->jmqueue.jsm[i]->to,
				&sto, jwl->aliases, flag) == 0)
			{
		
				/** send message from queue */
				DBG("XJAB:xj_worker:%d: SENDING THE MESSAGE FROM "
					" LOCAL QUEUE TO JABBER NETWORK ...\n", _xj_pid);
				xj_jcon_send_msg(jcp->jmqueue.ojc[i],
					sto.s, sto.len,
					jcp->jmqueue.jsm[i]->msg.s,
					jcp->jmqueue.jsm[i]->msg.len,
					(flag&XJ_ADDRTR_CON)?XJ_JMSG_GROUPCHAT:XJ_JMSG_CHAT);
			}
			else
				DBG("XJAB:xj_worker:%d: ERROR SENDING THE MESSAGE FROM "
				" LOCAL QUEUE TO JABBER NETWORK ...\n", _xj_pid);
				
			if(jcp->jmqueue.jsm[i]!=NULL)
			{
				xj_sipmsg_free(jcp->jmqueue.jsm[i]);
				jcp->jmqueue.jsm[i] = NULL;
			}
			/** delete message from queue */
			xj_jcon_pool_del_jmsg(jcp, i);
		} // end MSG queue checking
		
		if(ret <= 0)
			goto step_x;
		
		DBG("XJAB:xj_worker:%d: something is coming\n", _xj_pid);
		if(!FD_ISSET(pipe, &mset))
			goto step_y;
		
		if(read(pipe, &jsmsg, sizeof(jsmsg)) < sizeof(jsmsg))
		{
			DBG("XJAB:xj_worker:%d: BROKEN PIPE - exiting\n", _xj_pid);
			break;
		}

		DBG("XJAB:xj_worker:%d: job <%p> from SER\n", _xj_pid, jsmsg);
		
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
					xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to,
						jwl->contact_h, XJ_DMSG_ERR_NOTJCONF, NULL);
					goto step_w;
				}
				break;
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
				xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to,
					jwl->contact_h, XJ_DMSG_INF_JCONFEXIT, NULL);
				goto step_w;
			case XJ_GO_OFFLINE:
				if(jbc != NULL)
					jbc->expire = ltime = -1;
				goto step_w;
			case XJ_SEND_SUBSCRIBE:
			case XJ_SEND_BYE:
			default:
				break;
		}
		
		if(jbc != NULL)
		{
			DBG("XJAB:xj_worker:%d: connection already exists"
				" for <%s> ...\n", _xj_pid, buff);
			xj_jcon_update(jbc, jwl->cachet);
			goto step_z;
		}
		
		// NO OPEN CONNECTION FOR THIS SIP ID
		DBG("XJAB:xj_worker:%d: new connection for <%s>.\n",
			_xj_pid, buff);
		
		if(db_query(db_con, keys, vals, col, 2, 2, NULL, &res) != 0 ||
			RES_ROW_N(res) <= 0)
		{
			DBG("XJAB:xj_worker:%d: no database result\n", _xj_pid);
			xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to, 
				jwl->contact_h, XJ_DMSG_ERR_JGWFORB, NULL);
			
			goto step_v;
		}
		
		jbc = xj_jcon_init(jaddress, jport);
		
		if(xj_jcon_connect(jbc))
		{
			DBG("XJAB:xj_worker:%d: Cannot connect"
				" to the Jabber server ...\n", _xj_pid);
			xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to, 
				jwl->contact_h, XJ_DMSG_ERR_NOJSRV, NULL);

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
			
			xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to,
				jwl->contact_h, XJ_DMSG_ERR_JAUTH, NULL);
			
			xj_jcon_free(jbc);
			goto step_v;
		}
		
		if(xj_jcon_set_attrs(jbc, jsmsg->jkey, jwl->cachet, jwl->delayt)
			|| xj_jcon_pool_add(jcp, jbc))
		{
			DBG("XJAB:xj_worker:%d: Keeping connection to Jabber server"
				" failed! Not enough memory ...\n", _xj_pid);
			xj_jcon_disconnect(jbc);
			xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to, jwl->contact_h,	
				XJ_DMSG_ERR_JGWFULL, NULL);
			xj_jcon_free(jbc);
			goto step_v;
		}
								
		/** add socket descriptor to select */
		DBG("XJAB:xj_worker:%d: add connection on <%d> \n", _xj_pid, jbc->sock);
		if(jbc->sock > maxfd)
			maxfd = jbc->sock;
		FD_SET(jbc->sock, &set);
										
		xj_jcon_get_roster(jbc);
		xj_jcon_send_presence(jbc, NULL, NULL, "Online", "9");
		
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

		if(jsmsg->type == XJ_GO_ONLINE)
			goto step_w;
		
step_z:
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
						xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to, 
							jwl->contact_h, XJ_DMSG_ERR_JOINJCONF,
							&jbc->jkey->flag);
						goto step_w;
					}
				}
				flag |= XJ_ADDRTR_CON;
			}
			else
			{
				// unable to get the conference 
				// --- send back to SIP user a msg
				xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to, 
					jwl->contact_h, XJ_DMSG_ERR_NEWJCONF, &jbc->jkey->flag);
				goto step_w;
			}
		}
		if(jsmsg->type == XJ_JOIN_JCONF)
			goto step_w;
		
		// here will come only XJ_SEND_MESSAGE
		switch(xj_jcon_is_ready(jbc,jsmsg->to.s,jsmsg->to.len,jwl->aliases->dlm))
		{
			case 0:
				DBG("XJAB:xj_worker:%d: SENDING THE MESSAGE TO JABBER"
					" NETWORK ...\n", _xj_pid);
				/*** address corection ***/
				sto.s = buff; 
				sto.len = 0;
				flag |= XJ_ADDRTR_A2B;
				if(xj_address_translation(&jsmsg->to, &sto, jwl->aliases, 
							flag) == 0)
				{
					if(xj_jcon_send_msg(jbc, sto.s, sto.len,
						jsmsg->msg.s, jsmsg->msg.len,
						(flag&XJ_ADDRTR_CON)?XJ_JMSG_GROUPCHAT:XJ_JMSG_CHAT)<0)
							
						xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to,
							jwl->contact_h, XJ_DMSG_ERR_SENDJMSG,
							&jbc->jkey->flag);
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
					xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to,
						jwl->contact_h, XJ_DMSG_ERR_STOREJMSG,
						&jbc->jkey->flag);
					goto step_w;
				}
				else // skip freeing the SIP message - now is in queue
					goto step_y;
	
			case 2:
				xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to,
						jwl->contact_h, XJ_DMSG_ERR_NOREGIM,
						&jbc->jkey->flag);
				goto step_w;
			case 3: // not joined to Jabber conference
				xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to,
						jwl->contact_h, XJ_DMSG_ERR_NOTJCONF,
						&jbc->jkey->flag);
				goto step_w;
				
			default:
				xj_send_sip_msgz(jsmsg->jkey->id, &jsmsg->to,
						jwl->contact_h, XJ_DMSG_ERR_SENDJMSG, &jbc->jkey->flag);
				goto step_w;
		}

step_v: // error connecting to Jabber server
		
		// cleaning jab_wlist
		xj_wlist_del(jwl, jsmsg->jkey, _xj_pid);

		// cleaning db_query
		if ((res != NULL) && (db_free_query(db_con,res) < 0))
		{
			DBG("XJAB:xj_worker:%d:Error while freeing"
				" SQL result - worker terminated\n", _xj_pid);
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
					xj_send_sip_msgz(jcp->ojc[i]->jkey->id, &jab_gw_name, 
						jwl->contact_h, XJ_DMSG_ERR_DISCONNECTED,
						&jbc->jkey->flag);
					// make sure that will ckeck expired connections
					ltime = jcp->ojc[i]->expire = -1;
					FD_CLR(jcp->ojc[i]->sock, &set);
					goto step_xx;
				}
				DBG("XJAB:xj_worker:%d: received: %dbytes Err:%d/EA:%d\n", 
						_xj_pid, nr, errno, EAGAIN);		
				xj_jcon_update(jcp->ojc[i], jwl->cachet);

				if(nr>0)
					p[nr] = 0;
				nr = strlen(recv_buff);
				pos = 0;

				DBG("XJAB:xj_worker: JMSG START ----------\n%.*s\n"
					" JABBER: JMSGL:%d END ----------\n", nr, recv_buff, nr);

			
			} while(xj_manage_jab(recv_buff, nr, &pos, jwl->contact_h,
					jwl->aliases, jcp->ojc[i]) == 9	&& main_loop);
	
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
step_xx:
		if(ltime < 0 || ltime + jwl->sleept <= get_ticks())
		{
			ltime = get_ticks();
			DBG("XJAB:xj_worker:%d: scanning for expired connection\n",
				_xj_pid);
			for(i = 0; i < jcp->len && main_loop; i++)
			{
				if(jcp->ojc[i] == NULL)
					continue;
				if(jcp->ojc[i]->jkey->flag==0 &&
					jcp->ojc[i]->expire > ltime)
					continue;
				
				DBG("XJAB:xj_worker:%d: connection expired for"
					" <%.*s> \n", _xj_pid, jcp->ojc[i]->jkey->id->len,
					jcp->ojc[i]->jkey->id->s);

				xj_send_sip_msgz(jcp->ojc[i]->jkey->id,  &jab_gw_name,
					jwl->contact_h, XJ_DMSG_INF_JOFFLINE, NULL);

				DBG("XJAB:xj_worker:%d: connection's close flag =%d\n",
						_xj_pid, jcp->ojc[i]->jkey->flag);
				// CLEAN JAB_WLIST
				xj_wlist_del(jwl, jcp->ojc[i]->jkey, _xj_pid);

				// looking for open conference rooms
				DBG("XJAB:xj_worker:%d: having %d open"
					" conferences\n", _xj_pid, jcp->ojc[i]->nrjconf);
				while(jcp->ojc[i]->nrjconf > 0)
				{
					if((jcf=delpos234(jcp->ojc[i]->jconf,0))!=NULL)
					{
						// get out of room
						xj_jcon_jconf_presence(jcp->ojc[i],jcf,
								"unavailable", NULL);
						xj_jconf_free(jcf);
					}
					jcp->ojc[i]->nrjconf--;
				}
				FD_CLR(jcp->ojc[i]->sock, &set);
				xj_jcon_disconnect(jcp->ojc[i]);
				xj_jcon_free(jcp->ojc[i]);
				jcp->ojc[i] = NULL;
			}
		}
	} // END while

	DBG("XJAB:xj_worker:%d: cleaning procedure\n", _xj_pid);

	return 0;
} // end xj_worker_process

/**
 * parse incoming message from Jabber server
 */
int xj_manage_jab(char *buf, int len, int *pos, 
		str *sct, xj_jalias als, xj_jcon jbc)
{
	int i, err=0;
	char *p, *to, *from, *msg, *type, *emsg, *ecode, lbuf[4096], fbuf[128];
	xj_jconf jcf = NULL;
	str ts, tf;
	xode x, y, z;
	str *sid;

	if(!jbc)
		return -1;

	sid = jbc->jkey->id;	
	x = xode_from_strx(buf, len, &err, &i);
	DBG("XJAB:xj_parse_jab: XODE ret:%d pos:%d\n", err, i);
	
	if(err && pos != NULL)
		*pos= i;
	if(x == NULL)
		return -1;
	
	lbuf[0] = 0;
	ecode = NULL;
	
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
				ecode = xode_get_attrib(y, "code");
				if(ecode != NULL)
					sprintf(lbuf, 
						"{Error (%s/%s) when trying to send following messge}",
						ecode, emsg);
				else
					sprintf(lbuf, 
						"{Error (%s) when trying to send following messge}",
						emsg);
			}

		}

		// is from a conferece?!?!
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
						&& !strncasecmp(p+1, jcf->nick.s, jcf->nick.len))
						goto ready;
					sprintf(lbuf, "[%s] ", p+1);
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
	
			if(xj_send_sip_msg(sid, &jcf->uri, sct, &ts, &jbc->jkey->flag)<0)
				DBG("XJAB:xj_manage_jab: ERROR SIP MESSAGE was not sent!\n");
			else
				DBG("XJAB:xj_manage_jab: SIP MESSAGE was sent!\n");
			goto ready;
		}

		strcat(lbuf, msg);
		ts.s = from;
		ts.len = strlen(from);
		tf.s = fbuf;
		tf.len = 0;
		if(xj_address_translation(&ts, &tf, als, XJ_ADDRTR_B2A) == 0)
		{
			ts.s = lbuf;
			ts.len = strlen(lbuf);
	
			if(xj_send_sip_msg(sid, &tf, sct, &ts, &jbc->jkey->flag) < 0)
				DBG("XJAB:xj_manage_jab: ERROR SIP MESSAGE was not sent ...\n");
			else
				DBG("XJAB:xj_manage_jab: SIP MESSAGE was sent.\n");
		}
		goto ready;
	} // end MESSAGE
	
	/*** PRESENCE HANDLING ***/
	if(!strncasecmp(xode_get_name(x), "presence", 8))
	{
		DBG("XJAB:xj_manage_jab: jabber [presence] received\n");
		if(!jbc)
			goto ready;
		type = xode_get_attrib(x, "type");
		from = xode_get_attrib(x, "from");
		if(from == NULL)
			goto ready;
		if(type!=NULL && !strncasecmp(type, "error", 5))
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
					xj_send_sip_msgz(sid, &tf, sct, XJ_DMSG_ERR_JCONFNICK,
					&jbc->jkey->flag);
					goto ready;
				}
				xj_send_sip_msgz(sid, &tf, sct, XJ_DMSG_ERR_JCONFREFUSED,
						&jbc->jkey->flag);
			}

			goto ready;
		}
		if(type!=NULL && !strncasecmp(type, "subscribe", 9))
		{
			xj_jcon_send_presence(jbc, from, "subscribed", NULL, NULL);
			goto ready;
		}
		if(type == NULL || !strncasecmp(type, "online", 6))
		{
			if(strchr(from, '@') == NULL)
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
			else if((jcf=xj_jcon_check_jconf(jbc, from))!=NULL)
			{
				jcf->status = XJ_JCONF_READY;
				DBG("XJAB:xj_manage_jab: %s conference ready\n", from);
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
	DBG("XJAB:xj_worker:%d: SIGNAL received=%d\n **************", _xj_pid, s);
}

/*****************************     ****************************************/

/**
 * send a SIP MESSAGE message
 * - to : destination
 * - from : origin
 * - contact : contact header
 * - msg : body of the message
 * #return : 0 on success or <0 on error
 */
int xj_send_sip_msg(str *to, str *from, str *contact, str *msg, int *cbp)
{
	str  msg_type = { "MESSAGE", 7};
	char buf[512];
	str  tfrom;
	str  str_hdr;
	int **pcbp = NULL, beg, end, crt;
	char buf1[1024];

	if( !to || !to->s || to->len <= 0 
			|| !from || !from->s || from->len <= 0 
			|| !msg || !msg->s || msg->len <= 0
			|| (cbp && *cbp!=0) )
		return -1;

	// from correction
	beg = crt = 0;
	end = -1;
	while(crt < from->len && from->s[crt]!='@')
	{
		if(from->s[crt]=='%')
		{
			beg = end + 1;
			end = crt;
		}
		crt++;
	}
	if(end > 0)
		sprintf(buf, "\"%.*s\" <sip:%.*s>", end-beg, from->s+beg,
					from->len, from->s);
	else
		sprintf(buf, "<sip:%.*s>", from->len, from->s);
		
	tfrom.len = strlen(buf);
	tfrom.s = buf;
	
	// building Contact and Content-Type
	strcpy(buf1,"Content-Type: text/plain"CRLF"Contact: ");
	str_hdr.len = 24 + CRLF_LEN + 9;
	if(contact != NULL && contact->len > 2) 
	{
		strncat(buf1,contact->s,contact->len);
		str_hdr.len += contact->len;
	}
	else 
	{
		strncat(buf1,tfrom.s,tfrom.len);
		str_hdr.len += tfrom.len;
	}
	strcat(buf1, CRLF);
	str_hdr.len += CRLF_LEN;
	str_hdr.s = buf1;
	if(cbp)
	{
		DBG("XJAB:xj_send_sip_msg: uac callback parameter [%p==%d]\n",cbp,*cbp);
		if((pcbp = (int**)shm_malloc(sizeof(int*))) == NULL)
			return -1;
		*pcbp = cbp;
		return tmb.t_uac( &msg_type, to, &str_hdr , msg, &tfrom,
					xj_tuac_callback, (void*)pcbp, 0);
	}
	else
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
int xj_send_sip_msgz(str *to, str *from, str *contact, char *msg, int *cbp)
{
	str tstr;
	int n;

	if(!to || !from || !msg || (cbp && *cbp!=0))
		return -1;

	tstr.s = msg;
	tstr.len = strlen(msg);
	if((n = xj_send_sip_msg(to, from, contact, &tstr, cbp)) < 0)
		DBG("JABBER: jab_send_sip_msgz: ERROR SIP MESSAGE wasn't sent to"
			" [%.*s]...\n", tstr.len, tstr.s);
	else
		DBG("JABBER: jab_send_sip_msgz: SIP MESSAGE was sent to [%.*s]...\n",
			to->len, to->s);
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
	s_lock_at(jwl->sems, idx);
	while((p=(xj_jkey)delpos234(jwl->workers[idx].sip_ids, 0))!=NULL)
	{
		if(fl)
		{
			DBG("XJAB:xj_wlist_send_info: sending disconnect message"
				" to <%.*s>\n",	p->id->len, p->id->s);
			xj_send_sip_msgz(p->id, &jab_gw_name, NULL, 
				XJ_DMSG_INF_DISCONNECTED, NULL);
		}
		jwl->workers[idx].nr--;
		xj_jkey_free_p(p);
	}
	s_unlock_at(jwl->sems, idx);
	return 0;
}


/**
 *
 */
void xj_tuac_callback( struct cell *t, struct sip_msg *msg,
			int code, void *param)
{
	DBG("XJAB: xj_tuac_callback: completed with status %d\n", code);
	if(!t->cbp)
	{
		DBG("XJAB: m_tuac_callback: parameter not received\n");
		return;
	}
	DBG("XJAB: xj_tuac_callback: parameter [%p : ex-value=%d]\n", t->cbp,
					*(*((int**)t->cbp)) );
	if(code < 200 || code >= 300)
	{
		DBG("XJAB: xj_tuac_callback: no 2XX return code - connection set"
			" as expired \n");
		*(*((int**)t->cbp)) = 1;	
	}
}

/*****************************     ****************************************/


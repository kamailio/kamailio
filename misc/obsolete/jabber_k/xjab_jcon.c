/*
 * $Id$
 *
 * eXtended JABber module - functions used for SIP 2 JABBER communication
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
 */


#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<sys/un.h>
#include <string.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../timer.h"

#include "xjab_jcon.h"
#include "xjab_util.h"
#include "xode.h"
#include "mdefines.h"

#define JB_ID_BASE	"SJ"
#define JB_START_STREAM		"<?xml version='1.0'?>"
#define JB_START_STREAM_LEN	21

#define JB_CLIENT_OPEN_STREAM	"<stream:stream to='%s' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>"

#define JB_IQ_ROSTER_GET	"<iq type='get'><query xmlns='jabber:iq:roster'/></iq>"

#define XJ_MAX_JCONF	12


/**
 * init a JABBER connection
 */
xj_jcon xj_jcon_init(char *hostname, int port)
{
	xj_jcon jbc = NULL;
	if(hostname==NULL || strlen(hostname)<=0)
		return NULL;
	
	jbc = (xj_jcon)_M_MALLOC(sizeof(struct _xj_jcon));
	if(jbc == NULL)
		return NULL;
	jbc->sock=-1;
    jbc->port = port;
    jbc->juid = -1;
	jbc->seq_nr = 0;
    jbc->hostname = (char*)_M_MALLOC(strlen(hostname)+1);
	if(jbc->hostname == NULL)
	{
		_M_FREE(jbc);
		return NULL;
	}
    strcpy(jbc->hostname, hostname);
	jbc->allowed = jbc->ready = XJ_NET_NUL;
	jbc->jconf = NULL;
	jbc->nrjconf = 0;
	jbc->plist = xj_pres_list_init();
	if(jbc->plist == NULL)
	{
		_M_FREE(jbc->hostname);
		_M_FREE(jbc);
		return NULL;
	}
			
    return jbc;
}

/**
 * connect to JABBER server
 */
int xj_jcon_connect(xj_jcon jbc)
{
    struct sockaddr_in address;
    struct hostent *he;
    int sock;

    // open connection to server
    if((sock = socket(AF_INET, SOCK_STREAM, 0))<0)
    {
    	LM_DBG("failed to create the socket\n");
        return -1;
    }
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("socket [%d]\n", sock);
#endif
    he=gethostbyname(jbc->hostname);
    if(he == NULL)
    {
    	LM_DBG("failed to get info about Jabber server address\n");
        return -1;
    }

    // fill the fields of the address
    memcpy(&address.sin_addr, he->h_addr, he->h_length);
    address.sin_family=AF_INET;
    address.sin_port=htons(jbc->port);

    // try to connect with Jabber server
    if (connect(sock, (struct sockaddr *)&address, sizeof(address))<0)
    {
    	LM_DBG("failed to connect with Jabber server\n");
        return -1;
    }
    jbc->sock = sock;

    return 0;
}

/**
 * set the Jabber internal ID
 */
void xj_jcon_set_juid(xj_jcon jbc, int _juid)
{
	if(jbc == NULL)
		return;
	jbc->juid = _juid;
}

/**
 * return the Jabber internal ID
 */
int  xj_jcon_get_juid(xj_jcon jbc)
{
	if(jbc == NULL)
		return -1;
	return jbc->juid;
}

/**
 * disconnect from JABBER server
 */
int xj_jcon_disconnect(xj_jcon jbc)
{
	if(jbc == NULL || jbc->sock < 0)
		return -1;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("-----START-----\n");
    LM_DBG("socket [%d]\n", jbc->sock);
#endif
	xj_jcon_send_presence(jbc, NULL, "unavailable", NULL, NULL);
	if(send(jbc->sock, "</stream:stream>", 16, 0) < 16)
		LM_DBG("failed to close the stream\n");
	if(close(jbc->sock) == -1)
		LM_DBG("failed to close the socket\n");
	jbc->sock = -1;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("-----END-----\n");
#endif
	return 0;
}

/**
 * authentication to the JABBER server
 */
int xj_jcon_user_auth(xj_jcon jbc, char *username, char *passwd,
				char *resource)
{
	char msg_buff[4096];
	int n, i, err;
	char *p0, *p1;
	xode x, y, z;

	/*** send open stream tag **/
	sprintf(msg_buff, JB_CLIENT_OPEN_STREAM, jbc->hostname);
	if(send(jbc->sock, msg_buff, strlen(msg_buff), 0) != strlen(msg_buff))
		goto error;
	
	n = recv(jbc->sock, msg_buff, 4096, 0);
	msg_buff[n] = 0;
	if(strncasecmp(msg_buff, JB_START_STREAM, JB_START_STREAM_LEN))
		goto error;
	
	p0 = strstr(msg_buff + JB_START_STREAM_LEN, "id='");
	if(p0 == NULL)
		goto error;

	p0 += 4;
	p1 = strchr(p0, '\'');
	if(p1 == NULL)
		goto error;

	jbc->stream_id = (char*)_M_MALLOC(p1-p0+1);
	strncpy(jbc->stream_id, p0, p1-p0);
    jbc->stream_id[p1-p0] = 0;

	sprintf(msg_buff, "%08X", jbc->seq_nr);
	
	x = xode_new_tag("iq");
	if(!x)
		return -1;

	xode_put_attrib(x, "id", msg_buff);
	xode_put_attrib(x, "type", "get");
	y = xode_insert_tag(x, "query");
	xode_put_attrib(y, "xmlns", "jabber:iq:auth");
	z = xode_insert_tag(y, "username");
	xode_insert_cdata(z, username, -1);
	p0 = xode_to_str(x);
	n = strlen(p0);

	i = send(jbc->sock, p0, n, 0);
	if(i != n)
		goto errorx;
	
	xode_free(x);
	
	// receive  response
	// try 10 times
	i = 10;
	while(i)
	{
		if((n = recv(jbc->sock, msg_buff, 4096, 0)) > 0)
		{
			msg_buff[n] = 0;
			break;
		}
		usleep(1000);
		i--;
	}
	if(!i)
		goto error;

	x = xode_from_strx(msg_buff, n, &err, &i);
	p0 = msg_buff;
	if(err)
		p0 += i; 
	
	if(strncasecmp(xode_get_name(x), "iq", 2))
		goto errorx;
	
	if((x = xode_get_tag(x, "query?xmlns=jabber:iq:auth")) == NULL)
		goto errorx;
			
	y = xode_new_tag("query");
	xode_put_attrib(y, "xmlns", "jabber:iq:auth");
	z = xode_insert_tag(y, "username");
	xode_insert_cdata(z, username, -1);
	z = xode_insert_tag(y, "resource");
	xode_insert_cdata(z, resource, -1);
	
	if(xode_get_tag(x, "digest") != NULL)
	{ // digest authentication
			
		//sprintf(msg_buff, "%s%s", jbc->stream_id, passwd);
		strcpy(msg_buff, jbc->stream_id);
		strcat(msg_buff, passwd);
		//LM_DBG("[%s:%s]\n", jbc->stream_id, passwd);
		p1 = shahash(msg_buff);

		z = xode_insert_tag(y, "digest");
		xode_insert_cdata(z, p1, -1);
	}
	else
	{ // plaint text authentication
		z = xode_insert_tag(y, "password");
		xode_insert_cdata(z, passwd, -1);
	}

	y = xode_wrap(y, "iq");
	
	jbc->seq_nr++;
	sprintf(msg_buff, "%08X", jbc->seq_nr);
	
	xode_put_attrib(y, "id", msg_buff);
	xode_put_attrib(y, "type", "set");
	
	p1 = xode_to_str(y);
	n = strlen(p1);
	
	i = send(jbc->sock, p1, n, 0);
	if(i != n)
	{
		xode_free(y);
		goto errorx;
	}
	xode_free(x);
	xode_free(y);
	
	// receive  response
	// try 10 times
	i = 10;
	while(i)
	{
		if((n = recv(jbc->sock, msg_buff, 4096, 0)) > 0)
		{
			msg_buff[n] = 0;
			break;
		}
		usleep(1000);
		i--;
	}
	if(!i)
		goto error;

	x = xode_from_strx(msg_buff, n, &err, &i);
	p0 = msg_buff;
	if(err)
		p0 += i; 
	
	if(strncasecmp(xode_get_name(x), "iq", 2) || 
			strncasecmp(xode_get_attrib(x, "type"), "result", 6))
		goto errorx;
	
	jbc->resource = (char*)_M_MALLOC(strlen(resource)+1);
	strcpy(jbc->resource, resource);

	jbc->allowed = XJ_NET_ALL;
	jbc->ready = XJ_NET_JAB;	
	return 0;
	
errorx:
	xode_free(x);
error:
	return -1;
}

/**
 * receive the list of the roster
 */
int xj_jcon_get_roster(xj_jcon jbc)
{
	int n = strlen(JB_IQ_ROSTER_GET);
	if(send(jbc->sock, JB_IQ_ROSTER_GET, n, 0) != n)
		return -1;
	return 0;
}

/**
 * add a new contact in user's roster
 */
int xj_jcon_set_roster(xj_jcon jbc, char* jid, char *type)
{
	xode x;
	char *p;
	int n;
	char buff[16];
	
	if(!jbc || !jid)
		return -1;
	
	x = xode_new_tag("item");
	if(!x)
		return -1;
	xode_put_attrib(x, "jid", jid);
	if(type != NULL)
		xode_put_attrib(x, "subscription", type);
	
	x = xode_wrap(x, "query");
	xode_put_attrib(x, "xmlns", "jabber:iq:roster");

	x = xode_wrap(x, "iq");
	
	xode_put_attrib(x, "type", "set");
	jbc->seq_nr++;
	sprintf(buff, "%08X", jbc->seq_nr);
	xode_put_attrib(x, "id", buff);

	p = xode_to_str(x);
	n = strlen(p);
	
	if(send(jbc->sock, p, n, 0) != n)
	{
		LM_DBG("item not sent\n");
		goto error;
	}
	xode_free(x);
	return 0;
error:
	xode_free(x);
	return -1;
}

/**
 * send a message through a JABBER connection
 * params are pairs (buffer, len)
 */
int xj_jcon_send_msg(xj_jcon jbc, char *to, int tol, char *msg, 
		int msgl, int type)
{
	char msg_buff[4096], *p;
	int n;
	xode x;
	
	if(jbc == NULL)
		return -1;
	
	x = xode_new_tag("body");
	if(!x)
		return -1;
	
	xode_insert_cdata(x, msg, msgl);
	x = xode_wrap(x, "message");
	strncpy(msg_buff, to, tol);
	msg_buff[tol] = 0;
	xode_put_attrib(x, "to", msg_buff);
	switch(type)
	{
		case XJ_JMSG_CHAT:
			xode_put_attrib(x, "type", "chat");
			break;
		case XJ_JMSG_GROUPCHAT:
			xode_put_attrib(x, "type", "groupchat");
			break;
		default:
			xode_put_attrib(x, "type", "normal");
	}

	p = xode_to_str(x);
	n = strlen(p);
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("jabber msg:\n%s\n", p);
#endif	
	if(send(jbc->sock, p, n, 0) != n)
	{
		LM_DBG(" message not sent\n");
		goto error;
	}
	xode_free(x);
	return 0;
error:
	xode_free(x);
	return -1;
}

/**
 * receive a message from a JABBER connection
 * NOT USED
 */
int xj_jcon_recv_msg(xj_jcon jbc, char *from, char *msg)
{
	//char msg_buff[4096];
	//sprintf(msg_buff, JB_MSG_NORMAL, to, msg);
	//send(jbc->sock, msg_buff, strlen(msg_buff), 0);

	return 0;
}

/**
 * send presence
 * type - "unavailable", "subscribe", "subscribed" ....
 * status - "online", "away", "unavailable" ...
 * priority - "0", "1", ...
 */
int xj_jcon_send_presence(xj_jcon jbc, char *sto, char *type, char *status,
				char *priority)
{
	char *p;
	int n;
	xode x, y;
	
	if(jbc == NULL)
		return -1;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("-----START-----\n");
#endif	
	x = xode_new_tag("presence");
	if(!x)
		return -1;
	
	if(sto != NULL)
		xode_put_attrib(x, "to", sto);
	if(type != NULL)
		xode_put_attrib(x, "type", type);
	if(status != NULL)
	{
		y = xode_insert_tag(x, "status");
		xode_insert_cdata(y, status, strlen(status));
	}
	if(priority != NULL)
	{
		y = xode_insert_tag(x, "priority");
		xode_insert_cdata(y, priority, strlen(priority));
	}	
	
	p = xode_to_str(x);
	n = strlen(p);
	
	if(send(jbc->sock, p, n, 0) != n)
	{
		LM_DBG("presence not sent\n");
		goto error;
	}
	xode_free(x);
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("presence status was sent\n");
#endif
	return 0;
error:
	xode_free(x);
	return -1;
}

/**
 * send subscribe for user's presence
 */
int xj_jcon_send_subscribe(xj_jcon jbc, char *to, char *from, char *type)
{
	char *p;
	int n;
	xode x;
	
	if(!jbc || !to)
		return -1;
	
	x = xode_new_tag("presence");
	if(!x)
		return -1;

	xode_put_attrib(x, "to", to);
	if(from != NULL)
		xode_put_attrib(x, "from", from);
	if(type != NULL)
		xode_put_attrib(x, "type", type);

	p = xode_to_str(x);
	n = strlen(p);
	
	if(send(jbc->sock, p, n, 0) != n)
	{
		LM_DBG("subscribe not sent\n");
		goto error;
	}
	xode_free(x);
	return 0;

error:
	xode_free(x);
	return -1;
}

/**
 * free the allocated memory space of a JABBER connection
 */
int xj_jcon_free(xj_jcon jbc)
{
	xj_jconf jcf;
	
	if(jbc == NULL)
		return -1;

#ifdef XJ_EXTRA_DEBUG
	LM_DBG("-----START-----\n");
#endif
	//if(jbc->sock != -1)
	//	jb_disconnect(jbc);

	if(jbc->hostname != NULL)
		_M_FREE(jbc->hostname);
	if(jbc->stream_id != NULL)
		_M_FREE(jbc->stream_id);
	
	if(jbc->resource != NULL)
		_M_FREE(jbc->resource);
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("%d conferences\n", jbc->nrjconf);
#endif
	while(jbc->nrjconf > 0)
	{
		if((jcf=delpos234(jbc->jconf,0))!=NULL)
			xj_jconf_free(jcf);
		jbc->nrjconf--;
	}
	xj_pres_list_free(jbc->plist);
	_M_FREE(jbc);
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("-----END-----\n");
#endif
	return 0;
}

/**
 * create a open connection to Jabber
 * - id : id of the connection
 * - jbc : pointer to Jabber connection
 * - cache_time : life time of the connection
 * - delay_time : time needed to became an active connection
 * return : pointer to the structure or NULL on error
 */
int xj_jcon_set_attrs(xj_jcon jbc, xj_jkey jkey, int cache_time, 
				int delay_time)
{
	int t;
	if(jbc==NULL || jkey==NULL || jkey->id==NULL || jkey->id->s==NULL)
		return -1;
	jbc->jkey = jkey;
	t = get_ticks();
	jbc->expire = t + cache_time;
	jbc->ready = t + delay_time;
	return 0;
}

/**
 * update the life time of the connection
 * - ojc : pointer to the open connection
 * - cache_time : number of seconds to keep the connection open
 * return : 0 on success or <0 on error
 */
int xj_jcon_update(xj_jcon jbc, int cache_time)
{
	if(jbc == NULL)
		return -1;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("params [%.*s] %d\n", 
			jbc->jkey->id->len, jbc->jkey->id->s, cache_time);
#endif
	jbc->expire = get_ticks() + cache_time;
	return 0;	
}

int xj_jcon_is_ready(xj_jcon jbc, char *to, int tol, char dl)
{
	char *p;
	str sto;
	xj_jconf jcf = NULL;
	if(!jbc || !to || tol <= 0)
		return -1;
	
	sto.s = to;
	sto.len = tol;
	if(!xj_jconf_check_addr(&sto, dl))
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("destination=conference\n");
#endif		
		if((jcf=xj_jcon_get_jconf(jbc, &sto, dl))!=NULL)
			return (jcf->status & XJ_JCONF_READY)?0:3;
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("conference does not exist\n");
#endif
		return -1;
	}
	
	p = to;
	while(p < to+tol && *p!='@') 
		p++;
	if(p>=to+tol)
		return -1;
	p++;
	if(!strncasecmp(p, XJ_AIM_NAME, XJ_AIM_LEN))
		return (jbc->ready & XJ_NET_AIM)?0:((jbc->allowed & XJ_NET_AIM)?1:2);
	
	if(!strncasecmp(p, XJ_ICQ_NAME, XJ_ICQ_LEN))
		return (jbc->ready & XJ_NET_ICQ)?0:((jbc->allowed & XJ_NET_ICQ)?1:2);
	
	if(!strncasecmp(p, XJ_MSN_NAME, XJ_MSN_LEN))
		return (jbc->ready & XJ_NET_MSN)?0:((jbc->allowed & XJ_NET_MSN)?1:2);

	if(!strncasecmp(p, XJ_YAH_NAME, XJ_YAH_LEN))
		return (jbc->ready & XJ_NET_YAH)?0:((jbc->allowed & XJ_NET_YAH)?1:2);
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("destination=jabber\n");
#endif
	return 0;
}

xj_jconf  xj_jcon_get_jconf(xj_jcon jbc, str* sid, char dl)
{
	xj_jconf jcf = NULL, p;

	if(!jbc || !sid || !sid->s || sid->len <= 0)
		return NULL;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("looking for conference\n");	
#endif	
	if((jcf = xj_jconf_new(sid))==NULL)
		return NULL;
	if(xj_jconf_init_sip(jcf, jbc->jkey->id, dl))
		goto clean;
	if(jbc->nrjconf && (p = find234(jbc->jconf, (void*)jcf, NULL)) != NULL)
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("conference found\n");
#endif
		xj_jconf_free(jcf);
		return p;
	}
	
	if(jbc->nrjconf >= XJ_MAX_JCONF)
		goto clean;

	if(jbc->nrjconf==0)
		if(jbc->jconf==NULL)
			if((jbc->jconf = newtree234(xj_jconf_cmp)) == NULL)
				goto clean;

	if((p = add234(jbc->jconf, (void*)jcf)) != NULL)
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("new conference created\n");
#endif
		jbc->nrjconf++;
		return p;
	}

clean:
	LM_DBG("conference not found\n");
	xj_jconf_free(jcf);
	return NULL;
}

xj_jconf xj_jcon_check_jconf(xj_jcon jbc, char* id)
{
	str sid;
	xj_jconf jcf = NULL, p = NULL;

	if(!jbc || !id || !jbc->nrjconf)
		return NULL;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("conference not found\n");
#endif	
	sid.s = id;
	sid.len = strlen(id);
	if((jcf = xj_jconf_new(&sid))==NULL)
		return NULL;
	if(xj_jconf_init_jab(jcf))
		goto clean;
	if((p = find234(jbc->jconf, (void*)jcf, NULL)) != NULL)
	{
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("conference found\n");
#endif
		xj_jconf_free(jcf);
		return p;
	}
clean:
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("conference not found\n");
#endif
	xj_jconf_free(jcf);
	return NULL;	
}

int xj_jcon_jconf_presence(xj_jcon jbc, xj_jconf jcf, char* type, 
		char* status)
{
	char buff[256];
	
	strncpy(buff, jcf->room.s, 
			jcf->room.len + jcf->server.len +1);
	buff[jcf->room.len + jcf->server.len +1] = '/';
	buff[jcf->room.len + jcf->server.len +2] = 0;
	buff[jcf->room.len] = '@';
	strncat(buff, jcf->nick.s, jcf->nick.len);

	return xj_jcon_send_presence(jbc,buff,type,status,NULL);
}

int  xj_jcon_del_jconf(xj_jcon jbc, str *sid, char dl, int flag)
{
	xj_jconf jcf = NULL, p = NULL;
	
	if(!jbc || !sid || !sid->s || sid->len <= 0)
		return -1;
#ifdef XJ_EXTRA_DEBUG
	LM_DBG("deleting conference of <%.*s>\n", sid->len, sid->s);
#endif	
	if((jcf = xj_jconf_new(sid))==NULL)
		return -1;
	if(xj_jconf_init_sip(jcf, jbc->jkey->id, dl))
	{
		xj_jconf_free(jcf);
		return -1;
	}
	
	p = del234(jbc->jconf, (void*)jcf);

	if(p != NULL)
	{
		if(flag == XJ_JCMD_UNSUBSCRIBE)
			xj_jcon_jconf_presence(jbc, jcf, "unavailable", NULL);
		jbc->nrjconf--;
		xj_jconf_free(p);
#ifdef XJ_EXTRA_DEBUG
		LM_DBG("conference deleted\n");
#endif
	}

	xj_jconf_free(jcf);

	return 0;
}

/**********    *********/


/*
 * $Id$
 *
 * JABBER module - functions used for SIP 2 JABBER communication
 *
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

#include "sip2jabber.h"
#include "xml_jab.h"
#include "mdefines.h"

#define JB_ID_BASE	"SJ"

#define JB_CLIENT_OPEN_STREAM	"<stream:stream to='%s' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>"

#define JB_IQ_AUTH_REQ			"<iq id='%s%d' type='get'><query xmlns='jabber:iq:auth'><username>%s</username></query></iq>"

#define JB_IQ_AUTH_PL_SEND		"<iq id='%s%d' type='set'><query xmlns='jabber:iq:auth'><username>%s</username><password>%s</password><resource>%s</resource></query></iq>"

#define JB_IQ_AUTH_DG_SEND		"<iq id='%s%d' type='set'><query xmlns='jabber:iq:auth'><username>%s</username><digest>%s</digest><resource>%s</resource></query></iq>"

#define JB_IQ_ROSTER_GET		"<iq type='get'><query xmlns='jabber:iq:roster'/></iq>"

#define JB_MSG_NORMAL			"<message to='%s' type='normal'><body>%s</body></message>"

#define JB_MSG_CHAT				"<message to='%s' type='chat'><body>%s</body></message>"


/**
 * init a JABBER connection
 */
jbconnection jb_init_jbconnection(char *hostname, int port)
{
	jbconnection jbc = (jbconnection)_M_MALLOC(sizeof(struct _jbconnection));
	if(jbc == NULL)
		return NULL;
	jbc->sock=-1;
    jbc->port = port;
    jbc->juid = -1;
	jbc->seq_nr = 0;
    jbc->hostname = (char*)_M_MALLOC(strlen(hostname));
	if(jbc->hostname == NULL)
	{
		_M_FREE(jbc);
		return NULL;
	}
    strcpy(jbc->hostname, hostname);
    return jbc;
}

/**
 * connect to JABBER server
 */
int jb_connect_to_server(jbconnection jbc)
{
    struct sockaddr_in address;
    struct hostent *he;
    int sock;

    // open connection to server
    if((sock = socket(AF_INET, SOCK_STREAM, 0))<0)
    {
    	_M_PRINTF("S2JB: Error to create the socket\n");
        return -1;
    }
    DBG("JABBER: JB_CONNECT_TO_SERVER: socket [%d]\n", sock);
    he=gethostbyname(jbc->hostname);
    if(he == NULL)
    {
    	_M_PRINTF("S2JB: Error getting info about Jabber server address\n");
        return -1;
    }

    // fill the fields of the address
    memcpy(&address.sin_addr, he->h_addr, he->h_length);
    address.sin_family=AF_INET;
    address.sin_port=htons(jbc->port);

    // try to connect with JCI server
    if (connect(sock, (struct sockaddr *)&address, sizeof(address))<0)
    {
    	_M_PRINTF("S2JB: Error to connect with Jabber server\n");
        return -1;
    }
    jbc->sock = sock;

    return 0;
}

/**
 * set the Jabber internal ID
 */
void jb_set_juid(jbconnection jbc, int _juid)
{
	if(jbc == NULL)
		return;
	jbc->juid = _juid;
}

/**
 * return the Jabber internal ID
 */
int  jb_get_juid(jbconnection jbc)
{
	if(jbc == NULL)
		return -1;
	return jbc->juid;
}

/**
 * disconnect from JABBER server
 */
int jb_disconnect(jbconnection jbc)
{
	if(jbc == NULL || jbc->sock < 0)
		return -1;
	DBG("JABBER: JB_DISCONNECT ----------\n");
    DBG("JABBER: JB_DISCONNECT: socket [%d]\n", jbc->sock);
	jb_send_presence(jbc, "unavailable", NULL, NULL);
	if(send(jbc->sock, "</stream:stream>", 16, 0) < 16)
		DBG("JABBER: JB_DISCONNECT: error closing stream\n");
	if(close(jbc->sock) == -1)
		DBG("JABBER: JB_DISCONNECT: error closing socket\n");
	jbc->sock = -1;
	DBG("JABBER: JB_DISCONNECT --END--\n");
	return 0;
}

/**
 * authentication to the JABBER server
 */
int jb_user_auth_to_server(jbconnection jbc, char *username, char *passwd,
				char *resource)
{
	char msg_buff[4096];
	int n;
	char *p0, *p1;

	sprintf(msg_buff, JB_CLIENT_OPEN_STREAM, jbc->hostname);
	send(jbc->sock, msg_buff, strlen(msg_buff), 0);
	n = recv(jbc->sock, msg_buff, 4096, 0);
	msg_buff[n] = 0;
	p0 = strstr(msg_buff, "id='");
	if(p0 == NULL)
		return -1;

	p0 += 4;
	p1 = strchr(p0, '\'');
	if(p1 == NULL)
		return -2;

	jbc->stream_id = (char*)_M_MALLOC(p1-p0+1);
	strncpy(jbc->stream_id, p0, p1-p0);
    jbc->stream_id[p1-p0] = 0;

	sprintf(msg_buff, JB_IQ_AUTH_REQ, JB_ID_BASE, jbc->seq_nr, username);
	send(jbc->sock, msg_buff, strlen(msg_buff), 0);

	// receive  response
	while(1)
	{
		n = recv(jbc->sock, msg_buff, 4096, 0);
		msg_buff[n] = 0;
		if(strncmp(msg_buff, "<iq ", 4) == 0)
			break;
	}
	//if((p0 = strstr(msg_buff, "<password/>")) == NULL)
	//	return -1;

	jbc->seq_nr++;
	if((p0 = strstr(msg_buff, "<digest/>")) != NULL)
	{ // digest authentication
		sprintf(msg_buff, "%s%s", jbc->stream_id, passwd);
		DBG("JABBER: JB_USER_AUTH_TO_SERVER: [%s:%s]\n", jbc->stream_id, passwd);
		p0 = shahash(msg_buff);
		sprintf(msg_buff, JB_IQ_AUTH_DG_SEND, JB_ID_BASE, jbc->seq_nr, username,
						p0, resource);
	}
	else
	{ // plaint text authentication
		sprintf(msg_buff, JB_IQ_AUTH_PL_SEND, JB_ID_BASE, jbc->seq_nr, username,
						passwd, resource);
	}

	send(jbc->sock, msg_buff, strlen(msg_buff), 0);
	// receive  response
	while(1)
	{
		n = recv(jbc->sock, msg_buff, 4096, 0);
		msg_buff[n] = 0;
		if(strncmp(msg_buff, "<iq ", 4) == 0)
			break;
	}

	p0 = strstr(msg_buff, "type='error'");
	if(p0 != NULL)
		return -3;
		
	jbc->resource = (char*)_M_MALLOC(strlen(resource));
	strcpy(jbc->resource, resource);
	
	return 0;
}

/**
 * receive the list of the roster
 */
int jb_get_roster(jbconnection jbc)
{
	char msg_buff[4096];
	int n;
	DBG("JABBER: JB_GET_ROSTER -------\n");
	send(jbc->sock, JB_IQ_ROSTER_GET, strlen(JB_IQ_ROSTER_GET), 0);
	n = recv(jbc->sock, msg_buff, 4096, 0);
	msg_buff[n] = 0;
	return 0;
}

/**
 * send a message through a JABBER connection
 * params are pairs (buffer, len)
 */
int jb_send_msg(jbconnection jbc, char *to, int tol, char *msg, int msgl)
{
	char msg_buff[4096], *p;
	int i, l;

	strcpy(msg_buff, "<message to='");
	strncat(msg_buff, to, tol);
	strcat(msg_buff, "' type='chat'><body>");
    //strcat(msg_buff, "' type='normal'><body>");

	l = strlen(msg_buff);
	p = msg_buff + l;
	if((i = xml_escape(msg, msgl, p, 4096-l)) < 0)
	{
		DBG("JABBER: JB_SEND_MSG: error: message not sent"
			" - output buffer too small\n");
		return -2;
	}

	if(l+i > 4076)
	{
		DBG("JABBER: JB_SEND_MSG: error: message not sent"
			" - output buffer too small\n");
		return -2;
	}
	strcat(msg_buff, "</body></message>");

    //sprintf(msg_buff, JB_MSG_NORMAL, to, msg);
	//sprintf(msg_buff, JB_MSG_CHAT, to, msg);
	i = strlen(msg_buff);
	if(send(jbc->sock, msg_buff, i, 0) < i)
	{
		DBG("JABBER: JB_SEND_MSG: error: message not sent\n");
		return -2;
    }

	return 0;
}

/**
 * send a signed message through a JABBER connection
 * params are pairs (buffer, len)
 */
int jb_send_sig_msg(jbconnection jbc, char *to, int tol, char *msg, int msgl,
				char *sig, int sigl)
{
	char msg_buff[4096], *p;
	int i, l;

	strcpy(msg_buff, "<message to='");
	strncat(msg_buff, to, tol);
	strcat(msg_buff, "' type='chat'><body>");
    // strcat(msg_buff, "' type='normal'><body>");

	l = strlen(msg_buff);
	p = msg_buff + l;
	if((i = xml_escape(msg, msgl, p, 4096-l)) < 0)
	{
		DBG("JABBER: JB_SEND_SIG_MSG: error: message not sent"
			" - output buffer too small\n");
		return -2;
	}
	

	strncat(msg_buff, "\n[From:  ", 8);

	l = strlen(msg_buff);
	p = msg_buff + l;
	if((i=xml_escape(sig, sigl, p, 4096-l)) < 0)
	{
		DBG("JABBER: JB_SEND_SIG_MSG: error: message not sent"
			" -- output buffer too small\n");
		return -2;
	}

	strcat(msg_buff, "]</body></message>");

	//sprintf(msg_buff, JB_MSG_NORMAL, to, msg);
    //sprintf(msg_buff, JB_MSG_CHAT, to, msg);
	i = strlen(msg_buff);
	if(send(jbc->sock, msg_buff, i, 0) < i)
	{
		DBG("JABBER: JB_SEND_SIG_MSG: error: message not sent\n");
		return -2;
    }
	return 0;
}

/**
 * receive a message from a JABBER connection
 */
int jb_recv_msg(jbconnection jbc, char *from, char *msg)
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
int jb_send_presence(jbconnection jbc, char *type, char *status,
				char *priority)
{
	char msg_buff[4096];
	int n;
	if(jbc == NULL)
		return -1;
	DBG("JABBER: JB_SEND_PRESENCE -------\n");
	strcpy(msg_buff, "<presence");
	if(type != NULL)
	{
		strcat(msg_buff, " type='");
		strcat(msg_buff, type);
		strcat(msg_buff, "'");
	}
	if(status == NULL && priority == NULL )
	{
		strcat(msg_buff, "/>");
	}
	else
	{
		strcat(msg_buff, ">");
		if(status != NULL)
		{
			strcat(msg_buff, "<status>");
			strcat(msg_buff, status);
			strcat(msg_buff, "</status>");
		}
		if(priority != NULL)
		{
			strcat(msg_buff, "<priority>");
			strcat(msg_buff, priority);
			strcat(msg_buff, "</priority>");
		}

		strcat(msg_buff, "</presence>");
	}

	//sprintf(msg_buff, JB_PRESENCE, status, priority);
	n = strlen(msg_buff);
	if(send(jbc->sock, msg_buff, n, 0) < n)
	{
		DBG("JABBER: JB_SEND_PRESENCE: error: presence not sent\n");
		return -2;
	}

	return 0;
}

/**
 * free the allocated memory space of a JABBER connection
 */
int jb_free_jbconnection(jbconnection jbc)
{
	if(jbc == NULL)
		return -1;
	DBG("JABBER: JB_FREE_JBCONNECTION ----------\n");
	//if(jbc->sock != -1)
	//	jb_disconnect(jbc);
		
	if(jbc->hostname != NULL)
		_M_FREE(jbc->hostname);
	if(jbc->stream_id != NULL)
		_M_FREE(jbc->stream_id);
    /*******
	if(jbc->username != NULL)
		_M_FREE(jbc->username);
	if(jbc->passwd != NULL)
		_M_FREE(jbc->passwd);
	*/
	if(jbc->resource != NULL)
		_M_FREE(jbc->resource);

	_M_FREE(jbc);
	DBG("JABBER: JB_FREE_JBCONNECTION ---END---\n");
	return 0;
}


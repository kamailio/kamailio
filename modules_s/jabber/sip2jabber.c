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

#include "sip2jabber.h"
#include "mdefines.h"

#define JB_ID_BASE	"SJ"

#define JB_CLIENT_OPEN_STREAM	"<stream:stream to='%s' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>"
#define JB_IQ_AUTH_REQ	"<iq id='%s%d' type='get'><query xmlns='jabber:iq:auth'><username>%s</username></query></iq>"
#define JB_IQ_AUTH_PL_SEND	"<iq id='%s%d' type='set'><query xmlns='jabber:iq:auth'><username>%s</username><password>%s</password><resource>%s</resource></query></iq>"
#define JB_IQ_AUTH_DG_SEND	"<iq id='%s%d' type='set'><query xmlns='jabber:iq:auth'><username>%s</username><digest>%s</digest><resource>%s</resource></query></iq>"
#define JB_IQ_ROSTER_GET	"<iq type='get'><query xmlns='jabber:iq:roster'/></iq>"
#define JB_MSG_NORMAL	"<message to='%s' type='normal'><body>%s</body></message>"
#define JB_MSG_CHAT		"<message to='%s' type='chat'><body>%s</body></message><message to='%s' type='chat'><body>%s</body></message>"


#define _msg_transform(_dst, _src, _len, _i) \
	for((_i)=0; (_i) < (_len); (_i)++) \
	{ \
		if(*((_src)+(_i)) == '<') \
		{ \
			*(_dst)++ = '&'; \
			*(_dst)++ = 'l'; \
			*(_dst)++ = 't'; \
			*(_dst)++ = ';'; \
		} \
		else \
		{ \
			if(*((_src)+(_i)) == '>') \
			{ \
				*(_dst)++ = '&'; \
				*(_dst)++ = 'g'; \
				*(_dst)++ = 't'; \
				*(_dst)++ = ';'; \
			} \
			else \
			{ \
				if(*((_src)+(_i)) == '&') \
				{ \
					*(_dst)++ = '&'; \
					*(_dst)++ = 'a'; \
					*(_dst)++ = 'm'; \
					*(_dst)++ = 'p'; \
					*(_dst)++ = ';'; \
				} \
				else \
				{ \
					*(_dst)++ = *((_src)+(_i)); \
				} \
			} \
		} \
	} \
	*(_dst) = 0;

/**
 * init a JABBER connection
 */
jbconnection jb_init_jbconnection(char *hostname, int port)
{
	jbconnection jbc = (jbconnection)_M_MALLOC(sizeof(struct _jbconnection));
	jbc->sock=-1;
    jbc->port = port;
	jbc->seq_nr = 0;
    jbc->hostname = strdup(hostname);
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
    he=gethostbyname(jbc->hostname);
    if(he == NULL)
    {
    	_M_PRINTF("S2JB: Error to get the information about Jabber server address\n");
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
 * disconnect from JABBER server
 */
int jb_disconnect(jbconnection jbc)
{
	jb_send_presence(jbc, "unavailable", NULL, NULL);
	send(jbc->sock, "</stream:stream>", 16, 0);
	close(jbc->sock);
	return 0;
}

/**
 * authentication to the JABBER server
 */
int jb_user_auth_to_server(jbconnection jbc, char *username, char *passwd, char *resource)
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
		p0 = shahash(msg_buff);
		sprintf(msg_buff, JB_IQ_AUTH_DG_SEND, JB_ID_BASE, jbc->seq_nr, username, p0, resource);
	}
	else
	{ // plaint text authentication
		sprintf(msg_buff, JB_IQ_AUTH_PL_SEND, JB_ID_BASE, jbc->seq_nr, username, passwd, resource);
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

	jbc->username = strdup(resource);
	jbc->passwd = strdup(resource);
	jbc->resource = strdup(resource);

	return 0;
}

/**
 * receive the list of the roster
 */
int jb_get_roster(jbconnection jbc)
{
	char msg_buff[4096];
	int n;
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
	int i;

	strcpy(msg_buff, "<message to='");
	strncat(msg_buff, to, tol);
	strcat(msg_buff, "' type='normal'><body>");

	p = msg_buff + strlen(msg_buff);
	_msg_transform(p, msg, msgl, i);

	strcat(msg_buff, "</body></message>");

	//sprintf(msg_buff, JB_MSG_NORMAL, to, msg);
	send(jbc->sock, msg_buff, strlen(msg_buff), 0);

	return 0;
}

/**
 * send a signed message through a JABBER connection
 */
int jb_send_sig_msg(jbconnection jbc, char *to, int tol, char *msg, int msgl, char *sig, int sigl)
{
	char msg_buff[4096], *p;
	int i;

	strcpy(msg_buff, "<message to='");
	strncat(msg_buff, to, tol);
	strcat(msg_buff, "' type='normal'><body>");

	p = msg_buff + strlen(msg_buff);
	_msg_transform(p, msg, msgl, i);

	strncat(msg_buff, "\n[From:  ", 8);

	p = msg_buff + strlen(msg_buff);
	_msg_transform(p, sig, sigl, i);

	strcat(msg_buff, "]</body></message>");

	//sprintf(msg_buff, JB_MSG_NORMAL, to, msg);
	send(jbc->sock, msg_buff, strlen(msg_buff), 0);

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
 * status - "online", "away", "unavailable" ...
 */
int jb_send_presence(jbconnection jbc, char *type, char *status, char *priority)
{
	//JB_PRESENCE		"<presence type='%s'><status>%s</status><priority>%d</priority></presence>"
	char msg_buff[4096];
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
	send(jbc->sock, msg_buff, strlen(msg_buff), 0);

	return 0;
}

/**
 * free the allocated memory space of a JABBER connection
 */
int jb_free_jbconnection(jbconnection jbc)
{
	if(jbc->hostname != NULL)
		_M_FREE(jbc->hostname);
	if(jbc->stream_id != NULL)
		_M_FREE(jbc->stream_id);

	if(jbc->username != NULL)
		_M_FREE(jbc->username);
	if(jbc->passwd != NULL)
		_M_FREE(jbc->passwd);
	if(jbc->resource != NULL)
		_M_FREE(jbc->resource);

	if(jbc != NULL)
		_M_FREE(jbc);
	return 0;
}
/*
 * Digest Authentication - Diameter support
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>

/* memory management */
#include "../../core/mem/mem.h"

/* printing messages, dealing with strings and other utils */
#include "../../core/dprint.h"
#include "../../core/str.h"

/* headers defined by this module */
#include "auth_diameter.h"
#include "defs.h"
#include "tcp_comm.h"
#include "diameter_msg.h"

#define MAX_TRIES	10

/* it initializes the TCP connection */ 
int init_mytcp(char* host, int port)
{
	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *server;
    
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	
    if (sockfd < 0) 
	{
		LM_ERR("error creating the socket\n");
		return -1;
	}	
	
    server = gethostbyname(host);
    if (server == NULL) 
	{
		LM_ERR("error finding the host\n");
		close(sockfd);
		return -1;
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = PF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr,
					server->h_length);
    serv_addr.sin_port = htons(port);
	
    if (connect(sockfd, (const struct sockaddr *)&serv_addr, 
							sizeof(serv_addr)) < 0) 
	{
        LM_ERR("error connecting to the DIAMETER client\n");
		close(sockfd);
		return -1;
	}	

	return sockfd;
}



void reset_read_buffer(rd_buf_t *rb)
{
	rb->ret_code		= 0;
	rb->chall_len		= 0;
	if(rb->chall)
		pkg_free(rb->chall);
	rb->chall			= 0;

	rb->first_4bytes	= 0;
	rb->buf_len			= 0;
	if(rb->buf)
		pkg_free(rb->buf);
	rb->buf				= 0;
}

/* read from a socket, an AAA message buffer */
int do_read( int socket, rd_buf_t *p)
{
	unsigned char  *ptr;
	unsigned int   wanted_len, len;
	int n;

	if (p->buf==0)
	{
		wanted_len = sizeof(p->first_4bytes) - p->buf_len;
		ptr = ((unsigned char*)&(p->first_4bytes)) + p->buf_len;
	}
	else
	{
		wanted_len = p->first_4bytes - p->buf_len;
		ptr = p->buf + p->buf_len;
	}

	while( (n=recv( socket, ptr, wanted_len, MSG_DONTWAIT ))>0 ) 
	{
//		LM_DBG("(sock=%d)  -> n=%d (expected=%d)\n", p->sock,n,wanted_len);
		p->buf_len += n;
		if (n<wanted_len)
		{
			//LM_DBG("only %d bytes read from %d expected\n",n,wanted_len);
			wanted_len -= n;
			ptr += n;
		}
		else 
		{
			if (p->buf==0)
			{
				/* I just finished reading the first 4 bytes from msg */
				len = ntohl(p->first_4bytes)&0x00ffffff;
				if (len<AAA_MSG_HDR_SIZE || len>MAX_AAA_MSG_SIZE)
				{
					LM_ERR(" (sock=%d): invalid message "
						"length read %u (%x)\n", socket, len, p->first_4bytes);
					goto error;
				}
				//LM_DBG("message length = %d(%x)\n",len,len);
				if ( (p->buf=pkg_malloc(len))==0  )
				{
					LM_ERR("no more pkg memory\n");
					goto error;
				}
				*((unsigned int*)p->buf) = p->first_4bytes;
				p->buf_len = sizeof(p->first_4bytes);
				p->first_4bytes = len;
				/* update the reading position and len */
				ptr = p->buf + p->buf_len;
				wanted_len = p->first_4bytes - p->buf_len;
			}
			else
			{
				/* I finished reading the whole message */
				LM_DBG("(sock=%d): whole message read (len=%d)!\n",
					socket, p->first_4bytes);
				return CONN_SUCCESS;
			}
		}
	}

	if (n==0)
	{
		LM_INFO("(sock=%d): FIN received\n", socket);
		return CONN_CLOSED;
	}
	if ( n==-1 && errno!=EINTR && errno!=EAGAIN )
	{
		LM_ERR(" (sock=%d): n=%d , errno=%d (%s)\n",
			socket, n, errno, strerror(errno));
		goto error;
	}
error:
	return CONN_ERROR;
}


/* send a message over an already opened TCP connection */
int tcp_send_recv(int sockfd, char* buf, int len, rd_buf_t* rb, 
					unsigned int waited_id)
{
	int n, number_of_tries;
	fd_set active_fd_set, read_fd_set;
	struct timeval tv;
	unsigned long int result_code;
	AAAMessage *msg;
	AAA_AVP	*avp;
	char serviceType;
	unsigned int m_id;

	/* try to write the message to the Diameter client */
	while( (n=write(sockfd, buf, len))==-1 ) 
	{
		if (errno==EINTR)
			continue;
		LM_ERR("write returned error: %s\n", strerror(errno));
		return AAA_ERROR;
	}

	if (n!=len) 
	{
		LM_ERR("write gave no error but wrote less than asked\n");
		return AAA_ERROR;
	}

	/* wait for the answer a limited amount of time */
	tv.tv_sec = MAX_WAIT_SEC;
	tv.tv_usec = MAX_WAIT_USEC;

	/* Initialize the set of active sockets. */
	FD_ZERO (&active_fd_set);
	FD_SET (sockfd, &active_fd_set);
	number_of_tries = 0;

	while(number_of_tries<MAX_TRIES)
	{
		read_fd_set = active_fd_set;
		if (select (sockfd+1, &read_fd_set, NULL, NULL, &tv) < 0)
		{
			LM_ERR("select function failed\n");
			return AAA_ERROR;
		}
/*
		if (!FD_ISSET (sockfd, &read_fd_set))
		{
			LM_ERR("no response message received\n");
//			return AAA_ERROR;
		}
*/
		/* Data arriving on a already-connected socket. */
		reset_read_buffer(rb);
		switch( do_read(sockfd, rb) )
		{
			case CONN_ERROR:
				LM_ERR("error when trying to read from socket\n");
				return AAA_CONN_CLOSED;
			case CONN_CLOSED:
				LM_ERR("connection closed by diameter client!\n");
				return AAA_CONN_CLOSED;
		}
		
		/* obtain the structure corresponding to the message */
		msg = AAATranslateMessage(rb->buf, rb->buf_len, 0);	
		if(!msg)
		{
			LM_ERR("message structure not obtained\n");	
			return AAA_ERROR;
		}
		avp = AAAFindMatchingAVP(msg, NULL, AVP_SIP_MSGID,
								vendorID, AAA_FORWARD_SEARCH);
		if(!avp)
		{
			LM_ERR("AVP_SIP_MSGID not found\n");
			return AAA_ERROR;
		}
		m_id = *((unsigned int*)(avp->data.s));
		LM_DBG("######## m_id=%d\n", m_id);
		if(m_id!=waited_id)
		{
			number_of_tries ++;
			LM_NOTICE("old message received\n");
			continue;
		}
		goto next;
	}

	LM_ERR("too many old messages received\n");
	return AAA_TIMEOUT;
next:
	/* Finally die correct answer */
	avp = AAAFindMatchingAVP(msg, NULL, AVP_Service_Type,
							vendorID, AAA_FORWARD_SEARCH);
	if(!avp)
	{
		LM_ERR("AVP_Service_Type not found\n");
		return AAA_ERROR;
	}
	serviceType = avp->data.s[0];

	result_code = ntohl(*((unsigned long int*)(msg->res_code->data.s)));
	switch(result_code)
	{
		case AAA_SUCCESS:					/* 2001 */
			rb->ret_code = AAA_AUTHORIZED;
			break;
		case AAA_AUTHENTICATION_REJECTED:	/* 4001 */
			if(serviceType!=SIP_AUTH_SERVICE)
			{
				rb->ret_code = AAA_NOT_AUTHORIZED;
				break;
			}
			avp = AAAFindMatchingAVP(msg, NULL, AVP_Challenge,
							vendorID, AAA_FORWARD_SEARCH);
			if(!avp)
			{
				LM_ERR("AVP_Response not found\n");
				rb->ret_code = AAA_SRVERR;
				break;
			}
			rb->chall_len=avp->data.len;
			rb->chall = (unsigned char*)pkg_malloc(avp->data.len*sizeof(unsigned char));
			if(rb->chall == NULL)
			{
				LM_ERR("no more pkg memory\n");
				rb->ret_code = AAA_SRVERR;
				break;
			}
			memcpy(rb->chall, avp->data.s, avp->data.len);
			rb->ret_code = AAA_CHALENGE;
			break;
		case AAA_AUTHORIZATION_REJECTED:	/* 5003 */
			rb->ret_code = AAA_NOT_AUTHORIZED;
			break;
		default:							/* error */
			rb->ret_code = AAA_SRVERR;
	}
	
    return rb->ret_code;	
}
void close_tcp_connection(int sfd)
{
	shutdown(sfd, 2);
}



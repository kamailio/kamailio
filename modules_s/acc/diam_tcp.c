/*
 * $Id$
 * Accounting module
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

#ifdef DIAM_ACC

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../mem/mem.h"

#include "diam_message.h"
#include "diam_tcp.h"
#include "diam_dict.h"

#define M_NAME "acc"

/* TCP connection setup */ 
int init_mytcp(char* host, int port)
{
	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *server;
    
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	
    if (sockfd < 0) 
	{
		LOG(L_ERR, M_NAME":init_mytcp(): error creating the socket\n");
		return -1;
	}	
	
    server = gethostbyname(host);
    if (server == NULL) 
	{
		LOG(L_ERR, M_NAME":init_mytcp(): error finding the host\n");
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
        LOG(L_ERR, M_NAME":init_mytcp(): error connecting to the "
						"DIAMETER client\n");
		return -1;
	}	

	return sockfd;
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
		LOG(L_ERR, M_NAME": write returned error: %s\n", strerror(errno));
		return AAA_ERROR;
	}

	if (n!=len) 
	{
		LOG(L_ERR, M_NAME": write gave no error but wrote less than asked\n");
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
			LOG(L_ERR, M_NAME":tcp_send_msg(): select function failed\n");
			return AAA_ERROR;
		}

/*		if (!FD_ISSET (sockfd, &read_fd_set))
		{
			LOG(L_ERR, M_NAME":tcp_send_rcv(): no response received\n");
//			return AAA_ERROR;
		}
*/		/* Data arriving on a already-connected socket. */
		reset_read_buffer(rb);
		switch( do_read(sockfd, rb) )
		{
			case CONN_ERROR:
				LOG(L_ERR, M_NAME": error when trying to read from socket\n");
				return AAA_CONN_CLOSED;
			case CONN_CLOSED:
				LOG(L_ERR, M_NAME": error when trying to read from socket\n");
				return AAA_CONN_CLOSED;
		}
		
		/* obtain the structure corresponding to the message */
		msg = AAATranslateMessage(rb->buf, rb->buf_len, 0);	
		if(!msg)
		{
			LOG(L_ERR, M_NAME": message structure not obtained\n");	
			return AAA_ERROR;
		}
		avp = AAAFindMatchingAVP(msg, NULL, AVP_SIP_MSGID,
								vendorID, AAA_FORWARD_SEARCH);
		if(!avp)
		{
			LOG(L_ERR, M_NAME": AVP_SIP_MSGID not found\n");
			return AAA_ERROR;
		}
		m_id = *((unsigned int*)(avp->data.s));
		DBG("######## m_id=%d\n", m_id);
		if(m_id!=waited_id)
		{
			number_of_tries ++;
			LOG(L_NOTICE, M_NAME": old message received\n");
			continue;
		}
		goto next;
	}

	LOG(L_ERR, M_NAME": too many old messages received\n");
	return AAA_TIMEOUT;
next:

	/* endlich die richtige Antworte */
	avp = AAAFindMatchingAVP(msg, NULL, AVP_Service_Type,
							vendorID, AAA_FORWARD_SEARCH);
	if(!avp)
	{
		LOG(L_ERR, M_NAME": AVP_Service_Type not found\n");
		return AAA_ERROR;
	}
	serviceType = avp->data.s[0];

	result_code = ntohl(*((unsigned long int*)(msg->res_code->data.s)));
	switch(result_code)
	{
		case AAA_SUCCESS:					/* 2001 */
			return ACC_SUCCESS;
		default:							/* error */
			return ACC_FAILURE;
	}
}

void reset_read_buffer(rd_buf_t *rb)
{
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
//		DBG("DEBUG:do_read (sock=%d)  -> n=%d (expected=%d)\n",
//			p->sock,n,wanted_len);
		p->buf_len += n;
		if (n<wanted_len)
		{
			//DBG("only %d bytes read from %d expected\n",n,wanted_len);
			wanted_len -= n;
			ptr += n;
		}
		else 
		{
			if (p->buf==0)
			{
				/* I just finished reading the the first 4 bytes from msg */
				len = ntohl(p->first_4bytes)&0x00ffffff;
				if (len<AAA_MSG_HDR_SIZE || len>MAX_AAA_MSG_SIZE)
				{
					LOG(L_ERR,"ERROR:do_read (sock=%d): invalid message "
						"length read %u (%x)\n", socket, len, p->first_4bytes);
					goto error;
				}
				//DBG("message length = %d(%x)\n",len,len);
				if ( (p->buf=pkg_malloc(len))==0  )
				{
					LOG(L_ERR,"ERROR:do_read: no more free memory\n");
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
				DBG("DEBUG:do_read (sock=%d): whole message read (len=%d)!\n",
					socket, p->first_4bytes);
				return CONN_SUCCESS;
			}
		}
	}

	if (n==0)
	{
		LOG(L_INFO,"INFO:do_read (sock=%d): FIN received\n", socket);
		return CONN_CLOSED;
	}
	if ( n==-1 && errno!=EINTR && errno!=EAGAIN )
	{
		LOG(L_ERR,"ERROR:do_read (sock=%d): n=%d , errno=%d (%s)\n",
			socket, n, errno, strerror(errno));
		goto error;
	}
error:
	return CONN_ERROR;
}


void close_tcp_connection(int sfd)
{
	shutdown(sfd, 2);
}

/* 
 * Extract URI depending on the request from To or From header 
 */
int get_uri(struct sip_msg* m, str** uri)
{
	if ((REQ_LINE(m).method.len == 8) && 
					(memcmp(REQ_LINE(m).method.s, "REGISTER", 8) == 0)) 
	{/* REGISTER */
		if (!m->to && (parse_headers(m, HDR_TO, 0) == -1)/* || !m->to*/) 
		{
			LOG(L_ERR, M_NAME": To header field not found or malformed\n");
			return -1;
		}
		*uri = &(get_to(m)->uri);
	} 
	else 
	{
		if (parse_from_header(m) == -1) 
		{
			LOG(L_ERR, M_NAME": Error while parsing headers\n");
			return -2;
		}
		*uri = &(get_from(m)->uri);
	}
	return 0;
}


#endif

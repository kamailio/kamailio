/*
 * $Id$
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "forward.h"
#include "config.h"
#include "parser/msg_parser.h"
#include "route.h"
#include "dprint.h"
#include "udp_server.h"
#include "globals.h"
#include "data_lump.h"
#include "ut.h"
#include "mem/mem.h"
#include "msg_translator.h"
#include "sr_module.h"
#include "stats.h"
#include "ip_addr.h"
#include "resolve.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif



/* returns a socket_info pointer to the sending socket or 0 on error
 * params: destination socke_union pointer
 */
struct socket_info* get_send_socket(union sockaddr_union* to)
{
	struct socket_info* send_sock;
	
	send_sock=0;
	/* check if we need to change the socket (different address families -
	 * eg: ipv4 -> ipv6 or ipv6 -> ipv4) */
	if (to->s.sa_family!=bind_address->address.af){
		switch(to->s.sa_family){
			case AF_INET:	send_sock=sendipv4;
							break;
#ifdef USE_IPV6
			case AF_INET6:	send_sock=sendipv6;
							break;
#endif
			default:		LOG(L_ERR, "get_send_socket: BUG: don't know how"
									" to forward to af %d\n", to->s.sa_family);
		}
	}else send_sock=bind_address;
	return send_sock;
}



int forward_request( struct sip_msg* msg, struct proxy_l * p)
{
	unsigned int len;
	char* buf;
	union sockaddr_union* to;
	struct socket_info* send_sock;
	
	to=0;
	buf=0;
	
	to=(union sockaddr_union*)malloc(sizeof(union sockaddr_union));
	if (to==0){
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: forward_request: out of memory\n");
		goto error;
	}
	
	
	/* if error try next ip address if possible */
	if (p->ok==0){
		if (p->host.h_addr_list[p->addr_idx+1])
			p->addr_idx++;
		else p->addr_idx=0;
		p->ok=1;
	}
	
	hostent2su(to, &p->host, p->addr_idx, 
				(p->port)?htons(p->port):htons(SIP_PORT));
	p->tx++;
	p->tx_bytes+=len;
	

	send_sock=get_send_socket(to);
	if (send_sock==0){
		LOG(L_ERR, "forward_req: ERROR: cannot forward to af %d "
				"no coresponding listening socket\n", to->s.sa_family);
		ser_error=E_NO_SOCKET;
		goto error;
	}
	
	buf = build_req_buf_from_sip_req( msg, &len, send_sock);
	if (!buf){
		LOG(L_ERR, "ERROR: forward_reply: building failed\n");
		goto error;
	}
	 /* send it! */
	DBG("Sending:\n%s.\n", buf);
	DBG("orig. len=%d, new_len=%d\n", msg->len, len );
	
	if (udp_send(send_sock, buf, len,  to,
							sizeof(union sockaddr_union))==-1){
			ser_error=E_SEND;
			p->errors++;
			p->ok=0;
			STATS_TX_DROPS;
			goto error;
	}
	/* sent requests stats */
	else STATS_TX_REQUEST(  msg->first_line.u.request.method_value );
	pkg_free(buf);
	free(to);
	/* received_buf & line_buf will be freed in receiv_msg by free_lump_list*/
	return 0;
error:
	if (buf) pkg_free(buf);
	if (to) free(to);
	return -1;
}


int update_sock_struct_from_via( union sockaddr_union* to,
								 struct via_body* via )
{
	struct hostent* he;
	char *host_copy;


#ifdef DNS_IP_HACK
	int err;
	unsigned int ip;

	ip=str2ip((unsigned char*)via->host.s,via->host.len,&err);
	if (err==0){
		to->sin.sin_family=AF_INET;
		to->sin.sin_port=(via->port)?htons(via->port): htons(SIP_PORT);
		memcpy(&to->sin.sin_addr, (char*)&ip, 4);
	}else
#endif
	{
		/* we do now a malloc/memcpy because gethostbyname loves \0-terminated 
		   strings; -jiri 
		   but only if host is not null terminated
		   (host.s[len] will always be ok for a via)
           BTW: when is via->host.s non null terminated? tm copy?
		   - andrei 
			Yes -- it happened on generating a 408 by TM; -jiri
		*/
		if (via->host.s[via->host.len]){
			if (!(host_copy=pkg_malloc( via->host.len+1 ))) {
				LOG(L_NOTICE, "ERROR: update_sock_struct_from_via: not enough memory\n");
				return -1;
			}
			memcpy(host_copy, via->host.s, via->host.len );
			host_copy[via->host.len]=0;
			he=resolvehost(host_copy);
			pkg_free( host_copy );
		}else{
			he=resolvehost(via->host.s);
		}

		if (he==0){
			LOG(L_NOTICE, "ERROR:forward_reply:gethostbyname(%s) failure\n",
					via->host.s);
			return -1;
		}
		hostent2su(to, he, 0, (via->port)?htons(via->port): htons(SIP_PORT));
	}
	return 1;
}


/* removes first via & sends msg to the second */
int forward_reply(struct sip_msg* msg)
{
	int  r;
	char* new_buf;
	union sockaddr_union* to;
	struct socket_info* send_sock;
	unsigned int new_len;
	struct sr_module *mod;
	
	to=0;
	new_buf=0;
	/*check if first via host = us */
	if (check_via){
		for (r=0; r<sock_no; r++)
			if ( (msg->via1->host.len==sock_info[r].name.len) && 
					(memcmp(msg->via1->host.s, sock_info[r].name.s, 
										sock_info[r].name.len)==0) )
				break;
		if (r==sock_no){
			LOG(L_NOTICE, "ERROR: forward_reply: host in first via!=me :"
					" %.*s\n", msg->via1->host.len, msg->via1->host.s);
			/* send error msg back? */
			goto error;
		}
	}

	/* quick hack, slower for mutliple modules*/
	for (mod=modules;mod;mod=mod->next){
		if ((mod->exports) && (mod->exports->response_f)){
			DBG("forward_reply: found module %s, passing reply to it\n",
					mod->exports->name);
			if (mod->exports->response_f(msg)==0) goto skip;
		}
	}
	
	/* we have to forward the reply stateless, so we need second via -bogdan*/
	if ((msg->via2==0) || (msg->via2->error!=VIA_PARSE_OK))
	{
		/* no second via => error */
		LOG(L_ERR, "ERROR: forward_msg: no 2nd via found in reply\n");
		goto error;
	}

	to=(union sockaddr_union*)malloc(sizeof(union sockaddr_union));
	if (to==0){
		LOG(L_ERR, "ERROR: forward_reply: out of memory\n");
		goto error;
	}

	new_buf = build_res_buf_from_sip_res( msg, &new_len);
	if (!new_buf){
		LOG(L_ERR, "ERROR: forward_reply: building failed\n");
		goto error;
	}

	if (update_sock_struct_from_via( to, msg->via2 )==-1) goto error;
	send_sock=get_send_socket(to);
	if (send_sock==0){
		LOG(L_ERR, "forward_reply: ERROR: no sending socket found\n");
		goto error;
	}

	if (udp_send(send_sock, new_buf,new_len,  to,
				sizeof(union sockaddr_union))==-1)
	{
		STATS_TX_DROPS;
		goto error;
	} else {
#ifdef STATS
		int j = msg->first_line.u.reply.statuscode/100;
		STATS_TX_RESPONSE(  j );
#endif
	}

	DBG(" reply forwarded to %s:%d\n",msg->via2->host.s,
		(unsigned short) msg->via2->port);

	pkg_free(new_buf);
	free(to);
skip:
	return 0;
error:
	if (new_buf) pkg_free(new_buf);
	if (to) free(to);
	return -1;
}

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
#include "hash_func.h"
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
#include "name_alias.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif



/* returns a socket_info pointer to the sending socket or 0 on error
 * params: destination socket_union pointer
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



/* checks if the host is one of the address we listen on
* returns 1 if true, 0 if false, -1 on error
*/
int check_self(str* host)
{
	int r;
	
	for (r=0; r<sock_no; r++){
		DBG("check_self - checking if host==us: %d==%d && "
				" [%.*s] == [%.*s]\n", 
					host->len,
					sock_info[r].name.len,
					host->len, host->s,
					sock_info[r].name.len, sock_info[r].name.s
			);
		if ( (host->len==sock_info[r].name.len) && 
	#ifdef USE_IPV6
			(strncasecmp(host->s, sock_info[r].name.s,
								 sock_info[r].name.len)==0) /*slower*/
	#else
			(memcmp(host->s, sock_info[r].name.s, 
								sock_info[r].name.len)==0)
	#endif
			)
			break;
	/* check if host == ip address */
		if ( 	(!sock_info[r].is_ip) &&
				(host->len==sock_info[r].address_str.len) && 
	#ifdef USE_IPV6
			(strncasecmp(host->s, sock_info[r].address_str.s,
								 sock_info[r].address_str.len)==0) /*slower*/
	#else
			(memcmp(host->s, sock_info[r].address_str.s, 
								sock_info[r].address_str.len)==0)
	#endif
			)
			break;
	}
	if (r==sock_no){
		/* try to look into the aliases*/
		if (grep_aliases(host->s, host->len)==0){
			DBG("check_self: host != me\n");
			return 0;
		}
	}
	return 1;
}



int forward_request( struct sip_msg* msg, struct proxy_l * p)
{
	unsigned int len;
	char* buf;
	union sockaddr_union* to;
	struct socket_info* send_sock;
	char md5[MD5_LEN];
	
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
		goto error1;
	}

	/* calculate branch for outbound request;  if syn_branch is turned off,
	   calculate is from transaction key, i.e., as an md5 of From/To/CallID/
	   CSeq exactly the same way as TM does; good for reboot -- than messages
	   belonging to transaction lost due to reboot will still be forwarded
	   with the same branch parameter and will be match-able downstream

       if it is turned on, we don't care about reboot; we simply put a simple
	   value in there; better for performance
	*/

	if (syn_branch ) {
		*msg->add_to_branch_s='0';
		msg->add_to_branch_len=1;
	} else {
		if (!char_msg_val( msg, md5 )) 	{ /* parses transaction key */
			LOG(L_ERR, "ERROR: forward_request: char_msg_val failed\n");
			goto error1;
		}
		msg->hash_index=hash( msg->callid->body, get_cseq(msg)->number);
		if (!branch_builder( msg->hash_index, 0, md5, 0 /* 0-th branch */,
					msg->add_to_branch_s, &msg->add_to_branch_len )) {
			LOG(L_ERR, "ERROR: forward_request: branch_builder failed\n");
			goto error1;
		}
	}

	buf = build_req_buf_from_sip_req( msg, &len, send_sock);
	if (!buf){
		LOG(L_ERR, "ERROR: forward_request: building failed\n");
		goto error1;
	}
	 /* send it! */
	DBG("Sending:\n%s.\n", buf);
	DBG("orig. len=%d, new_len=%d\n", msg->len, len );
	
	if (udp_send(send_sock, buf, len,  to)==-1){
			ser_error=E_SEND;
			p->errors++;
			p->ok=0;
			STATS_TX_DROPS;
			goto error1;
	}
	/* sent requests stats */
	else STATS_TX_REQUEST(  msg->first_line.u.request.method_value );
	pkg_free(buf);
	free(to);
	/* received_buf & line_buf will be freed in receiv_msg by free_lump_list*/
	return 0;

error1:
	free(to);
error:
	if (buf) pkg_free(buf);
	return -1;
}


int update_sock_struct_from_ip( union sockaddr_union* to,
	struct sip_msg *msg )
{
	to->sin.sin_port=(msg->via1->port)
		?htons(msg->via1->port): htons(SIP_PORT);
	to->sin.sin_family=msg->src_ip.af;
	memcpy(&to->sin.sin_addr, &msg->src_ip.u, msg->src_ip.len);

	return 1;
}

int update_sock_struct_from_via( union sockaddr_union* to,
								 struct via_body* via )
{
	struct hostent* he;
	char *host_copy;
	str* name;
	unsigned short port;


	if (via->received){
		DBG("update_sock_struct_from_via: using 'received'\n");
		name=&(via->received->value);
		/* making sure that we won't do SRV lookup on "received"
		 * (possible if no DNS_IP_HACK is used)*/
		port=via->port?via->port:SIP_PORT; 
	}else{
		DBG("update_sock_struct_from_via: using via host\n");
		name=&(via->host);
		port=via->port;
	}
	/* we do now a malloc/memcpy because gethostbyname loves \0-terminated 
	   strings; -jiri 
	   but only if host is not null terminated
	   (host.s[len] will always be ok for a via)
	    BTW: when is via->host.s non null terminated? tm copy? - andrei 
	    Yes -- it happened on generating a 408 by TM; -jiri
	*/
	if (name->s[name->len]){
		host_copy=pkg_malloc( name->len+1 );
		if (!host_copy) {
			LOG(L_NOTICE, "ERROR: update_sock_struct_from_via:"
							" not enough memory\n");
			return -1;
		}
		memcpy(host_copy, name->s, name->len );
		host_copy[name->len]=0;
		DBG("update_sock_struct_from_via: trying SRV lookup\n");
		he=sip_resolvehost(host_copy, &port);
		
		pkg_free( host_copy );
	}else{
		DBG("update_sock_struct_from_via: trying SRV lookup\n");
		he=sip_resolvehost(name->s, &port);
	}
	
	if (he==0){
		LOG(L_NOTICE, "ERROR:forward_reply:resolve_host(%s) failure\n",
				name->s);
		return -1;
	}
	hostent2su(to, he, 0, htons(port));
	return 1;
}


/* removes first via & sends msg to the second */
int forward_reply(struct sip_msg* msg)
{
	char* new_buf;
	union sockaddr_union* to;
	struct socket_info* send_sock;
	unsigned int new_len;
	struct sr_module *mod;
	
	to=0;
	new_buf=0;
	/*check if first via host = us */
	if (check_via){
		if (check_self(&(msg->via1->host))!=1){
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
	if (parse_headers( msg, HDR_VIA2, 0 )==-1 
		|| (msg->via2==0) || (msg->via2->error!=PARSE_OK))
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

	if (udp_send(send_sock, new_buf,new_len,  to)==-1)
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

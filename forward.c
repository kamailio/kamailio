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
#include "msg_parser.h"
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

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif



int forward_request( struct sip_msg* msg, struct proxy_l * p)
{
	unsigned int len;
	char* buf;
	struct sockaddr_in* to;

	to=0;
	buf = build_req_buf_from_sip_req( msg, &len);
	if (!buf){
		LOG(L_ERR, "ERROR: forward_reply: building failed\n");
		goto error;
	}

	to=(struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	if (to==0){
		LOG(L_ERR, "ERROR: forward_request: out of memory\n");
		goto error;
	}

	 /* send it! */
	DBG("Sending:\n%s.\n", buf);
	DBG("orig. len=%d, new_len=%d\n", msg->len, len );

	to->sin_family = AF_INET;
	to->sin_port = (p->port)?htons(p->port):htons(SIP_PORT);
	/* if error try next ip address if possible */
	if (p->ok==0){
		if (p->host.h_addr_list[p->addr_idx+1])
			p->addr_idx++;
		p->ok=1;
	}
	
	memcpy(&(to->sin_addr.s_addr), p->host.h_addr_list[p->addr_idx],
			sizeof(to->sin_addr.s_addr));
	/* 
	to->sin_addr.s_addr=*((long*)p->host.h_addr_list[p->addr_idx]);
	*/

	p->tx++;
	p->tx_bytes+=len;

	if (udp_send( buf, len, (struct sockaddr*) to,
				sizeof(struct sockaddr_in))==-1){
			p->errors++;
			p->ok=0;
			STATS_TX_DROPS;
			goto error;
	}
	/* sent requests stats */
	else STATS_TX_REQUEST(  msg->first_line.u.request.method_value );
	free(buf);
	free(to);
	/* received_buf & line_buf will be freed in receiv_msg by free_lump_list*/
	return 0;
error:
	if (buf) free(buf);
	if (to) free(to);
	return -1;
}


int update_sock_struct_from_via( struct sockaddr_in* to,  struct via_body* via )
{
	int err;
	struct hostent* he;

	to->sin_family = AF_INET;
	to->sin_port = (via->port)?htons(via->port): htons(SIP_PORT);

#ifdef DNS_IP_HACK
	to->sin_addr.s_addr=str2ip((unsigned char*)via->host.s,via->host.len,&err);
	if (err)
#endif
	{
		/* fork? gethostbyname will probably block... */
		he=gethostbyname(via->host.s);
		if (he==0){
			LOG(L_NOTICE, "ERROR:forward_reply:gethostbyname(%s) failure\n",
					via->host.s);
			return -1;
		}
		memcpy(&(to->sin_addr.s_addr), he->h_addr_list[0], 
				sizeof(to->sin_addr.s_addr));
		/*
		to->sin_addr.s_addr=*((long*)he->h_addr_list[0]);
		*/
	}
	return 1;
}


/* removes first via & sends msg to the second */
int forward_reply(struct sip_msg* msg)
{
	int  r;
	char* new_buf;
	struct sockaddr_in* to;
	unsigned int new_len;
	struct sr_module *mod;
#ifdef DNS_IP_HACK
	int err;
#endif



	to=0;
	new_buf=0;
	/*check if first via host = us */
	if (check_via){
		for (r=0; r<addresses_no; r++)
			if(strcmp(msg->via1->host.s, names[r])==0) break;
		if (r==addresses_no){
			LOG(L_NOTICE, "ERROR: forward_reply: host in first via!=me :"
					" %s\n", msg->via1->host);
			/* send error msg back? */
			goto error;
		}
	}

	/* here will be called the T Module !!!!!!  */
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

	to=(struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
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

	if (udp_send(new_buf,new_len, (struct sockaddr*) to,
					sizeof(struct sockaddr_in))==-1)
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

	free(new_buf);
	free(to);
skip:
	return 0;
error:
	if (new_buf) free(new_buf);
	if (to) free(to);
	return -1;
}

/*
 * $Id$
 */


#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "forward.h"
#include "config.h"
#include "msg_parser.h"
#include "route.h"
#include "dprint.h"
#include "udp_server.h"

#define MAX_VIA_LINE_SIZE      240
#define MAX_RECEIVED_SIZE  57



int forward_request(char * orig, char* buf, 
					 unsigned int len,
					 struct sip_msg* msg,
					 struct route_elem* re)
{
	unsigned int new_len, via_len, received_len;
	char line_buf[MAX_VIA_LINE_SIZE];
	char received_buf[MAX_RECEIVED_SIZE];
	char* new_buf;
	int offset, s_offset, size;
	struct sockaddr_in to;

	received_len=0;

	via_len=snprintf(line_buf, MAX_VIA_LINE_SIZE, "Via: SIP/2.0/UDP %s:%d\r\n",
						our_name, our_port);
	/* check if received needs to be added */
	/* if check_address(source_ip, msg->via1.host) */
	received_len=snprintf(received_buf, MAX_RECEIVED_SIZE, ";received=%s",
							"10.11.12.13");
	
	new_len=len+via_len+received_len;
	new_buf=(char*)malloc(new_len+1);
	if (new_buf==0){
		DPrint("ERROR: forward_request: out of memory\n");
		goto error1;
	}
/* copy msg till first via */
	offset=s_offset=0;
	size=msg->via1.hdr-buf;
	memcpy(new_buf, orig, size);
	offset+=size;
	s_offset+=size;
 /* add our via */
	memcpy(new_buf+offset, line_buf, via_len);
	offset+=via_len;
 /* modify original via if neccesarry (received=...)*/
	if (received_len){
		if (msg->via1.params){
				size= msg->via1.params-msg->via1.hdr-1; /*compensate for ';' */
		}else{
				size= msg->via1.host-msg->via1.hdr+strlen(msg->via1.host);
				if (msg->via1.port!=0){
					size+=strlen(msg->via1.hdr+size+1)+1; /* +1 for ':'*/
				}
		}
		memcpy(new_buf+offset, orig+s_offset, 
								size);
		offset+=size;
		s_offset+=size;
		memcpy(new_buf+offset, received_buf, received_len);
		offset+=received_len;
	}
 	/* copy the rest of the msg */
 	memcpy(new_buf+offset, orig+s_offset, len-s_offset);
	new_buf[new_len]=0;

	 /* send it! */
	printf("Sending:\n%s.\n", new_buf);
	printf("orig. len=%d, new_len=%d, via_len=%d, received_len=%d\n",
			len, new_len, via_len, received_len);

	to.sin_family = AF_INET;
	to.sin_port = (re->port)?htons(re->port):htons(SIP_PORT);
	/* if error try next ip address if possible */
	if (re->ok==0){
		if (re->host.h_addr_list[re->current_addr_idx+1])
			re->current_addr_idx++;
		re->ok=1;
	}
	/* ? not 64bit clean?*/
	to.sin_addr.s_addr=*((long*)re->host.h_addr_list[re->current_addr_idx]);

	re->tx++;
	re->tx_bytes+=new_len;
	if (udp_send(new_buf, new_len, &to, sizeof(to))==-1){
			re->errors++;
			re->ok=0;
			goto error;
	}

	free(new_buf);
	return 0;
error:
	free(new_buf);
error1:
	return -1;

}



/* removes first via & sends msg to the second */
int forward_reply(char * orig, char* buf, 
					 unsigned int len,
					 struct sip_msg* msg)
{


	unsigned int new_len, via_len;
	char* new_buf;
	int offset, s_offset, size;
	struct hostent* he;
	struct sockaddr_in to;


	/* we must remove the first via */
	via_len=msg->via1.size;
	size=msg->via1.hdr-buf;
	if (msg->via1.next){
		/* keep hdr =substract hdr size +1 (hdr':') and add
		 */
		via_len-=strlen(msg->via1.hdr)+1;
		size+=strlen(msg->via1.hdr)+1;
	}
	new_len=len-size;
	new_buf=(char*)malloc(new_len);
	if (new_buf==0){
		DPrint("ERROR: forward_reply: out of memory\n");
		goto error;
	}
	memcpy(new_buf, orig, size);
	offset=size;
	s_offset=size+via_len;
	memcpy(new_buf+offset,orig+s_offset, len-s_offset);
	 /* send it! */
	printf("Sending: to %s:%d, \n%s.\n",
			msg->via2.host, 
			(unsigned short)msg->via2.port,
			new_buf);
	/* fork? gethostbyname will probably block... */
	he=gethostbyname(msg->via2.host);
	if (he==0){
		DPrint("ERROR:forward_reply:gethostbyname failure\n");
		goto error;
	}
	to.sin_family = AF_INET;
	to.sin_port = (msg->via2.port)?htons(msg->via2.port):htons(SIP_PORT);
	to.sin_addr.s_addr=*((long*)he->h_addr_list[0]);
	
	if (udp_send(new_buf,new_len, &to, sizeof(to))==-1)
		goto error;
	
	free(new_buf);
	return 0;

error:
	if (new_buf) free(new_buf);
	return -1;
}

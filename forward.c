/*
 * $Id$
 */


#include <string.h>

#include "forward.h"
#include "msg_parser.h"
#include "route.h"
#include "dprint.h"

#define MAX_VIA_LINE_SIZE      240
#define MAX_RECEIVED_SIZE  57

#define our_address "dorian.fokus.gmd.de"
#define our_port 1234


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

	received_len=0;
	printf("0\n");

	via_len=snprintf(line_buf, MAX_VIA_LINE_SIZE, "Via: SIP/2.0/UDP %s:%d\r\n",
						our_address, our_port);
	/* check if received needs to be added */
	/* if check_address(source_ip, msg->via1.host) */
	received_len=snprintf(received_buf, MAX_RECEIVED_SIZE, ";received=%s",
							"10.11.12.13");
	
	new_len=len+via_len+received_len;
	new_buf=(char*)malloc(new_len+1);
	if (new_buf==0){
		DPrint("ERROR: forward_request: out of memory\n");
		goto error;
	}
	printf("1\n");
 /* copy msg till first via */
 	offset=s_offset=0;
 	size=msg->via1.hdr-buf;
 	memcpy(new_buf, orig, size);
	offset+=size;
	s_offset+=size;
	printf("2\n");
 /* add our via */
 	memcpy(new_buf+offset, line_buf, via_len);
	offset+=via_len;
	printf("3\n");
 /* modify original via if neccesarry (received=...)*/
 	if (received_len){
		if (msg->via1.params){
				size= msg->via1.params-msg->via1.hdr-1; /*compensate for ';' */
		}else{
				size= msg->via1.host-msg->via1.hdr+strlen(msg->via1.host);
				if (msg->via1.port!=0){
					size+=strlen(msg->via1.hdr+size+1);
				}
		}
		memcpy(new_buf+offset, orig+s_offset, 
								size);
		offset+=size;
		s_offset+=size;
		printf("4\n");
		memcpy(new_buf+offset, received_buf, received_len);
		printf("5\n");
		offset+=received_len;
	}
 	/* copy the rest of the msg */
 	memcpy(new_buf+offset, orig+s_offset, len-s_offset);
	printf("6\n");
	new_buf[new_len]=0;

	 /* send it! */
	 printf("Sending:\n%s.\n", new_buf);
	 
	return 0;
error:
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


	/* we must remove first via */
	via_len=msg->via1.size;
	size=msg->via1.hdr-buf;
	if (msg->via1.next){
		via_len-=strlen(msg->via1.hdr)+1; /* +1 from ':' */
		size+=strlen(msg->via1.hdr)+1;
	}
	new_len=len-size;
	printf("r1\n");
	new_buf=(char*)malloc(new_len);
	if (new_buf==0){
		DPrint("ERROR: forward_reply: out of memory\n");
		goto error;
	}
	printf("r2\n");
	memcpy(new_buf, orig, size);
	offset=size;
	s_offset=size+via_len;
	printf("r3\n");
	memcpy(new_buf+offset,orig+s_offset, len-s_offset);
	printf("r4\n");
	 /* send it! */
	printf("Sending: to %s:%d, \n%s.\n",
			msg->via2.host, 
			(unsigned short)msg->via2.port,
			new_buf);
	
	return 0;

error:
	return -1;
}

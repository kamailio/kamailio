/*
 * $Id$
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

#include "config.h"
#include "dprint.h"
#include "route.h"
#include "udp_server.h"



/* debuging function */
/*
void receive_stdin_loop()
{
	#define BSIZE 1024
	char buf[BSIZE+1];
	int len;
	
	while(1){
		len=fread(buf,1,BSIZE,stdin);
		buf[len+1]=0;
		receive_msg(buf, len);
		printf("-------------------------\n");
	}
}
*/

#define NAME "dorian.fokus.gmd.de"


int main(int argc, char** argv)
{

	char * cfg_file;
	FILE* cfg_stream;
	struct hostent* he;

	cfg_file=CFG_FILE;
	
	/* process command line (get port no, cfg. file path etc) */
	/* ...*/

	our_port=SIP_PORT;
	our_name=NAME;
	/* get ip */
	he=gethostbyname(our_name);
	if (he==0){
		DPrint("ERROR: could not resolve %s\n", our_name);
		goto error;
	}
	our_address=*((long*)he->h_addr_list[0]);
	printf("Listening on %s[%x]:%d\n",our_name,
				(unsigned long)our_address,
				(unsigned short)our_port);
		
	
	

	/* load config file or die */
	cfg_stream=fopen (cfg_file, "r");
	if (cfg_stream==0){
		DPrint("ERROR: could not load config file: %s\n", strerror(errno));
		goto error;
	}

	if (cfg_parse_stream(cfg_stream)!=0){
		DPrint("ERROR: config parser failure\n");
		goto error;
	}
	
		
	print_rl();


	/* init_daemon? */
	if (udp_init(our_address,our_port)==-1) goto error;
	/* start/init other processes/threads ? */

	/* receive loop */
	udp_rcv_loop();


error:
	return -1;

}

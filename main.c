/*
 * $Id$
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "dprint.h"
#include "route.h"

#define CFG_FILE "./sip_router.cfg"


/* debuging function */

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



int main(int argc, char** argv)
{

	char * cfg_file;
	FILE* cfg_stream;

	cfg_file=CFG_FILE;
	
	/* process command line (get port no, cfg. file path etc) */
	/* ...*/

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



	/* start/init other processes/threads ? */

	/* receive loop */

	receive_stdin_loop();


error:
	return -1;

}

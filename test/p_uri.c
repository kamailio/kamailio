
#include <stdio.h>
#include <stdlib.h> /* exit() */
#include <string.h>
#include "../str.h"

/* ser compat defs */
#define EXTRA_DEBUG
#include "../parser/parse_uri.c"
#include "../dprint.c"


int ser_error=0;
int debug=L_DBG;
int log_stderr=1;
int log_facility=LOG_DAEMON;
int process_no=0;
struct process_table* pt=0;



int main (int argc, char** argv)
{

	int r;
	struct sip_uri uri;

	if (argc<2){
		printf("usage:    %s  uri [, uri...]\n", argv[0]);
		exit(1);
	}
	
	for (r=1; r<argc; r++){
		if (parse_uri(argv[r], strlen(argv[r]), &uri)<0){
			printf("error: parsing %s\n", argv[r]);
		}
	}
	return 0;
}

/*
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
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


static char *id="$Id$";
static char *version="resolver_test 0.1";
static char* help_msg="\
Usage: resolver -n address [-c count] [-v]\n\
Options:\n\
    -n address    address to be resolved\n\
    -c count      how many times to resolve it\n\
    -v            increase verbosity level\n\
    -V            version number\n\
    -h            this help message\n\
";



int main (int argc, char** argv)
{
	char c;
	struct hostent* he;
	int ok;
	int errors;
	int r;
	char *tmp;
	
	int count;
	int verbose;
	char *address;
	
	/* init */
	count=0;
	verbose=0;
	address=0;

	ok=errors=0;

	opterr=0;
	while ((c=getopt(argc,argv, "n:c:vhV"))!=-1){
		switch(c){
			case 'n':
				address=optarg;
				break;
			case 'v':
				verbose++;
				break;
			case 'c':
				count=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)){
					fprintf(stderr, "bad count: -c %s\n", optarg);
					goto error;
				}
				break;
			case 'V':
				printf("version: %s\n", version);
				printf("%s\n",id);
				exit(0);
				break;
			case 'h':
				printf("version: %s\n", version);
				printf("%s", help_msg);
				exit(0);
				break;
			case '?':
				if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c´\n", optopt);
				else
					fprintf(stderr, "Unknown character `\\x%x´\n", optopt);
				goto error;
			case ':':
				fprintf(stderr, "Option `-%c´ requires an argument.\n",
						optopt);
				goto error;
				break;
			default:
					abort();
		}
	}
	
	/* check if all the required params are present */
	if (address==0){
		fprintf(stderr, "Missing -a address\n");
		exit(-1);
	}
	if(count==0){
		fprintf(stderr, "Missing count (-c number)\n");
		exit(-1);
	}else if(count<0){
		fprintf(stderr, "Invalid count (-c %d)\n", count);
		exit(-1);
	}
	


	/* flood loop */
	for (r=0; r<count; r++){
		if ((verbose>1)&&(r%1000))  putchar('.');
		/* resolve destination loop */
		he=gethostbyname(address);
		if (he==0){
			errors++;
			if (verbose>1) 
				putchar('?');
		}else ok++;
	}

	printf("\n%d requests, %d succeeded, %d errors\n", count, ok, errors);

	exit(0);

error:
	exit(-1);
}

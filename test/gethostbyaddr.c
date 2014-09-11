/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>




static char* id="$Id$";
static char* version="gethostbyaddr 0.1";
static char* help_msg="\
Usage: gethostbyaddr   [-hV] -n host\n\
Options:\n\
    -n host       host name\n\
    -V            version number\n\
    -h            this help message\n\
";


int main(int argc, char** argv)
{
	char c;
	char* name;
	struct hostent* he;
	unsigned char** h;

	name=0;
	
	opterr=0;
	while ((c=getopt(argc, argv, "n:hV"))!=-1){
		switch(c){
			case 'n':
				name=optarg;
				break;
			case 'V':
				printf("version: %s\n", version);
				printf("%s\n", id);
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
	
	if (name==0){
		fprintf(stderr, "Missing domain name (-n name)\n");
		goto error;
	}
	
	he=gethostbyname(name);
	if (he==0){
			printf("bad address <%s>\n", name);
			goto error;
	}
	he=gethostbyaddr(he->h_addr_list[0], he->h_length, he->h_addrtype); 
	if (he==0) printf("no answer\n");
	else{
		printf("h_name=%s\n", he->h_name);
		for(h=he->h_aliases;*h;h++)
			printf("   alias=%s\n", *h);
		for(h=he->h_addr_list;*h;h++)
			printf("   ip=%d.%d.%d.%d\n", (*h)[0],(*h)[1],(*h)[2],(*h)[3] );
	}
	printf("done\n");
	exit(0);
error:
	exit(-1);
}

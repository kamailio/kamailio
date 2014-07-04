/*
 * $Id$
 *
 * tests for ../resolver.c
 *
 * Compile with:
 *  gcc -o dns_query2 dns_query.c ../resolve.o ../dprint.o ../mem/ *.o -lresolv
 *  (and first compile ser with qm_malloc)
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
/*
 * tester for ser resolver  (andrei) */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "../resolve.h"
#include "../mem/q_malloc.h"

/* symbols needed by dprint */
int log_stderr=1;
int debug=0;
int pids[1];
int process_no=0;
long shm_mem_size=0;
char mem_pool[1024*1024];
struct qm_block* mem_block;
int log_facility=0;
int memlog=0;
int memdbg=0;
int ser_error=0;
struct process_table* pt=0;


static char* id="$Id$";
static char* version="dns_query 0.1";
static char* help_msg="\
Usage: dns_query  [-t type] [-hV] -n host\n\
Options:\n\
    -n host       host name\n\
    -t type       query type (default A)\n\
    -V            version number\n\
    -h            this help message\n\
";


int main(int argc, char** argv)
{
	char c;
	char* name;
	char* type_str;
	int type;
	int r;
	struct rdata* head;
	struct rdata* l;
	struct srv_rdata* srv;
	struct naptr_rdata* naptr;
	struct a_rdata* ip;

	name=type_str=0;
	
	opterr=0;
	while ((c=getopt(argc, argv, "n:t:hV"))!=-1){
		switch(c){
			case 'n':
				name=optarg;
				break;
			case 't':
				type_str=optarg;
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
	type=T_A;
	if (type_str){
		if (strcasecmp(type_str, "A")==0) type=T_A;
		else if (strcasecmp(type_str, "NS")==0) type=T_NS;
		else if (strcasecmp(type_str, "MD")==0) type=T_MD;
		else if (strcasecmp(type_str, "MF")==0) type=T_MF;
		else if (strcasecmp(type_str, "CNAME")==0) type=T_CNAME;
		else if (strcasecmp(type_str, "SOA")==0) type=T_SOA;
		else if (strcasecmp(type_str, "PTR")==0) type=T_PTR;
		else if (strcasecmp(type_str, "HINFO")==0) type=T_HINFO;
		else if (strcasecmp(type_str, "MINFO")==0) type=T_MINFO;
		else if (strcasecmp(type_str, "MX")==0) type=T_MX;
		else if (strcasecmp(type_str, "TXT")==0) type=T_TXT;
		else if (strcasecmp(type_str, "AAAA")==0) type=T_AAAA;
		else if (strcasecmp(type_str, "SRV")==0) type=T_SRV;
		else if (strcasecmp(type_str, "NAPTR")==0) type=T_NAPTR;
		else if (strcasecmp(type_str, "AXFR")==0) type=T_AXFR;
		else{
			fprintf(stderr, "Unknown query type %s\n", type_str);
			goto error;
		}
	}
	/* init mallocs*/
	mem_block=qm_malloc_init(mem_pool, 1024*1024);
	printf("calling get_record...\n");
	head=get_record(name, type);
	if (head==0) printf("no answer\n");
	else{
		printf("records:\n");
		for(l=head; l; l=l->next){
			switch(l->type){
				case T_SRV:
					srv=(struct srv_rdata*)l->rdata;
					printf("SRV    type= %d class=%d  ttl=%d\n",
							l->type, l->class, l->ttl);
					printf("       prio= %d weight=%d port=%d\n",
								srv->priority, srv->weight, srv->port);
					printf("       name_len= %d (%d), name= [%.*s]\n",
									srv->name_len, strlen(srv->name),
									srv->name_len, srv->name);
					break;
				case T_CNAME:
					printf("CNAME  type= %d class=%d  ttl=%d\n",
							l->type, l->class, l->ttl);
					printf("       name=[%s]\n", 
								((struct cname_rdata*)l->rdata)->name);
					break;
				case T_A:
					ip=(struct a_rdata*)l->rdata;
					printf("A      type= %d class=%d  ttl=%d\n",
								l->type, l->class, l->ttl);
					printf("       ip= %d.%d.%d.%d\n",
								ip->ip[0], ip->ip[1], ip->ip[2], ip->ip[3]);
					break;
				case T_AAAA:
					printf("AAAA   type= %d class=%d  ttl=%d\n",
							l->type, l->class, l->ttl);
					printf("        ip6= ");
					for(r=0;r<16;r++) 
						printf("%x ", ((struct aaaa_rdata*)l->rdata)->ip6[r]);
					printf("\n");
					break;
				case T_NAPTR:
					naptr=(struct naptr_rdata*)l->rdata;
					printf("NAPTR  type= %d class=%d  ttl=%d\n",
							l->type, l->class, l->ttl);
					printf("       order= %d pref=%d\n",
								naptr->order, naptr->pref);
					printf("       flags_len= %d,     flags= [%.*s]\n",
									naptr->flags_len, 
									naptr->flags_len, naptr->flags);
					printf("       services_len= %d,  services= [%.*s]\n",
									naptr->services_len, 
									naptr->services_len, naptr->services);
					printf("       regexp_len= %d,    regexp= [%.*s]\n",
									naptr->regexp_len, 
									naptr->regexp_len, naptr->regexp);
					printf("       repl_len= %d,      repl= [%s]\n",
									naptr->repl_len, naptr->repl);
					break;

				default:
					printf("UNKN    type= %d class=%d  ttl=%d\n",
								l->type, l->class, l->ttl);
					printf("       rdata=%p\n", l->rdata);
			}
		}
	}
	printf("done\n");
	exit(0);
error:
	exit(-1);
}


/* $Id$ */
/*
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
 * regexec test program (good to find re lib. bugs)
 * uses the same flags as ser/textops for re-matching
 * History:
 * --------
 *  created by andrei
 */



#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>


static char *id="$Id$";
static char *version="re_test 0.1";
static char* help_msg="\
Usage: re_test [-f file] regular_expression \n\
Options:\n\
    -f file       file with the content of a sip packet (max 65k)\n\
    -h            this help message\n\
    -n            non matching list ([^...]) will match newlines\n\
    -s            case sensitive\n\
    -v            increase verbosity\n\
";

#define BUF_SIZE 65535


int main (int argc, char** argv)
{
	int fd;
	char c;
	int n;
	char* re_str;
	char buffer[BUF_SIZE+1]; /* space for null-term. */
	char* buf;
	regex_t re;
	int flags;
	regmatch_t pmatch;
	int match;
	int eflags;
	
	int verbose;
	char *fname;
	
	/* init */
	verbose=0;
	fname=0;
	re_str=0;
	flags=REG_EXTENDED|REG_ICASE|REG_NEWLINE;
	match=0;
	buf=buffer;
	eflags=0;

	opterr=0;
	while ((c=getopt(argc,argv, "f:nsvhV"))!=-1){
		switch(c){
			case 'f':
				fname=optarg;
				break;
			case 'v':
				verbose++;
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
			case 's':
				flags&=~REG_ICASE;
				break;
			case 'n':
				flags&=~REG_NEWLINE;
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
	/* check if we have non-options */
	if (optind < argc){
		re_str=argv[optind];
	}
	
	/* check if all the required params are present */
	if (re_str==0){
		fprintf(stderr, "ERROR: no regular expression specified\n");
		goto error;
	}else{
		if (regcomp(&re, re_str, flags)){
			fprintf(stderr, "ERROR: bad regular expression <%s>\n", re_str);
			goto error;
		}
	}
		
	if ((fname!=0 ) &&(strcmp(fname, "-")!=0)){
		/* open packet file */
		fd=open(fname, O_RDONLY);
		if (fd<0){
			fprintf(stderr, "ERROR: loading packet-file(%s): %s\n", fname,
					strerror(errno));
			goto error;
		}
	}else fd=0;
	n=read(fd, buf, BUF_SIZE);
	if (n<0){
		fprintf(stderr, "ERROR: reading file(%s): %s\n", fname,
				strerror(errno));
		goto error;
	}
	buf[n]=0; /* null terminate it */
	if (verbose) printf("read %d bytes from file %s\n", n, fname);
	if (fd!=0) close(fd); /* we don't want to close stdin */
	
	while (regexec(&re, buf, 1, &pmatch, eflags)==0){
		eflags|=REG_NOTBOL;
		match++;
		if (pmatch.rm_so==-1){
			fprintf(stderr, "ERROR: unknown match offset\n");
			goto error;
		}else{
			if (verbose){
				printf("%4d -%4d: ", (int)pmatch.rm_so+buf-buffer,
						(int)pmatch.rm_eo+buf-buffer);
			}
			printf("%.*s\n", (int)(pmatch.rm_eo-pmatch.rm_so),
								buf+pmatch.rm_so);
		}
		buf+=pmatch.rm_eo;
	}
	if (verbose) printf("\n%d matches\n", match);
	if (match) exit(0);
	else exit(1);
	
error:
	exit(-1);
}

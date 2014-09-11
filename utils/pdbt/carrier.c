/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define _GNU_SOURCE
#include "carrier.h"
#include "log.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



char *cnames[MAX_CARRIERID+1];




void init_carrier_names() {
	int i;

	for (i=0; i<=MAX_CARRIERID; i++) cnames[i]=NULL;
	cnames[OTHER_CARRIERID] = "other carriers merged by this tool";
}




int load_carrier_names(char *filename) {
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	char *p;
	int n;
	char idstr[4];
	long int id;
	int ret=0;

	idstr[3]=0;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		LERR("cannot open file '%s'\n", filename);
		return -1;
	}

	n=1;
	while ((read = getline(&line, &len, fp)) != -1) {
		p=line;
		len=strlen(p);

		if (len<=5) {
			LWARNING("invalid line %ld\n", (long int)n);
			ret=-1;
			goto nextline;
		}

		idstr[0] = p[1];
		idstr[1] = p[2];
		idstr[2] = p[3];
		p+=5;
		len-=5;
		
		id = strtol(idstr, NULL, 10);
		if (!IS_VALID_PDB_CARRIERID(id)) {
			LWARNING("invalid carrier id '%s'\n", idstr);
			ret=-1;
			goto nextline;
		}
		
		cnames[id]=malloc(len+1);
		if (cnames[id]==NULL) {
			LERR("out of memory (needed %ld bytes)\n", (long int)len);
			ret=-1;
			exit(-1);
		}
		
		strncpy(cnames[id], p, len - 1);
		cnames[id][len - 1]=0;

	nextline:
		n++;
	}

	if (line) free(line);
	fclose(fp);

	return ret;
}




char *carrierid2name(carrier_t carrier) {
	char *s;
	if (!IS_VALID_CARRIERID(carrier)) s="invalid carrier id";
	else s=cnames[carrier];
	if (s==NULL) s="unknown carrier";
	return s;
}

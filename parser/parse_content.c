/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "../dprint.h"
#include "../str.h"
#include "../ut.h"
#include "parse_content.h"


typedef struct type_node_s {
	char c;
	unsigned char final;
	unsigned char nr_sons;
	int next;
}type_node_t;


char* parse_content_length( char* buffer, char* end, int* length)
{
	int number;
	char *p;
	int  size;

	p = buffer;
	/* search the begining of the number */
	while ( p<end && (*p==' ' || *p=='\t' ||
	(*p=='\n' && (*(p+1)==' '||*(p+1)=='\t')) ))
		p++;
	if (p==end)
		goto error;
	/* parse the number */
	size = 0;
	number = 0;
	while (p<end && *p>='0' && *p<='9') {
		number = number*10 + (*p)-'0';
		size ++;
		p++;
	}
	if (p==end || size==0)
		goto error;
	/* now we should have only spaces at the end */
	while ( p<end && (*p==' ' || *p=='\t' ||
	(*p=='\n' && (*(p+1)==' '||*(p+1)=='\t')) ))
		p++;
	if (p==end)
		goto error;
	/* the header ends proper? */
	if ( (*(p++)!='\n') && (*(p-1)!='\r' || *(p++)!='\n' ) )
		goto error;

	*length = number;
	return p;
error:
	LOG(L_ERR,"ERROR:parse_content_length: parse error near char [%d][%c]\n",
		*p,*p);
	return 0;
}




char* parse_content_type( char* buffer, char* end, int* type)
{
	static type_node_t type_tree[] = {
		{'t',-1,1,4}, {'e',-1,1,-1}, {'x',-1,1,-1}, {'t',0,0,-1},
		{'m',-1,1,11}, {'e',-1,1,-1}, {'s',-1,1,-1}, {'s',-1,1,-1},
			{'a',-1,1,-1},{'g',-1,1,-1}, {'e',5,0,-1},
		{'a',-1,1,-1}, {'p',-1,1,-1}, {'p',-1,1,-1}, {'l',-1,1,-1},
			{'i',-1,1,-1},{'c',-1,1,-1},{'a',-1,1,-1},{'t',-1,1,-1},
			{'i',-1,1,-1},{'o',-1,1,-1},{'n',9,0,-1}
	};
	static type_node_t subtype_tree[] = {
		{'p',0,1,5}, {'l',0,1,-1}, {'a',0,1,-1}, {'i',0,1,-1},
			{'n',CONTENT_TYPE_TEXT_PLAIN,0,-1},
		{'c',0,1,9}, {'p',0,1,-1}, {'i',0,1,-1},
			{'m',CONTENT_TYPE_MESSAGE_CPIM,0,-1},
		{'s',0,1,-1}, {'d',0,1,-1},
			{'p',CONTENT_TYPE_APPLICATION_SDP,0,-1},
	};
	int node;
	int mime;
	char *p;

	p = buffer;
	mime = CONTENT_TYPE_UNKNOWN;

	/* search the begining of the type */
	while ( p<end && (*p==' ' || *p=='\t' ||
	(*p=='\n' && (*(p+1)==' '||*(p+1)=='\t')) ))
		p++;
	if (p==end)
		goto error;

	/* parse the type */
	node = 0;
	while (p<end && ((*p>='a' && *p<='z') || (*p>='A' && *p<='Z')) ) {
		while ( node!=-1 && type_tree[node].c!=*p && type_tree[node].c+32!=*p){
			node = type_tree[node].next;
		}
		if (node!=-1 && type_tree[node].nr_sons)
			node++;
		p++;
	}
	if (p==end || node==0)
		goto error;
	if (node!=-1)
		node = type_tree[node].final;

	/* search the '/' separator */
	while ( p<end && (*p==' ' || *p=='\t' ||
	(*p=='\n' && (*(p+1)==' '||*(p+1)=='\t')) ))
		p++;
	if ( p==end || *(p++)!='/')
		goto error;

	/* search the begining of the sub-type */
	while ( p<end && (*p==' ' || *p=='\t' ||
	(*p=='\n' && (*(p+1)==' '||*(p+1)=='\t')) ))
		p++;
	if (p==end)
		goto error;

	/* parse the sub-type */
	while (p<end && ((*p>='a' && *p<='z') || (*p>='A' && *p<='Z')) ) {
		while(node!=-1&&subtype_tree[node].c!=*p&&subtype_tree[node].c+32!=*p)
			node = subtype_tree[node].next;
		if (node!=-1 && subtype_tree[node].nr_sons)
			node++;
		p++;
	}
	if (p==end || node==0)
		goto error;
	if (node!=-1)
		mime = subtype_tree[node].final;

	/* now its possible to have some spaces */
	while ( p<end && (*p==' ' || *p=='\t' ||
	(*p=='\n' && (*(p+1)==' '||*(p+1)=='\t')) ))
		p++;
	if (p==end)
		goto error;

	/* if there are params, eat everything to the end */
	if (*p==';') {
		while ( p<end && (*p!='\n' || (*(p+1)==' '||*(p+1)=='\t')) )
			p++;
		if (p==end)
			goto error;
	}

	/* the header ends proper? */
	if ( (*(p++)!='\n') && (*(p-1)!='\r' || *(p++)!='\n' ) )
		goto error;

	*type = mime;
	return p;
error:
	LOG(L_ERR,"ERROR:parse_content_type: parse error near char [%d][%c]\n",
		*p,*p);
	return 0;
}




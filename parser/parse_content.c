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
 *
 * History:
 * 2003-08-04 parse_content_type_hdr separates type from subtype inside
 * the mime type (bogdan)
 * 2003-08-04 CPL subtype added (bogdan)
 * 2003-08-05 parse_accept_hdr function added (bogdan)
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "../mem/mem.h"
#include "../dprint.h"
#include "../str.h"
#include "../ut.h"
#include "parse_content.h"


#define is_mime_char(_c_) \
	(isalpha(_c_) || (_c_)=='-' || (_c_)=='+')
#define is_char_equal(_c_,_cs_) \
	( (isalpha(_c_)?(((_c_)|0x20)==(_cs_)):((_c_)==(_cs_)))==1 )


/*
 * Node of the type's tree; this tree contains all the known types;
 */
typedef struct type_node_s {
	char c;                      /* char contained by this node */
	unsigned char final;         /* says what mime type/subtype was detected
	                              * if string ends at this node */
	unsigned char nr_sons;       /* the number of sub-nodes */
	int next;                    /* the next sibling node */
}type_node_t;


static type_node_t type_tree[] = {
	{'t',TYPE_UNKNOWN,1,4},
		{'e',TYPE_UNKNOWN,1,-1},
			{'x',TYPE_UNKNOWN,1,-1},
				{'t',TYPE_TEXT,0,-1},
	{'m',TYPE_UNKNOWN,1,11},
		{'e',TYPE_UNKNOWN,1,-1},
			{'s',TYPE_UNKNOWN,1,-1},
				{'s',TYPE_UNKNOWN,1,-1},
					{'a',TYPE_UNKNOWN,1,-1},
						{'g',TYPE_UNKNOWN,1,-1},
							{'e',TYPE_MESSAGE,0,-1},
	{'a',TYPE_UNKNOWN,1,-1},
		{'p',TYPE_UNKNOWN,1,-1},
			{'p',TYPE_UNKNOWN,1,-1},
				{'l',TYPE_UNKNOWN,1,-1},
					{'i',TYPE_UNKNOWN,1,-1},
						{'c',TYPE_UNKNOWN,1,-1},
							{'a',TYPE_UNKNOWN,1,-1},
								{'t',TYPE_UNKNOWN,1,-1},
									{'i',TYPE_UNKNOWN,1,-1},
										{'o',TYPE_UNKNOWN,1,-1},
											{'n',TYPE_APPLICATION,0,-1}
	};

static type_node_t subtype_tree[] = {
		{'p',SUBTYPE_UNKNOWN,1,5},
			{'l',SUBTYPE_UNKNOWN,1,-1},
				{'a',SUBTYPE_UNKNOWN,1,-1},
					{'i',SUBTYPE_UNKNOWN,1,-1},
						{'n',SUBTYPE_PLAIN,0,-1},
		{'c',SUBTYPE_UNKNOWN,1,10},
			{'p',SUBTYPE_UNKNOWN,2,-1},
				{'i',SUBTYPE_UNKNOWN,1,9},
					{'m',SUBTYPE_CPIM,0,-1},
				{'l',SUBTYPE_CPL,0,-1},
		{'s',SUBTYPE_UNKNOWN,1,-1},
			{'d',SUBTYPE_UNKNOWN,1,-1},
				{'p',SUBTYPE_SDP,0,-1},
	};



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



char* decode_mime_type(char *start, char *end, unsigned int *mime_type)
{
	int node;
	char *mark;
	char *p;

	p = start;

	/* search the begining of the type */
	while ( p<end && (*p==' ' || *p=='\t' ||
	(*p=='\n' && (*(p+1)==' '||*(p+1)=='\t')) ))
		p++;
	if (p==end)
		goto error;

	/* parse the type */
	if (*p=='*') {
		*mime_type = TYPE_ALL<<16;
		p++;
	} else {
		node = 0;
		mark = p;
		while (p<end && is_mime_char(*p)  ) {
			while ( node!=-1 && !is_char_equal(*p,type_tree[node].c) ){
				node = type_tree[node].next;
			}
			if (node!=-1 && type_tree[node].nr_sons)
				node++;
			p++;
		}
		if (p==end || mark==p)
			goto error;
		if (node!=-1)
			*mime_type = type_tree[node].final<<16;
		else
			*mime_type = TYPE_UNKNOWN<<16;
	}

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
	if (*p=='*') {
		*mime_type |= SUBTYPE_ALL;
		p++;
	} else {
		node = 0;
		mark = p;
		while (p<end && is_mime_char(*p) ) {
			while(node!=-1 && !is_char_equal(*p,subtype_tree[node].c) )
				node = subtype_tree[node].next;
			if (node!=-1 && subtype_tree[node].nr_sons)
				node++;
			p++;
		}
		if (p==mark)
			goto error;
		if (node!=-1)
			*mime_type |= subtype_tree[node].final;
		else
			*mime_type |= SUBTYPE_UNKNOWN;
	}

	/* now its possible to have some spaces */
	while ( p<end && (*p==' ' || *p=='\t' ||
	(*p=='\n' && (*(p+1)==' '||*(p+1)=='\t')) ))
		p++;

	/* if there are params, ignore them!! -> eat everything to
	 * the end or to the first ',' */
	if ( p<end && *p==';' )
		for(p++; p<end && *p!=','; p++);

	/* is this the correct end? */
	if (p!=end && *p!=',' )
		goto error;

	/* check the format of the decoded mime */
	if ((*mime_type)>>16==TYPE_ALL && ((*mime_type)&0x00ff)!=SUBTYPE_ALL) {
		LOG(L_ERR,"ERROR:decode_mine_type: invalid mime format found "
			" <*/submime> in [%.*s]!!\n", end-start,start);
		return 0;
	}

	return p;
error:
	LOG(L_ERR,"ERROR:decode_mine_type: parse error near in [%.*s] char"
		"[%d][%c] offset=%d\n", end-start,start,*p,*p,(int)(p-start));
	return 0;
}



/* returns: > 0 mime found
 *          = 0 hdr not found
 *          =-1 error */
int parse_content_type_hdr( struct sip_msg *msg )
{
	char *end;
	char *ret;
	int  mime;

	/* is the header already found? */
	if ( msg->content_type==0 ) {
		/* if not, found it */
		if ( parse_headers(msg,HDR_CONTENTTYPE,0)==-1)
			goto error;
		if ( msg->content_type==0 ) {
			DBG("DEBUG:parse_content_type_hdr: missing Content-Type"
				"header\n");
			return 0;
		}
	}

	/* maybe the header is already parsed! */
	if ( msg->content_type->parsed!=0)
		return get_content_type(msg);

	/* it seams we have to parse it! :-( */
	end = msg->content_type->body.s + msg->content_type->body.len;
	ret = decode_mime_type(msg->content_type->body.s, end , &mime);
	if (ret==0)
		goto error;
	if (ret!=end) {
		LOG(L_ERR,"ERROR:parse_content_type_hdr: CONTENT_TYPE hdr containes "
			"more then one mime type :-(!\n");
		goto error;
	}
	if ((mime&0x00ff)==SUBTYPE_ALL || (mime>>16)==TYPE_ALL) {
		LOG(L_ERR,"ERROR:parse_content_type_hdr: invalid mime with wildcard "
			"'*' in Content-Type hdr!\n");
		goto error;
	}

	msg->content_type->parsed = (void*)mime;
	return mime;

error:
	return -1;
}



/* returns: > 0 ok
 *          = 0 hdr not found
 *          = -1 error */
int parse_accept_hdr( struct sip_msg *msg )
{
	static int mimes[MAX_MIMES_NR];
	int nr_mimes;
	int mime;
	char *end;
	char *ret;

	/* is the header already found? */
	if ( msg->accept==0 ) {
		/* if not, found it */
		if ( parse_headers(msg,HDR_ACCEPT,0)==-1)
			goto error;
		if ( msg->accept==0 ) {
			DBG("DEBUG:parse_accept_hdr: missing Accept header\n");
			return 0;
		}
	}

	/* maybe the header is already parsed! */
	if ( msg->accept->parsed!=0)
		return 1;

	/* it seams we have to parse it! :-( */
	ret = msg->accept->body.s;
	end = ret + msg->accept->body.len;
	nr_mimes = 0;
	while (1){
		ret = decode_mime_type(ret, end , &mime);
		if (ret==0)
			goto error;
		/* a new mime was found  -> put it into array */
		if (nr_mimes==MAX_MIMES_NR) {
			LOG(L_ERR,"ERROR:parse_accept_hdr: Accept hdr contains more than"
				" %d mime type -> buffer ovrflow!!\n",MAX_MIMES_NR);
			goto error;
		}
		mimes[nr_mimes++] = mime;
		/* is another mime following? */
		if (ret==end )
			break;
		/* parse the mime separator ',' */
		if (*ret!=',' || ret+1==end) {
			LOG(L_ERR,"ERROR:parse_accept_hdr: parse error between mimes at "
				"char <%x> (offset=%d) in <%.*s>!\n",
				*ret,ret-msg->accept->body.s,
				msg->accept->body.len,msg->accept->body.s);
			goto error;
		}
		/* skip the ',' */
		ret++;
	}

	/* copy and link the mime buffer into the message */
	msg->accept->parsed = (void*)pkg_malloc((nr_mimes+1)*sizeof(int));
	if (msg->accept->parsed==0) {
		LOG(L_ERR,"ERROR:parse_accept_hdr: no more pkg memory\n");
		goto error;
	}
	memcpy(msg->accept->parsed,mimes,nr_mimes*sizeof(int));
	/* make the buffer null terminated */
	((int*)msg->accept->parsed)[nr_mimes] = 0;

	return 1;
error:
	return -1;
}


/*
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
 *
 */

/*! \file
 * \brief Parser :: Content part
 *
 * \ingroup parser
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
	(isalpha((int)_c_) || (_c_)=='-' || (_c_)=='+' || (_c_)=='.')
#define is_char_equal(_c_,_cs_) \
	( (isalpha((int)_c_)?(((_c_)|0x20)==(_cs_)):((_c_)==(_cs_)))==1 )


/*! \brief
 * Node of the type's tree; this tree contains all the known types;
 */
typedef struct type_node_s {
	char c;                      /*!< char contained by this node */
	unsigned char final;         /*!< says what mime type/subtype was detected
	                              *!< if string ends at this node */
	unsigned char nr_sons;       /*!< the number of sub-nodes */
	int next;                    /*!< the next sibling node */
}type_node_t;


static type_node_t type_tree[] = {
	{'t',TYPE_UNKNOWN,1,4}, /* 0 */
		{'e',TYPE_UNKNOWN,1,-1},
			{'x',TYPE_UNKNOWN,1,-1},
				{'t',TYPE_TEXT,0,-1},
	{'m',TYPE_UNKNOWN,2,19}, /* 4 */
		{'e',TYPE_UNKNOWN,1,11}, /* 5 */
			{'s',TYPE_UNKNOWN,1,-1},
				{'s',TYPE_UNKNOWN,1,-1},
					{'a',TYPE_UNKNOWN,1,-1},
						{'g',TYPE_UNKNOWN,1,-1},
							{'e',TYPE_MESSAGE,0,-1},
		{'u',TYPE_UNKNOWN,1,-1}, /* 11 */
			{'l',TYPE_UNKNOWN,1,-1},
				{'t',TYPE_UNKNOWN,1,-1},
					{'i',TYPE_UNKNOWN,1,-1},
						{'p',TYPE_UNKNOWN,1,-1},
							{'a',TYPE_UNKNOWN,1,-1},
								{'r',TYPE_UNKNOWN,1,-1},
									{'t',TYPE_MULTIPART,0,-1},
	{'a',TYPE_UNKNOWN,1,-1}, /* 19 */
		{'p',TYPE_UNKNOWN,1,-1},
			{'p',TYPE_UNKNOWN,1,-1},
				{'l',TYPE_UNKNOWN,1,-1},
					{'i',TYPE_UNKNOWN,1,-1},
						{'c',TYPE_UNKNOWN,1,-1},
							{'a',TYPE_UNKNOWN,1,-1},
								{'t',TYPE_UNKNOWN,1,-1},
									{'i',TYPE_UNKNOWN,1,-1},
										{'o',TYPE_UNKNOWN,1,-1},
											{'n',TYPE_APPLICATION,0,-1},
	};

static type_node_t subtype_tree[] = {
	{'p',SUBTYPE_UNKNOWN,2,13},
		{'l',SUBTYPE_UNKNOWN,1,5},
			{'a',SUBTYPE_UNKNOWN,1,-1},
				{'i',SUBTYPE_UNKNOWN,1,-1},
					{'n',SUBTYPE_PLAIN,0,-1},
		{'i',SUBTYPE_UNKNOWN,1,-1}, /* 5 */
			{'d',SUBTYPE_UNKNOWN,1,-1},
				{'f',SUBTYPE_UNKNOWN,1,-1},
					{'+',TYPE_UNKNOWN,1,-1},
						{'x',TYPE_UNKNOWN,1,-1},
							{'m',TYPE_UNKNOWN,1,-1},
								{'l',SUBTYPE_PIDFXML,0,-1},
									{'l',SUBTYPE_PIDFXML,0,-1},
	{'s',SUBTYPE_UNKNOWN,1,16}, /* 13 */
		{'d',SUBTYPE_UNKNOWN,1,-1},
			{'p',SUBTYPE_SDP,0,-1},
	{'c',SUBTYPE_UNKNOWN,1,34}, /* 16 */
		{'p',SUBTYPE_UNKNOWN,2,-1},
			{'i',SUBTYPE_UNKNOWN,1,29},
				{'m',SUBTYPE_CPIM,1,-1},
					{'-',SUBTYPE_UNKNOWN,1,-1},
						{'p',SUBTYPE_UNKNOWN,1,-1},
							{'i',SUBTYPE_UNKNOWN,1,-1},
								{'d',SUBTYPE_UNKNOWN,1,-1},
									{'f',SUBTYPE_UNKNOWN,1,-1},
										{'+',SUBTYPE_UNKNOWN,1,-1},
											{'x',SUBTYPE_UNKNOWN,1,-1},
												{'m',SUBTYPE_UNKNOWN,1,-1},
													{'l',SUBTYPE_CPIM_PIDFXML,0,-1},
			{'l',SUBTYPE_UNKNOWN,1,-1}, /* 29 */
				{'+',TYPE_UNKNOWN,1,-1},
					{'x',TYPE_UNKNOWN,1,-1},
						{'m',TYPE_UNKNOWN,1,-1},
							{'l',SUBTYPE_CPLXML,0,-1},
	{'r',SUBTYPE_UNKNOWN,2,48}, /* 34 */
		{'l',SUBTYPE_UNKNOWN,1,42},/* 35 */
			{'m',SUBTYPE_UNKNOWN,1,-1},
				{'i',SUBTYPE_UNKNOWN,1,-1},
					{'+',TYPE_UNKNOWN,1,-1},
						{'x',TYPE_UNKNOWN,1,-1},
							{'m',TYPE_UNKNOWN,1,-1},
								{'l',SUBTYPE_RLMIXML,0,-1},
		{'e',SUBTYPE_UNKNOWN,1,-1}, /* 42 */
			{'l',SUBTYPE_UNKNOWN,1,-1},
				{'a',SUBTYPE_UNKNOWN,1,-1},
					{'t',SUBTYPE_UNKNOWN,1,-1},
						{'e',SUBTYPE_UNKNOWN,1,-1},
							{'d',SUBTYPE_RELATED,0,-1},
	{'l',SUBTYPE_UNKNOWN,1,57}, /* 48 */
		{'p',SUBTYPE_UNKNOWN,1,-1},
			{'i',SUBTYPE_UNKNOWN,1,-1},
				{'d',SUBTYPE_UNKNOWN,1,-1},
					{'f',SUBTYPE_UNKNOWN,1,-1},
						{'+',SUBTYPE_UNKNOWN,1,-1},
							{'x',SUBTYPE_UNKNOWN,1,-1},
								{'m',SUBTYPE_UNKNOWN,1,-1},
									{'l',SUBTYPE_LPIDFXML,0,-1},
	{'w',SUBTYPE_UNKNOWN,1,72}, /* 57 */
		{'a',SUBTYPE_UNKNOWN,1,-1},
			{'t',SUBTYPE_UNKNOWN,1,-1},
				{'c',SUBTYPE_UNKNOWN,1,-1},
					{'h',SUBTYPE_UNKNOWN,1,-1},
						{'e',SUBTYPE_UNKNOWN,1,-1},
							{'r',SUBTYPE_UNKNOWN,1,-1},
								{'i',TYPE_UNKNOWN,1,-1},
									{'n',TYPE_UNKNOWN,1,-1},
										{'f',TYPE_UNKNOWN,1,-1},
											{'o',TYPE_UNKNOWN,1,-1},
												{'+',TYPE_UNKNOWN,1,-1},
													{'x',TYPE_UNKNOWN,1,-1},
														{'m',TYPE_UNKNOWN,1,-1},
															{'l',SUBTYPE_WATCHERINFOXML,0,-1},
	{'x',SUBTYPE_UNKNOWN,2,94}, /* 72 */
		{'p',SUBTYPE_UNKNOWN,1,81}, /* 73 */
			{'i',SUBTYPE_UNKNOWN,1,-1},
				{'d',SUBTYPE_UNKNOWN,1,-1},
					{'f',SUBTYPE_UNKNOWN,1,-1},
						{'+',SUBTYPE_UNKNOWN,1,-1},
							{'x',SUBTYPE_UNKNOWN,1,-1},
								{'m',SUBTYPE_UNKNOWN,1,-1},
									{'l',SUBTYPE_XPIDFXML,0,-1},
		{'m',SUBTYPE_UNKNOWN,1,-1}, /* 81 */
			{'l',SUBTYPE_UNKNOWN,1,-1},
				{'+',SUBTYPE_UNKNOWN,1,-1},
					{'m',SUBTYPE_UNKNOWN,1,-1},
						{'s',SUBTYPE_UNKNOWN,1,-1},
							{'r',SUBTYPE_UNKNOWN,1,-1},
								{'t',SUBTYPE_UNKNOWN,1,-1},
									{'c',SUBTYPE_UNKNOWN,1,-1},
										{'.',SUBTYPE_UNKNOWN,1,-1},
											{'p',SUBTYPE_UNKNOWN,1,-1},
												{'i',SUBTYPE_UNKNOWN,1,-1}, 
													{'d',SUBTYPE_UNKNOWN,1,-1},
														{'f',SUBTYPE_XML_MSRTC_PIDF,0,-1},
	{'e',SUBTYPE_UNKNOWN,1,107}, /* 94 */
		{'x',SUBTYPE_UNKNOWN,1,-1},
			{'t',SUBTYPE_UNKNOWN,1,-1},
				{'e',SUBTYPE_UNKNOWN,1,-1},
					{'r',SUBTYPE_UNKNOWN,1,-1},
						{'n',SUBTYPE_UNKNOWN,1,-1},
							{'a',SUBTYPE_UNKNOWN,1,-1},
								{'l',SUBTYPE_UNKNOWN,1,-1},
									{'-',SUBTYPE_UNKNOWN,1,-1},
										{'b',SUBTYPE_UNKNOWN,1,-1},
											{'o',SUBTYPE_UNKNOWN,1,-1},
												{'d',SUBTYPE_UNKNOWN,1,-1},
													{'y',SUBTYPE_EXTERNAL_BODY,0,-1},
	{'m',SUBTYPE_UNKNOWN,1,112}, /* 107 */
		{'i',SUBTYPE_UNKNOWN,1,-1},
			{'x',SUBTYPE_UNKNOWN,1,-1},
				{'e',SUBTYPE_UNKNOWN,1,-1},
					{'d',SUBTYPE_MIXED,0,-1},
	{'i',SUBTYPE_UNKNOWN,1,-1}, /* 112 */
		{'s',SUBTYPE_UNKNOWN,1,-1},
			{'u',SUBTYPE_UNKNOWN,1,-1},
				{'p',SUBTYPE_ISUP,0,-1},
};



char* parse_content_length(char* const buffer, const char* const end, int* const length)
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



char* decode_mime_type(char* const start, const char* const end, unsigned int* const mime_type)
{
	int node;
	char *mark;
	char *p;
	unsigned int type_candidate;

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
		type_candidate = TYPE_UNKNOWN;
		while (p<end && is_mime_char(*p)  ) {
			while ( node!=-1 && !is_char_equal(*p,type_tree[node].c) ){
				node = type_tree[node].next;
			}
			if (node!=-1) {
				type_candidate = type_tree[node].final;
				if (type_tree[node].nr_sons)
					node++;
				else
					node = -1;
			} else {
				/* end of the type tree has reached,
				but the type has still some remaining
				characters (Miklos) */
				type_candidate = TYPE_UNKNOWN;
			}
			p++;
		}
		if (p==end || mark==p)
			goto error;
		*mime_type = type_candidate<<16;
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
		type_candidate = SUBTYPE_UNKNOWN;
		while (p<end && is_mime_char(*p) ) {
			while(node!=-1 && !is_char_equal(*p,subtype_tree[node].c) )
				node = subtype_tree[node].next;
			if (node!=-1) {
				type_candidate = subtype_tree[node].final;
				if (subtype_tree[node].nr_sons)
        				node++;
				else
					node = -1;
			} else {
				/* end of the subtype tree has reached,
				but the subtype has still some remaining
				characters (Miklos) */
				type_candidate = SUBTYPE_UNKNOWN;
			}
			p++;
		}
		if (p==mark)
			goto error;
		*mime_type |= type_candidate;;
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
		LOG(L_ERR,"ERROR:decode_mime_type: invalid mime format found "
			" <*/submime> in [%.*s]!!\n", (int)(end-start),start);
		return 0;
	}

	return p;
error:
	LOG(L_ERR,"ERROR:decode_mime_type: parse error near in [%.*s] char"
		"[%d][%c] offset=%d\n", (int)(end-start),start,*p,*p,(int)(p-start));
	return 0;
}



/*! \brief
 * \return
 *  	-   > 0 mime found
 *      -   = 0 hdr not found
 *      -   =-1 error */
int parse_content_type_hdr(struct sip_msg* const msg)
{
	char *end;
	const char *ret;
	unsigned int  mime;

	/* is the header already found? */
	if ( msg->content_type==0 ) {
		/* if not, found it */
		if ( parse_headers(msg, HDR_CONTENTTYPE_F, 0)==-1)
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
		LOG(L_ERR,"ERROR:parse_content_type_hdr: CONTENT_TYPE hdr contains "
			"more then one mime type :-(!\n");
		goto error;
	}
	if ((mime&0x00ff)==SUBTYPE_ALL || (mime>>16)==TYPE_ALL) {
		LOG(L_ERR,"ERROR:parse_content_type_hdr: invalid mime with wildcard "
			"'*' in Content-Type hdr!\n");
		goto error;
	}

	msg->content_type->parsed = (void*)(unsigned long)mime;
	return mime;

error:
	return -1;
}

int parse_accept_body(struct hdr_field* const hdr)
{
	static unsigned int mimes[MAX_MIMES_NR];
	int nr_mimes;
	unsigned int mime;
	char *end;
	char *ret;

	if (!hdr) return -1;
	
	/* maybe the header is already parsed! */
	if (hdr->parsed!=0) return 1;
	
	/* it seams we have to parse it! :-( */
	ret = hdr->body.s;
	end = ret + hdr->body.len;
	nr_mimes = 0;
	while (1){
		ret = decode_mime_type(ret, end , &mime);
		if (ret==0)
			goto error;
		/* a new mime was found  -> put it into array */
		if (nr_mimes==MAX_MIMES_NR) {
			LOG(L_ERR,"ERROR:parse_accept_hdr: Accept hdr contains more than"
				" %d mime type -> buffer overflow!!\n",MAX_MIMES_NR);
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
				*ret, (int)(ret-hdr->body.s),
				hdr->body.len, hdr->body.s);
			goto error;
		}
		/* skip the ',' */
		ret++;
	}

	/* copy and link the mime buffer into the message */
	hdr->parsed = (void*)pkg_malloc((nr_mimes+1)*sizeof(int));
	if (hdr->parsed==0) {
		LOG(L_ERR,"ERROR:parse_accept: no more pkg memory\n");
		goto error;
	}
	memcpy(hdr->parsed,mimes,nr_mimes*sizeof(int));
	/* make the buffer null terminated */
	((int*)hdr->parsed)[nr_mimes] = 0;

	return 1;
error:
	return -1;
}

/*! \brief
 * returns: > 0 ok
 *          = 0 hdr not found
 *          = -1 error */
int parse_accept_hdr(struct sip_msg* const msg)
{
	static unsigned int mimes[MAX_MIMES_NR];
	int nr_mimes;
	unsigned int mime;
	char *end;
	char *ret;

	/* is the header already found? */
	if ( msg->accept==0 ) {
		/* if not, found it */
		if ( parse_headers(msg, HDR_ACCEPT_F, 0)==-1)
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
				" %d mime type -> buffer overflow!!\n",MAX_MIMES_NR);
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
				*ret, (int)(ret-msg->accept->body.s),
				msg->accept->body.len, msg->accept->body.s);
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


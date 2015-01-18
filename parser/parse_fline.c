/*
 * sip first line parsing automaton
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
 *
 */

/*! \file
 * \brief Parser :: SIP first line parsing automaton
 *
 * \ingroup parser
 */


#include "../comp_defs.h"
#include "../dprint.h"
#include "msg_parser.h"
#include "parser_f.h"
#include "../mem/mem.h"
#include "../ut.h"

/* flags for first line
 * - stored on a short field (16 flags) */
#define FLINE_FLAG_PROTO_SIP	(1<<0)
#define FLINE_FLAG_PROTO_HTTP	(1<<1)

int http_reply_parse = 0;

/* grammar:
	request  =  method SP uri SP version CRLF
	response =  version SP status  SP reason  CRLF
	(version = "SIP/2.0")
*/


/* parses the first line, returns pointer to  next line  & fills fl;
   also  modifies buffer (to avoid extra copy ops) */
char* parse_first_line(char* buffer, unsigned int len, struct msg_start* fl)
{
	
	char *tmp;
	char* second;
	char* third;
	char* nl;
	int offset;
	/* int l; */
	char* end;
	char s1,s2,s3;
	char *prn;
	unsigned int t;

	/* grammar:
		request  =  method SP uri SP version CRLF
		response =  version SP status  SP reason  CRLF
		(version = "SIP/2.0")
	*/
	

	memset(fl, 0, sizeof(struct msg_start));
	offset = 0;
	end=buffer+len;
	/* see if it's a reply (status) */

	/* jku  -- parse well-known methods */

	/* drop messages which are so short they are for sure useless;
           utilize knowledge of minimum size in parsing the first
	   token 
        */
	if (len <=16 ) {
		LOG(L_INFO, "ERROR: parse_first_line: message too short: %d\n", len);
		goto error1;
	}
	tmp=buffer;
  	/* is it perhaps a reply, ie does it start with "SIP...." ? */
	if ( 	(*tmp=='S' || *tmp=='s') && 
		strncasecmp( tmp+1, SIP_VERSION+1, SIP_VERSION_LEN-1)==0 &&
		(*(tmp+SIP_VERSION_LEN)==' ')) {
			fl->type=SIP_REPLY;
			fl->flags|=FLINE_FLAG_PROTO_SIP;
			fl->u.reply.version.len=SIP_VERSION_LEN;
			tmp=buffer+SIP_VERSION_LEN;
	} else if (http_reply_parse != 0 &&
		 	(*tmp=='H' || *tmp=='h') &&
			/* 'HTTP/1.' */
			strncasecmp( tmp+1, HTTP_VERSION+1, HTTP_VERSION_LEN-1)==0 &&
			/* [0|1] */
			((*(tmp+HTTP_VERSION_LEN)=='0') || (*(tmp+HTTP_VERSION_LEN)=='1')) &&
			(*(tmp+HTTP_VERSION_LEN+1)==' ')  ){ 
			/* ugly hack to be able to route http replies
			 * Note: - the http reply must have a via
			 *       - the message is marked as SIP_REPLY (ugly)
			 */
				fl->type=SIP_REPLY;
				fl->flags|=FLINE_FLAG_PROTO_HTTP;
				fl->u.reply.version.len=HTTP_VERSION_LEN+1 /*include last digit*/;
				tmp=buffer+HTTP_VERSION_LEN+1 /* last digit */;
	} else IFISMETHOD( INVITE, 'I' )
	else IFISMETHOD( CANCEL, 'C')
	else IFISMETHOD( ACK, 'A' )
	else IFISMETHOD( BYE, 'B' ) 
	else IFISMETHOD( INFO, 'I' )
	else IFISMETHOD( REGISTER, 'R')
	else IFISMETHOD( SUBSCRIBE, 'S')
	else IFISMETHOD( NOTIFY, 'N')
	else IFISMETHOD( MESSAGE, 'M')
	else IFISMETHOD( OPTIONS, 'O')
	else IFISMETHOD( PRACK, 'P')
	else IFISMETHOD( UPDATE, 'U')
	else IFISMETHOD( REFER, 'R')
	else IFISMETHOD( PUBLISH, 'P')
	/* if you want to add another method XXX, include METHOD_XXX in
           H-file (this is the value which you will take later in
           processing and define XXX_LEN as length of method name;
	   then just call IFISMETHOD( XXX, 'X' ) ... 'X' is the first
	   latter; everything must be capitals
	*/
	else {
		/* neither reply, nor any of known method requests, 
		   let's believe it is an unknown method request
        	*/
		tmp=eat_token_end(buffer,buffer+len);
		if ((tmp==buffer)||(tmp>=end)){
			LOG(L_INFO, "ERROR:parse_first_line: empty  or bad first line\n");
			goto error1;
		}
		if (*tmp!=' ') {
			LOG(L_INFO, "ERROR:parse_first_line: method not followed by SP\n");
			goto error1;
		}
		fl->type=SIP_REQUEST;
		fl->u.request.method_value=METHOD_OTHER;
		fl->u.request.method.len=tmp-buffer;
	}


	/* identifying type of message over now; 
	   tmp points at space after; go ahead */

	fl->u.request.method.s=buffer;  /* store ptr to first token */
	second=tmp+1;			/* jump to second token */
	offset=second-buffer;

/* EoJku */
	
	/* next element */
	tmp=eat_token_end(second, second+len-offset);
	if (tmp>=end){
		goto error;
	}
	offset+=tmp-second;
	third=eat_space_end(tmp, tmp+len-offset);
	offset+=third-tmp;
	if ((third==tmp)||(tmp>=end)){
		goto error;
	}
	fl->u.request.uri.s=second;
	fl->u.request.uri.len=tmp-second;

	/* jku: parse status code */
	if (fl->type==SIP_REPLY) {
		if (fl->u.request.uri.len!=3) {
			LOG(L_INFO, "ERROR:parse_first_line: len(status code)!=3: %.*s\n",
				fl->u.request.uri.len, ZSW(second) );
			goto error;
		}
		s1=*second; s2=*(second+1);s3=*(second+2);
		if (s1>='0' && s1<='9' && 
		    s2>='0' && s2<='9' &&
		    s3>='0' && s3<='9' ) {
			fl->u.reply.statuscode=(s1-'0')*100+10*(s2-'0')+(s3-'0');
		} else {
			LOG(L_INFO, "ERROR:parse_first_line: status_code non-numerical: %.*s\n",
				fl->u.request.uri.len, ZSW(second) );
			goto error;
		}
	}
	/* EoJku */

	/*  last part: for a request it must be the version, for a reply
	 *  it can contain almost anything, including spaces, so we don't care
	 *  about it*/
	if (fl->type==SIP_REQUEST){
		tmp=eat_token_end(third,third+len-offset);
		offset+=tmp-third;
		if ((tmp==third)||(tmp>=end)){
			goto error;
		}
		if (! is_empty_end(tmp, tmp+len-offset)){
			goto error;
		}
	}else{
		tmp=eat_token2_end(third,third+len-offset,'\r'); /* find end of line 
												  ('\n' or '\r') */
		if (tmp>=end){ /* no crlf in packet => invalid */
			goto error;
		}
		offset+=tmp-third;
	}
	nl=eat_line(tmp,len-offset);
	if (nl>=end){ /* no crlf in packet or only 1 line > invalid */
		goto error;
	}
	fl->u.request.version.s=third;
	fl->u.request.version.len=tmp-third;
	fl->len=nl-buffer;

	if (fl->type==SIP_REQUEST) {
		if(fl->u.request.version.len >= SIP_VERSION_LEN
				&& (fl->u.request.version.s[0]=='S'
					|| fl->u.request.version.s[0]=='s')
				&& !strncasecmp(fl->u.request.version.s+1,
					SIP_VERSION+1, SIP_VERSION_LEN-1)) {
			fl->flags|=FLINE_FLAG_PROTO_SIP;
		} else if(fl->u.request.version.len >= HTTP_VERSION_LEN
				&& (fl->u.request.version.s[0]=='H'
					|| fl->u.request.version.s[0]=='h')
				&& !strncasecmp(fl->u.request.version.s+1,
					HTTP_VERSION+1, HTTP_VERSION_LEN-1)) {
			fl->flags|=FLINE_FLAG_PROTO_HTTP;
		}
	}

	return nl;

error:
	LOG(L_DBG, "parse_first_line: bad %s first line\n",
		(fl->type==SIP_REPLY)?"reply(status)":"request");

	LOG(L_DBG, "at line 0 char %d: \n", offset );
	prn=pkg_malloc( offset );
	if (prn) {
		for (t=0; t<offset; t++)
			if (*(buffer+t)) *(prn+t)=*(buffer+t);
			else *(prn+t)=176; /* '°' */
		LOG(L_DBG, "parsed so far: %.*s\n", offset, ZSW(prn) );
		pkg_free( prn );
	};
error1:
	fl->type=SIP_INVALID;
	LOG(L_ERR, "parse_first_line: bad message (offset: %d)\n", offset);
	/* skip  line */
	nl=eat_line(buffer,len);
	return nl;
}

char* parse_fline(char* buffer, char* end, struct msg_start* fl)
{
	if(end<=buffer) {
		/* make it throw error via parse_first_line() for consistency */
		return parse_first_line(buffer, 0, fl);
	}
	return parse_first_line(buffer, (unsigned int)(end-buffer), fl);
}

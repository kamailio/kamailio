/*
 * $Id$
 *
 * sip msg. header proxy parser 
 *
 */

#include <string.h>
#include <stdlib.h>

#include "msg_parser.h"
#include "parser_f.h"
#include "ut.h"
#include "error.h"
#include "dprint.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif



#define DEBUG



/* parses the first line, returns pointer to  next line  & fills fl;
   also  modifies buffer (to avoid extra copy ops) */
char* parse_first_line(char* buffer, unsigned int len, struct msg_start * fl)
{
	
	char *tmp;
	char* second;
	char* third;
	char* nl;
	int offset;
	int l;
	char* end;
	
	/* grammar:
		request  =  method SP uri SP version CRLF
		response =  version SP status  SP reason  CRLF
		(version = "SIP/2.0")
	*/
	

	end=buffer+len;
	/* see if it's a reply (status) */
	tmp=eat_token(buffer, len);
	if ((tmp==buffer)||(tmp>=end)){
		LOG(L_INFO, "ERROR:parse_first_line: empty  or bad first line\n");
		goto error1;
	}
	l=tmp-buffer;
	if ((SIP_VERSION_LEN==l) &&
		(memcmp(buffer,SIP_VERSION,l)==0)){
		
		fl->type=SIP_REPLY;
	}else{
		fl->type=SIP_REQUEST;
	}
	
	offset=l;
	second=eat_space(tmp, len-offset);
	offset+=second-tmp;
	if ((second==tmp)||(tmp>=end)){
		goto error;
	}
	*tmp=0; /* mark the end of the token */
	fl->u.request.method.s=buffer;
	fl->u.request.method.len=l;
	
	/* next element */
	tmp=eat_token(second, len-offset);
	if (tmp>=end){
		goto error;
	}
	offset+=tmp-second;
	third=eat_space(tmp, len-offset);
	offset+=third-tmp;
	if ((third==tmp)||(tmp>=end)){
		goto error;
	}
	*tmp=0; /* mark the end of the token */
	fl->u.request.uri.s=second;
	fl->u.request.uri.len=tmp-second;

	/*  last part: for a request it must be the version, for a reply
	 *  it can contain almost anything, including spaces, so we don't care
	 *  about it*/
	if (fl->type==SIP_REQUEST){
		tmp=eat_token(third,len-offset);
		offset+=tmp-third;
		if ((tmp==third)||(tmp>=end)){
			goto error;
		}
		if (! is_empty(tmp, len-offset)){
			goto error;
		}
	}else{
		tmp=eat_token2(third,len-offset,'\r'); /* find end of line 
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
	*tmp=0;
	fl->u.request.version.s=third;
	fl->u.request.version.len=tmp-third;
	
	return nl;

error:
	LOG(L_INFO, "ERROR:parse_first_line: bad %s first line\n", 
		(fl->type==SIP_REPLY)?"reply(status)":"request");
error1:
	fl->type=SIP_INVALID;
	LOG(L_INFO, "ERROR: at line 0 char %d\n", offset);
	/* skip  line */
	nl=eat_line(buffer,len);
	return nl;
}


#ifdef OLD_PARSER
/* returns integer field name type */
int field_name(char *s, int l)
{
	if (l<1) return HDR_OTHER;
	else if ((l==1) && ((*s=='v')||(*s=='V')))
		return HDR_VIA;
	else if (strcasecmp(s, "Via")==0)
		return  HDR_VIA;
/*	if ((strcmp(s, "To")==0)||(strcmp(s,"t")==0))
		return HDR_TO;*/
	return HDR_OTHER;
}
#endif


#ifndef OLD_PARSER
/* returns pointer to next header line, and fill hdr_f ;
 * if at end of header returns pointer to the last crlf  (always buf)*/
char* get_hdr_field(char* buf, unsigned int len, struct hdr_field* hdr)
{
	char* end;
	char* tmp;
	char *match;
	struct via_body *vb;

	end=buf+len;
	if ((*buf)=='\n' || (*buf)=='\r'){
		/* double crlf or lflf or crcr */
		DBG("found end of header\n");
		hdr->type=HDR_EOH;
		return buf;
	}

	tmp=parse_hname(buf, end, hdr);
	if (hdr->type==HDR_ERROR){
		LOG(L_ERR, "ERROR: get_hdr_field: bad header\n");
		goto error;
	}else if (hdr->type==HDR_VIA){
		vb=malloc(sizeof(struct via_body));
		if (vb==0){
			LOG(L_ERR, "get_hdr_field: out of memory\n");
			goto error;
		}
		memset(vb,0,sizeof(struct via_body));

		hdr->body.s=tmp;
		tmp=parse_via(tmp, end, vb);
		if (vb->error==VIA_PARSE_ERROR){
			LOG(L_ERR, "ERROR: get_hdr_field: bad via\n");
			free(vb);
			goto error;
		}
		hdr->parsed=vb;
		vb->hdr.s=hdr->name.s;
		vb->hdr.len=hdr->name.len;
		vb->size=tmp-hdr->name.s;
		hdr->body.len=tmp-hdr->body.s;
	}else{
		/* just skip over it*/
		hdr->body.s=tmp;
		/* find lf*/
		match=q_memchr(tmp, '\n', end-tmp);
		if (match){
			/* null terminate*/
			*match=0;
			hdr->body.len=match-tmp;
			match++; /*skip*/
			tmp=match;
		}else {
			tmp=end;
			LOG(L_ERR, "ERROR: get_hdr_field: bad body for <%s>(%d)\n",
					hdr->name.s, hdr->type);
			goto error;
		}
	}
	return tmp;
error:
	DBG("get_hdr_field: error exit\n");
	hdr->type=HDR_ERROR;
	return tmp;
}

#else
/* returns pointer to next header line, and fill hdr_f */
char* get_hdr_field(char *buffer, unsigned int len, struct hdr_field*  hdr_f)
{
	/* grammar (rfc822):
		field = field-name ":" field-body CRLF
		field-body = text [ CRLF SP field-body ]
	   (CRLF in the field body must be removed)
	*/

	char* tmp, *tmp2;
	char* nl;
	char* body;
	int offset;
	int l;

	
	/* init content to the empty string */
	hdr_f->name.s="";
	hdr_f->name.len=0;
	hdr_f->body.s="";
	hdr_f->body.len=0;
	
	if ((*buffer=='\n')||(*buffer=='\r')){
		/* double crlf */
		tmp=eat_line(buffer,len);
		hdr_f->type=HDR_EOH;
		return tmp;
	}
	
	tmp=eat_token2(buffer, len, ':');
	if ((tmp==buffer) || (tmp-buffer==len) ||
		(is_empty(buffer, tmp-buffer))|| (*tmp!=':')){
		hdr_f->type=HDR_ERROR;
		goto error;
	}
	*tmp=0;
	/* take care of possible spaces (e.g: "Via  :") */
	tmp2=eat_token(buffer, tmp-buffer);
	/* in the worst case tmp2=buffer+tmp-buffer=tmp */
	*tmp2=0;
	l=tmp2-buffer;
	if (tmp2<tmp){
		tmp2++;
		/* catch things like: "Via foo bar:" */
		tmp2=eat_space(tmp2, tmp-tmp2);
		if (tmp2!=tmp){
			hdr_f->type=HDR_ERROR;
			goto error;
		}
	}

	hdr_f->type=field_name(buffer, l);
	body= ++tmp;
	hdr_f->name.s=buffer;
	hdr_f->name.len=l;
	offset=tmp-buffer;
	/* get all the lines in this field  body */
	do{
		nl=eat_line(tmp, len-offset);
		offset+=nl-tmp;
		tmp=nl;
	
	}while( (*tmp==' ' ||  *tmp=='\t') && (offset<len) );
	if (offset==len){
		hdr_f->type=HDR_ERROR;
		LOG(L_INFO, "ERROR: get_hdr_field: field body too  long\n");
		goto error;
	}
	*(tmp-1)=0; /* should be an LF */
	hdr_f->body.s=body;
	hdr_f->body.len=tmp-1-body;;
error:
	return tmp;
}
#endif


char* parse_hostport(char* buf, str* host, short int* port)
{
	char *tmp;
	int err;
	
	host->s=buf;
	for(tmp=buf;(*tmp)&&(*tmp!=':');tmp++);
	host->len=tmp-buf;
	if (*tmp==0){
		*port=0;
	}else{
		*tmp=0;
		*port=str2s(tmp+1, strlen(tmp+1), &err);
		if (err ){
			LOG(L_INFO, 
					"ERROR: hostport: trailing chars in port number: %s\n",
					tmp+1);
			/* report error? */
		}
	}
	return host->s;
}



/* buf= pointer to begining of uri (sip:x@foo.bar:5060;a=b?h=i)
   len= len of uri
returns: fills uri & returns <0 on error or 0 if ok */
int parse_uri(char *buf, int len, struct sip_uri* uri)
{
	char* next, *end;
	char *user, *passwd, *host, *port, *params, *headers;
	int host_len, port_len, params_len, headers_len;
	int ret;
	

	ret=0;
	end=buf+len;
	memset(uri, 0, sizeof(struct sip_uri)); /* zero it all, just to be sure */
	/* look for "sip:"*/;
	next=q_memchr(buf, ':',  len);
	if ((next==0)||(strncmp(buf,"sip",next-buf)!=0)){
		LOG(L_DBG, "ERROR: parse_uri: bad sip uri\n");
		ret=E_UNSPEC;
		goto error;
	}
	buf=next+1; /* next char after ':' */
	if (buf>end){
		LOG(L_DBG, "ERROR: parse_uri: uri too short\n");
		ret=E_UNSPEC;
		goto error;
	}
	/*look for '@' */
	next=q_memchr(buf,'@', end-buf);
	if (next==0){
		/* no '@' found, => no userinfo */
		uri->user.s=0;
		uri->passwd.s=0;
		host=buf;
	}else{
		/* found it */
		user=buf;
		/* try to find passwd */
		passwd=q_memchr(user,':', next-user);
		if (passwd==0){
			/* no ':' found => no password */
			uri->passwd.s=0;
			uri->user.s=(char*)malloc(next-user+1);
			if (uri->user.s==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->user.s, user, next-user);
			uri->user.len=next-user;
			uri->user.s[next-user]=0; /* null terminate it, 
									   usefull for easy printing*/
		}else{
			uri->user.s=(char*)malloc(passwd-user+1);
			if (uri->user.s==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->user.s, user, passwd-user);
			uri->user.len=passwd-user;
			uri->user.s[passwd-user]=0;
			passwd++; /*skip ':' */
			uri->passwd.s=(char*)malloc(next-passwd+1);
			if (uri->passwd.s==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->passwd.s, passwd, next-passwd);
			uri->passwd.len=next-passwd;
			uri->passwd.s[next-passwd]=0;
		}
		host=next+1; /* skip '@' */
	}
	/* try to find the rest */
	if(host>=end){
		LOG(L_DBG, "ERROR: parse_uri: missing hostport\n");
		ret=E_UNSPEC;
		goto error;
	}
	headers=q_memchr(host,'?',end-host);
	params=q_memchr(host,';',end-host);
	port=q_memchr(host,':',end-host);
	host_len=(port)?port-host:(params)?params-host:(headers)?headers-host:end-host;
	/* get host */
	uri->host.s=malloc(host_len+1);
	if (uri->host.s==0){
		LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memcpy(uri->host.s, host, host_len);
	uri->host.len=host_len;
	uri->host.s[host_len]=0;
	/* get port*/
	if ((port)&&(port+1<end)){
		port++;
		if ( ((params) &&(params<port))||((headers) &&(headers<port)) ){
			/* error -> invalid uri we found ';' or '?' before ':' */
			LOG(L_DBG, "ERROR: parse_uri: malformed sip uri\n");
			ret=E_UNSPEC;
			goto error;
		}
		port_len=(params)?params-port:(headers)?headers-port:end-port;
		uri->port.s=malloc(port_len+1);
		if (uri->port.s==0){
			LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(uri->port.s, port, port_len);
		uri->port.len=port_len;
		uri->port.s[port_len]=0;
	}else uri->port.s=0;
	/* get params */
	if ((params)&&(params+1<end)){
		params++;
		if ((headers) && (headers<params)){
			/* error -> invalid uri we found '?' or '?' before ';' */
			LOG(L_DBG, "ERROR: parse_uri: malformed sip uri\n");
			ret=E_UNSPEC;
			goto error;
		}
		params_len=(headers)?headers-params:end-params;
		uri->params.s=malloc(params_len+1);
		if (uri->params.s==0){
			LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(uri->params.s, params, params_len);
		uri->params.len=params_len;
		uri->params.s[params_len]=0;
	}else uri->params.s=0;
	/*get headers */
	if ((headers)&&(headers+1<end)){
		headers++;
		headers_len=end-headers;
		uri->headers.s=malloc(headers_len+1);
		if(uri->headers.s==0){
			LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(uri->headers.s, headers, headers_len);
		uri->headers.len=headers_len;
		uri->headers.s[headers_len]=0;
	}else uri->headers.s=0;
	
	return ret;
error:
	return ret;
}



/* parses a via body, returns next via (for compact vias) & fills vb,
 * the buffer should be null terminated! */
char* parse_via_body(char* buffer,unsigned int len, struct via_body * vb)
{
	/* format: sent-proto sent-by  *(";" params) [comment] 

	           sent-proto = name"/"version"/"transport
		   sent-by    = host [":" port]
		   
	*/

	char* tmp;
	char *name,*version, *transport, *comment, *params, *hostport;
	int name_len, version_len, transport_len, comment_len, params_len;
	char * next_via;
	str host;
	short int port;
	int offset;
	

	name=version=transport=comment=params=hostport=next_via=host.s=0;
	name_len=version_len=transport_len=comment_len=params_len=host.len=0;
	name=eat_space(buffer, len);
	if (name-buffer==len) goto error;
	offset=name-buffer;
	tmp=name;

	version=eat_token2(tmp,len-offset,'/');
	if (version+1-buffer>=len) goto error;
	*version=0;
	name_len=version-name;
	version++;
	offset+=version-tmp;
	
	transport=eat_token2(tmp,len-offset,'/');
	if (transport+1-buffer>=len) goto error;
	*transport=0;
	version_len=transport-version;
	transport++;
	offset+=transport-tmp;
	
	tmp=eat_token(transport,len-offset);
	if (tmp+1-buffer>=len) goto error;
	*tmp=0;
	transport_len=tmp-transport;
	tmp++;
	offset+=tmp-transport;
	
	hostport=eat_space(tmp,len-offset);
	if (hostport+1-buffer>=len) goto error;
	offset+=hostport-tmp;

	/* find end of hostport */
 	for(tmp=hostport; (tmp-buffer)<len &&
			(*tmp!=' ')&&(*tmp!=';')&&(*tmp!=','); tmp++);
	if (tmp-buffer<len){
		switch (*tmp){
			case ' ':
				*tmp=0;
				tmp++;
				/*the rest is comment? */
				if (tmp-buffer<len){
					comment=tmp;
					/* eat the comment */
					for(;((tmp-buffer)<len)&&
						(*tmp!=',');tmp++);
					/* mark end of compact via (also end of comment)*/
					comment_len=tmp-comment;
					if (tmp-buffer<len){
						*tmp=0;
					}else break;
					/* eat space & ',' */
					for(tmp=tmp+1;((tmp-buffer)<len)&&
						(*tmp==' '|| *tmp==',');tmp++);
					
				}
				break;

			case ';':
				*tmp=0;
				tmp++;
				if (tmp-buffer>=len) goto error;
				params=tmp;
				/* eat till end, first space  or ',' */
				for(;((tmp-buffer)<len)&&
					(*tmp!=' '&& *tmp!=',');tmp++);
				params_len=tmp-params;
				if (tmp-buffer==len)  break;
				if (*tmp==' '){
					/* eat comment */
					*tmp=0;
					tmp++;
					comment=tmp;
					for(;((tmp-buffer)<len)&&
						(*tmp!=',');tmp++);
					comment_len=tmp-comment;
					if (tmp-buffer==len)  break;
				}
				/* mark end of via*/
				*tmp=0;
				
				/* eat space & ',' */
				for(tmp=tmp+1;((tmp-buffer)<len)&&
					(*tmp==' '|| *tmp==',');tmp++);
				break;

			case ',':
				*tmp=0;
				tmp++;
				if (tmp-buffer<len){
					/* eat space and ',' */
					for(;((tmp-buffer)<len)&&
						(*tmp==' '|| *tmp==',');
					   tmp++);
				}
		}
	}
	/* if we are not at the end of the body => we found another compact via */
	if (tmp-buffer<len) next_via=tmp;
	
	/* parse hostport */
	parse_hostport(hostport, &host, &port);
	vb->name.s=name;
	vb->name.len=name_len;
	vb->version.s=version;
	vb->version.len=version_len;
	vb->transport.s=transport;
	vb->transport.len=transport_len;
	vb->host.s=host.s;
	vb->host.len=host.len;
	vb->port=port;
	vb->params.s=params;
	vb->params.len=params_len;
	vb->comment.s=comment;
	vb->comment.len=comment_len;
	vb->next=next_via;
	vb->error=VIA_PARSE_OK;

	
	/* tmp points to end of body or to next via (if compact)*/
	
	return tmp;

error:
	vb->error=VIA_PARSE_ERROR;
	return tmp;
}



/* returns 0 if ok, -1 for errors */
int parse_msg(char* buf, unsigned int len, struct sip_msg* msg)
{

	char *tmp, *bar;
	char* rest;
	char* first_via;
	char* second_via;
	struct msg_start *fl;
	struct hdr_field hf;
	struct via_body *vb1, *vb2;
	int offset;

#ifdef OLD_PARSER
	/* init vb1 & vb2 to the null string */
	/*memset(&vb1,0, sizeof(struct via_body));
	memset(&vb2,0, sizeof(struct via_body));*/
	vb1=&(msg->via1);
	vb2=&(msg->via2);
	vb1->error=VIA_PARSE_ERROR;
	vb2->error=VIA_PARSE_ERROR;
#else
	vb1=vb2=0;
#endif
	/* eat crlf from the beginning */
	for (tmp=buf; (*tmp=='\n' || *tmp=='\r')&&
			tmp-buf < len ; tmp++);
	offset=tmp-buf;
	fl=&(msg->first_line);
	rest=parse_first_line(tmp, len-offset, fl);
	offset+=rest-tmp;
	tmp=rest;
	switch(fl->type){
		case SIP_INVALID:
			DBG("parse_msg: invalid message\n");
			goto error;
			break;
		case SIP_REQUEST:
			DBG("SIP Request:\n");
			DBG(" method:  <%s>\n",fl->u.request.method);
			DBG(" uri:     <%s>\n",fl->u.request.uri);
			DBG(" version: <%s>\n",fl->u.request.version);
			break;
		case SIP_REPLY:
			DBG("SIP Reply  (status):\n");
			DBG(" version: <%s>\n",fl->u.reply.version);
			DBG(" status:  <%s>\n",fl->u.reply.status);
			DBG(" reason:  <%s>\n",fl->u.reply.reason);
			break;
		default:
			DBG("unknown type %d\n",fl->type);
	}
	
	/*find first Via: */
	hf.type=HDR_ERROR;
	first_via=0;
	second_via=0;
	do{
		rest=get_hdr_field(tmp, len-offset, &hf);
		offset+=rest-tmp;
		switch (hf.type){
			case HDR_ERROR:
				LOG(L_INFO,"ERROR: bad header  field\n");
				goto  error;
			case HDR_EOH: 
				goto skip;
			case HDR_VIA:
				if (first_via==0){
					first_via=hf.body.s;
#ifndef OLD_PARSER
					vb1=(struct via_body*)hf.parsed;
#else
						vb1->hdr.s=hf.name.s;
						vb1->hdr.len=hf.name.len;
						/* replace cr/lf with space in first via */
						for (bar=first_via;(first_via) && (*bar);bar++)
							if ((*bar=='\r')||(*bar=='\n'))	*bar=' ';
#endif
				#ifdef DEBUG
						DBG("first via: <%s>\n", first_via);
				#endif
#ifdef OLD_PARSER
						bar=parse_via_body(first_via, hf.body.len, vb1);
						if (vb1->error!=VIA_PARSE_OK){
							LOG(L_INFO, "ERROR: parsing via body: %s\n",
									first_via);
							goto error;
						}
						
						vb1->size=bar-first_via+first_via-vb1->hdr.s; 
						
#endif
						/* compact via */
						if (vb1->next) {
							second_via=vb1->next;
							/* not interested in the rest of the header */
							goto skip;
						}else{
#ifdef OLD_PARSER
						/*  add 1 (we don't see the trailing lf which
						 *  was zeroed by get_hfr_field) */
							vb1->size+=1;
#endif
						}
						if (fl->type!=SIP_REPLY) goto skip; /* we are interested
															  in the 2nd via 
															 only in replies */
				}else if (second_via==0){
							second_via=hf.body.s;
#ifndef OLD_PARSER
							vb2=hf.parsed;
#else
							vb2->hdr.s=hf.name.s;
							vb2->hdr.len=hf.name.len;
#endif
							goto skip;
				}
				break;
		}
	#ifdef DEBUG
		DBG("header field type %d, name=<%s>, body=<%s>\n",
			hf.type, hf.name.s, hf.body.s);
	#endif
		tmp=rest;
	}while(hf.type!=HDR_EOH && rest-buf < len);

skip:
	/* replace cr/lf with space in the second via */
#ifdef OLD_PARSER
	for (tmp=second_via;(second_via) && (*tmp);tmp++)
		if ((*tmp=='\r')||(*tmp=='\n'))	*tmp=' ';

	if (second_via) {
		tmp=parse_via_body(second_via, hf.body.len, vb2);
		if (vb2->error!=VIA_PARSE_OK){
			LOG(L_INFO, "ERROR: parsing via2 body: %s\n", second_via);
			goto error;
		}
		vb2->size=tmp-second_via; 
		if (vb2->next==0) vb2->size+=1; /* +1 from trailing lf */
		if (vb2->hdr.s) vb2->size+=second_via-vb2->hdr.s;
	}
#endif
	

#ifdef DEBUG
	/* dump parsed data */
	if (first_via){
		DBG(" first  via: <%s/%s/%s> <%s:%s(%d)>",
				vb1->name.s, vb1->version.s, vb1->transport.s, vb1->host.s,
				vb1->port_str, vb1->port);
		if (vb1->params.s)  DBG(";<%s>", vb1->params.s);
		if (vb1->comment.s) DBG(" <%s>", vb1->comment.s);
		DBG ("\n");
	}
#ifdef OLD_PARSER
	if (second_via){
		DBG(" second via: <%s/%s/%s> <%s:%d>",
				vb2->name.s, vb2->version.s, vb2->transport.s, vb2->host.s,
				vb2->port);
		if (vb2->params.s)  DBG(";<%s>", vb2->params.s);
		if (vb2->comment.s) DBG(" <%s>", vb2->comment.s);
		DBG("\n");
	}
#endif
#endif
	
	/* copy data into msg */
#ifndef OLD_PARSER
	memcpy(&(msg->via1), vb1, sizeof(struct via_body));
	if (second_via) memcpy(&(msg->via2), vb2, sizeof(struct via_body));
	if (vb1) free(vb1);
	if (vb2) free(vb1);
#endif

#ifdef DEBUG
	DBG("exiting parse_msg\n");
#endif

	return 0;
	
error:
#ifndef OLD_PARSER
	if (vb1) free(vb1);
	if (vb2) free(vb1);
#endif
	return -1;
}



void free_uri(struct sip_uri* u)
{
	if (u){
		if (u->user.s) free(u->user.s);
		if (u->passwd.s) free(u->passwd.s);
		if (u->host.s) free(u->host.s);
		if (u->port.s) free(u->port.s);
		if (u->params.s) free(u->params.s);
		if (u->headers.s) free(u->headers.s);
	}
}

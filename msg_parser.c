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
#include "error.h"
#include "dprint.h"



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
	if ((strlen(SIP_VERSION)==(tmp-buffer)) &&
		(memcmp(buffer,SIP_VERSION,tmp-buffer)==0)){
		
		fl->type=SIP_REPLY;
	}else{
		fl->type=SIP_REQUEST;
	}
	
	offset=tmp-buffer;
	second=eat_space(tmp, len-offset);
	offset+=second-tmp;
	if ((second==tmp)||(tmp>=end)){
		goto error;
	}
	*tmp=0; /* mark the end of the token */
	fl->u.request.method=buffer;
	
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
	fl->u.request.uri=second;
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
	fl->u.request.version=third;
	
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


/* returns integer field name type */
int field_name(char *s)
{
	int l;
	l=strlen(s);
	if (l<1) return HDR_OTHER;
	else if ((l==1) && ((*s=='v')||(*s=='V')))
		return HDR_VIA;
	else if (strcasecmp(s, "Via")==0)
		return  HDR_VIA;
/*	if ((strcmp(s, "To")==0)||(strcmp(s,"t")==0))
		return HDR_TO;*/
	return HDR_OTHER;
}




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

	
	/* init content to the empty string */
	hdr_f->name="";
	hdr_f->body="";
	
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
	if (tmp2<tmp){
		tmp2++;
		/* catch things like: "Via foo bar:" */
		tmp2=eat_space(tmp2, tmp-tmp2);
		if (tmp2!=tmp){
			hdr_f->type=HDR_ERROR;
			goto error;
		}
	}

	hdr_f->type=field_name(buffer);
	body= ++tmp;
	hdr_f->name=buffer;
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
	hdr_f->body=body;
error:
	return tmp;
}



char* parse_hostport(char* buf, char** host, short int* port)
{
	char *tmp;
	char *invalid;
	
	*host=buf;
	for(tmp=buf;(*tmp)&&(*tmp!=':');tmp++);
	if (*tmp==0){
		*port=0;
	}else{
		*tmp=0;
		invalid=0;
		*port=strtol(tmp+1, &invalid, 10);
		if ((invalid!=0)&&(*invalid)){
			LOG(L_INFO, 
					"ERROR: hostport: trailing chars in port number: %s(%x)\n",
					invalid, invalid);
			/* report error? */
		}
	}
	return *host;
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
	next=memchr(buf, ':',  len);
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
	next=memchr(buf,'@', end-buf);
	if (next==0){
		/* no '@' found, => no userinfo */
		uri->user=0;
		uri->passwd=0;
		host=buf;
	}else{
		/* found it */
		user=buf;
		/* try to find passwd */
		passwd=memchr(user,':', next-user);
		if (passwd==0){
			/* no ':' found => no password */
			uri->passwd=0;
			uri->user=(char*)malloc(next-user+1);
			if (uri->user==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->user,user, next-user);
			uri->user[next-user]=0; /* null terminate it, usefull for easy printing*/
		}else{
			uri->user=(char*)malloc(passwd-user+1);
			if (uri->user==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->user,user, passwd-user);
			uri->user[passwd-user]=0;
			passwd++; /*skip ':' */
			uri->passwd=(char*)malloc(next-passwd+1);
			if (uri->passwd==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->passwd,passwd, next-passwd);
			uri->passwd[next-passwd]=0;
		}
		host=next+1; /* skip '@' */
	}
	/* try to find the rest */
	if(host>=end){
		LOG(L_DBG, "ERROR: parse_uri: missing hostport\n");
		ret=E_UNSPEC;
		goto error;
	}
	headers=memchr(host,'?',end-host);
	params=memchr(host,';',end-host);
	port=memchr(host,':',end-host);
	host_len=(port)?port-host:(params)?params-host:(headers)?headers-host:end-host;
	/* get host */
	uri->host=malloc(host_len+1);
	if (uri->host==0){
		LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memcpy(uri->host,host, host_len);
	uri->host[host_len]=0;
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
		uri->port=malloc(port_len+1);
		if (uri->port==0){
			LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(uri->port, port, port_len);
		uri->port[port_len]=0;
	}else uri->port=0;
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
		uri->params=malloc(params_len+1);
		if (uri->params==0){
			LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(uri->params, params, params_len);
		uri->params[params_len]=0;
	}else uri->params=0;
	/*get headers */
	if ((headers)&&(headers+1<end)){
		headers++;
		headers_len=end-headers;
		uri->headers=malloc(headers_len+1);
		if(uri->headers==0){
			LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(uri->headers, headers, headers_len);
		uri->headers[headers_len]=0;
	}else uri->headers=0;
	
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
	char * next_via;
	char * host;
	short int port;
	int offset;
	

	name=version=transport=comment=params=hostport=next_via=host=0;
	name=eat_space(buffer, len);
	if (name-buffer==len) goto error;
	offset=name-buffer;
	tmp=name;

	version=eat_token2(tmp,len-offset,'/');
	if (version+1-buffer>=len) goto error;
	*version=0;
	version++;
	offset+=version-tmp;
	
	transport=eat_token2(tmp,len-offset,'/');
	if (transport+1-buffer>=len) goto error;
	*transport=0;
	transport++;
	offset+=transport-tmp;
	
	tmp=eat_token(transport,len-offset);
	if (tmp+1-buffer>=len) goto error;
	*tmp=0;
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
				if (tmp-buffer==len)  break;
				if (*tmp==' '){
					/* eat comment */
					*tmp=0;
					tmp++;
					comment=tmp;
					for(;((tmp-buffer)<len)&&
						(*tmp!=',');tmp++);
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
	vb->name=name;
	vb->version=version;
	vb->transport=transport;
	vb->host=host;
	vb->port=port;
	vb->params=params;
	vb->comment=comment;
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
	struct msg_start fl;
	struct hdr_field hf;
	struct via_body vb1, vb2;
	int offset;

	
	/* init vb1 & vb2 to the null string */
	vb1.error=VIA_PARSE_ERROR;
	vb1.hdr=vb1.name=vb1.version=vb1.transport=vb1.host=0;
	vb1.params=vb1.comment=0;
	vb1.next=0;
	vb1.size=0;
	memcpy(&vb2, &vb1, sizeof(struct via_body));

	/* eat crlf from the beginning */
	for (tmp=buf; (*tmp=='\n' || *tmp=='\r')&&
			tmp-buf < len ; tmp++);
	offset=tmp-buf;
	rest=parse_first_line(tmp, len-offset, &fl);
	offset+=rest-tmp;
	tmp=rest;
	switch(fl.type){
		case SIP_INVALID:
			DBG("parse_msg: invalid message\n");
			goto error;
			break;
		case SIP_REQUEST:
			DBG("SIP Request:\n");
			DBG(" method:  <%s>\n",fl.u.request.method);
			DBG(" uri:     <%s>\n",fl.u.request.uri);
			DBG(" version: <%s>\n",fl.u.request.version);
			break;
		case SIP_REPLY:
			DBG("SIP Reply  (status):\n");
			DBG(" version: <%s>\n",fl.u.reply.version);
			DBG(" status:  <%s>\n",fl.u.reply.status);
			DBG(" reason:  <%s>\n",fl.u.reply.reason);
			break;
		default:
			DBG("unknown type %d\n",fl.type);
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
						first_via=hf.body;
						vb1.hdr=hf.name;
						/* replace cr/lf with space in first via */
						for (bar=first_via;(first_via) && (*bar);bar++)
							if ((*bar=='\r')||(*bar=='\n'))	*bar=' ';
				#ifdef DEBUG
						DBG("first via: <%s>\n", first_via);
				#endif
						bar=parse_via_body(first_via, strlen(first_via), &vb1);
						if (vb1.error!=VIA_PARSE_OK){
							LOG(L_INFO, "ERROR: parsing via body: %s\n",
									first_via);
							goto error;
						}
						
						vb1.size=bar-first_via+first_via-vb1.hdr; 
						
						/* compact via */
						if (vb1.next) {
							second_via=vb1.next;
							/* not interested in the rest of the header */
							goto skip;
						}else{
						/*  add 1 (we don't see the trailing lf which
						 *  was zeroed by get_hfr_field) */
							vb1.size+=1;
						}
						if (fl.type!=SIP_REPLY) goto skip; /* we are interested in the 2nd via 
															   only in replies */
				}else if (second_via==0){
							second_via=hf.body;
							vb2.hdr=hf.name;
							goto skip;
				}
				break;
		}
	#ifdef DEBUG
		DBG("header field type %d, name=<%s>, body=<%s>\n",
			hf.type, hf.name, hf.body);
	#endif
		tmp=rest;
	}while(hf.type!=HDR_EOH && rest-buf < len);

skip:
	/* replace cr/lf with space in the second via */
	for (tmp=second_via;(second_via) && (*tmp);tmp++)
		if ((*tmp=='\r')||(*tmp=='\n'))	*tmp=' ';

	if (second_via) {
		tmp=parse_via_body(second_via, strlen(second_via), &vb2);
		if (vb2.error!=VIA_PARSE_OK){
			LOG(L_INFO, "ERROR: parsing via2 body: %s\n", second_via);
			goto error;
		}
		vb2.size=tmp-second_via; 
		if (vb2.next==0) vb2.size+=1; /* +1 from trailing lf */
		if (vb2.hdr) vb2.size+=second_via-vb2.hdr;
	}
	

#ifdef DEBUG
	/* dump parsed data */
	if (first_via){
		DBG(" first  via: <%s/%s/%s> <%s:%d>",
				vb1.name, vb1.version, vb1.transport, vb1.host, vb1.port);
		if (vb1.params)  DBG(";<%s>", vb1.params);
		if (vb1.comment) DBG(" <%s>", vb1.comment);
		DBG ("\n");
	}
	if (second_via){
		DBG(" second via: <%s/%s/%s> <%s:%d>",
				vb2.name, vb2.version, vb2.transport, vb2.host, vb2.port);
		if (vb2.params)  DBG(";<%s>", vb2.params);
		if (vb2.comment) DBG(" <%s>", vb2.comment);
		DBG("\n");
	}
#endif
	
	/* copy data into msg */
	memcpy(&(msg->first_line), &fl, sizeof(struct msg_start));
	memcpy(&(msg->via1), &vb1, sizeof(struct via_body));
	memcpy(&(msg->via2), &vb2, sizeof(struct via_body));

#ifdef DEBUG
	DBG("exiting parse_msg\n");
#endif

	return 0;
	
error:
	return -1;
}


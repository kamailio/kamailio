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
#include "mem/mem.h"

#ifdef DEBUG_DMALLOC
#include <mem/dmalloc.h>
#endif





/* parses the first line, returns pointer to  next line  & fills fl;
   also  modifies buffer (to avoid extra copy ops) */
char* parse_first_line(char* buffer, unsigned int len, struct msg_start * fl)
{
	
	char *tmp;
	char* second;
	char* third;
	char* nl;
	int offset;
	/* int l; */
	char* end;
	char s1,s2,s3;
	
	/* grammar:
		request  =  method SP uri SP version CRLF
		response =  version SP status  SP reason  CRLF
		(version = "SIP/2.0")
	*/
	

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
			fl->u.reply.version.len=SIP_VERSION_LEN;
			tmp=buffer+SIP_VERSION_LEN;
	} else IFISMETHOD( INVITE, 'I' )
	else IFISMETHOD( CANCEL, 'C')
	else IFISMETHOD( ACK, 'A' )
	else IFISMETHOD( BYE, 'B' )
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
	(*tmp)=0;			/* mark the 1st token end */
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
	*tmp=0; /* mark the end of the token */
	fl->u.request.uri.s=second;
	fl->u.request.uri.len=tmp-second;

	/* jku: parse status code */
	if (fl->type==SIP_REPLY) {
		if (fl->u.request.uri.len!=3) {
			LOG(L_INFO, "ERROR:parse_first_line: len(status code)!=3: %s\n",
				second );
			goto error;
		}
		s1=*second; s2=*(second+1);s3=*(second+2);
		if (s1>='0' && s1<='9' && 
		    s2>='0' && s2<='9' &&
		    s3>='0' && s3<='9' ) {
			fl->u.reply.statuscode=(s1-'0')*100+10*(s2-'0')+(s3-'0');
		} else {
			LOG(L_INFO, "ERROR:parse_first_line: status_code non-numerical: %s\n",
				second );
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



/* returns pointer to next header line, and fill hdr_f ;
 * if at end of header returns pointer to the last crlf  (always buf)*/
char* get_hdr_field(char* buf, char* end, struct hdr_field* hdr)
{

	char* tmp;
	char *match;
	struct via_body *vb;
	struct cseq_body* cseq_b;

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
	}
	switch(hdr->type){
		case HDR_VIA:
			vb=pkg_malloc(sizeof(struct via_body));
			if (vb==0){
				LOG(L_ERR, "get_hdr_field: out of memory\n");
				goto error;
			}
			memset(vb,0,sizeof(struct via_body));

			hdr->body.s=tmp;
			tmp=parse_via(tmp, end, vb);
			if (vb->error==VIA_PARSE_ERROR){
				LOG(L_ERR, "ERROR: get_hdr_field: bad via\n");
				pkg_free(vb);
				goto error;
			}
			hdr->parsed=vb;
			vb->hdr.s=hdr->name.s;
			vb->hdr.len=hdr->name.len;
			hdr->body.len=tmp-hdr->body.s;
			break;
		case HDR_CSEQ:
			cseq_b=pkg_malloc(sizeof(struct cseq_body));
			if (cseq_b==0){
				LOG(L_ERR, "get_hdr_field: out of memory\n");
				goto error;
			}
			memset(cseq_b, 0, sizeof(struct cseq_body));
			hdr->body.s=tmp;
			tmp=parse_cseq(tmp, end, cseq_b);
			if (cseq_b->error==PARSE_ERROR){
				LOG(L_ERR, "ERROR: get_hdr_field: bad cseq\n");
				pkg_free(cseq_b);
				goto error;
			}
			hdr->parsed=cseq_b;
			hdr->body.len=tmp-hdr->body.s;
			DBG("get_hdr_field: cseq <%s>: <%s> <%s>\n",
					hdr->name.s, cseq_b->number.s, cseq_b->method.s);
			break;
		case HDR_TO:
		case HDR_FROM:
		case HDR_CALLID:
		case HDR_CONTACT:
		case HDR_ROUTE:   /* janakj, HDR_ROUTE was missing here */
	        case HDR_RECORDROUTE:
		case HDR_MAXFORWARDS:
		case HDR_OTHER:
			/* just skip over it */
			hdr->body.s=tmp;
			/* find end of header */
			
			/* find lf */
			do{
				match=q_memchr(tmp, '\n', end-tmp);
				if (match){
					match++;
				}else {
					tmp=end;
					LOG(L_ERR,
							"ERROR: get_hdr_field: bad body for <%s>(%d)\n",
							hdr->name.s, hdr->type);
					goto error;
				}
				tmp=match;
			}while( match<end &&( (*match==' ')||(*match=='\t') ) );
			*(match-1)=0; /*null terminate*/
			hdr->body.len=match-hdr->body.s;
			break;
		default:
			LOG(L_CRIT, "BUG: get_hdr_field: unknown header type %d\n",
					hdr->type);
			goto error;
	}

	/* jku: if \r covered by current length, shrink it */
	trim_r( hdr->body );
	return tmp;
error:
	DBG("get_hdr_field: error exit\n");
	hdr->type=HDR_ERROR;
	return tmp;
}



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



/*BUGGY*/
char * parse_cseq(char *buf, char* end, struct cseq_body* cb)
{
	char *t, *m, *m_end;
	char c;

	cb->error=PARSE_ERROR;
	t=eat_space_end(buf, end);
	if (t>=end) goto error;
	
	cb->number.s=t;
	t=eat_token_end(t, end);
	if (t>=end) goto error;
	m=eat_space_end(t, end);
	m_end=eat_token_end(m, end);
	*t=0; /*null terminate it*/
	cb->number.len=t-cb->number.s;
	DBG("parse_cseq: found number %s\n", cb->number.s);
	
	if (m_end>=end) goto error;
	if (m_end==m){
		/* null method*/
		LOG(L_ERR,  "ERROR:parse_cseq: no method found\n");
		goto error;
	}
	cb->method.s=m;
	t=m_end;
	c=*t;
	*t=0; /*null terminate it*/
	cb->method.len=t-cb->method.s;
	DBG("parse_cseq: found method %s\n", cb->method.s);
	t++;
	/*check if the header ends here*/
	if (c=='\n') goto check_continue;
	do{
		for (;(t<end)&&((*t==' ')||(*t=='\t')||(*t=='\r'));t++);
		if (t>=end) goto error;
		if (*t!='\n'){
			LOG(L_ERR, "ERROR:parse_cseq: unexpected char <%c> at end of"
					" cseq\n", *t);
			goto error;
		}
		t++;
check_continue:
	}while( (t<end) && ((*t==' ')||(*t=='\t')) );

	cb->error=PARSE_OK;
	return t;
error:
	LOG(L_ERR, "ERROR: parse_cseq: bad cseq\n");
	return t;
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
			uri->user.s=(char*)pkg_malloc(next-user+1);
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
			uri->user.s=(char*)pkg_malloc(passwd-user+1);
			if (uri->user.s==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->user.s, user, passwd-user);
			uri->user.len=passwd-user;
			uri->user.s[passwd-user]=0;
			passwd++; /*skip ':' */
			uri->passwd.s=(char*)pkg_malloc(next-passwd+1);
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
	host_len=(port)?port-host:(params)?params-host:(headers)?headers-host:
		end-host;
	/* get host */
	uri->host.s=pkg_malloc(host_len+1);
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
		uri->port.s=pkg_malloc(port_len+1);
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
		uri->params.s=pkg_malloc(params_len+1);
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
		uri->headers.s=pkg_malloc(headers_len+1);
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
	free_uri(uri);
	return ret;
}


/* parse the headers and adds them to msg->headers and msg->to, from etc.
 * It stops when all the headers requested in flags were parsed, on error
 * (bad header) or end of headers */
/* note: it continues where it previously stopped and goes ahead until
   end is encountered or desired HFs are found; if you call it twice
   for the same HF which is present only once, it will fail the second
   time; if you call it twice and the HF is found on second time too,
   it's not replaced in the well-known HF pointer but just added to
   header list; if you want to use a dumbie convenience function which will
   give you the first occurance of a header you are interested in,
   look at check_transaction_quadruple
*/
int parse_headers(struct sip_msg* msg, int flags)
{
	struct hdr_field* hf;
	char* tmp;
	char* rest;
	char* end;

	end=msg->buf+msg->len;
	tmp=msg->unparsed;

	DBG("parse_headers: flags=%d\n", flags);
	while( tmp<end && (flags & msg->parsed_flag) != flags){
		hf=pkg_malloc(sizeof(struct hdr_field));
		if (hf==0){
			LOG(L_ERR, "ERROR:parse_headers: memory allocation error\n");
			goto error;
		}
		memset(hf,0, sizeof(struct hdr_field));
		hf->type=HDR_ERROR;
		rest=get_hdr_field(tmp, msg->buf+msg->len, hf);
		switch (hf->type){
			case HDR_ERROR:
				LOG(L_INFO,"ERROR: bad header  field\n");
				goto  error;
			case HDR_EOH:
				msg->eoh=tmp; /* or rest?*/
				msg->parsed_flag|=HDR_EOH;
				pkg_free(hf);
				goto skip;
			case HDR_OTHER: /*do nothing*/
				break;
			case HDR_CALLID:
				if (msg->callid==0) msg->callid=hf;
				msg->parsed_flag|=HDR_CALLID;
				break;
			case HDR_TO:
				if (msg->to==0) msg->to=hf;
				msg->parsed_flag|=HDR_TO;
				break;
			case HDR_CSEQ:
				if (msg->cseq==0) msg->cseq=hf;
				msg->parsed_flag|=HDR_CSEQ;
				break;
			case HDR_FROM:
				if (msg->from==0) msg->from=hf;
				msg->parsed_flag|=HDR_FROM;
				break;
			case HDR_CONTACT:
				if (msg->contact==0) msg->contact=hf;
				msg->parsed_flag|=HDR_CONTACT;
				break;
			case HDR_MAXFORWARDS:
				if(msg->maxforwards==0) msg->maxforwards=hf;
				msg->parsed_flag|=HDR_MAXFORWARDS;
				break;
			case HDR_ROUTE:
				if (msg->route==0) msg->route=hf;
				msg->parsed_flag|=HDR_ROUTE;
				break;
		        case HDR_RECORDROUTE:
				if (msg->record_route==0) msg->record_route = hf;
				msg->parsed_flag|=HDR_RECORDROUTE;
				break;
		        case HDR_VIA:
				msg->parsed_flag|=HDR_VIA;
				DBG("parse_headers: Via1 found, flags=%d\n", flags);
				if (msg->via1==0) {
					msg->h_via1=hf;
					msg->via1=hf->parsed;
					if (msg->via1->next){
						msg->via2=msg->via1->next;
						msg->parsed_flag|=HDR_VIA2;
					}
				}else if (msg->via2==0){
					msg->h_via2=hf;
					msg->via2=hf->parsed;
					msg->parsed_flag|=HDR_VIA2;
				DBG("parse_headers: Via2 found, flags=%d\n", flags);
				}
				break;
			default:
				LOG(L_CRIT, "BUG: parse_headers: unknown header type %d\n",
							hf->type);
				goto error;
		}
		/* add the header to the list*/
		if (msg->last_header==0){
			msg->headers=hf;
			msg->last_header=hf;
		}else{
			msg->last_header->next=hf;
			msg->last_header=hf;
		}
	#ifdef EXTRA_DEBUG
		DBG("header field type %d, name=<%s>, body=<%s>\n",
			hf->type, hf->name.s, hf->body.s);
	#endif
		tmp=rest;
	}
skip:
	msg->unparsed=tmp;
	return 0;

error:
	if (hf) pkg_free(hf);
	return -1;
}





/* returns 0 if ok, -1 for errors */
int parse_msg(char* buf, unsigned int len, struct sip_msg* msg)
{

	char *tmp, *bar;
	char* rest;
	char* first_via;
	char* second_via;
	struct msg_start *fl;
	int offset;
	int flags;

	/* eat crlf from the beginning */
	for (tmp=buf; (*tmp=='\n' || *tmp=='\r')&&
			tmp-buf < len ; tmp++);
	offset=tmp-buf;
	fl=&(msg->first_line);
	rest=parse_first_line(tmp, len-offset, fl);
#if 0
	rest=parse_fline(tmp, buf+len, fl);
#endif
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
			flags=HDR_VIA;
			break;
		case SIP_REPLY:
			DBG("SIP Reply  (status):\n");
			DBG(" version: <%s>\n",fl->u.reply.version);
			DBG(" status:  <%s>\n",fl->u.reply.status);
			DBG(" reason:  <%s>\n",fl->u.reply.reason);
			flags=HDR_VIA|HDR_VIA2;
			break;
		default:
			DBG("unknown type %d\n",fl->type);
	}
	msg->unparsed=tmp;
	/*find first Via: */
	first_via=0;
	second_via=0;
	if (parse_headers(msg, flags)==-1) goto error;

#ifdef EXTRA_DEBUG
	/* dump parsed data */
	if (msg->via1){
		DBG(" first  via: <%s/%s/%s> <%s:%s(%d)>",
			msg->via1->name.s, msg->via1->version.s,
			msg->via1->transport.s, msg->via1->host.s,
			msg->via1->port_str, msg->via1->port);
		if (msg->via1->params.s)  DBG(";<%s>", msg->via1->params.s);
		if (msg->via1->comment.s) DBG(" <%s>", msg->via1->comment.s);
		DBG ("\n");
	}
	if (msg->via2){
		DBG(" first  via: <%s/%s/%s> <%s:%s(%d)>",
			msg->via2->name.s, msg->via2->version.s,
			msg->via2->transport.s, msg->via2->host.s,
			msg->via2->port_str, msg->via2->port);
		if (msg->via2->params.s)  DBG(";<%s>", msg->via2->params.s);
		if (msg->via2->comment.s) DBG(" <%s>", msg->via2->comment.s);
		DBG ("\n");
	}
#endif
	

#ifdef EXTRA_DEBUG
	DBG("exiting parse_msg\n");
#endif

	return 0;
	
error:
	return -1;
}



void free_uri(struct sip_uri* u)
{
	if (u){
		if (u->user.s)    pkg_free(u->user.s);
		if (u->passwd.s)  pkg_free(u->passwd.s);
		if (u->host.s)    pkg_free(u->host.s);
		if (u->port.s)    pkg_free(u->port.s);
		if (u->params.s)  pkg_free(u->params.s);
		if (u->headers.s) pkg_free(u->headers.s);
	}
}



void free_via_param_list(struct via_param* vp)
{
	struct via_param* foo;
	while(vp){
		foo=vp;
		vp=vp->next;
		pkg_free(foo);
	}
}



void free_via_list(struct via_body* vb)
{
	struct via_body* foo;
	while(vb){
		foo=vb;
		vb=vb->next;
		if (foo->param_lst) free_via_param_list(foo->param_lst);
		pkg_free(foo);
	}
}


/* frees a hdr_field structure, 
 * WARNING: it frees only parsed (and not name.s, body.s)*/
void clean_hdr_field(struct hdr_field* hf)
{
	if (hf->parsed){
		switch(hf->type){
			case HDR_VIA:
				free_via_list(hf->parsed);
				break;
			case HDR_CSEQ:
				pkg_free(hf->parsed);
				break;
			default:
				LOG(L_CRIT, "BUG: clean_hdr_field: unknown header type %d\n",
						hf->type);
		}
	}
}



/* frees a hdr_field list,
 * WARNING: frees only ->parsed and ->next*/
void free_hdr_field_lst(struct hdr_field* hf)
{
	struct hdr_field* foo;
	
	while(hf){
		foo=hf;
		hf=hf->next;
		clean_hdr_field(foo);
		pkg_free(foo);
	}
}



/*only the content*/
void free_sip_msg(struct sip_msg* msg)
{
	if (msg->new_uri.s) { pkg_free(msg->new_uri.s); msg->new_uri.len=0; }
	if (msg->headers)     free_hdr_field_lst(msg->headers);
	if (msg->add_rm)      free_lump_list(msg->add_rm);
	if (msg->repl_add_rm) free_lump_list(msg->repl_add_rm);
	pkg_free(msg->orig);
	/* don't free anymore -- now a pointer to a static buffer */
#	ifdef DYN_BUF
	pkg_free(msg->buf); */
#	endif
}


#if 0
/* it's a macro now*/
/* make sure all HFs needed for transaction identification have been
   parsed; return 0 if those HFs can't be found
*/
int check_transaction_quadruple( struct sip_msg* msg )
{
   return 
	(parse_headers(msg, HDR_FROM|HDR_TO|HDR_CALLID|HDR_CSEQ)!=-1 &&
	 msg->from && msg->to && msg->callid && msg->cseq);
  /* replaced by me ( :) andrei)
   ( (msg->from || (parse_headers( msg, HDR_FROM)!=-1 && msg->from)) &&
   (msg->to|| (parse_headers( msg, HDR_TO)!=-1 && msg->to)) &&
   (msg->callid|| (parse_headers( msg, HDR_CALLID)!=-1 && msg->callid)) &&
   (msg->cseq|| (parse_headers( msg, HDR_CSEQ)!=-1 && msg->cseq)) ) ? 1 : 0;
  */

}
#endif

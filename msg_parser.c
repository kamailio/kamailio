/*
 * $Id$
 *
 * sip msg. header proxy parser 
 *
 */

#include <string.h>

#include "msg_parser.h"
#include "parser_f.h"
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
	
	/* grammar:
		request  =  method SP uri SP version CRLF
		response =  version SP status  SP reason  CRLF
		(version = "SIP/2.0")
	*/
	

	/* see if it's a reply (status) */
	tmp=eat_token(buffer, len);
	if (tmp==buffer){
		DPrint("ERROR: empty  or bad first line\n");
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
	if (second==tmp){
		goto error;
	}
	*tmp=0; /* mark the end of the token */
	fl->u.request.method=buffer;
	
	/* next element */
	tmp=eat_token(second, len-offset);
	offset+=tmp-second;
	third=eat_space(tmp, len-offset);
	offset+=third-tmp;
	if(third==tmp){
		goto error;
	}
	*tmp=0; /* mark the end of the token */
	fl->u.request.uri=second;
	/*  last part */
	tmp=eat_token(third,len-offset);
	offset+=tmp-third;
	if (tmp==third){
		goto error;
	}
	if (! is_empty(tmp, len-offset)){		
		goto error;
	}
	nl=eat_line(tmp,len-offset);
	*tmp=0;
	fl->u.request.version=third;
	
	return nl;

error:
	DPrint("ERROR: bad %s first line\n", 
		(fl->type==SIP_REPLY)?"reply(status)":"request");
error1:
	fl->type=SIP_INVALID;
	DPrint("ERROR: at line 0 char %d\n", offset);
	/* skip  line */
	nl=eat_line(buffer,len);
	return nl;
}


/* returns integer field name type */
int field_name(char *s)
{
	if ((strcmp(s, "Via")==0 )||(strcmp(s,"v")==0))
		return  HDR_VIA;
	if ((strcmp(s, "To")==0)||(strcmp(s,"t")==0))
		return HDR_TO;
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

	char* tmp;
	char* nl;
	char* body;
	int offset;

	if ((*buffer=='\n')||(*buffer=='\r')){
		/* double crlf */
		tmp=eat_line(buffer,len);
		hdr_f->type=HDR_EOH;
		return tmp;
	}
	
	tmp=eat_token2(buffer, len, ':');
	if ((tmp==buffer) || (tmp-buffer==len) ||
		(is_empty(buffer, tmp-buffer-1))|| (*tmp!=':')){
		hdr_f->type=HDR_ERROR;
		goto error;
	}
	*tmp=0;
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
		DPrint("ERROR: field body too  long\n");
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
			DPrint("ERROR: hostport: trailing chars in port number: %s(%x)\n",
					invalid, invalid);
			/* report error? */
		}
	}
	return *host;
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
				/*the rest is comment? */
				if (tmp+1-buffer<len){
					tmp++;
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
				if (tmp+1-buffer>=len) goto error;
				tmp++;
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
				if (tmp+1-buffer<len){
					/* eat space and ',' */
					for(tmp=tmp+1; 
						((tmp-buffer)<len)&&
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
	int r;

	
	/* eat crlf from the beginning */
	for (tmp=buf; (*tmp=='\n' || *tmp=='\r')&&
			tmp-buf < len ; tmp++);
	offset=tmp-buf;
	rest=parse_first_line(tmp, len-offset, &fl);
	offset+=rest-tmp;
	tmp=rest;
	switch(fl.type){
		case SIP_INVALID:
			DPrint("invalid message\n");
			goto error;
			break;
		case SIP_REQUEST:
			DPrint("SIP Request:\n");
			DPrint(" method:  <%s>\n",fl.u.request.method);
			DPrint(" uri:     <%s>\n",fl.u.request.uri);
			DPrint(" version: <%s>\n",fl.u.request.version);
			break;
		case SIP_REPLY:
			DPrint("SIP Reply  (status):\n");
			DPrint(" version: <%s>\n",fl.u.reply.version);
			DPrint(" status:  <%s>\n",fl.u.reply.status);
			DPrint(" reason:  <%s>\n",fl.u.reply.reason);
			break;
		default:
			DPrint("unknown type %d\n",fl.type);
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
				DPrint("ERROR: bad header  field\n");
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
						printf("first via: <%s>\n", first_via);
				#endif
						bar=parse_via_body(first_via, strlen(first_via), &vb1);
						if (vb1.error!=VIA_PARSE_OK){
							DPrint("ERROR: parsing via body: %s\n", first_via);
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
						 *  was zeroed by get_hfr_field */
							vb1.size+=1;
						}
				}else if (second_via==0){
							second_via=hf.body;
							vb2.hdr=hf.name;
							goto skip;
				}
				break;
		}
	#ifdef DEBUG
		printf("header field type %d, name=<%s>, body=<%s>\n",
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
			DPrint("ERROR: parsing via body: %s\n", second_via);
			goto error;
		}
		vb2.size=tmp-second_via; 
		if (vb2.next==0) vb2.size+=1; /* +1 from trailing lf */
		if (vb2.hdr) vb2.size+=second_via-vb2.hdr;
	}
	

#ifdef DEBUG
	/* dump parsed data */
	printf(" first  via: <%s/%s/%s> <%s:%d>",
			vb1.name, vb1.version, vb1.transport, vb1.host, vb1.port);
	if (vb1.params) printf(";<%s>", vb1.params);
	if (vb1.comment) printf(" <%s>", vb1.comment);
	printf ("\n");
	if (second_via){
		printf(" second via: <%s/%s/%s> <%s:%d>",
				vb2.name, vb2.version, vb2.transport, vb2.host, vb2.port);
		if (vb2.params) printf(";<%s>", vb2.params);
		if (vb2.comment) printf(" <%s>", vb2.comment);
		printf ("\n");
	}
#endif
	
	/* copy data into msg */
	memcpy(&(msg->first_line), &fl, sizeof(struct msg_start));
	memcpy(&(msg->via1), &vb1, sizeof(struct via_body));
	memcpy(&(msg->via2), &vb2, sizeof(struct via_body));

#ifdef DEBUG
	printf ("exiting parse_msg\n");
#endif

	return 0;
	
error:
	return -1;
}


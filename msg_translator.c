/* $Id$
 *
 */


#include <sys/socket.h>

#include "msg_translator.h"
#include "mem/mem.h"
#include "dprint.h"
#include "config.h"
#include "md5utils.h"
#include "data_lump_rpl.h"
#include <netdb.h>



#define MAX_VIA_LINE_SIZE      240
#define MAX_RECEIVED_SIZE  57


#define append_str(_dest,_src,_len,_msg) \
	do{\
		memcpy( (_dest) , (_src) , (_len) );\
		(_dest) += (_len) ;\
	}while(0);

#define append_str_trans(_dest,_src,_len,_msg) \
	do{\
		memcpy( (_dest) , (_msg)->orig+((_src)-(_msg)->buf) , (_len) );\
		(_dest) += (_len) ;\
	}while(0);

extern char version[];
extern int version_len;

/* faster than inet_ntoa */
static inline char* q_inet_itoa(unsigned long ip)
{
	static char q_inet_itoa_buf[16]; /* 123.567.901.345\0 */
	unsigned char* p;
	unsigned char a,b,c;  /* abc.def.ghi.jkl */
	int offset;
	int r;
	p=(unsigned char*)&ip;

	offset=0;
	/* unrolled loops (faster)*/
	for(r=0;r<3;r++){
		a=p[r]/100;
		c=p[r]%10;
		b=p[r]%100/10;
		if (a){
			q_inet_itoa_buf[offset]=a+'0';
			q_inet_itoa_buf[offset+1]=b+'0';
			q_inet_itoa_buf[offset+2]=c+'0';
			q_inet_itoa_buf[offset+3]='.';
			offset+=4;
		}else if (b){
			q_inet_itoa_buf[offset]=b+'0';
			q_inet_itoa_buf[offset+1]=c+'0';
			q_inet_itoa_buf[offset+2]='.';
			offset+=3;
		}else{
			q_inet_itoa_buf[offset]=c+'0';
			q_inet_itoa_buf[offset+1]='.';
			offset+=2;
		}
	}
	/* last number */
	a=p[r]/100;
	c=p[r]%10;
	b=p[r]%100/10;
	if (a){
		q_inet_itoa_buf[offset]=a+'0';
		q_inet_itoa_buf[offset+1]=b+'0';
		q_inet_itoa_buf[offset+2]=c+'0';
		q_inet_itoa_buf[offset+3]=0;
	}else if (b){
		q_inet_itoa_buf[offset]=b+'0';
		q_inet_itoa_buf[offset+1]=c+'0';
		q_inet_itoa_buf[offset+2]=0;
	}else{
		q_inet_itoa_buf[offset]=c+'0';
		q_inet_itoa_buf[offset+1]=0;
	}

	return q_inet_itoa_buf;
}




/* checks if ip is in host(name) and ?host(ip)=name?
 * ip must be in network byte order!
 *  resolver = DO_DNS | DO_REV_DNS; if 0 no dns check is made
 * return 0 if equal */
int check_address(unsigned long ip, char *name, int resolver)
{
	struct hostent* he;
	int i;

	/* maybe we are lucky and name it's an ip */
	if (strcmp(name, q_inet_itoa( /* *(struct in_addr *)&*/ip ))==0)
		return 0;
	if (resolver&DO_DNS){
		DBG("check_address: doing dns lookup\n");
		/* try all names ips */
		he=gethostbyname(name);
		for(i=0;he && he->h_addr_list[i];i++){
			if (*(unsigned long*)he->h_addr_list[i]==ip)
				return 0;
		}
	}
	if (resolver&DO_REV_DNS){
		DBG("check_address: doing rev. dns lookup\n");
		print_ip(ip);
		/* try reverse dns */
		he=gethostbyaddr((char*)&ip, sizeof(ip), AF_INET);
		if (he && (strcmp(he->h_name, name)==0))
			return 0;
		for (i=0; he && he->h_aliases[i];i++){
			if (strcmp(he->h_aliases[i],name)==0)
				return 0;
		}
	}
	return -1;
}








char* via_builder( struct sip_msg *msg , unsigned int *len )
{
	unsigned int  via_len, branch_len;
	char               *line_buf;

	line_buf=0;

	line_buf=pkg_malloc(sizeof(char)*MAX_VIA_LINE_SIZE);
	if (line_buf==0){
		LOG(L_ERR, "ERROR: via_builder: out of memory\n");
		goto error;
	}
	via_len=MY_VIA_LEN+names_len[0]; /* space included in MY_VIA*/

	/* jku: if we compute branches using MD5 it will take 32 bytes */
	branch_len= (loop_checks ? MY_BRANCH_LEN : MY_BRANCH_LEN -1 + MD5_LEN)+
					msg->add_to_branch_len;

	if ((via_len+port_no_str_len+branch_len+CRLF_LEN)<MAX_VIA_LINE_SIZE){
		memcpy(line_buf, MY_VIA, MY_VIA_LEN);
		memcpy(line_buf+MY_VIA_LEN, names[0], names_len[0]);
		if (port_no!=SIP_PORT){
			memcpy(line_buf+via_len, port_no_str, port_no_str_len);
			via_len+=port_no_str_len;
		}

		/* jku: branch parameter */
		memcpy(line_buf+via_len, MY_BRANCH, MY_BRANCH_LEN );
		via_len+=MY_BRANCH_LEN;
		/* loop checks ?
		if (loop_checks) {

			if (check_transaction_quadruple( msg )) {
				str src[5];
				int r;

				src[0]= msg->from->body;
				src[1]= msg->to->body;
				src[2]= msg->callid->body;
				src[3]= msg->first_line.u.request.uri;
				src[4]= get_cseq( msg )->number;
				MDStringArray ( line_buf+via_len-1, src, 5 );
				via_len+=MD5_LEN - 1;

			} else DBG("DEBUG: via_builder: required HFs for "
					"loop checking missing\n");
		}  */
		//DBG("DEBUG: XXX will add branch now: %s (%d)\n", msg->add_to_branch_s, msg->add_to_branch_len );
		/* someone wants me to add something to branch here ? */
		if ( msg->add_to_branch_len ){
			memcpy(line_buf+via_len-1, msg->add_to_branch_s,
				msg->add_to_branch_len );
			via_len+=msg->add_to_branch_len-1;
		}

		memcpy(line_buf+via_len, CRLF, CRLF_LEN);
		via_len+=CRLF_LEN;
		line_buf[via_len]=0; /* null terminate the string*/
	}else{
		LOG(L_ERR, " ERROR: via_builder: via too long (%d)\n",
				via_len);
		goto error;
	}

	*len = via_len;
	return line_buf;

error:
	if (line_buf) pkg_free(line_buf);
	return 0;
}











char * build_req_buf_from_sip_req(	struct sip_msg* msg,
				unsigned int *returned_len)
{
	unsigned int len, new_len, received_len, uri_len, via_len;
	char* line_buf;
	char* received_buf;
	char* tmp;
	int tmp_len;
	char* new_buf;
	char* orig;
	char* buf;
	unsigned int offset, s_offset, size;
	unsigned long source_ip;
	struct lump *t,*r;
	struct lump* anchor;

	orig=msg->orig;
	buf=msg->buf;
	len=msg->len;
	source_ip=msg->src_ip;
	received_len=0;
	new_buf=0;
	received_buf=0;


	line_buf = via_builder( msg, &via_len );
	if (!line_buf){
		LOG(L_ERR,"ERROR: build_req_buf_from_sip_req: no via received!\n");
		goto error1;
	}

	/* check if received needs to be added */
	if (check_address(source_ip, msg->via1->host.s, received_dns)!=0){
		received_buf=pkg_malloc(sizeof(char)*MAX_RECEIVED_SIZE);
		if (received_buf==0){
			LOG(L_ERR, "ERROR: build_req_buf_from_sip_req: out of memory\n");
			goto error1;
		}
		/*
		received_len=snprintf(received_buf, MAX_RECEIVED_SIZE,
								";received=%s",
								inet_ntoa(*(struct in_addr *)&source_ip));
		*/
		memcpy(received_buf, RECEIVED, RECEIVED_LEN);
		tmp=q_inet_itoa( /* *(struct in_addr *)& */source_ip);
		tmp_len=strlen(tmp);
		received_len=RECEIVED_LEN+tmp_len;
		memcpy(received_buf+RECEIVED_LEN, tmp, tmp_len);
		received_buf[received_len]=0; /*null terminate it */
	}

	/* add via header to the list */
	/* try to add it before msg. 1st via */
	/*add first via, as an anchor for second via*/
	anchor=anchor_lump(&(msg->add_rm), msg->via1->hdr.s-buf, 0, HDR_VIA);
	if (anchor==0) goto error;
	if (insert_new_lump_before(anchor, line_buf, via_len, HDR_VIA)==0)
		goto error;
	/* if received needs to be added, add anchor after host and add it */
	if (received_len){
		if (msg->via1->params.s){
				size= msg->via1->params.s-msg->via1->hdr.s-1; /*compensate
															  for ';' */
		}else{
				size= msg->via1->host.s-msg->via1->hdr.s+msg->via1->host.len;
				if (msg->via1->port!=0){
					size+=strlen(msg->via1->hdr.s+size+1)+1; /* +1 for ':'*/
				}
		}
		anchor=anchor_lump(&(msg->add_rm),msg->via1->hdr.s-buf+size,0,
				HDR_VIA);
		if (anchor==0) goto error;
		if (insert_new_lump_after(anchor, received_buf, received_len, HDR_VIA)
				==0 ) goto error;
	}


	/* compute new msg len and fix overlapping zones*/
	new_len=len;
	s_offset=0;
	for(t=msg->add_rm;t;t=t->next){
		for(r=t->before;r;r=r->before){
			switch(r->op){
				case LUMP_ADD:
					new_len+=r->len;
					break;
				default:
					/* only ADD allowed for before/after */
					LOG(L_CRIT, "BUG:build_req_buf_from_sip_req: invalid op "
							"for data lump (%x)\n", r->op);
			}
		}
		switch(t->op){
			case LUMP_ADD:
				new_len+=t->len;
				break;
			case LUMP_DEL:
				/* fix overlapping deleted zones */
				if (t->u.offset < s_offset){
					/* change len */
					if (t->len>s_offset-t->u.offset)
							t->len-=s_offset-t->u.offset;
					else t->len=0;
					t->u.offset=s_offset;
				}
				s_offset=t->u.offset+t->len;
				new_len-=t->len;
				break;
			case LUMP_NOP:
				/* fix offset if overlapping on a deleted zone */
				if (t->u.offset < s_offset){
					t->u.offset=s_offset;
				}else
					s_offset=t->u.offset;
				/* do nothing */
				break;
			debug:
				LOG(L_CRIT,"BUG:build_req_buf_from_sip_req: invalid"
							" op for data lump (%x)\n", r->op);
		}
		for (r=t->after;r;r=r->after){
			switch(r->op){
				case LUMP_ADD:
					new_len+=r->len;
					break;
				default:
					/* only ADD allowed for before/after */
					LOG(L_CRIT, "BUG:build_req_buf_from_sip_req: invalid"
								" op for data lump (%x)\n", r->op);
			}
		}
	}


	if (msg->new_uri.s){
		uri_len=msg->new_uri.len;
		new_len=new_len-msg->first_line.u.request.uri.len+uri_len;
	}
	new_buf=(char*)malloc(new_len+1);
	if (new_buf==0){
		LOG(L_ERR, "ERROR: build_req_buf_from_sip_req: out of memory\n");
		goto error;
	}

	offset=s_offset=0;
	if (msg->new_uri.s){
		/* copy message up to uri */
		size=msg->first_line.u.request.uri.s-buf;
		memcpy(new_buf, orig, size);
		offset+=size;
		s_offset+=size;
		/* add our uri */
		memcpy(new_buf+offset, msg->new_uri.s, uri_len);
		offset+=uri_len;
		s_offset+=msg->first_line.u.request.uri.len; /* skip original uri */
	}
	new_buf[new_len]=0;
	/* copy msg adding/removing lumps */
	for (t=msg->add_rm;t;t=t->next){
		switch(t->op){
			case LUMP_ADD:
				/* just add it here! */
				/* process before  */
				for(r=t->before;r;r=r->before){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:build_req_buf_from_sip_req: "
									"invalid op for data lump (%x)\n", r->op);

					}
				}
				/* copy "main" part */
				memcpy(new_buf+offset, t->u.value, t->len);
				offset+=t->len;
				/* process after */
				for(r=t->after;r;r=r->after){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:build_req_buf_from_sip_req: "
									"invalid op for data lump (%x)\n", r->op);
					}
				}
				break;
			case LUMP_NOP:
			case LUMP_DEL:
				/* copy till offset */
				if (s_offset>t->u.offset){
					DBG("Warning: (%d) overlapped lumps offsets,"
						" ignoring(%x, %x)\n", t->op, s_offset,t->u.offset);
					/* this should've been fixed above (when computing len) */
					/* just ignore it*/
					break;
				}
				size=t->u.offset-s_offset;
				if (size){
					memcpy(new_buf+offset, orig+s_offset,size);
					offset+=size;
					s_offset+=size;
				}
				/* process before  */
				for(r=t->before;r;r=r->before){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:build_req_buf_from_sip_req: "
									"invalid op for data lump (%x)\n", r->op);

					}
				}
				/* process main (del only) */
				if (t->op==LUMP_DEL){
					/* skip len bytes from orig msg */
					s_offset+=t->len;
				}
				/* process after */
				for(r=t->after;r;r=r->after){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:build_req_buf_from_sip_req: "
									"invalid op for data lump (%x)\n", r->op);
					}
				}
				break;
			default:
					LOG(L_CRIT, "BUG: build_req_buf_from_sip_req: "
							"unknown op (%x)\n", t->op);
		}
	}
	/* copy the rest of the message */
	memcpy(new_buf+offset, orig+s_offset, len-s_offset);
	new_buf[new_len]=0;

	*returned_len=new_len;
	return new_buf;

error1:
	if (received_buf) pkg_free(received_buf);
	if (line_buf) pkg_free(line_buf);
error:
	if (new_buf) free(new_buf);
	*returned_len=0;
	return 0;
}




char * build_res_buf_from_sip_res( struct sip_msg* msg,
				unsigned int *returned_len)
{
	unsigned int new_len, via_len,r;
	char* new_buf;
	unsigned offset, s_offset, size;
	struct hostent* he;
	char* orig;
	char* buf;
	unsigned int len;

	orig=msg->orig;
	buf=msg->buf;
	len=msg->len;
	new_buf=0;
	/* we must remove the first via */
	via_len=msg->via1->bsize;
	size=msg->via1->hdr.s-buf;
	DBG("via len: %d, initial size: %d\n", via_len, size);
	if (msg->via1->next){
		/* add hdr size*/
		size+=msg->via1->hdr.len+1;
	    DBG(" adjusted via len: %d, initial size: %d\n",
				via_len, size);
	}else{
		/* add hdr size ("Via:")*/
		via_len+=msg->via1->hdr.len+1;
	}
	new_len=len-via_len;

	DBG(" old size: %d, new size: %d\n", len, new_len);
	new_buf=(char*)malloc(new_len+1);/* +1 is for debugging
											(\0 to print it )*/
	if (new_buf==0){
		LOG(L_ERR, "ERROR: build_res_buf_from_sip_res: out of memory\n");
		goto error;
	}
	new_buf[new_len]=0; /* debug: print the message */
	memcpy(new_buf, orig, size);
	offset=size;
	s_offset=size+via_len;
	memcpy(new_buf+offset,orig+s_offset, len-s_offset);
	 /* send it! */
	DBG(" copied size: orig:%d, new: %d, rest: %d\n",
			s_offset, offset,
			len-s_offset );

	*returned_len=new_len;
	return new_buf;
error:
	if (new_buf) free(new_buf);
	*returned_len=0;
	return 0;
}





char * build_res_buf_from_sip_req(	unsigned int code ,
	char *text, char *new_tag, unsigned int new_tag_len,
	struct sip_msg* msg, unsigned int *returned_len)
{
	char                    *buf, *p;
	unsigned int       len,foo;
	struct hdr_field  *hdr;
	struct lump_rpl  *lump;
	int                       i;
	str                        *tag_str;

	/* force parsing all headers -- we want to return all
	Via's in the reply and they may be scattered down to the
	end of header (non-block Vias are a really poor property
	of SIP :( ) */
	parse_headers( msg, HDR_EOH );

	/*computes the lenght of the new response buffer*/
	len = 0;
	/* first line */
	len += SIP_VERSION_LEN + 1/*space*/ + 3/*code*/ + 1/*space*/ + strlen(text) + CRLF_LEN/*new line*/;
	/*headers that will be copied (TO, FROM, CSEQ,CALLID,VIA)*/
	for ( hdr=msg->headers ; hdr ; hdr=hdr->next )
		switch (hdr->type)
		{
			case HDR_TO:
				if (new_tag)
				{
					if (get_to(msg)->tag_value.s )
						len+=new_tag_len-get_to(msg)->tag_value.len;
					else
						len+=new_tag_len+5/*";tag="*/;
				}
			case HDR_VIA:
			case HDR_FROM:
			case HDR_CALLID:
			case HDR_CSEQ:
				len += ((hdr->body.s+hdr->body.len )-hdr->name.s )+CRLF_LEN;
		}
	/*lumps length*/
	for(lump=msg->reply_lump;lump;lump=lump->next)
		len += lump->text.len;
#ifdef NOISY_REPLIES
	/*user agent header*/
	len += USER_AGENT_LEN + CRLF_LEN;
	/*content length header*/
	len +=CONTENT_LEN_LEN + CRLF_LEN;
#endif
	/* end of message */
	len += CRLF_LEN; /*new line*/
	/*allocating mem*/
	buf = 0;
	buf = (char*) malloc( len+1 );
	if (!buf)
	{
		LOG(L_ERR, "ERROR: build_res_buf_from_sip_req: out of memory "
			" ; needs %d\n",len);
		goto error;
	}

	/* filling the buffer*/
	p=buf;
	/* first line */
	memcpy( p , SIP_VERSION , SIP_VERSION_LEN );
	p += SIP_VERSION_LEN;
	*(p++) = ' ' ;
	/*code*/
	for ( i=2 , foo = code  ;  i>=0  ;  i-- , foo=foo/10 )
		*(p+i) = '0' + foo - ( foo/10 )*10;
	p += 3;
	*(p++) = ' ' ;
	memcpy( p , text , strlen(text) );
	p += strlen(text);
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
	/* headers*/
	for ( hdr=msg->headers ; hdr ; hdr=hdr->next )
		switch (hdr->type)
		{
			case HDR_TO:
				if (new_tag){
					if (get_to(msg)->tag_value.s ) {
						tag_str =&(get_to(msg)->tag_value);
						append_str_trans( p, hdr->name.s ,
							tag_str->s-hdr->name.s,msg);
						append_str( p, new_tag,new_tag_len,msg);
						append_str_trans( p,tag_str->s+tag_str->len,
							((hdr->body.s+hdr->body.len )-
							(tag_str->s+tag_str->len)),msg);
						append_str( p, CRLF,CRLF_LEN,msg);
					}else{
						append_str_trans( p, hdr->name.s ,
							((hdr->body.s+hdr->body.len )-hdr->name.s ),
							msg);
						append_str( p, ";tag=",5,msg);
						append_str( p, new_tag,new_tag_len,msg);
						append_str( p, CRLF,CRLF_LEN,msg);
					}
					break;
				}
			case HDR_VIA:
			case HDR_FROM:
			case HDR_CALLID:
			case HDR_CSEQ:
				append_str_trans( p, hdr->name.s ,
					((hdr->body.s+hdr->body.len )-hdr->name.s ),msg);
				append_str( p, CRLF,CRLF_LEN,msg);
		}
	/*lumps*/
	for(lump=msg->reply_lump;lump;lump=lump->next)
	{
		memcpy(p,lump->text.s,lump->text.len);
		p += lump->text.len;
	}
	/*user agent header*/
#ifdef NOISY_REPLIES
	memcpy( p, USER_AGENT , USER_AGENT_LEN );
	p+=USER_AGENT_LEN;
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
	/* content length header*/
	memcpy( p, CONTENT_LEN , CONTENT_LEN_LEN );
	p+=CONTENT_LEN_LEN;
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
#endif
	/*end of message*/
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
	*(p) = 0;
	*returned_len = len;
	return buf;
error:
	if (buf) free(buf);
	*returned_len=0;
	return 0;
}

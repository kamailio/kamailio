/* $Id$
 *
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>

#include "msg_translator.h"
#include "globals.h"
#include "error.h"
#include "mem/mem.h"
#include "dprint.h"
#include "config.h"
#include "md5utils.h"
#include "data_lump_rpl.h"
#include "ip_addr.h"
#include "resolve.h"



#define MAX_VIA_LINE_SIZE      240
#define MAX_RECEIVED_SIZE  57

/* mallocs for local stuff (not needed to be shared mem?)*/
#define local_malloc pkg_malloc
#define local_free   pkg_free


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



/* checks if ip is in host(name) and ?host(ip)=name?
 * ip must be in network byte order!
 *  resolver = DO_DNS | DO_REV_DNS; if 0 no dns check is made
 * return 0 if equal */
int check_address(struct ip_addr* ip, char *name, int resolver)
{
	struct hostent* he;
	int i;
	char* s;

	/* maybe we are lucky and name it's an ip */
	s=ip_addr2a(ip);
	if (s){
		DBG("check_address(%s, %s, %d)\n", ip_addr2a(ip), name, resolver);
		if (strcmp(name, s)==0)
			return 0;
	}else{
		LOG(L_CRIT, "check_address: BUG: could not convert ip address\n");
		return -1;
	}
		
	if (resolver&DO_DNS){
		DBG("check_address: doing dns lookup\n");
		/* try all names ips */
		he=resolvehost(name);
		if (he && ip->af==he->h_addrtype){
			for(i=0;he && he->h_addr_list[i];i++){
				if ( memcmp(&he->h_addr_list[i], ip->u.addr, ip->len)==0)
					return 0;
			}
		}
	}
	if (resolver&DO_REV_DNS){
		DBG("check_address: doing rev. dns lookup\n");
		/* try reverse dns */
		he=rev_resolvehost(ip);
		if (he && (strcmp(he->h_name, name)==0))
			return 0;
		for (i=0; he && he->h_aliases[i];i++){
			if (strcmp(he->h_aliases[i],name)==0)
				return 0;
		}
	}
	return -1;
}


char* via_builder( struct sip_msg *msg , unsigned int *len, 
					struct socket_info* send_sock )
{
	unsigned int  via_len, branch_len, extra_len;
	char               *line_buf;

	line_buf=0;
	extra_len=0;

	line_buf=pkg_malloc(sizeof(char)*MAX_VIA_LINE_SIZE);
	if (line_buf==0){
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: via_builder: out of memory\n");
		goto error;
	}
	via_len=MY_VIA_LEN+send_sock->address_str.len; /*space included in MY_VIA*/
#ifdef USE_IPV6
	if (send_sock->address.af==AF_INET6) via_len+=2; /* [ ]*/
#endif

	/* jku: if we compute branches using MD5 it will take 32 bytes */
	branch_len= (loop_checks ? MY_BRANCH_LEN : MY_BRANCH_LEN -1 + MD5_LEN)+
					msg->add_to_branch_len;

	if ((via_len+send_sock->port_no_str.len+branch_len
								+CRLF_LEN)<MAX_VIA_LINE_SIZE){
		memcpy(line_buf, MY_VIA, MY_VIA_LEN);
#ifdef USE_IPV6
	if (send_sock->address.af==AF_INET6) {
		line_buf[MY_VIA_LEN]='[';
		line_buf[MY_VIA_LEN+1+send_sock->address_str.len]=']';
		extra_len=1;
	}
#endif
		memcpy(line_buf+MY_VIA_LEN+extra_len, send_sock->address_str.s,
									send_sock->address_str.len);
		if (send_sock->port_no!=SIP_PORT){
			memcpy(line_buf+via_len, send_sock->port_no_str.s,
									 send_sock->port_no_str.len);
			via_len+=send_sock->port_no_str.len;
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
		ser_error=E_BUG;
		goto error;
	}

	*len = via_len;
	return line_buf;

error:
	if (line_buf) pkg_free(line_buf);
	return 0;
}




#ifdef VERY_NOISY_REPLIES
char * warning_builder( struct sip_msg *msg, unsigned int *returned_len)
{
	static char buf[MAX_WARNING_LEN];
	static unsigned int fix_len=0;
	str *foo;
	char *p;

	if (!fix_len)
	{
		memcpy(buf+fix_len,"Warning: 392 ",13);
		fix_len +=13;
		memcpy(buf+fix_len, sock_info[0].name.s,sock_info[0].name.len);
		fix_len += sock_info[0].name.len;
		//*(buf+fix_len++) = ':';
		memcpy(buf+fix_len,sock_info[0].port_no_str.s,
			sock_info[0].port_no_str.len);
		fix_len += sock_info[0].port_no_str.len;
		memcpy(buf+fix_len, " \"Noisy feedback tells: ",24);
		fix_len += 24;
	}

	p = buf+fix_len;
	/* adding pid */
	if (p-buf+10+2>=MAX_WARNING_LEN)
		goto done;
	p += sprintf(p, "pid=%d", pids?pids[process_no]:0 );
	*(p++)=' ';

	/*adding src_ip*/
	if (p-buf+26+2>=MAX_WARNING_LEN)
		goto done;
	p += sprintf(p,"req_src_ip=%s",ip_addr2a(&msg->src_ip));
	*(p++)=' ';

	/*adding in_uri*/
	if(p-buf+7+msg->first_line.u.request.uri.len+2>=MAX_WARNING_LEN)
		goto done;
	p += sprintf( p, "in_uri=%.*s",msg->first_line.u.request.uri.len,
		msg->first_line.u.request.uri.s);
	*(p++) = ' ';

	/*adding out_uri*/
	if (msg->new_uri.s)
		foo=&(msg->new_uri);
	else
		foo=&(msg->first_line.u.request.uri);
	if(p-buf+8+foo->len+2>=MAX_WARNING_LEN)
		goto done;
	p += sprintf( p, "out_uri=%.*s", foo->len, foo->s);

done:
	*(p++) = '\"';
	*(p) = 0;
	*returned_len = p-buf;
	return buf;
}
#endif




char* received_builder(struct sip_msg *msg, int *received_len)
{
	char *buf;
	int  len;
	struct ip_addr *source_ip;
	char *tmp;
	int  tmp_len;
	int extra_len;

	extra_len = 0;
	source_ip=&msg->src_ip;
	buf = 0;

	buf=pkg_malloc(sizeof(char)*MAX_RECEIVED_SIZE);
	if (buf==0){
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: build_req_buf_from_sip_req: out of memory\n");
		return 0;
	}
	/*
	received_len=snprintf(buf, MAX_RECEIVED_SIZE,
							";received=%s",
							inet_ntoa(*(struct in_addr *)&source_ip));
	*/
	memcpy(buf, RECEIVED, RECEIVED_LEN);
	tmp=ip_addr2a(source_ip);
	tmp_len=strlen(tmp);
	len=RECEIVED_LEN+tmp_len;
	if(source_ip->af==AF_INET6){
		len+=2;
		buf[RECEIVED_LEN]='[';
		buf[RECEIVED_LEN+tmp_len+1]=']';
		extra_len=1;
	}
	
	memcpy(buf+RECEIVED_LEN+extra_len, tmp, tmp_len);
	buf[len]=0; /*null terminate it */

	*received_len = len;
	return buf;
}




char * build_req_buf_from_sip_req( struct sip_msg* msg,
								unsigned int *returned_len,
								struct socket_info* send_sock)
{
	unsigned int len, new_len, received_len, uri_len, via_len;
	char* line_buf;
	char* received_buf;
	char* new_buf;
	char* orig;
	char* buf;
	char  backup;
	unsigned int offset, s_offset, size;
	struct lump *t,*r;
	struct lump* anchor;

	uri_len=0;
	orig=msg->orig;
	buf=msg->buf;
	len=msg->len;
	received_len=0;
	new_buf=0;
	received_buf=0;


	line_buf = via_builder( msg, &via_len, send_sock);
	if (!line_buf){
		LOG(L_ERR,"ERROR: build_req_buf_from_sip_req: no via received!\n");
		goto error1;
	}
	/* check if received needs to be added */
	backup = msg->via1->host.s[msg->via1->host.len];
	msg->via1->host.s[msg->via1->host.len] = 0;
	if (check_address(&msg->src_ip, msg->via1->host.s, received_dns)!=0){
		if ((received_buf=received_builder(msg,&received_len))==0)
			goto error;
	}
	msg->via1->host.s[msg->via1->host.len] = backup;

	/* add via header to the list */
	/* try to add it before msg. 1st via */
	/* add first via, as an anchor for second via*/
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
					/*size+=strlen(msg->via1->hdr.s+size+1)+1;*/
					size += msg->via1->port_str.len + 1; /* +1 for ':'*/
				}
			#ifdef USE_IPV6
				if(send_sock->address.af==AF_INET6) size+=1; /* +1 for ']'*/
			#endif
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
			default:
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
	new_buf=(char*)local_malloc(new_len+1);
	if (new_buf==0){
		ser_error=E_OUT_OF_MEM;
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
									"invalid op for data lump (%x)\n",r->op);
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
	if (new_buf) local_free(new_buf);
	*returned_len=0;
	return 0;
}




char * build_res_buf_from_sip_res( struct sip_msg* msg,
				unsigned int *returned_len)
{
	unsigned int new_len, via_len;
	char* new_buf;
	unsigned offset, s_offset, size;
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
	new_buf=(char*)local_malloc(new_len+1);/* +1 is for debugging
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
	if (new_buf) local_free(new_buf);
	*returned_len=0;
	return 0;
}





char * build_res_buf_from_sip_req( unsigned int code, char *text,
					char *new_tag, unsigned int new_tag_len,
					struct sip_msg* msg, unsigned int *returned_len)
{
	char              *buf, *p;
	unsigned int      len,foo;
	struct hdr_field  *hdr;
	struct lump_rpl   *lump;
	int               i;
	str               *tag_str;
	char              backup;
	char              *received_buf;
	int               received_len;
#ifdef VERY_NOISY_REPLIES
	char              *warning;
	unsigned int      warning_len;
#endif

	received_buf=0;
	received_len=0;
	buf=0;

	/* force parsing all headers -- we want to return all
	Via's in the reply and they may be scattered down to the
	end of header (non-block Vias are a really poor property
	of SIP :( ) */
	parse_headers( msg, HDR_EOH );

	/* check if received needs to be added */
	backup = msg->via1->host.s[msg->via1->host.len];
	msg->via1->host.s[msg->via1->host.len] = 0;
	if (check_address(&msg->src_ip, msg->via1->host.s, received_dns)!=0){
		if ((received_buf=received_builder(msg,&received_len))==0)
			goto error;
	}
	msg->via1->host.s[msg->via1->host.len] = backup;

	/*computes the lenght of the new response buffer*/
	len = 0;
	/* first line */
	len += SIP_VERSION_LEN + 1/*space*/ + 3/*code*/ + 1/*space*/ +
		strlen(text) + CRLF_LEN/*new line*/;
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
				if (hdr==msg->h_via1) len += received_len;
			case HDR_FROM:
			case HDR_CALLID:
			case HDR_CSEQ:
			case HDR_RECORDROUTE:
				len += ((hdr->body.s+hdr->body.len )-hdr->name.s )+CRLF_LEN;
		}
	/*lumps length*/
	for(lump=msg->reply_lump;lump;lump=lump->next)
		len += lump->text.len;
#ifdef NOISY_REPLIES
	/*server header*/
	len += SERVER_HDR_LEN + CRLF_LEN;
	/*content length header*/
	len +=CONTENT_LEN_LEN + CRLF_LEN;
#endif
#ifdef VERY_NOISY_REPLIES
	warning = warning_builder(msg,&warning_len);
	len += warning_len + CRLF_LEN;
#endif
	/* end of message */
	len += CRLF_LEN; /*new line*/
	/*allocating mem*/
	buf = (char*) local_malloc( len+1 );
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
				append_str_trans( p, hdr->name.s ,
					((hdr->body.s+hdr->body.len )-hdr->name.s ),msg);
				if (hdr==msg->h_via1 && received_buf)
					append_str( p, received_buf, received_len, msg);
				append_str( p, CRLF,CRLF_LEN,msg);
				break;
			case HDR_FROM:
			case HDR_CALLID:
			case HDR_CSEQ:
			case HDR_RECORDROUTE:
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
#ifdef NOISY_REPLIES
	/*server header*/
	memcpy( p, SERVER_HDR , SERVER_HDR_LEN );
	p+=SERVER_HDR_LEN;
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
	/* content length header*/
	memcpy( p, CONTENT_LEN , CONTENT_LEN_LEN );
	p+=CONTENT_LEN_LEN;
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
#endif
#ifdef VERY_NOISY_REPLIES
	memcpy( p, warning, warning_len);
	p+=warning_len;
	memcpy( p, CRLF, CRLF_LEN);
	p+=CRLF_LEN;
#endif
	/*end of message*/
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
	*(p) = 0;
	*returned_len = len;
	return buf;
error:
	if (buf) local_free(buf);
	*returned_len=0;
	return 0;
}

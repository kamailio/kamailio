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



#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
#include "ut.h"
#include "pt.h"


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
		DBG("check_address(%s, %s, %d)\n", s, name, resolver);
	#ifdef USE_IPV6
		if ((ip->af==AF_INET6) && (strcasecmp(name, s)==0))
				return 0;
		else
	#endif
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


char * warning_builder( struct sip_msg *msg, unsigned int *returned_len)
{
	static char buf[MAX_WARNING_LEN];
	static unsigned int fix_len=0;
	str *foo;
	int print_len;

	if (!fix_len)
	{
		memcpy(buf+fix_len,"Warning: 392 ",13);
		fix_len +=13;
		memcpy(buf+fix_len, bind_address->name.s,bind_address->name.len);
		fix_len += bind_address->name.len;
		//*(buf+fix_len++) = ':';
		memcpy(buf+fix_len,bind_address->port_no_str.s,
			bind_address->port_no_str.len);
		fix_len += bind_address->port_no_str.len;
		memcpy(buf+fix_len, " \"Noisy feedback tells: ",24);
		fix_len += 24;
	}

	/*adding out_uri*/
	if (msg->new_uri.s)
		foo=&(msg->new_uri);
	else
		foo=&(msg->first_line.u.request.uri);
	print_len=snprintf(buf+fix_len, MAX_WARNING_LEN-fix_len,
		"pid=%d req_src_ip=%s in_uri=%.*s out_uri=%.*s via_cnt%c=%d\"",
		my_pid(),
		ip_addr2a(&msg->src_ip),
		msg->first_line.u.request.uri.len, msg->first_line.u.request.uri.s,
		foo->len, foo->s, 
		msg->parsed_flag & HDR_EOH ? '=' : '>', /* should be = */
		via_cnt );

	if (print_len==-1) {
		*returned_len=0;
		return 0;
	} else {
		*returned_len=fix_len+print_len;
		return buf;
	}
}




char* received_builder(struct sip_msg *msg, unsigned int *received_len)
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
		LOG(L_ERR, "ERROR: received_builder: out of memory\n");
		return 0;
	}
	/*
	received_len=snprintf(buf, MAX_RECEIVED_SIZE,
							";received=%s",
							inet_ntoa(*(struct in_addr *)&source_ip));
	*/
	memcpy(buf, RECEIVED, RECEIVED_LEN);
	if ( (tmp=ip_addr2a(source_ip))==0)
		return 0; /* error*/
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



/* computes the "unpacked" len of a lump list,
   code moved from build_req_from_req */
static inline int lumps_len(struct lump* l)
{
	int s_offset;
	int new_len;
	struct lump* t;
	struct lump* r;

	s_offset=0;
	new_len=0;
	for(t=l;t;t=t->next){
		for(r=t->before;r;r=r->before){
			switch(r->op){
				case LUMP_ADD:
					new_len+=r->len;
					break;
				default:
					/* only ADD allowed for before/after */
					LOG(L_CRIT, "BUG: lumps_len: invalid op "
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
				LOG(L_CRIT,"BUG:lumps_len: invalid"
							" op for data lump (%x)\n", r->op);
		}
		for (r=t->after;r;r=r->after){
			switch(r->op){
				case LUMP_ADD:
					new_len+=r->len;
					break;
				default:
					/* only ADD allowed for before/after */
					LOG(L_CRIT, "BUG:lumps_len: invalid"
								" op for data lump (%x)\n", r->op);
			}
		}
	}
	return new_len;
}



/* another helper functions, adds/Removes the lump,
	code moved form build_req_from_req  */

static inline void process_lumps(	struct lump* l,	char* new_buf, 
									unsigned int* new_buf_offs, char* orig,
									unsigned int* orig_offs)
{
	struct lump *t;
	struct lump *r;
	int size;
	int offset;
	int s_offset;
	
	offset=*new_buf_offs;
	s_offset=*orig_offs;
	
	for (t=l;t;t=t->next){
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
							LOG(L_CRIT, "BUG:process_lumps: "
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
							LOG(L_CRIT, "BUG:process_lumps: "
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
							LOG(L_CRIT, "BUG:process_lumps: "
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
							LOG(L_CRIT, "BUG:process_lumps: "
									"invalid op for data lump (%x)\n", r->op);
					}
				}
				break;
			default:
					LOG(L_CRIT, "BUG: process_lumps: "
							"unknown op (%x)\n", t->op);
		}
	}
	*new_buf_offs=offset;
	*orig_offs=s_offset;
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
	struct lump* anchor;
	int r;

	uri_len=0;
	orig=msg->orig;
	buf=msg->buf;
	len=msg->len;
	received_len=0;
	new_buf=0;
	received_buf=0;


	line_buf = via_builder( &via_len, send_sock, 
		msg->add_to_branch_s, msg->add_to_branch_len);
	if (!line_buf){
		LOG(L_ERR,"ERROR: build_req_buf_from_sip_req: no via received!\n");
		goto error00;
	}
	/* check if received needs to be added */
	backup = msg->via1->host.s[msg->via1->host.len];
	msg->via1->host.s[msg->via1->host.len] = 0;
	r=check_address(&msg->src_ip, msg->via1->host.s, received_dns);
	msg->via1->host.s[msg->via1->host.len] = backup;
	if (r!=0){
		if ((received_buf=received_builder(msg,&received_len))==0)
			goto error01;  /* free also line_buf */
	}

	/* add via header to the list */
	/* try to add it before msg. 1st via */
	/* add first via, as an anchor for second via*/
	anchor=anchor_lump(&(msg->add_rm), msg->via1->hdr.s-buf, 0, HDR_VIA);
	if (anchor==0) goto error01;
	if (insert_new_lump_before(anchor, line_buf, via_len, HDR_VIA)==0)
		goto error01;
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
		if (anchor==0) goto error02; /* free also line_buf */
		if (insert_new_lump_after(anchor, received_buf, received_len, HDR_VIA)
				==0 ) goto error02; /* free also line_buf */
	}

	/* compute new msg len and fix overlapping zones*/
	new_len=len+lumps_len(msg->add_rm);

	if (msg->new_uri.s){
		uri_len=msg->new_uri.len;
		new_len=new_len-msg->first_line.u.request.uri.len+uri_len;
	}
	new_buf=(char*)pkg_malloc(new_len+1);
	if (new_buf==0){
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: build_req_buf_from_sip_req: out of memory\n");
		goto error00;
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
	process_lumps(msg->add_rm, new_buf, &offset, orig, &s_offset);
	/* copy the rest of the message */
	memcpy(new_buf+offset, orig+s_offset, len-s_offset);
	new_buf[new_len]=0;

#ifdef DBG_MSG_QA
	if (new_buf[new_len-1]==0) {
		LOG(L_ERR, "ERROR: build_req_buf_from_sip_req: 0 in the end\n");
		abort();
	}
#endif

	*returned_len=new_len;
	return new_buf;

error01:
	pkg_free(line_buf);
error02:
	if (received_buf) pkg_free(received_buf);
error00:
	*returned_len=0;
	return 0;
}


char * build_res_buf_from_sip_res( struct sip_msg* msg,
				unsigned int *returned_len)
{
	unsigned int new_len, via_len;
	char* new_buf;
	unsigned offset, s_offset, via_offset;
	char* orig;
	char* buf;
	unsigned int len;

	orig=msg->orig;
	buf=msg->buf;
	len=msg->len;
	new_buf=0;
	/* we must remove the first via */
	via_len=msg->via1->bsize;
	via_offset=msg->via1->hdr.s-buf;
	DBG("via len: %d, initial via offset: %d\n", via_len, via_offset);
	if (msg->via1->next){
		/* add hdr size*/
		via_offset+=msg->via1->hdr.len+1;
	    DBG(" adjusted via len: %d, initial offset: %d\n",
				via_len, via_offset);
	}else{
		/* add hdr size ("Via:")*/
		via_len+=msg->via1->hdr.len+1;
	}
	/* remove the first via*/
	if (del_lump( &(msg->repl_add_rm), via_offset, via_len, HDR_VIA)==0){
		LOG(L_ERR, "build_res_buf_from_sip_res: error trying to remove first"
					"via\n");
		goto error;
	}
	new_len=len+lumps_len(msg->repl_add_rm);

	DBG(" old size: %d, new size: %d\n", len, new_len);
	new_buf=(char*)pkg_malloc(new_len+1); /* +1 is for debugging 
											 (\0 to print it )*/
	if (new_buf==0){
		LOG(L_ERR, "ERROR: build_res_buf_from_sip_res: out of mem\n");
		goto error;
	}
	new_buf[new_len]=0; /* debug: print the message */
	offset=s_offset=0;
	process_lumps(msg->repl_add_rm, new_buf, &offset, orig, &s_offset);
	/* copy the rest of the message */
	memcpy(new_buf+offset,orig+s_offset, len-s_offset);
	 /* send it! */
	DBG(" copied size: orig:%d, new: %d, rest: %d\n",
			s_offset, offset,
			len-s_offset );

	*returned_len=new_len;
	return new_buf;
error:
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
	char              backup;
	char              *received_buf;
	unsigned int               received_len;
	char              *warning;
	unsigned int      warning_len;
	int r;
	str to_tag;

	received_buf=0;
	received_len=0;
	buf=0;
	/* make -Wall happy */
	warning=0;

	/* force parsing all headers -- we want to return all
	Via's in the reply and they may be scattered down to the
	end of header (non-block Vias are a really poor property
	of SIP :( ) */
	if (parse_headers( msg, HDR_EOH, 0 )==-1) {
		LOG(L_ERR, "ERROR: build_res_buf_from_sip_req: "
			"alas, parse_headers failed\n");
		goto error00;
	}

	/* check if received needs to be added */
	backup = msg->via1->host.s[msg->via1->host.len];
	msg->via1->host.s[msg->via1->host.len] = 0;
	r=check_address(&msg->src_ip, msg->via1->host.s, received_dns);
	msg->via1->host.s[msg->via1->host.len] = backup;
	if (r!=0) {
		if ((received_buf=received_builder(msg,&received_len))==0) {
			LOG(L_ERR, "ERROR: build_res_buf_from_sip_req: "
				"alas, received_builder failed\n");
			goto error00;
		}
	}

	/*computes the lenght of the new response buffer*/
	len = 0;
	/* first line */
	len += SIP_VERSION_LEN + 1/*space*/ + 3/*code*/ + 1/*space*/ +
		strlen(text) + CRLF_LEN/*new line*/;
	/*headers that will be copied (TO, FROM, CSEQ,CALLID,VIA)*/
	for ( hdr=msg->headers ; hdr ; hdr=hdr->next ) {
		if (hdr->type==HDR_TO) {
			if (new_tag)
			{
				to_tag=get_to(msg)->tag_value;
				if (to_tag.s )
					len+=new_tag_len-to_tag.len;
				else
					len+=new_tag_len+TOTAG_TOKEN_LEN/*";tag="*/;
			}
		} else if (hdr->type==HDR_VIA) {
				if (hdr==msg->h_via1) len += received_len;
		} else if (hdr->type==HDR_RECORDROUTE) {
				/* RR only for 1xx and 2xx replies */
				if (code<180 || code>=300) continue;
		} else if (!(hdr->type==HDR_FROM 
					|| hdr->type==HDR_CALLID
					|| hdr->type==HDR_CSEQ)) {
			continue;
		}
		len += ((hdr->body.s+hdr->body.len )-hdr->name.s )+CRLF_LEN;
	}
	/*lumps length*/
	for(lump=msg->reply_lump;lump;lump=lump->next)
		len += lump->text.len;
	if (server_signature) {
		/*server header*/
		len += SERVER_HDR_LEN + CRLF_LEN;
		/*content length header*/
		len +=CONTENT_LENGTH_LEN+1 + CRLF_LEN;
	}
	if (sip_warning) {
		warning = warning_builder(msg,&warning_len);
		if (warning==0) {
			LOG(L_ERR, "ERROR: warning too big\n");
			goto error01;
		}
		len += warning_len + CRLF_LEN;
	}
	/* end of message */
	len += CRLF_LEN; /*new line*/
	/*allocating mem*/
	buf = (char*) pkg_malloc( len+1 );
	if (!buf)
	{
		LOG(L_ERR, "ERROR: build_res_buf_from_sip_req: out of memory "
			" ; needs %d\n",len);
		goto error01;
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
					if (to_tag.s ) {
						append_str_trans( p, hdr->name.s ,
							to_tag.s-hdr->name.s,msg);
						append_str( p, new_tag,new_tag_len,msg);
						append_str_trans( p,to_tag.s+to_tag.len,
							((hdr->body.s+hdr->body.len )-
							(to_tag.s+to_tag.len)),msg);
						append_str( p, CRLF,CRLF_LEN,msg);
					}else{
						append_str_trans( p, hdr->name.s ,
							((hdr->body.s+hdr->body.len )-hdr->name.s ),
							msg);
						append_str( p, TOTAG_TOKEN,TOTAG_TOKEN_LEN,msg);
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
			case HDR_RECORDROUTE:
				/* RR only for 1xx and 2xx replies */
				if (code<180 || code>=300) break;
			case HDR_FROM:
			case HDR_CALLID:
			case HDR_CSEQ:
					append_str_trans( p, hdr->name.s ,
						((hdr->body.s+hdr->body.len )-hdr->name.s ),msg);
					append_str( p, CRLF,CRLF_LEN,msg);
		} /* for switch */
	/*lumps*/
	for(lump=msg->reply_lump;lump;lump=lump->next)
	{
		memcpy(p,lump->text.s,lump->text.len);
		p += lump->text.len;
	}
	if (server_signature) {
		/*server header*/
		memcpy( p, SERVER_HDR , SERVER_HDR_LEN );
		p+=SERVER_HDR_LEN;
		memcpy( p, CRLF, CRLF_LEN );
		p+=CRLF_LEN;
		/* content length header*/
		memcpy( p, CONTENT_LENGTH "0" , CONTENT_LENGTH_LEN+1 );
		p+=CONTENT_LENGTH_LEN+1;
		memcpy( p, CRLF, CRLF_LEN );
		p+=CRLF_LEN;
	}
	if (sip_warning) {
		memcpy( p, warning, warning_len);
		p+=warning_len;
		memcpy( p, CRLF, CRLF_LEN);
		p+=CRLF_LEN;
	}
	/*end of message*/
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
	*(p) = 0;
	*returned_len = len;
	/* in req2reply, received_buf is not introduced to lumps and
	   needs to be deleted here
	*/
	if (received_buf) pkg_free(received_buf);
	return buf;

error01:
	if (received_buf) pkg_free(received_buf);
error00:
	*returned_len=0;
	return 0;
}

/* return number of chars printed or 0 if space exceeded;
   assumes buffer sace of at least MAX_BRANCH_PARAM_LEN
 */

int branch_builder( unsigned int hash_index,
	/* only either parameter useful */
	unsigned int label, char * char_v,
	int branch,
	char *branch_str, int *len )
{

	char *begin;
	int size;

#ifdef _OBSOLETED
	/* no hash_id --- whoever called me wants to have
	   very simple branch_id
	*/
	if (hash_index==0) {
		*branch_str='0';
		*len=1;
		return *len;
	}
#endif

	/* hash id provided ... start with it */
	size=MAX_BRANCH_PARAM_LEN;
	begin=branch_str;
	*len=0;

	memcpy(begin, MCOOKIE, MCOOKIE_LEN );
	size-=MCOOKIE_LEN;begin+=MCOOKIE_LEN;

	if (int2reverse_hex( &begin, &size, hash_index)==-1)
		return 0;

	if (size) {
		*begin=BRANCH_SEPARATOR;
		begin++; size--;
	} else return 0;

	/* string with request's characteristic value ... use it ... */
	if (char_v) {
		if (memcpy(begin,char_v,MD5_LEN)) {
			begin+=MD5_LEN; size-=MD5_LEN;
		} else return 0;
	} else { /* ... use the "label" value otherwise */
		if (int2reverse_hex( &begin, &size, label )==-1)
			return 0;
	}

	if (size) {
		*begin=BRANCH_SEPARATOR;
		begin++; size--;
	} else return 0;

	if (int2reverse_hex( &begin, &size, branch)==-1)
		return 0;

	*len=MAX_BRANCH_PARAM_LEN-size;
	return size;
		
}


char* via_builder( unsigned int *len, 
	struct socket_info* send_sock,
	char *branch, int branch_len )
{
	unsigned int  via_len, extra_len;
	char               *line_buf;
	int max_len;


	max_len=MY_VIA_LEN+send_sock->address_str.len /* space in MY_VIA */
		+2 /* just in case it it a v6 address ... [ ] */
		+send_sock->port_no_str.len
		+MY_BRANCH_LEN+branch_len+CRLF_LEN+1;
	line_buf=pkg_malloc( max_len );
	if (line_buf==0){
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: via_builder: out of memory\n");
		return 0;
	}

	extra_len=0;

	via_len=MY_VIA_LEN+send_sock->address_str.len; /*space included in MY_VIA*/

	memcpy(line_buf, MY_VIA, MY_VIA_LEN);
#	ifdef USE_IPV6
	if (send_sock->address.af==AF_INET6) {
		line_buf[MY_VIA_LEN]='[';
		line_buf[MY_VIA_LEN+1+send_sock->address_str.len]=']';
		extra_len=1;
		via_len+=2; /* [ ]*/
	}
#	endif
	memcpy(line_buf+MY_VIA_LEN+extra_len, send_sock->address_str.s,
		send_sock->address_str.len);
	if (send_sock->port_no!=SIP_PORT){
		memcpy(line_buf+via_len, send_sock->port_no_str.s,
			 send_sock->port_no_str.len);
		via_len+=send_sock->port_no_str.len;
	}

	/* branch parameter */
	memcpy(line_buf+via_len, MY_BRANCH, MY_BRANCH_LEN );
	via_len+=MY_BRANCH_LEN;
	memcpy(line_buf+via_len, branch, branch_len );
	via_len+=branch_len;
	memcpy(line_buf+via_len, CRLF, CRLF_LEN);
	via_len+=CRLF_LEN;
	line_buf[via_len]=0; /* null terminate the string*/

	*len = via_len;
	return line_buf;
}

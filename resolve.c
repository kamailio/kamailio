/* $Id$*/
/*
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
/*
 * History:
 * -------
 *  2003-02-13  added proto to sip_resolvehost, for SRV lookups (andrei)
 *  2003-07-03  default port value set according to proto (andrei)
 */ 


#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>

#include "resolve.h"
#include "dprint.h"
#include "mem/mem.h"
#include "ip_addr.h"



/* mallocs for local stuff */
#define local_malloc pkg_malloc
#define local_free   pkg_free



 /*  skips over a domain name in a dns message
 *  (it can be  a sequence of labels ending in \0, a pointer or
 *   a sequence of labels ending in a pointer -- see rfc1035
 *   returns pointer after the domain name or null on error*/
unsigned char* dns_skipname(unsigned char* p, unsigned char* end)
{
	while(p<end){
		/* check if \0 (root label length) */
		if (*p==0){
			p+=1;
			break;
		}
		/* check if we found a pointer */
		if (((*p)&0xc0)==0xc0){
			/* if pointer skip over it (2 bytes) & we found the end */
			p+=2;
			break;
		}
		/* normal label */
		p+=*p+1;
	}
	return (p>=end)?0:p;
}



/* parses the srv record into a srv_rdata structure
 *   msg   - pointer to the dns message
 *   end   - pointer to the end of the message
 *   rdata - pointer  to the rdata part of the srv answer
 * returns 0 on error, or a dyn. alloc'ed srv_rdata structure */
/* SRV rdata format:
 *            111111
 *  0123456789012345
 * +----------------+
 * |     priority   |
 * |----------------|
 * |     weight     |
 * |----------------|
 * |   port number  |
 * |----------------|
 * |                |
 * ~      name      ~
 * |                |
 * +----------------+
 */
struct srv_rdata* dns_srv_parser( unsigned char* msg, unsigned char* end,
								  unsigned char* rdata)
{
	struct srv_rdata* srv;
	int len;
	
	srv=0;
	if ((rdata+6)>=end) goto error;
	srv=(struct srv_rdata*)local_malloc(sizeof(struct srv_rdata));
	if (srv==0){
		LOG(L_ERR, "ERROR: dns_srv_parser: out of memory\n");
		goto error;
	}
	
	memcpy((void*)&srv->priority, rdata, 2);
	memcpy((void*)&srv->weight,   rdata+2, 2);
	memcpy((void*)&srv->port,     rdata+4, 2);
	rdata+=6;
	srv->priority=ntohs(srv->priority);
	srv->weight=ntohs(srv->weight);
	srv->port=ntohs(srv->port);
	if ((len=dn_expand(msg, end, rdata, srv->name, MAX_DNS_NAME-1))==-1)
		goto error;
	/* add terminating 0 ? (warning: len=compressed name len) */
	return srv;
error:
	if (srv) local_free(srv);
	return 0;
}


/* parses the naptr record into a naptr_rdata structure
 *   msg   - pointer to the dns message
 *   end   - pointer to the end of the message
 *   rdata - pointer  to the rdata part of the naptr answer
 * returns 0 on error, or a dyn. alloc'ed naptr_rdata structure */
/* NAPTR rdata format:
 *            111111
 *  0123456789012345
 * +----------------+
 * |      order     |
 * |----------------|
 * |   preference   |
 * |----------------|
 * ~     flags      ~
 * |   (string)     |
 * |----------------|
 * ~    services    ~
 * |   (string)     |
 * |----------------|
 * ~    regexp      ~
 * |   (string)     |
 * |----------------|
 * ~  replacement   ~
   |    (name)      |
 * +----------------+
 */
struct naptr_rdata* dns_naptr_parser( unsigned char* msg, unsigned char* end,
								  unsigned char* rdata)
{
	struct naptr_rdata* naptr;
	int len;
	
	naptr = 0;
	if ((rdata + 7) >= end) goto error;
	naptr=(struct naptr_rdata*)local_malloc(sizeof(struct naptr_rdata));
	if (naptr == 0){
		LOG(L_ERR, "ERROR: dns_naptr_parser: out of memory\n");
		goto error;
	}
	
	memcpy((void*)&naptr->order, rdata, 2);
	naptr->order=ntohs(naptr->order);
	memcpy((void*)&naptr->pref, rdata + 2, 2);
	naptr->pref=ntohs(naptr->pref);
	naptr->flags_len = (int)rdata[4];
	if ((rdata + 7 +  naptr->flags_len) >= end) goto error;
	memcpy((void*)&naptr->flags, rdata + 5, naptr->flags_len);
	naptr->services_len = (int)rdata[5 + naptr->flags_len];
	if ((rdata + 7 + naptr->flags_len + naptr->services_len) >= end) goto error;
	memcpy((void*)&naptr->services, rdata + 6 + naptr->flags_len, naptr->services_len);
	naptr->regexp_len = (int)rdata[6 + naptr->flags_len + naptr->services_len];
	if ((rdata + 7 + naptr->flags_len + naptr->services_len +
					naptr->regexp_len) >= end) goto error;
	memcpy((void*)&naptr->regexp, rdata + 7 + naptr->flags_len +
				naptr->services_len, naptr->regexp_len);
	rdata = rdata + 7 + naptr->flags_len + naptr->services_len + 
			naptr->regexp_len;
	if ((len=dn_expand(msg, end, rdata, naptr->repl, MAX_DNS_NAME-1)) == -1)
		goto error;
	/* add terminating 0 ? (warning: len=compressed name len) */
	return naptr;
error:
	if (naptr) local_free(naptr);
	return 0;
}



/* parses a CNAME record into a cname_rdata structure */
struct cname_rdata* dns_cname_parser( unsigned char* msg, unsigned char* end,
									  unsigned char* rdata)
{
	struct cname_rdata* cname;
	int len;
	
	cname=0;
	cname=(struct cname_rdata*)local_malloc(sizeof(struct cname_rdata));
	if(cname==0){
		LOG(L_ERR, "ERROR: dns_cname_parser: out of memory\n");
		goto error;
	}
	if ((len=dn_expand(msg, end, rdata, cname->name, MAX_DNS_NAME-1))==-1)
		goto error;
	return cname;
error:
	if (cname) local_free(cname);
	return 0;
}



/* parses an A record rdata into an a_rdata structure
 * returns 0 on error or a dyn. alloc'ed a_rdata struct
 */
struct a_rdata* dns_a_parser(unsigned char* rdata, unsigned char* end)
{
	struct a_rdata* a;
	
	if (rdata+4>=end) goto error;
	a=(struct a_rdata*)local_malloc(sizeof(struct a_rdata));
	if (a==0){
		LOG(L_ERR, "ERROR: dns_a_parser: out of memory\n");
		goto error;
	}
	memcpy(a->ip, rdata, 4);
	return a;
error:
	return 0;
}



/* parses an AAAA (ipv6) record rdata into an aaaa_rdata structure
 * returns 0 on error or a dyn. alloc'ed aaaa_rdata struct */
struct aaaa_rdata* dns_aaaa_parser(unsigned char* rdata, unsigned char* end)
{
	struct aaaa_rdata* aaaa;
	
	if (rdata+16>=end) goto error;
	aaaa=(struct aaaa_rdata*)local_malloc(sizeof(struct aaaa_rdata));
	if (aaaa==0){
		LOG(L_ERR, "ERROR: dns_aaaa_parser: out of memory\n");
		goto error;
	}
	memcpy(aaaa->ip6, rdata, 16);
	return aaaa;
error:
	return 0;
}



/* frees completely a struct rdata list */
void free_rdata_list(struct rdata* head)
{
	struct rdata* l;
	for(l=head; l; l=l->next){
		/* free the parsed rdata*/
		if (l->rdata) local_free(l->rdata);
		local_free(l);
	}
}



/* gets the DNS records for name:type
 * returns a dyn. alloc'ed struct rdata linked list with the parsed responses
 * or 0 on error
 * see rfc1035 for the query/response format */
struct rdata* get_record(char* name, int type)
{
	int size;
	int qno, answers_no;
	int r;
	int ans_len;
	static union dns_query buff;
	unsigned char* p;
	unsigned char* t;
	unsigned char* end;
	static unsigned char answer[ANS_SIZE];
	unsigned short rtype, class, rdlength;
	unsigned int ttl;
	struct rdata* head;
	struct rdata** crt;
	struct rdata** last;
	struct rdata* rd;
	struct srv_rdata* srv_rd;
	struct srv_rdata* crt_srv;
	
	size=res_search(name, C_IN, type, buff.buff, sizeof(buff));
	if (size<0) {
		DBG("get_record: lookup(%s, %d) failed\n", name, type);
		goto not_found;
	}
	else if (size > sizeof(buff)) size=sizeof(buff);
	head=rd=0;
	last=crt=&head;
	
	p=buff.buff+DNS_HDR_SIZE;
	end=buff.buff+size;
	if (p>=end) goto error_boundary;
	qno=ntohs((unsigned short)buff.hdr.qdcount);

	for (r=0; r<qno; r++){
		/* skip the name of the question */
		if ((p=dns_skipname(p, end))==0) {
			LOG(L_ERR, "ERROR: get_record: skipname==0\n");
			goto error;
		}
		p+=2+2; /* skip QCODE & QCLASS */
	#if 0
		for (;(p<end && (*p)); p++);
		p+=1+2+2; /* skip the ending  '\0, QCODE and QCLASS */
	#endif
		if (p>=end) {
			LOG(L_ERR, "ERROR: get_record: p>=end\n");
			goto error;
		}
	};
	answers_no=ntohs((unsigned short)buff.hdr.ancount);
	ans_len=ANS_SIZE;
	t=answer;
	for (r=0; (r<answers_no) && (p<end); r++){
		/*  ignore it the default domain name */
		if ((p=dns_skipname(p, end))==0) {
			LOG(L_ERR, "ERROR: get_record: skip_name=0 (#2)\n");
			goto error;
		}
		/*
		skip=dn_expand(buff.buff, end, p, t, ans_len);
		p+=skip;
		*/
		/* check if enough space is left for type, class, ttl & size */
		if ((p+2+2+4+2)>=end) goto error_boundary;
		/* get type */
		memcpy((void*) &rtype, (void*)p, 2);
		rtype=ntohs(rtype);
		p+=2;
		/* get  class */
		memcpy((void*) &class, (void*)p, 2);
		class=ntohs(class);
		p+=2;
		/* get ttl*/
		memcpy((void*) &ttl, (void*)p, 4);
		ttl=ntohl(ttl);
		p+=4;
		/* get size */
		memcpy((void*)&rdlength, (void*)p, 2);
		rdlength=ntohs(rdlength);
		p+=2;
		/* check for type */
		/*
		if (rtype!=type){
			LOG(L_ERR, "WARNING: get_record: wrong type in answer (%d!=%d)\n",
					rtype, type);
			p+=rdlength;
			continue;
		}
		*/
		/* expand the "type" record  (rdata)*/
		
		rd=(struct rdata*) local_malloc(sizeof(struct rdata));
		if (rd==0){
			LOG(L_ERR, "ERROR: get_record: out of memory\n");
			goto error;
		}
		rd->type=rtype;
		rd->class=class;
		rd->ttl=ttl;
		rd->next=0;
		switch(rtype){
			case T_SRV:
				srv_rd= dns_srv_parser(buff.buff, end, p);
				rd->rdata=(void*)srv_rd;
				if (srv_rd==0) goto error_parse;
				
				/* insert sorted into the list */
				for (crt=&head; *crt; crt= &((*crt)->next)){
					crt_srv=(struct srv_rdata*)(*crt)->rdata;
					if ((srv_rd->priority <  crt_srv->priority) ||
					   ( (srv_rd->priority == crt_srv->priority) && 
							 (srv_rd->weight > crt_srv->weight) ) ){
						/* insert here */
						goto skip;
					}
				}
				last=&(rd->next); /*end of for => this will be the last elem*/
			skip:
				/* insert here */
				rd->next=*crt;
				*crt=rd;
				
				break;
			case T_A:
				rd->rdata=(void*) dns_a_parser(p,end);
				if (rd->rdata==0) goto error_parse;
				*last=rd; /* last points to the last "next" or the list head*/
				last=&(rd->next);
				break;
			case T_AAAA:
				rd->rdata=(void*) dns_aaaa_parser(p,end);
				if (rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_CNAME:
				rd->rdata=(void*) dns_cname_parser(buff.buff, end, p);
				if(rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_NAPTR:
				rd->rdata=(void*) dns_naptr_parser(buff.buff, end, p);
				if(rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			default:
				LOG(L_ERR, "WARNING: get_record: unknown type %d\n", rtype);
				rd->rdata=0;
				*last=rd;
				last=&(rd->next);
		}
		
		p+=rdlength;
		
	}
	return head;
error_boundary:
		LOG(L_ERR, "ERROR: get_record: end of query buff reached\n");
		return 0;
error_parse:
		LOG(L_ERR, "ERROR: get_record: rdata parse error \n");
		if (rd) local_free(rd); /* rd->rdata=0 & rd is not linked yet into
								   the list */
error:
		LOG(L_ERR, "ERROR: get_record \n");
		if (head) free_rdata_list(head);
not_found:
	return 0;
}



/* resolves a host name trying SRV lookup if *port==0 or normal A/AAAA lookup
 * if *port!=0.
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* sip_resolvehost(str* name, unsigned short* port, int proto)
{
	struct hostent* he;
	struct rdata* head;
	struct rdata* l;
	struct srv_rdata* srv;
	struct ip_addr* ip;
	static char tmp[MAX_DNS_NAME]; /* tmp. buff. for SRV lookups */

	/* try SRV if no port specified (draft-ietf-sip-srv-06) */
	if ((port)&&(*port==0)){
		*port=(proto==PROTO_TLS)?SIPS_PORT:SIP_PORT; /* just in case we don't
														find another */
		if ((name->len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME){
			LOG(L_WARN, "WARNING: sip_resolvehost: domain name too long (%d),"
						" unable to perform SRV lookup\n", name->len);
		}else{
			/* check if it's an ip address */
			if ( ((ip=str2ip(name))!=0)
#ifdef	USE_IPV6
				  || ((ip=str2ip6(name))!=0)
#endif
				){
				/* we are lucky, this is an ip address */
				return ip_addr2he(name,ip);
			}
			
			switch(proto){
				case PROTO_NONE: /* no proto specified, use udp */
					goto skip_srv;
				case PROTO_UDP:
					memcpy(tmp, SRV_UDP_PREFIX, SRV_UDP_PREFIX_LEN);
					memcpy(tmp+SRV_UDP_PREFIX_LEN, name->s, name->len);
					tmp[SRV_UDP_PREFIX_LEN + name->len] = '\0';
					break;
				case PROTO_TCP:
					memcpy(tmp, SRV_TCP_PREFIX, SRV_TCP_PREFIX_LEN);
					memcpy(tmp+SRV_TCP_PREFIX_LEN, name->s, name->len);
					tmp[SRV_TCP_PREFIX_LEN + name->len] = '\0';
					break;
				case PROTO_TLS:
					memcpy(tmp, SRV_TLS_PREFIX, SRV_TLS_PREFIX_LEN);
					memcpy(tmp+SRV_TLS_PREFIX_LEN, name->s, name->len);
					tmp[SRV_TLS_PREFIX_LEN + name->len] = '\0';
					break;
				default:
					LOG(L_CRIT, "BUG: sip_resolvehost: unknown proto %d\n",
							proto);
					return 0;
			}

			head=get_record(tmp, T_SRV);
			for(l=head; l; l=l->next){
				if (l->type!=T_SRV) continue; /*should never happen*/
				srv=(struct srv_rdata*) l->rdata;
				if (srv==0){
					LOG(L_CRIT, "sip_resolvehost: BUG: null rdata\n");
					free_rdata_list(head);
					break;
				}
				he=resolvehost(srv->name);
				if (he!=0){
					/* we found it*/
					DBG("sip_resolvehost: SRV(%s) = %s:%d\n",
							tmp, srv->name, srv->port);
					*port=srv->port;
					free_rdata_list(head); /*clean up*/
					return he;
				}
			}
			if (head) free_rdata_list(head); /*clean up*/
			DBG("sip_resolvehost: not SRV record found for %.*s," 
					" trying 'normal' lookup...\n", name->len, name->s);
		}
	}
skip_srv:
	if (name->len >= MAX_DNS_NAME) {
		LOG(L_ERR, "sip_resolvehost: domain name too long\n");
		return 0;
	}
	memcpy(tmp, name->s, name->len);
	tmp[name->len] = '\0';
	he=resolvehost(tmp);
	return he;
}

/*
 * $Id$
 *
 * resolver related functions
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
/* History:
 * --------
 *  2003-04-12  support for resolving ipv6 address references added (andrei)
 */



#ifndef __resolve_h
#define __resolve_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/nameser.h>

#include "ip_addr.h"


#define MAX_QUERY_SIZE 8192
#define ANS_SIZE       8192
#define DNS_HDR_SIZE     12
#define MAX_DNS_NAME 256
#define MAX_DNS_STRING 255



/* query union*/
union dns_query{
	HEADER hdr;
	unsigned char buff[MAX_QUERY_SIZE];
};


/* rdata struct*/
struct rdata {
	unsigned short type;
	unsigned short class;
	unsigned int   ttl;
	void* rdata;
	struct rdata* next;
};


/* srv rec. struct*/
struct srv_rdata {
	unsigned short priority;
	unsigned short weight;
	unsigned short port;
	unsigned int name_len;
	char name[MAX_DNS_NAME];
};

/* naptr rec. struct*/
struct naptr_rdata {
	unsigned short order;
	unsigned short pref;
	unsigned int flags_len;
	char flags[MAX_DNS_STRING];
	unsigned int services_len;
	char services[MAX_DNS_STRING];
	unsigned int regexp_len;
	char regexp[MAX_DNS_STRING];
	unsigned int repl_len; /* not currently used */
	char repl[MAX_DNS_NAME];
};


/* A rec. struct */
struct a_rdata {
	unsigned char ip[4];
};

struct aaaa_rdata {
	unsigned char ip6[16];
};

/* cname rec. struct*/
struct cname_rdata {
	char name[MAX_DNS_NAME];
};



struct rdata* get_record(char* name, int type);
void free_rdata_list(struct rdata* head);




#define rev_resolvehost(ip)\
					gethostbyaddr((char*)(ip)->u.addr, (ip)->len, (ip)->af);



#define HEX2I(c) \
	(	(((c)>='0') && ((c)<='9'))? (c)-'0' :  \
		(((c)>='A') && ((c)<='F'))? ((c)-'A')+10 : \
		(((c)>='a') && ((c)<='f'))? ((c)-'a')+10 : -1 )





/* converts a str to an ipv4 address, returns the address or 0 on error
   Warning: the result is a pointer to a statically allocated structure */
static inline struct ip_addr* str2ip(str* st)
{
	int i;
	unsigned char *limit;
	static struct ip_addr ip;
	unsigned char* s;

	s=(unsigned char*)st->s;

	/*init*/
	ip.u.addr32[0]=0;
	i=0;
	limit=(unsigned char*)(st->s + st->len);

	for(;s<limit ;s++){
		if (*s=='.'){
				i++;
				if (i>3) goto error_dots;
		}else if ( (*s <= '9' ) && (*s >= '0') ){
				ip.u.addr[i]=ip.u.addr[i]*10+*s-'0';
		}else{
				//error unknown char
				goto error_char;
		}
	}
	ip.af=AF_INET;
	ip.len=4;
	
	return &ip;
error_dots:
	DBG("str2ip: ERROR: too many dots in [%.*s]\n", st->len, st->s);
	return 0;
 error_char:
	/*
	DBG("str2ip: WARNING: unexpected char %c in [%.*s]\n", *s, st->len, st->s);
	*/
	return 0;
}



/* returns an ip_addr struct.; on error returns 0
 * the ip_addr struct is static, so subsequent calls will destroy its content*/
static inline struct ip_addr* str2ip6(str* st)
{
	int i, idx1, rest;
	int no_colons;
	int double_colon;
	int hex;
	static struct ip_addr ip;
	unsigned short* addr_start;
	unsigned short addr_end[8];
	unsigned short* addr;
	unsigned char* limit;
	unsigned char* s;
	
	/* init */
	if ((st->len) && (st->s[0]=='[')){
		/* skip over [ ] */
		if (st->s[st->len-1]!=']') goto error_char;
		s=(unsigned char*)(st->s+1);
		limit=(unsigned char*)(st->s+st->len-1);
	}else{
		s=(unsigned char*)st->s;
		limit=(unsigned char*)(st->s+st->len);
	}
	i=idx1=rest=0;
	double_colon=0;
	no_colons=0;
	ip.af=AF_INET6;
	ip.len=16;
	addr_start=ip.u.addr16;
	addr=addr_start;
	memset(addr_start, 0 , 8*sizeof(unsigned short));
	memset(addr_end, 0 , 8*sizeof(unsigned short));
	for (; s<limit; s++){
		if (*s==':'){
			no_colons++;
			if (no_colons>7) goto error_too_many_colons;
			if (double_colon){
				idx1=i;
				i=0;
				if (addr==addr_end) goto error_colons;
				addr=addr_end;
			}else{
				double_colon=1;
				addr[i]=htons(addr[i]);
				i++;
			}
		}else if ((hex=HEX2I(*s))>=0){
				addr[i]=addr[i]*16+hex;
				double_colon=0;
		}else{
			/* error, unknown char */
			goto error_char;
		}
	}
	if (no_colons<2) goto error_too_few_colons;
	if (!double_colon){ /* not ending in ':' */
		addr[i]=htons(addr[i]);
		i++; 
	}
	/* if address contained '::' fix it */
	if (addr==addr_end){
		rest=8-i-idx1;
		memcpy(addr_start+idx1+rest, addr_end, i*sizeof(unsigned short));
	}
/*
	DBG("str2ip6: idx1=%d, rest=%d, no_colons=%d, hex=%x\n",
			idx1, rest, no_colons, hex);
	DBG("str2ip6: address %x:%x:%x:%x:%x:%x:%x:%x\n", 
			addr_start[0], addr_start[1], addr_start[2],
			addr_start[3], addr_start[4], addr_start[5],
			addr_start[6], addr_start[7] );
*/
	return &ip;

error_too_many_colons:
	DBG("str2ip6: ERROR: too many colons in [%.*s]\n", st->len, st->s);
	return 0;

error_too_few_colons:
	DBG("str2ip6: ERROR: too few colons in [%.*s]\n", st->len, st->s);
	return 0;

error_colons:
	DBG("str2ip6: ERROR: too many double colons in [%.*s]\n", st->len, st->s);
	return 0;

error_char:
	/*
	DBG("str2ip6: WARNING: unexpected char %c in  [%.*s]\n", *s, st->len,
			st->s);*/
	return 0;
}



struct hostent* sip_resolvehost(str* name, unsigned short* port, int proto);



/* gethostbyname wrappers
 * use this, someday they will use a local cache */

static inline struct hostent* resolvehost(char* name)
{
	static struct hostent* he=0;
#ifdef HAVE_GETIPNODEBYNAME 
	int err;
	static struct hostent* he2=0;
#endif
#ifndef DNS_IP_HACK
	int len;
#endif
#ifdef DNS_IP_HACK
	struct ip_addr* ip;
	str s;

	s.s = (char*)name;
	s.len = strlen(name);

	/* check if it's an ip address */
	if ( ((ip=str2ip(&s))!=0)
#ifdef	USE_IPV6
		  || ((ip=str2ip6(&s))!=0)
#endif
		){
		/* we are lucky, this is an ip address */
		return ip_addr2he(&s, ip);
	}
	
#else /* DNS_IP_HACK */
	len=0;
	if (*name=='['){
		len=strlen(name);
		if (len && (name[len-1]==']')){
			name[len-1]=0; /* remove '[' */
			name++; /* skip '[' */
			goto skip_ipv4;
		}
	}
#endif
	/* ipv4 */
	he=gethostbyname(name);
#ifdef USE_IPV6
	if(he==0){
#ifndef DNS_IP_HACK
skip_ipv4:
#endif
		/*try ipv6*/
	#ifdef HAVE_GETHOSTBYNAME2
		he=gethostbyname2(name, AF_INET6);
	#elif defined HAVE_GETIPNODEBYNAME
		/* on solaris 8 getipnodebyname has a memory leak,
		 * after some time calls to it will fail with err=3
		 * solution: patch your solaris 8 installation */
		if (he2) freehostent(he2);
		he=he2=getipnodebyname(name, AF_INET6, 0, &err);
	#else
		#error neither gethostbyname2 or getipnodebyname present
	#endif
#ifndef DNS_IP_HACK
		if (len) name[len-2]=']'; /* restore */
#endif
	}
#endif
	return he;
}



#endif

/*
 * resolver related functions
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
 */

/*!
 * \file
 * \brief Kamailio core :: DNS resolver
 * \author andrei
 * \ingroup core
 * Module: \ref core
 */

#ifndef __resolve_h
#define __resolve_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "counters.h"
#include "dns_func.h"

#ifdef __OS_darwin
#include <arpa/nameser_compat.h>
#endif

#include "ip_addr.h"
#ifdef USE_DNS_CACHE
#include "dns_wrappers.h"
#endif


#define MAX_QUERY_SIZE 8192
#define ANS_SIZE       8192
#define DNS_HDR_SIZE     12
#define MAX_DNS_NAME 256
#define MAX_DNS_STRING 255

#ifndef T_EBL
/** not official yet - iEnum. */
#define T_EBL 65300
#endif

/* get_record flags */
#define RES_ONLY_TYPE 1   /* return only the specified type records */
#define RES_AR		  2   /* return also the additional records */

/* counter for failed/slow DNS requests
*/
struct dns_counters_h {
    counter_handle_t failed_dns_req;
    counter_handle_t slow_dns_req;
};

extern struct dns_counters_h dns_cnts_h;
extern struct dns_func_t dns_func;

/* query union*/
union dns_query{
	HEADER hdr;
	unsigned char buff[MAX_QUERY_SIZE];
};


/* rdata struct*/
struct rdata {
	unsigned short type;
	unsigned short pclass;
	unsigned int   ttl;
	void* rdata;
	struct rdata* next;
	unsigned char name_len; /* name length w/o the terminating 0 */
	char name[1]; /* null terminated name (len=name_len+1) */
};
/* real size of the structure */
#define RDATA_SIZE(s) (sizeof(struct rdata)+(s).name_len) /* +1-1 */


/* srv rec. struct*/
struct srv_rdata {
	unsigned short priority;
	unsigned short weight;
	unsigned short port;
	unsigned char name_len; /* name length w/o the terminating 0 */
	char name[1]; /* null terminated name (len=name_len+1) */
};


/* real size of the structure */
#define SRV_RDATA_SIZE(s) (sizeof(struct srv_rdata)+(s).name_len)

/* naptr rec. struct*/
struct naptr_rdata {
	char* flags;    /* points inside str_table */
	char* services; /* points inside str_table */
	char* regexp;   /* points inside str_table */
	char* repl;     /* points inside str_table, null terminated */
	
	unsigned short order;
	unsigned short pref;
	
	unsigned char flags_len;
	unsigned char services_len;
	unsigned char regexp_len;
	unsigned char repl_len; /* not currently used */
	unsigned char skip_record;
	
	char str_table[1]; /* contains all the strings */
};

/* real size of the structure */
#define NAPTR_RDATA_SIZE(s) (sizeof(struct naptr_rdata) \
								+ (s).flags_len \
								+ (s).services_len \
								+ (s).regexp_len \
								+ (s).repl_len + 1 - 1)


/* A rec. struct */
struct a_rdata {
	unsigned char ip[4];
};

struct aaaa_rdata {
	unsigned char ip6[16];
};

/* cname rec. struct*/
struct cname_rdata {
	unsigned char name_len; /* name length w/o the terminating 0 */
	char name[1]; /* null terminated name (len=name_len+1) */
};

/* real size of the structure */
#define CNAME_RDATA_SIZE(s) (sizeof(struct cname_rdata)+(s).name_len)

/* dns character-string */
struct dns_cstr{
	char* cstr; /* pointer to null term. string */
	unsigned char cstr_len;
};

/* txt rec. struct */
struct txt_rdata {
	unsigned short cstr_no; /* number of strings */
	unsigned short tslen; /* total strings table len */
	struct dns_cstr txt[1]; /* at least 1 */
	/* all txt[*].cstr point inside a string table at the end of the struct.*/
};

#define TXT_RDATA_SIZE(s) \
	(sizeof(struct txt_rdata)+((s).cstr_no-1)*sizeof(struct dns_cstr)+\
	 	(s).tslen)

/* ebl rec. struct, see
   http://tools.ietf.org/html/draft-ietf-enum-branch-location-record-03 */
struct ebl_rdata {
	char* separator; /* points inside str_table */
	char* apex;      /* point inside str_table */
	unsigned char separator_len; /* separator len w/o the terminating 0 */
	unsigned char apex_len;      /* apex len w/p the terminating 0 */
	unsigned char position;
	char str_table[1]; /* contains the 2 strings: separator and apex */
};
#define EBL_RDATA_SIZE(s) \
	(sizeof(struct ebl_rdata)-1+(s).separator_len+1+(s).apex_len+1)


struct ptr_rdata {
	unsigned char ptrdname_len; /* name length w/o the terminating 0 */
	char ptrdname[1]; /* null terminated name (len=name_len+1) */
};
/* real size of the structure */
#define PTR_RDATA_SIZE(s) (sizeof(struct ptr_rdata)-1+(s).ptrdname_len+1)


#ifdef HAVE_RESOLV_RES
int match_search_list(const struct __res_state* res, char* name);
#endif
struct rdata* get_record(char* name, int type, int flags);
void free_rdata_list(struct rdata* head);



#define rev_resolvehost(ip)\
					gethostbyaddr((char*)(ip)->u.addr, (ip)->len, (ip)->af)



#define HEX2I(c) \
	(	(((c)>='0') && ((c)<='9'))? (c)-'0' :  \
		(((c)>='A') && ((c)<='F'))? ((c)-'A')+10 : \
		(((c)>='a') && ((c)<='f'))? ((c)-'a')+10 : -1 )


int str2ipbuf(str* st, ip_addr_t* ipb);
int str2ip6buf(str* st, ip_addr_t* ipb);
int str2ipxbuf(str* st, ip_addr_t* ipb);
ip_addr_t* str2ip(str* st);
ip_addr_t* str2ip6(str* st);
ip_addr_t* str2ipx(str* st);

struct hostent* _sip_resolvehost(str* name, unsigned short* port, char* proto);



/* gethostbyname wrapper, handles ip/ipv6 automatically */
static inline struct hostent* _resolvehost(char* name)
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
		  || ((ip=str2ip6(&s))!=0)
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
	he=dns_func.sr_gethostbyname(name);

	if(he==0 && cfg_get(core, core_cfg, dns_try_ipv6)){
#ifndef DNS_IP_HACK
skip_ipv4:
#endif
		/*try ipv6*/
	#ifdef HAVE_GETHOSTBYNAME2
		he=dns_func.sr_gethostbyname2(name, AF_INET6);
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
	return he;
}


int resolv_init(void);

/* callback/fixup functions executed by the configuration framework */
void resolv_reinit(str *gname, str *name);
int dns_reinit_fixup(void *handle, str *gname, str *name, void **val);
int dns_try_ipv6_fixup(void *handle, str *gname, str *name, void **val);
void reinit_proto_prefs(str *gname, str *name);

struct dns_srv_proto {
	char proto;
	int proto_pref;
};
void create_srv_name(char proto, str *name, char *srv);
size_t create_srv_pref_list(char *proto, struct dns_srv_proto *list);

#ifdef DNS_WATCHDOG_SUPPORT
/* callback function that is called by the child processes
 * when they reinitialize the resolver
 *
 * Note, that this callback is called by each chiled process separately!!!
 * If the callback is registered after forking, only the child process
 * that installs the hook will call the callback.
 */
typedef void (*on_resolv_reinit)(str*);
int register_resolv_reinit_cb(on_resolv_reinit cb);
#endif


int sip_hostport2su(union sockaddr_union* su, str* host, unsigned short port,
						char* proto);



/* wrappers */
#ifdef USE_DNS_CACHE
#define resolvehost dns_resolvehost
#define sip_resolvehost dns_sip_resolvehost
#else
#define resolvehost _resolvehost
#define sip_resolvehost _sip_resolvehost
#endif



#ifdef USE_NAPTR
/* NAPTR helper functions */
typedef unsigned int naptr_bmp_t; /* type used for keeping track of tried
									 naptr records*/
#define MAX_NAPTR_RRS (sizeof(naptr_bmp_t)*8)

/* use before first call to naptr_sip_iterate */
#define naptr_iterate_init(bmp) \
	do{ \
		*(bmp)=0; \
	}while(0) \

struct rdata* naptr_sip_iterate(struct rdata* naptr_head, 
										naptr_bmp_t* tried,
										str* srv_name, char* proto);
/* returns sip proto if valis sip naptr record, .-1 otherwise */
char naptr_get_sip_proto(struct naptr_rdata* n);
/* returns true if new_proto is preferred over old_proto */
int naptr_proto_preferred(char new_proto, char old_proto);
/* returns true if we support the protocol */
int naptr_proto_supported(char proto);
/* choose between 2 naptr records, should take into account local
 * preferences too
 * returns 1 if the new record was selected, 0 otherwise */
int naptr_choose (struct naptr_rdata** crt, char* crt_proto,
									struct naptr_rdata* n , char n_proto);

#endif/* USE_NAPTR */

struct hostent* no_naptr_srv_sip_resolvehost(str* name, unsigned short* port,
		char* proto);

#endif

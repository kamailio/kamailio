/*
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
 * \ingroup core
 * Module: \ref core
 */


#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>

#include "resolve.h"
#include "compiler_opt.h"
#include "dprint.h"
#include "mem/mem.h"
#include "ip_addr.h"
#include "error.h"
#include "globals.h" /* tcp_disable, tls_disable a.s.o */
#include "cfg_core.h"
#include "socket_info.h"

#ifdef USE_DNS_CACHE
#include "dns_cache.h"
#endif

/* counters framework */
struct dns_counters_h dns_cnts_h;
counter_def_t dns_cnt_defs[] =  {
	{&dns_cnts_h.failed_dns_req, "failed_dns_request", 0, 0, 0,
		"incremented each time a DNS request has failed."},
	{0, 0, 0, 0, 0, 0 }
};

/* mallocs for local stuff */
#define local_malloc pkg_malloc
#define local_free   pkg_free

#ifdef USE_NAPTR
static int naptr_proto_pref[PROTO_LAST+1];
#endif
static int srv_proto_pref[PROTO_LAST+1];

#ifdef USE_NAPTR
static void init_naptr_proto_prefs()
{
	int ignore_rfc, udp, tcp, tls, sctp;

	if ((PROTO_UDP > PROTO_LAST) || (PROTO_TCP > PROTO_LAST) ||
		(PROTO_TLS > PROTO_LAST) || (PROTO_SCTP > PROTO_LAST)){
		BUG("init_naptr_proto_prefs: array too small \n");
		return;
	}

	ignore_rfc = cfg_get(core, core_cfg, dns_naptr_ignore_rfc);
	udp = cfg_get(core, core_cfg, dns_udp_pref);
	tcp = cfg_get(core, core_cfg, dns_tcp_pref);
	tls = cfg_get(core, core_cfg, dns_tls_pref);
	sctp = cfg_get(core, core_cfg, dns_sctp_pref);

	/* Old implementation ignored the Order field in the NAPTR RR and
	 * thus violated a MUST in RFC 2915. Currently still the default. */
	if (ignore_rfc) {
		naptr_proto_pref[PROTO_UDP] = udp;
		naptr_proto_pref[PROTO_TCP] = tcp;
		naptr_proto_pref[PROTO_TLS] = tls;
		naptr_proto_pref[PROTO_SCTP] = sctp;
	} else {
		/* If value is less than 0, proto is disabled, otherwise
		 * ignored. */
		naptr_proto_pref[PROTO_UDP] = udp < 0 ? udp : 1;
		naptr_proto_pref[PROTO_TCP] = tcp < 0 ? tcp : 1;
		naptr_proto_pref[PROTO_TLS] = tls < 0 ? tls : 1;
		naptr_proto_pref[PROTO_SCTP] = sctp < 0 ? sctp : 1;
	}
}

#endif /* USE_NAPTR */

static void init_srv_proto_prefs()
{
	if ((PROTO_UDP > PROTO_LAST) || (PROTO_TCP > PROTO_LAST) ||
		(PROTO_TLS > PROTO_LAST) || (PROTO_SCTP > PROTO_LAST)){
		BUG("init_srv_proto_prefs: array too small \n");
		return;
	}

	srv_proto_pref[PROTO_UDP] = cfg_get(core, core_cfg, dns_udp_pref);
	srv_proto_pref[PROTO_TCP] = cfg_get(core, core_cfg, dns_tcp_pref);
	srv_proto_pref[PROTO_TLS] = cfg_get(core, core_cfg, dns_tls_pref);
	srv_proto_pref[PROTO_SCTP] = cfg_get(core, core_cfg, dns_sctp_pref);
}

#ifdef DNS_WATCHDOG_SUPPORT
static on_resolv_reinit	on_resolv_reinit_cb = NULL;

/* register the callback function */
int register_resolv_reinit_cb(on_resolv_reinit cb)
{
	if (on_resolv_reinit_cb) {
		LM_ERR("callback function has been already registered\n");
		return -1;
	}
	on_resolv_reinit_cb = cb;
	return 0;
}
#endif

/* counter init function
  must be called before fork
*/
static int stat_init(void)
{
	if (counter_register_array("dns", dns_cnt_defs) < 0)
		goto error;
	return 0;
error:
	return -1;
}

/** init. the resolver
 * params: retr_time  - time before retransmitting (must be >0)
 *         retr_no    - retransmissions number
 *         servers_no - how many dns servers will be used
 *                      (from the one listed in /etc/resolv.conf)
 *         search     - if 0 the search list in /etc/resolv.conf will
 *                      be ignored (HINT: even if you don't have a
 *                      search list in resolv.conf, it's still better
 *                      to set search to 0, because an empty seachlist
 *                      means in fact search "" => it takes more time)
 * If any of the parameters <0, the default (system specific) value
 * will be used. See also resolv.conf(5).
 * returns: 0 on success, -1 on error
 */
static int _resolv_init(void)
{
	dns_func.sr_res_init();
#ifdef HAVE_RESOLV_RES
	if (cfg_get(core, core_cfg, dns_retr_time)>0)
		_res.retrans=cfg_get(core, core_cfg, dns_retr_time);
	if (cfg_get(core, core_cfg, dns_retr_no)>0)
		_res.retry=cfg_get(core, core_cfg, dns_retr_no);
	if ((cfg_get(core, core_cfg, dns_servers_no)>=0)
		&& (cfg_get(core, core_cfg, dns_servers_no)<_res.nscount))
			_res.nscount=cfg_get(core, core_cfg, dns_servers_no);
	if (cfg_get(core, core_cfg, dns_search_list)==0)
		_res.options&=~(RES_DEFNAMES|RES_DNSRCH);
#else
#warning "no resolv timeout support"
	LM_WARN("no resolv options support - resolv options will be ignored\n");
#endif
	return 0;
}

/** wrapper function to initialize the resolver at startup */
int resolv_init(void)
{
	int res = -1;
	_resolv_init();

	reinit_proto_prefs(NULL,NULL);
	/* init counter API only at startup
	 * This function must be called before DNS cache init method (if available)
	 */
	res = stat_init();
	return res;
}

/** wrapper function to reinitialize the resolver
 * This function must be called by each child process whenever
 * a resolver option changes
 */
void resolv_reinit(str *gname, str *name)
{
	_resolv_init();

#ifdef DNS_WATCHDOG_SUPPORT
	if (on_resolv_reinit_cb) on_resolv_reinit_cb(name);
#endif
	LM_DBG("DNS resolver has been reinitialized\n");
}

/** fixup function for dns_reinit variable
 * (resets the variable to 0)
 */
int dns_reinit_fixup(void *handle, str *gname, str *name, void **val)
{
	*val = (void *)(long)0;
	return 0;
}

/** wrapper function to recalculate the naptr and srv protocol preferences */
void reinit_proto_prefs(str *gname, str *name)
{
#ifdef USE_NAPTR
	init_naptr_proto_prefs();
#endif
	init_srv_proto_prefs();
}

/** fixup function for dns_try_ipv6
 * verifies that Kamailio really listens on an ipv6 interface
 */
int dns_try_ipv6_fixup(void *handle, str *gname, str *name, void **val)
{
	if ((int)(long)(*val) && !(socket_types & SOCKET_T_IPV6)) {
		LM_ERR("SER does not listen on any ipv6 interface, "
			"there is no point in resolving ipv6 addresses\n");
		return -1;
	}
	return 0;
}

/**  skips over a domain name in a dns message
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
	return (p>end)?0:p;
}



/** parses the srv record into a srv_rdata structure
 *   msg   - pointer to the dns message
 *   end   - pointer to the end of the message
 *   eor   - pointer to the end of the record/rdata
 *   rdata - pointer  to the rdata part of the srv answer
 * returns 0 on error, or a dyn. alloc'ed srv_rdata structure 
 *
 * SRV rdata format:
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
								  unsigned char* eor,
								  unsigned char* rdata)
{
	struct srv_rdata* srv;
	unsigned short priority;
	unsigned short weight;
	unsigned short port;
	int len;
	char name[MAX_DNS_NAME];
	
	srv=0;
	if ((rdata+6+1)>eor) goto error;
	
	memcpy((void*)&priority, rdata, 2);
	memcpy((void*)&weight,   rdata+2, 2);
	memcpy((void*)&port,     rdata+4, 2);
	rdata+=6;
	if (dn_expand(msg, end, rdata, name, MAX_DNS_NAME-1)<0)
		goto error;
	len=strlen(name);
	if (len>255)
		goto error;
	/* alloc enought space for the struct + null terminated name */
	srv=local_malloc(sizeof(struct srv_rdata)-1+len+1);
	if (srv==0){
		LM_ERR("out of memory\n");
		goto error;
	}
	srv->priority=ntohs(priority);
	srv->weight=ntohs(weight);
	srv->port=ntohs(port);
	srv->name_len=len;
	memcpy(srv->name, name, srv->name_len);
	srv->name[srv->name_len]=0;
	
	return srv;
error:
	if (srv) local_free(srv);
	return 0;
}


/** parses the naptr record into a naptr_rdata structure
 *   msg   - pointer to the dns message
 *   end   - pointer to the end of the message
 *   eor   - pointer to the end of the record/rdata
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
										unsigned char* eor,
										unsigned char* rdata)
{
	struct naptr_rdata* naptr;
	unsigned char* flags;
	unsigned char* services;
	unsigned char* regexp;
	unsigned short order;
	unsigned short pref;
	unsigned char flags_len;
	unsigned char services_len;
	unsigned char regexp_len;
	int len;
	char repl[MAX_DNS_NAME];
	
	naptr = 0;
	if ((rdata + 7 + 1)>eor) goto error;
	
	memcpy((void*)&order, rdata, 2);
	memcpy((void*)&pref, rdata + 2, 2);
	flags_len = rdata[4];
	if ((rdata + 7 + 1 +  flags_len) > eor)
		goto error;
	flags=rdata+5;
	services_len = rdata[5 + flags_len];
	if ((rdata + 7 + 1 + flags_len + services_len) > eor)
		goto error;
	services=rdata + 6 + flags_len;
	regexp_len = rdata[6 + flags_len + services_len];
	if ((rdata + 7 +1 + flags_len + services_len + regexp_len) > eor)
		goto error;
	regexp=rdata + 7 + flags_len + services_len;
	rdata = rdata + 7 + flags_len + services_len + regexp_len;
	if (dn_expand(msg, end, rdata, repl, MAX_DNS_NAME-1) == -1)
		goto error;
	len=strlen(repl);
	if (len>255)
		goto error;
	naptr=local_malloc(sizeof(struct naptr_rdata)+flags_len+services_len+
						regexp_len+len+1-1);
	if (naptr == 0){
		LM_ERR("out of memory\n");
		goto error;
	}
	naptr->order=ntohs(order);
	naptr->pref=ntohs(pref);
	
	naptr->flags=&naptr->str_table[0];
	naptr->flags_len=flags_len;
	memcpy(naptr->flags, flags, naptr->flags_len);
	naptr->services=&naptr->str_table[flags_len];
	naptr->services_len=services_len;
	memcpy(naptr->services, services, naptr->services_len);
	naptr->regexp=&naptr->str_table[flags_len+services_len];
	naptr->regexp_len=regexp_len;
	memcpy(naptr->regexp, regexp, naptr->regexp_len);
	naptr->repl=&naptr->str_table[flags_len+services_len+regexp_len];
	naptr->repl_len=len;
	memcpy(naptr->repl, repl, len);
	naptr->repl[len]=0; /* null term. */
	
	return naptr;
error:
	if (naptr) local_free(naptr);
	return 0;
}



/** parses a CNAME record into a cname_rdata structure */
struct cname_rdata* dns_cname_parser( unsigned char* msg, unsigned char* end,
									  unsigned char* rdata)
{
	struct cname_rdata* cname;
	int len;
	char name[MAX_DNS_NAME];
	
	cname=0;
	if (dn_expand(msg, end, rdata, name, MAX_DNS_NAME-1)==-1)
		goto error;
	len=strlen(name);
	if (len>255)
		goto error;
	/* alloc sizeof struct + space for the null terminated name */
	cname=local_malloc(sizeof(struct cname_rdata)-1+len+1);
	if(cname==0){
		LM_ERR("out of memory\n");
		goto error;
	}
	cname->name_len=len;
	memcpy(cname->name, name, cname->name_len);
	cname->name[cname->name_len]=0;
	return cname;
error:
	if (cname) local_free(cname);
	return 0;
}



/** parses an A record rdata into an a_rdata structure
 * returns 0 on error or a dyn. alloc'ed a_rdata struct
 */
struct a_rdata* dns_a_parser(unsigned char* rdata, unsigned char* eor)
{
	struct a_rdata* a;
	
	if (rdata+4>eor) goto error;
	a=(struct a_rdata*)local_malloc(sizeof(struct a_rdata));
	if (a==0){
		LM_ERR("out of memory\n");
		goto error;
	}
	memcpy(a->ip, rdata, 4);
	return a;
error:
	return 0;
}



/** parses an AAAA (ipv6) record rdata into an aaaa_rdata structure
 * returns 0 on error or a dyn. alloc'ed aaaa_rdata struct */
struct aaaa_rdata* dns_aaaa_parser(unsigned char* rdata, unsigned char* eor)
{
	struct aaaa_rdata* aaaa;
	
	if (rdata+16>eor) goto error;
	aaaa=(struct aaaa_rdata*)local_malloc(sizeof(struct aaaa_rdata));
	if (aaaa==0){
		LM_ERR("out of memory\n");
		goto error;
	}
	memcpy(aaaa->ip6, rdata, 16);
	return aaaa;
error:
	return 0;
}



/** parses a TXT record into a txt_rdata structure.
 *   @param msg   - pointer to the dns message
 *   @param end   - pointer to the end of the record (rdata end)
 *   @param rdata - pointer  to the rdata part of the txt answer
 * returns 0 on error, or a dyn. alloc'ed txt_rdata structure */
/*  TXT rdata format:
 *
 * one or several character strings:
 *  01234567
 * +--------------------+
 * | len    | string   / ...
 * |------------------+
 */
static struct txt_rdata* dns_txt_parser(unsigned char* msg, unsigned char* end,
										unsigned char* rdata)
{
	struct txt_rdata* txt;
	int len, n, i;
	int str_size;
	unsigned char* p;
	unsigned char* st;
	
	txt=0;
	if (unlikely((rdata+1)>end)) goto error;
	n=0;
	str_size=0;
	/* count the number of strings */
	p=rdata;
	do{
		len=*p;
		p+=len+1;
		str_size+=len+1; /* 1 for the term. 0 */
		if (unlikely(p>end)) goto error;
		n++;
	}while(p<end);
	/* alloc sizeof struct + space for the dns_cstr array + space for
	   the strings */
	txt=local_malloc(sizeof(struct txt_rdata) +(n-1)*sizeof(struct dns_cstr)+
						str_size);
	if(unlikely(txt==0)){
		LM_ERR("out of memory\n");
		goto error;
	}
	/* string table */
	st=(unsigned char*)txt+sizeof(struct txt_rdata) +
		(n-1)*sizeof(struct dns_cstr);
	txt->cstr_no=n;
	txt->tslen=str_size;
	/* fill the structure */
	p=rdata;
	for (i=0; i<n; i++){
		len=*p;
		memcpy(st, p+1, len);
		st[len]=0;
		txt->txt[i].cstr_len=len;
		txt->txt[i].cstr=(char*)st;
		st+=len+1;
		p+=len+1;
	}
	return txt;
error:
	if (txt) local_free(txt);
	return 0;
}



/** parses an EBL record into a txt_rdata structure.
 *   @param msg   - pointer to the dns message
 *   @param end   - pointer to the end of the dns message
 *   @param eor   - pointer to the end of the record (rdata end)
 *   @param rdata - pointer  to the rdata part of the txt answer
 * returns 0 on error, or a dyn. alloc'ed txt_rdata structure */
/*  EBL rdata format:
 *  (see http://tools.ietf.org/html/draft-ietf-enum-branch-location-record-03)
 * one or several character strings:
 *  01234567
 * +--------+
 * | postion|
 * +-----------+
 * / separator /
 * +-----------+
 * /   apex    /
 * +----------+
 *
 * where separator is a character string ( 8 bit len, followed by len chars)
 * and apex is a domain-name.
 */
static struct ebl_rdata* dns_ebl_parser(unsigned char* msg, unsigned char* end,
										unsigned char* eor,
										unsigned char* rdata)
{
	struct ebl_rdata* ebl;
	int sep_len;
	int apex_len;
	char apex[MAX_DNS_NAME];
	
	ebl=0;
	/* check if len is at least 4 chars (minimum possible):
	     pos (1 byte) +  sep. (min 1 byte) + apex (min. 2 bytes) 
	   and also check if rdata+1 (pos) + 1 (sep. len) + sep_len + 1 is ok*/
	if (unlikely(((rdata+4)>eor)||((rdata+1+1+rdata[1]+2)>eor))) goto error;
	sep_len=rdata[1];
	if (unlikely(dn_expand(msg, end, rdata+1+1+sep_len,
							apex, MAX_DNS_NAME-1)==-1))
		goto error;
	apex_len=strlen(apex);
	/* alloc sizeof struct + space for the 2 null-terminated strings */
	ebl=local_malloc(sizeof(struct ebl_rdata)-1+sep_len+1+apex_len+1);
	if (ebl==0){
		LM_ERR("out of memory\n");
		goto error;
	}
	ebl->position=rdata[0];
	ebl->separator=&ebl->str_table[0];
	ebl->apex=ebl->separator+sep_len+1;
	ebl->separator_len=sep_len;
	ebl->apex_len=apex_len;
	memcpy(ebl->separator, rdata+2, sep_len);
	ebl->separator[sep_len]=0;
	memcpy(ebl->apex, apex, apex_len);
	ebl->apex[apex_len]=0;
	
	return ebl;
error:
	if (ebl) local_free(ebl);
	return 0;
}



/** parses a PTR record into a ptr_rdata structure */
struct ptr_rdata* dns_ptr_parser( unsigned char* msg, unsigned char* end,
									  unsigned char* rdata)
{
	struct ptr_rdata* pname;
	int len;
	char name[MAX_DNS_NAME];
	
	pname=0;
	if (dn_expand(msg, end, rdata, name, MAX_DNS_NAME-1)==-1)
		goto error;
	len=strlen(name);
	if (len>255)
		goto error;
	/* alloc sizeof struct + space for the null terminated name */
	pname=local_malloc(sizeof(struct ptr_rdata)-1+len+1);
	if(pname==0){
		LM_ERR("out of memory\n");
		goto error;
	}
	pname->ptrdname_len=len;
	memcpy(pname->ptrdname, name, pname->ptrdname_len);
	pname->ptrdname[pname->ptrdname_len]=0;
	return pname;
error:
	if (pname) local_free(pname);
	return 0;
}



/** frees completely a struct rdata list */
void free_rdata_list(struct rdata* head)
{
	struct rdata* l;
	struct rdata* next_l;
	l=head;
	while (l != 0) {
		next_l = l->next;
		/* free the parsed rdata*/
		if (l->rdata) local_free(l->rdata);
		local_free(l);
		l = next_l;
	}
}

#ifdef HAVE_RESOLV_RES
/** checks whether supplied name exists in the resolver search list
 * returns 1 if found
 *         0 if not found
 */
int match_search_list(const struct __res_state* res, char* name) {
	int i;
	for (i=0; (i<MAXDNSRCH) && (res->dnsrch[i]); i++) {
		if (strcasecmp(name, res->dnsrch[i])==0) 
			return 1;
	}
	return 0;
}
#endif

/** gets the DNS records for name:type
 * returns a dyn. alloc'ed struct rdata linked list with the parsed responses
 * or 0 on error
 * see rfc1035 for the query/response format */
struct rdata* get_record(char* name, int type, int flags)
{
	int size;
	int skip;
	int qno, answers_no;
	int i, r;
	static union dns_query buff;
	unsigned char* p;
	unsigned char* end;
	unsigned char* rd_end;
	static char rec_name[MAX_DNS_NAME]; /* placeholder for the record name */
	int rec_name_len;
	unsigned short rtype, class, rdlength;
	unsigned int ttl;
	struct rdata* head;
	struct rdata** crt;
	struct rdata** last;
	struct rdata* rd;
	struct srv_rdata* srv_rd;
	struct srv_rdata* crt_srv;
	int search_list_used;
	int name_len;
	struct rdata* fullname_rd;
	char c;
	
	name_len=strlen(name);

	for (i = 0; i < name_len; i++) {
	    c = name[i];
	    if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) ||
		((c >= '0') && (c <= '9')) || (name[i] == '.') ||
		(name[i] == '-') || (name[i] == '_'))
			continue;
	    LM_DBG("'%s' is not domain name\n", name);
	    return 0;
	}

	if (cfg_get(core, core_cfg, dns_search_list)==0) {
		search_list_used=0;
		name_len=0;
	} else {
		search_list_used=1;
	}
	fullname_rd=0;

	size=dns_func.sr_res_search(name, C_IN, type, buff.buff, sizeof(buff));

	if (unlikely(size<0)) {
		LM_DBG("lookup(%s, %d) failed\n", name, type);
		goto not_found;
	}
	else if (unlikely(size > sizeof(buff))) size=sizeof(buff);
	head=rd=0;
	last=crt=&head;
	
	p=buff.buff+DNS_HDR_SIZE;
	end=buff.buff+size;
	if (unlikely(p>=end)) goto error_boundary;
	qno=ntohs((unsigned short)buff.hdr.qdcount);

	for (r=0; r<qno; r++){
		/* skip the name of the question */
		if (unlikely((p=dns_skipname(p, end))==0)) {
			LM_ERR("skipname==0\n");
			goto error;
		}
		p+=2+2; /* skip QCODE & QCLASS */
	#if 0
		for (;(p<end && (*p)); p++);
		p+=1+2+2; /* skip the ending  '\0, QCODE and QCLASS */
	#endif
		if (unlikely(p>end)) {
			LM_ERR("p>=end\n");
			goto error;
		}
	};
	answers_no=ntohs((unsigned short)buff.hdr.ancount);
again:
	for (r=0; (r<answers_no) && (p<end); r++){
#if 0
		/*  ignore it the default domain name */
		if ((p=dns_skipname(p, end))==0) {
			LM_ERR("get_record: skip_name=0 (#2)\n");
			goto error;
		}
#else
		if (unlikely((skip=dn_expand(buff.buff, end, p, rec_name,
							MAX_DNS_NAME-1))==-1)){
			LM_ERR("dn_expand(rec_name) failed\n");
			goto error;
		}
#endif
		p+=skip;
		rec_name_len=strlen(rec_name);
		if (unlikely(rec_name_len>255)){
			LM_ERR("dn_expand(rec_name): name too long (%d)\n",
					rec_name_len);
			goto error;
		}
		/* check if enough space is left for type, class, ttl & size */
		if (unlikely((p+2+2+4+2)>end)) goto error_boundary;
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
		rd_end=p+rdlength;
		if (unlikely((rd_end)>end)) goto error_boundary;
		if ((flags & RES_ONLY_TYPE) && (rtype!=type)){
			/* skip */
			p=rd_end;
			continue;
		}
		/* expand the "type" record  (rdata)*/
		
		rd=(struct rdata*) local_malloc(sizeof(struct rdata)+rec_name_len+
										1-1);
		if (rd==0){
			LM_ERR("out of memory\n");
			goto error;
		}
		rd->type=rtype;
		rd->pclass=class;
		rd->ttl=ttl;
		rd->next=0;
		memcpy(rd->name, rec_name, rec_name_len);
		rd->name[rec_name_len]=0;
		rd->name_len=rec_name_len;
		/* check if full name matches */
		if ((search_list_used==1)&&(fullname_rd==0)&&
				(rec_name_len>=name_len)&&
				(strncasecmp(rec_name, name, name_len)==0)) {
			/* now we have record whose name is the same (up-to the
			 * name_len with the searched one):
			 * if the length is the same - we found full match, no fake
			 *  cname needed, just clear the flag
			 * if the length of the name differs - it has matched using
			 *  search list remember the rd, so we can create fake CNAME
			 *  record when all answers are used and no better match found
			 */
			if (rec_name_len==name_len)
				search_list_used=0;
			/* this is safe.... here was rec_name_len > name_len */
			else if (rec_name[name_len]=='.') {
#ifdef HAVE_RESOLV_RES
				if ((cfg_get(core, core_cfg, dns_search_fmatch)==0) ||
						(match_search_list(&_res, rec_name+name_len+1)!=0))
#endif
					fullname_rd=rd;
			}
		}
		switch(rtype){
			case T_SRV:
				srv_rd= dns_srv_parser(buff.buff, end, rd_end, p);
				rd->rdata=(void*)srv_rd;
				if (unlikely(srv_rd==0)) goto error_parse;
				
				/* insert sorted into the list */
				for (crt=&head; *crt; crt= &((*crt)->next)){
					if ((*crt)->type!=T_SRV)
						continue;
					crt_srv=(struct srv_rdata*)(*crt)->rdata;
					if ((srv_rd->priority <  crt_srv->priority) ||
					   ( (srv_rd->priority == crt_srv->priority) && 
							 (srv_rd->weight > crt_srv->weight) ) ){
						/* insert here */
						goto skip;
					}
				}
				last=&(rd->next); /*end of for => this will be the last
									element*/
			skip:
				/* insert here */
				rd->next=*crt;
				*crt=rd;
				break;
			case T_A:
				rd->rdata=(void*) dns_a_parser(p, rd_end);
				if (unlikely(rd->rdata==0)) goto error_parse;
				*last=rd; /* last points to the last "next" or the list
							 	head*/
				last=&(rd->next);
				break;
			case T_AAAA:
				rd->rdata=(void*) dns_aaaa_parser(p, rd_end);
				if (unlikely(rd->rdata==0)) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_CNAME:
				rd->rdata=(void*) dns_cname_parser(buff.buff, end, p);
				if(unlikely(rd->rdata==0)) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_NAPTR:
				rd->rdata=(void*)dns_naptr_parser(buff.buff, end, rd_end, p);
				if(unlikely(rd->rdata==0)) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_TXT:
				rd->rdata= dns_txt_parser(buff.buff, rd_end, p);
				if (rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_EBL:
				rd->rdata= dns_ebl_parser(buff.buff, end, rd_end, p);
				if (rd->rdata==0) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			case T_PTR:
				rd->rdata=(void*) dns_ptr_parser(buff.buff, end, p);
				if(unlikely(rd->rdata==0)) goto error_parse;
				*last=rd;
				last=&(rd->next);
				break;
			default:
				LM_ERR("unknown type %d\n", rtype);
				rd->rdata=0;
				*last=rd;
				last=&(rd->next);
		}
		
		p+=rdlength;
		
	}
	if (flags & RES_AR){
		flags&=~RES_AR;
		answers_no=ntohs((unsigned short)buff.hdr.nscount);
#ifdef RESOLVE_DBG
		LM_DBG("skipping %d NS (p=%p, end=%p)\n", answers_no, p, end);
#endif
		for (r=0; (r<answers_no) && (p<end); r++){
			/* skip over the ns records */
			if ((p=dns_skipname(p, end))==0) {
				LM_ERR("skip_name=0 (#3)\n");
				goto error;
			}
			/* check if enough space is left for type, class, ttl & size */
			if (unlikely((p+2+2+4+2)>end)) goto error_boundary;
			memcpy((void*)&rdlength, (void*)p+2+2+4, 2);
			p+=2+2+4+2+ntohs(rdlength);
		}
		answers_no=ntohs((unsigned short)buff.hdr.arcount);
#ifdef RESOLVE_DBG
		LM_DBG("parsing %d ARs (p=%p, end=%p)\n", answers_no, p, end);
#endif
		goto again; /* add also the additional records */
	}

	/* if the name was expanded using DNS search list
	 * create fake CNAME record to convert the short name
	 * (queried) to long name (answered)
	 */
	if ((search_list_used==1)&&(fullname_rd!=0)) {
		rd=(struct rdata*) local_malloc(sizeof(struct rdata)+name_len+1-1);
		if (unlikely(rd==0)){
			LM_ERR("out of memory\n");
			goto error;
		}
		rd->type=T_CNAME;
		rd->pclass=fullname_rd->pclass;
		rd->ttl=fullname_rd->ttl;
		rd->next=head;
		memcpy(rd->name, name, name_len);
		rd->name[name_len]=0;
		rd->name_len=name_len;
		/* alloc sizeof struct + space for the null terminated name */
		rd->rdata=(void*)local_malloc(sizeof(struct cname_rdata)-1+
										head->name_len+1);
		if(unlikely(rd->rdata==0)){
			LM_ERR("out of memory\n");
			goto error_rd;
		}
		((struct cname_rdata*)(rd->rdata))->name_len=fullname_rd->name_len;
		memcpy(((struct cname_rdata*)(rd->rdata))->name, fullname_rd->name,
				fullname_rd->name_len);
		((struct cname_rdata*)(rd->rdata))->name[head->name_len]=0;
		head=rd;
	}

	return head;
error_boundary:
		LM_ERR("end of query buff reached\n");
		if (head) free_rdata_list(head);
		return 0;
error_parse:
		LM_ERR("rdata parse error (%s, %d), %p-%p"
				" rtype=%d, class=%d, ttl=%d, rdlength=%d\n",
				name, type,
				p, end, rtype, class, ttl, rdlength);
error_rd:
		if (rd) local_free(rd); /* rd->rdata=0 & rd is not linked yet into
								   the list */
error:
		LM_ERR("get_record\n");
		if (head) free_rdata_list(head);
not_found:
	/* increment error counter */
	counter_inc(dns_cnts_h.failed_dns_req);
	return 0;
}

#ifdef USE_NAPTR

/* service matching constants, lowercase */
#define SIP_SCH		0x2b706973
#define SIPS_SCH	0x73706973
#define SIP_D2U		0x00753264
#define SIP_D2T		0x00743264
#define SIP_D2S		0x00733264
#define SIPS_D2T	0x7432642b


/** get protocol from a naptr rdata and check for validity
 * returns > 0 (PROTO_UDP, PROTO_TCP, PROTO_SCTP or PROTO_TLS)
 *         <=0  on error 
 */
char naptr_get_sip_proto(struct naptr_rdata* n)
{
	unsigned int s;
	char proto;

	proto=-1;
	
	if ((n->flags_len!=1) || ((*n->flags | 0x20 )!='s'))
		return -1;
	if (n->regexp_len!=0)
		return -1;
	/* SIP+D2U, SIP+D2T, SIP+D2S, SIPS+D2T */
	if (n->services_len==7){ /* SIP+D2X */
		s=n->services[0]+(n->services[1]<<8)+(n->services[2]<<16)+
				(n->services[3]<<24);
		s|=0x20202020;
		if (s==SIP_SCH){
			s=n->services[4]+(n->services[5]<<8)+(n->services[6]<<16);
			s|=0x00202020;
			switch(s){
				case SIP_D2U:
					proto=PROTO_UDP;
					break;
				case SIP_D2T:
					proto=PROTO_TCP;
					break;
				case SIP_D2S:
					proto=PROTO_SCTP;
					break;
				default:
					return -1;
			}
		}else{
			return -1;
		}
	}else if  (n->services_len==8){ /*SIPS+D2T */
		s=n->services[0]+(n->services[1]<<8)+(n->services[2]<<16)+
				(n->services[3]<<24);
		s|=0x20202020;
		if (s==SIPS_SCH){
			s=n->services[4]+(n->services[5]<<8)+(n->services[6]<<16)+
					(n->services[7]<<24);
			s|=0x20202020;
			if (s==SIPS_D2T){
				proto=PROTO_TLS;
			}
		}else{
			return -1;
		}
	}else{
		return -1;
	}
	return proto;
}



inline static int naptr_proto_pref_score(char proto)
{
	if ((proto>=PROTO_UDP) && (proto<= PROTO_LAST))
		return naptr_proto_pref[(int)proto];
	return 0;
}

inline static int srv_proto_pref_score(char proto)
{
	if ((proto>=PROTO_UDP) && (proto<= PROTO_LAST))
		return srv_proto_pref[(int)proto];
	return 0;
}



/** returns true if we support the protocol */
int naptr_proto_supported(char proto)
{
	if (naptr_proto_pref_score(proto)<0)
		return 0;
	switch(proto){
		case PROTO_UDP:
			return 1;
#ifdef USE_TCP
		case PROTO_TCP:
			return !tcp_disable;
#ifdef USE_TLS
		case PROTO_TLS:
			return !tls_disable;
#endif /* USE_TLS */
#endif /* USE_TCP */
#ifdef USE_SCTP
		case PROTO_SCTP:
			return !sctp_disable;
#endif
	}
	return 0;
}




/** returns true if new_proto is preferred over old_proto */
int naptr_proto_preferred(char new_proto, char old_proto)
{
	return naptr_proto_pref_score(new_proto)>naptr_proto_pref_score(old_proto);
}


/** choose between 2 naptr records, should take into account local
 * preferences too
 * returns 1 if the new record was selected, 0 otherwise */
int naptr_choose (struct naptr_rdata** crt, char* crt_proto,
									struct naptr_rdata* n , char n_proto)
{
#ifdef NAPTR_DBG
	LM_DBG("o:%d w:%d p:%d, o:%d w:%d p:%d\n",
			*crt?(int)(*crt)->order:-1, *crt?(int)(*crt)->pref:-1,
			(int)*crt_proto,
			(int)n->order, (int)n->pref, (int)n_proto);
#endif
	if ((*crt==0) || ((*crt_proto!=n_proto) && 
						( naptr_proto_preferred(n_proto, *crt_proto))) )
			goto change;
	if (!naptr_proto_preferred(*crt_proto, n_proto) && 
			((n->order<(*crt)->order) || ((n->order== (*crt)->order) &&
								(n->pref < (*crt)->pref)))){
			goto change;
	}
#ifdef NAPTR_DBG
	LM_DBG("no change\n");
#endif
	return 0;
change:
#ifdef NAPTR_DBG
	LM_DBG("changed\n");
#endif
	*crt_proto=n_proto;
	*crt=n;
	return 1;
}
#endif /* USE_NAPTR */



/** internal sip srv resolver: resolves a host name trying:
 * - SRV lookup if the address is not an ip *port==0. The result of the SRV
 *   query will be used for an A/AAAA lookup.
 *  - normal A/AAAA lookup (either fallback from the above or if *port!=0
 *   and *proto!=0 or port==0 && proto==0)
 * when performing SRV lookup (*port==0) it will use *proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 * If zt is set, name will be assumed to be 0 terminated and some copy 
 * operations will be avoided.
 * If is_srv is set it will assume name has the srv prefixes for sip already
 *  appended and it's already 0-term'ed; if not it will append them internally.
 * If ars !=0, it will first try to look through them and only if the SRV
 *   record is not found it will try doing a DNS query  (ars will not be
 *   freed, the caller should take care of them)
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* srv_sip_resolvehost(str* name, int zt, unsigned short* port,
	char* proto, int is_srv, struct rdata* ars)
{
	struct hostent* he;
	struct ip_addr* ip;
	static char tmp[MAX_DNS_NAME]; /* tmp. buff. for SRV lookups and
	                                  null. term  strings */
	struct rdata* l;
	struct srv_rdata* srv;
	struct rdata* srv_head;
	char* srv_target;
	char srv_proto;

	/* init */
	srv_head=0;
	srv_target=0;
	if (name->len >= MAX_DNS_NAME) {
		LM_ERR("domain name too long\n");
		he=0;
		goto end;
	}
#ifdef RESOLVE_DBG
	LM_DBG("%.*s:%d proto=%d\n", name->len, name->s,
			port?(int)*port:-1, proto?(int)*proto:-1);
#endif
	if (is_srv){
		/* skip directly to srv resolving */
		srv_proto=(proto)?*proto:0;
		*port=(srv_proto==PROTO_TLS)?SIPS_PORT:SIP_PORT;
		if (zt){
			srv_target=name->s; /* name.s must be 0 terminated in
								  this case */
		}else{
			memcpy(tmp, name->s, name->len);
			tmp[name->len] = '\0';
			srv_target=tmp;
		}
		goto do_srv; /* skip to the actual srv query */
	}
	if (proto){ /* makes sure we have a protocol set*/
		if (*proto==0)
			*proto=srv_proto=PROTO_UDP; /* default */
		else
			srv_proto=*proto;
	}else{
		srv_proto=PROTO_UDP;
	}
	/* try SRV if no port specified (draft-ietf-sip-srv-06) */
	if ((port)&&(*port==0)){
		*port=(srv_proto==PROTO_TLS)?SIPS_PORT:SIP_PORT; /* just in case we
														  don't find another */
		/* check if it's an ip address */
		if (((ip=str2ip(name))!=0)
			  || ((ip=str2ip6(name))!=0) 
			 ){
			/* we are lucky, this is an ip address */
			he=ip_addr2he(name, ip);
			goto end;
		}
		if ((name->len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME){
			LM_WARN("domain name too long (%d), unable to perform SRV lookup\n",
						name->len);
		}else{
			
			switch(srv_proto){
				case PROTO_UDP:
				case PROTO_TCP:
				case PROTO_TLS:
				case PROTO_SCTP:
					create_srv_name(srv_proto, name, tmp);
					break;
				default:
					LM_CRIT("unknown proto %d\n", srv_proto);
					he=0;
					goto end;
			}
			srv_target=tmp;
do_srv:
			/* try to find the SRV records inside previous ARs  first*/
			for (l=ars; l; l=l->next){
				if (l->type!=T_SRV) continue; 
				srv=(struct srv_rdata*) l->rdata;
				if (srv==0){
					LM_CRIT("null rdata\n");
					/* cleanup on exit only */
					break;
				}
				he=resolvehost(srv->name);
				if (he!=0){
					/* we found it*/
#ifdef RESOLVE_DBG
					LM_DBG("found SRV(%s) = %s:%d in AR\n",
							srv_target, srv->name, srv->port);
#endif
					*port=srv->port;
					/* cleanup on exit */
					goto end;
				}
			}
			srv_head=get_record(srv_target, T_SRV, RES_ONLY_TYPE);
			for(l=srv_head; l; l=l->next){
				if (l->type!=T_SRV) continue; /*should never happen*/
				srv=(struct srv_rdata*) l->rdata;
				if (srv==0){
					LM_CRIT("null rdata\n");
					/* cleanup on exit only */
					break;
				}
				he=resolvehost(srv->name);
				if (he!=0){
					/* we found it*/
#ifdef RESOLVE_DBG
					LM_DBG("SRV(%s) = %s:%d\n",
							srv_target, srv->name, srv->port);
#endif
					*port=srv->port;
					/* cleanup on exit */
					goto end;
				}
			}
			if (is_srv){
				/* if the name was already into SRV format it doesn't make
				 * any sense to fall back to A/AAAA */
				he=0;
				goto end;
			}
			/* cleanup on exit */
#ifdef RESOLVE_DBG
			LM_DBG("no SRV record found for %.*s," 
					" trying 'normal' lookup...\n", name->len, name->s);
#endif
		}
	}
	if (likely(!zt)){
		memcpy(tmp, name->s, name->len);
		tmp[name->len] = '\0';
		he=resolvehost(tmp);
	}else{
		he=resolvehost(name->s);
	}
end:
#ifdef RESOLVE_DBG
	LM_DBG("returning %p (%.*s:%d proto=%d)\n",
			he, name->len, name->s,
			port?(int)*port:-1, proto?(int)*proto:-1);
#endif
	if (srv_head)
		free_rdata_list(srv_head);
	return he;
}



#ifdef USE_NAPTR 


/** iterates over a naptr rr list, returning each time a "good" naptr record
 * is found.( srv type, no regex and a supported protocol)
 * params:
 *         naptr_head - naptr rr list head
 *         tried      - bitmap used to keep track of the already tried records
 *                      (no more then sizeof(tried)*8 valid records are 
 *                      ever walked
 *         srv_name   - if succesfull, it will be set to the selected record
 *                      srv name (naptr repl.)
 *         proto      - if succesfull it will be set to the selected record
 *                      protocol
 * returns  0 if no more records found or a pointer to the selected record
 *  and sets  protocol and srv_name
 * WARNING: when calling first time make sure you run first 
 *           naptr_iterate_init(&tried)
 */
struct rdata* naptr_sip_iterate(struct rdata* naptr_head, 
										naptr_bmp_t* tried,
										str* srv_name, char* proto)
{
	int i, idx;
	struct rdata* l;
	struct rdata* l_saved;
	struct naptr_rdata* naptr;
	struct naptr_rdata* naptr_saved;
	char saved_proto;
	char naptr_proto;

	idx=0;
	naptr_proto=PROTO_NONE;
	naptr_saved=0;
	l_saved=0;
	saved_proto=0;
	i=0;
	for(l=naptr_head; l && (i<MAX_NAPTR_RRS); l=l->next){
		if (l->type!=T_NAPTR) continue; 
		naptr=(struct naptr_rdata*) l->rdata;
		if (naptr==0){
			LM_CRIT("null rdata\n");
			goto end;
		}
		/* check if valid and get proto */
		if ((naptr_proto=naptr_get_sip_proto(naptr))<=0) continue;
		if (*tried& (1<<i)){
			i++;
			continue; /* already tried */
		}
#ifdef NAPTR_DBG
		LM_DBG("found a valid sip NAPTR rr %.*s, proto %d\n",
					naptr->repl_len, naptr->repl, (int)naptr_proto);
#endif
		if ((naptr_proto_supported(naptr_proto))){
			if (naptr_choose(&naptr_saved, &saved_proto,
								naptr, naptr_proto))
				idx=i;
				l_saved=l;
			}
		i++;
	}
	if (naptr_saved){
		/* found something */
#ifdef NAPTR_DBG
		LM_DBG("choosed NAPTR rr %.*s, proto %d tried: 0x%x\n",
					naptr_saved->repl_len, 
					naptr_saved->repl, (int)saved_proto, *tried);
#endif
		*tried|=1<<idx;
		*proto=saved_proto;
		srv_name->s=naptr_saved->repl;
		srv_name->len=naptr_saved->repl_len;
		return l_saved;
	}
end:
	return 0;
}

/** Prepend srv prefix according to the proto. */
void create_srv_name(char proto, str *name, char *srv) {
	switch (proto) {
		case PROTO_UDP:
			memcpy(srv, SRV_UDP_PREFIX, SRV_UDP_PREFIX_LEN);
			memcpy(srv+SRV_UDP_PREFIX_LEN, name->s, name->len);
			srv[SRV_UDP_PREFIX_LEN + name->len] = '\0';
			break;
		case PROTO_TCP:
			memcpy(srv, SRV_TCP_PREFIX, SRV_TCP_PREFIX_LEN);
			memcpy(srv+SRV_TCP_PREFIX_LEN, name->s, name->len);
			srv[SRV_TCP_PREFIX_LEN + name->len] = '\0';
			break;
		case PROTO_TLS:
			memcpy(srv, SRV_TLS_PREFIX, SRV_TLS_PREFIX_LEN);
			memcpy(srv+SRV_TLS_PREFIX_LEN, name->s, name->len);
			srv[SRV_TLS_PREFIX_LEN + name->len] = '\0';
			break;
		case PROTO_SCTP:
			memcpy(srv, SRV_SCTP_PREFIX, SRV_SCTP_PREFIX_LEN);
			memcpy(srv+SRV_SCTP_PREFIX_LEN, name->s, name->len);
			srv[SRV_SCTP_PREFIX_LEN + name->len] = '\0';
			break;
		default:
			LM_CRIT("%s: unknown proto %d\n", __func__, proto);
	}
}

size_t create_srv_pref_list(char *proto, struct dns_srv_proto *list) {
	struct dns_srv_proto tmp;
	size_t i,j,list_len;
	int default_order,max;

	/* if proto available, then add only the forced protocol to the list */
	if (proto && *proto!=PROTO_NONE){
		list[0].proto=*proto;
		list_len=1;
	} else {
		list_len = 0;
		/*get protocols and preference scores, and add availble protocol(s) and score(s) to the list*/
		for (i=PROTO_UDP; i<PROTO_LAST;i++) {
			tmp.proto_pref = srv_proto_pref_score(i);
			/* if -1 so disabled continue with next protocol*/
			if (naptr_proto_supported(i) == 0) {
				continue;
			} else {
				list[i-1].proto_pref=tmp.proto_pref;
				list[i-1].proto=i;
				list_len++;
			}
		};

		/* if all protocol prefence scores equal, then set the perference to default values: udp,tcp,tls,sctp */
		for (i=1; i<list_len;i++) {
			if(list[0].proto_pref!=list[i].proto_pref){
				default_order=0;
			}
		}
		if (default_order){
			for (i=0; i<list_len;i++) {
				list[i].proto_pref=srv_proto_pref_score(i);
			}
		}

		/* sorting the list */
		for (i=0;i<list_len-1;i++) {
			max=i;
			for (j=i+1;j<list_len;j++) {
				if (list[j].proto_pref>list[max].proto_pref) { 
					max=j; 
				}
			}
			if (i!=max) {
				tmp=list[i];
				list[i]=list[max];
				list[max]=tmp;
			}
		}

	}
	return list_len;
}

/** Resolves SRV if no naptr found. 
 * It reuse dns_pref values and according that resolves supported protocols. 
 * If dns_pref are equal then it use udp,tcp,tls,sctp order.
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* no_naptr_srv_sip_resolvehost(str* name, unsigned short* port, char* proto)
{
	struct dns_srv_proto srv_proto_list[PROTO_LAST];
	struct hostent* he;
	struct ip_addr* ip;
	str srv_name;
	static char tmp_srv[MAX_DNS_NAME]; /* tmp. buff. for SRV lookups */
	size_t i,list_len;
	/* init variables */
	he=0;

	/* check if it's an ip address */
	if (((ip=str2ip(name))!=0)
			  || ((ip=str2ip6(name))!=0)
			 ){
		/* we are lucky, this is an ip address */
		/* set proto if needed - default udp */
		if ((proto)&&(*proto==PROTO_NONE))
			*proto=PROTO_UDP;
		/* set port if needed - default 5060/5061 */
		if ((port)&&(*port==0))
			*port=((proto) && (*proto==PROTO_TLS))?SIPS_PORT:SIP_PORT;
		he=ip_addr2he(name, ip);
		return he;
	}

	if ((name->len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME){
		LM_WARN("domain name too long (%d), unable to perform SRV lookup\n", name->len);
	} else {
		/* looping on the ordered list until we found a protocol what has srv record */
		list_len = create_srv_pref_list(proto, srv_proto_list);
		for (i=0; i<list_len;i++) {	
			switch (srv_proto_list[i].proto) {
				case PROTO_UDP:
				case PROTO_TCP:
				case PROTO_TLS:
				case PROTO_SCTP:
					create_srv_name(srv_proto_list[i].proto, name, tmp_srv);
					break;
				default:
					LM_CRIT("unknown proto %d\n", (int)srv_proto_list[i].proto);
					return 0;
			}
			/* set default port */
			if ((port)&&(*port==0)){
				*port=(srv_proto_list[i].proto==PROTO_TLS)?SIPS_PORT:SIP_PORT; /* just in case we don't find another */
			}
			if ((proto)&&(*proto==0)){
				*proto = PROTO_UDP;
			}
			srv_name.s=tmp_srv;
			srv_name.len=strlen(tmp_srv);
			#ifdef USE_DNS_CACHE
			he=dns_srv_get_he(&srv_name, port, dns_flags);
			#else
			he=srv_sip_resolvehost(&srv_name, 0, port, proto, 1, 0);
			#endif
			if (he!=0) {
				if(proto) *proto = srv_proto_list[i].proto;
				return he;
			}
		}
	}
	return 0;

} 

/** internal sip naptr resolver function: resolves a host name trying:
 * - NAPTR lookup if the address is not an ip and *proto==0 and *port==0.
 *   The result of the NAPTR query will be used for a SRV lookup
 * - SRV lookup if the address is not an ip *port==0. The result of the SRV
 *   query will be used for an A/AAAA lookup.
 *  - normal A/AAAA lookup (either fallback from the above or if *port!=0
 *   and *proto!=0 or port==0 && proto==0)
 * when performing SRV lookup (*port==0) it will use proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* naptr_sip_resolvehost(str* name,  unsigned short* port,
										char* proto)
{
	struct hostent* he;
	struct ip_addr* ip;
	static char tmp[MAX_DNS_NAME]; /* tmp. buff. for SRV lookups and
	                                  null. term  strings */
	struct rdata* l;
	struct rdata* naptr_head;
	char n_proto;
	str srv_name;
	naptr_bmp_t tried_bmp; /* tried bitmap */
	char origproto;

	origproto = *proto;
	naptr_head=0;
	he=0;
	if (name->len >= MAX_DNS_NAME) {
		LM_ERR("domain name too long\n");
		goto end;
	}
	/* try NAPTR if no port or protocol is specified and NAPTR lookup is
	 * enabled */
	if (port && proto && (*proto==0) && (*port==0)){
		*proto=PROTO_UDP; /* just in case we don't find another */
		if ( ((ip=str2ip(name))!=0)
			  || ((ip=str2ip6(name))!=0)
		){
			/* we are lucky, this is an ip address */
			he=ip_addr2he(name,ip);
			*port=SIP_PORT;
			goto end;
		}
		memcpy(tmp, name->s, name->len);
		tmp[name->len] = '\0';
		naptr_head=get_record(tmp, T_NAPTR, RES_AR);
		naptr_iterate_init(&tried_bmp);
		while((l=naptr_sip_iterate(naptr_head, &tried_bmp,
										&srv_name, &n_proto))!=0){
			if ((he=srv_sip_resolvehost(&srv_name, 1, port, proto, 1, l))!=0){
				*proto=n_proto;
				return he;
			}
		}
		/*clean up on exit*/
#ifdef RESOLVE_DBG
		LM_DBG("no NAPTR record found for %.*s, trying SRV lookup...\n",
					name->len, name->s);
#endif
	}
	/* fallback to srv lookup */
	*proto = origproto;
	he=no_naptr_srv_sip_resolvehost(name,port,proto);
	/* fallback all the way down to A/AAAA */
	if (he==0) {
		he=dns_get_he(name,dns_flags);
	}
end:
	if (naptr_head)
		free_rdata_list(naptr_head);
	return he;
}
#endif /* USE_NAPTR */



/** resolves a host name trying:
 * - NAPTR lookup if enabled, the address is not an ip and *proto==0 and 
 *   *port==0. The result of the NAPTR query will be used for a SRV lookup
 * - SRV lookup if the address is not an ip *port==0. The result of the SRV
 *   query will be used for an A/AAAA lookup.
 *  - normal A/AAAA lookup (either fallback from the above or if *port!=0
 *   and *proto!=0 or port==0 && proto==0)
 * when performing SRV lookup (*port==0) it will use *proto to look for
 * tcp or udp hosts, otherwise proto is unused; if proto==0 => no SRV lookup
 *
 * returns: hostent struct & *port filled with the port from the SRV record;
 *  0 on error
 */
struct hostent* _sip_resolvehost(str* name, unsigned short* port, char* proto)
{
	struct hostent* res = NULL;
#ifdef USE_NAPTR
	if (cfg_get(core, core_cfg, dns_try_naptr))
		res = naptr_sip_resolvehost(name, port, proto);
	else
#endif
	res = srv_sip_resolvehost(name, 0, port, proto, 0, 0);
	if( unlikely(!res) ){
		/* failed DNS request */
		counter_inc(dns_cnts_h.failed_dns_req);
	}
	return res;
}


/** resolve host, port, proto using sip rules (e.g. use SRV if port=0 a.s.o)
 *  and write the result in the sockaddr_union to
 *  returns -1 on error (resolve failed), 0 on success */
int sip_hostport2su(union sockaddr_union* su, str* name, unsigned short port,
						char* proto)
{
	struct hostent* he;
	
	he=sip_resolvehost(name, &port, proto);
	if (he==0){
		ser_error=E_BAD_ADDRESS;
		LM_ERR("could not resolve hostname: \"%.*s\"\n",
					name->len, name->s);
		goto error;
	}
	/* port filled by sip_resolvehost if empty*/
	if (hostent2su(su, he, 0, port)<0){
		ser_error=E_BAD_ADDRESS;
		goto error;
	}
	return 0;
error:
	return -1;
}

/*
 * $Id$
 *
 * resolver related functions
 */


#ifndef resolve_h
#define resolve_h

#include <netdb.h>
#include <arpa/nameser.h>


#define MAX_QUERY_SIZE 8192
#define ANS_SIZE       8192
#define DNS_HDR_SIZE     12
#define MAX_DNS_NAME 256



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


/* A rec. struct */
struct a_rdata {
	unsigned char ip[4];
};

struct aaaa_rdata {
	unsigned char ip6[16];
};



struct rdata* get_record(char* name, int type);
void free_rdata_list(struct rdata* head);


/* gethostbyname wrappers
 * use this, someday htey will use a local cache */

static inline struct hostent* resolvehost(const char* name)
{
	struct hostent* he;
	
#ifdef DNS_IP_HACK
#endif

	he=gethostbyname(name); /*ipv4*/

#ifdef USE_IPV6
	if(he==0){
		/*try ipv6*/
		he=gethostbyname2(name, AF_INET6);
	}
#endif
	return he;
}

struct hostent* sip_resolvehost(char* name, unsigned short* port);



#define HEX2I(c) \
	(	(((c)>='0') && ((c)<='9'))? (c)-'0' :  \
		(((c)>='A') && ((c)<='F'))? ((c)-'A')+10 : \
		(((c)>='a') && ((c)<='f'))? ((c)-'a')+10 : -1 )


#define rev_resolvehost(ip) gethostbyaddr((ip)->u.addr, (ip)->len, (ip)->af);



#if 0
/* returns an ip_addr struct.; on error retunrs 0 and sets err (if !=0)
 * the ip_addr struct is static, so subsequent calls will destroy its 
 * content */
static /*inline*/ struct ip_addr* str2ip6(unsigned char* str, unsigned int len,
		int* err)
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
	unsigned char* init;
	
	/* init */
	init=str;
	i=idx1=rest=0;
	double_colon=0;
	no_colons=0;
	ip.af=AF_INET6;
	ip.len=16;
	addr_start=ip.u.addr16;
	addr=addr_start;
	limit=str+len;
	memset(addr_start, 0 , 8*sizeof(unsigned short));
	memset(addr_end, 0 , 8*sizeof(unsigned short));
	
	for (; str<limit; str++){
		if (*str==':'){
			no_colons++;
			if (no_colons>7) goto error_too_many_colons;
			if (double_colon){
				idx1=i;
				i=0;
				if (addr==addr_end) goto error_colons;
				addr=addr_end;
			}else{
				double_colon=1;
				i++;
			}
		}else if ((hex=HEX2I(*str))>=0){
				addr[i]=addr[i]*16+hex;
				double_colon=0;
		}else{
			/* error, unknown char */
			goto error_char;
		}
	}
	
	rest=8-i-idx1;
	memcpy(addr_start+idx1+rest, addr_end, i*sizeof(unsigned short));
	if (err) *err=0;
	return &ip;

error_too_many_colons:
	DBG("str2ip6: ERROR: too many colons in [%.*s]\n", (int) len, init);
	if (err) *err=1;
	return 0;

error_colons:
	DBG("str2ip6: ERROR: too many double colons in [%.*s]\n", (int) len, init);
	if (err) *err=1;
	return 0;

error_char:
	DBG("str2ip6: WARNING: unexpected char %c in  [%.*s]\n", *str, (int) len,
			init);
	if (err) *err=1;
	return 0;
}
#endif



#endif

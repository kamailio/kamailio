/*
 * proxy list & assoc. functions
 *
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
 * \brief Kamailio core :: proxy list & assoc. functions
 * \ingroup core
 * Module: \ref core
 */



#include "config.h"
#include "proxy.h"
#include "error.h"
#include "dprint.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"

#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

#ifdef DNS_IP_HACK
#include "ut.h"
#endif

#include "resolve.h"
#include "ip_addr.h"
#include "globals.h"


struct proxy_l* proxies=0;



/* searches for the proxy named 'name', on port 'port' with 
   proto 'proto'; if proto==0 => proto wildcard (will match any proto)
   returns: pointer to proxy_l on success or 0 if not found */ 
static struct proxy_l* find_proxy(str *name, unsigned short port, int proto)
{
	struct proxy_l* t;
	for(t=proxies; t; t=t->next)
		if (((t->name.len == name->len) &&
			 ((proto==PROTO_NONE)||(t->proto==proto))&&
			(strncasecmp(t->name.s, name->s, name->len)==0)) &&
				(t->port==port))
			break;
	return t;
}


#define HOSTENT_CPY(dst, src, he_malloc, he_free)						\
	do {																\
		unsigned len,len2;												\
		int r,i;														\
																		\
		/* start copying the host entry.. */							\
		/* copy h_name */												\
		len=strlen(src->h_name)+1;										\
		dst->h_name=(char*)he_malloc(sizeof(char) * len);				\
		if (dst->h_name) strncpy(dst->h_name,src->h_name, len);			\
		else{															\
			ser_error=ret=E_OUT_OF_MEM;									\
			goto error;													\
		}																\
																		\
		/* copy h_aliases */											\
		len=0;															\
		if (src->h_aliases)												\
			for (;src->h_aliases[len];len++);							\
		dst->h_aliases=(char**)he_malloc(sizeof(char*)*(len+1));		\
		if (dst->h_aliases==0){											\
			ser_error=ret=E_OUT_OF_MEM;									\
			he_free(dst->h_name);										\
			goto error;													\
		}																\
		memset((void*)dst->h_aliases, 0, sizeof(char*) * (len+1) );		\
		for (i=0;i<len;i++){											\
			len2=strlen(src->h_aliases[i])+1;							\
			dst->h_aliases[i]=(char*)he_malloc(sizeof(char)*len2);		\
			if (dst->h_aliases==0){										\
				ser_error=ret=E_OUT_OF_MEM;								\
				he_free(dst->h_name);									\
				for(r=0; r<i; r++)	he_free(dst->h_aliases[r]);			\
				he_free(dst->h_aliases);								\
				goto error;												\
			}															\
			strncpy(dst->h_aliases[i], src->h_aliases[i], len2);		\
		}																\
		/* copy h_addr_list */											\
		len=0;															\
		if (src->h_addr_list)											\
			for (;src->h_addr_list[len];len++);							\
		dst->h_addr_list=(char**)he_malloc(sizeof(char*)*(len+1));		\
		if (dst->h_addr_list==0){										\
			ser_error=ret=E_OUT_OF_MEM;									\
			he_free(dst->h_name);										\
			for(r=0; dst->h_aliases[r]; r++)							\
				he_free(dst->h_aliases[r]);								\
			he_free(dst->h_aliases[r]);									\
			he_free(dst->h_aliases);									\
			goto error;													\
		}																\
		memset((void*)dst->h_addr_list, 0, sizeof(char*) * (len+1) );	\
		for (i=0;i<len;i++){											\
			dst->h_addr_list[i]=										\
				(char*)he_malloc(sizeof(char)*src->h_length);			\
			if (dst->h_addr_list[i]==0){								\
				ser_error=ret=E_OUT_OF_MEM;								\
				he_free(dst->h_name);									\
				for(r=0; dst->h_aliases[r]; r++)						\
					he_free(dst->h_aliases[r]);							\
				he_free(dst->h_aliases[r]);								\
				he_free(dst->h_aliases);								\
				for (r=0; r<i;r++) he_free(dst->h_addr_list[r]);		\
				he_free(dst->h_addr_list);								\
				goto error;												\
			}															\
			memcpy(dst->h_addr_list[i], src->h_addr_list[i],			\
				   src->h_length);										\
		}																\
																		\
		/* copy h_addr_type & length */									\
		dst->h_addrtype=src->h_addrtype;								\
		dst->h_length=src->h_length;									\
		/*finished hostent copy */										\
																		\
		return 0;														\
	} while(0)


#define FREE_HOSTENT(dst, he_free)						\
	do {												\
		int r;											\
		if (dst->h_name) he_free(dst->h_name);			\
		if (dst->h_aliases){							\
			for(r=0; dst->h_aliases[r]; r++) {			\
				he_free(dst->h_aliases[r]);				\
			}											\
			he_free(dst->h_aliases);					\
		}												\
		if (dst->h_addr_list){							\
			for (r=0; dst->h_addr_list[r];r++) {		\
				he_free(dst->h_addr_list[r]);			\
			}											\
			he_free(dst->h_addr_list);					\
		}												\
	} while(0)



/* copies a hostent structure*, returns 0 on success, <0 on error*/
static int hostent_cpy(struct hostent *dst, struct hostent* src)
{
	int ret;
	HOSTENT_CPY(dst, src, pkg_malloc, pkg_free);
error:
	LM_CRIT("memory allocation failure\n");
	return ret;
}


static int hostent_shm_cpy(struct hostent *dst, struct hostent* src)
{
	int ret;
	HOSTENT_CPY(dst, src, shm_malloc, shm_free);
error:
	LM_CRIT("memory allocation failure\n");
	return ret;
}


void free_hostent(struct hostent* dst)
{
	FREE_HOSTENT(dst, pkg_free);
}

void free_shm_hostent(struct hostent* dst)
{
	FREE_HOSTENT(dst, shm_free);
}



struct proxy_l* add_proxy(str* name, unsigned short port, int proto)
{
	struct proxy_l* p;
	
	if ((p=find_proxy(name, port, proto))!=0) return p;
	if ((p=mk_proxy(name, port, proto))==0) goto error;
	/* add p to the proxy list */
	p->next=proxies;
	proxies=p;
	return p;

error:
	return 0;
}


#define MK_PROXY(name, port, protocol, p_malloc, p_free, he_cpy)		\
	do {																\
		struct proxy_l* p;												\
		struct hostent* he;												\
		char proto;														\
																		\
		p=(struct proxy_l*) p_malloc(sizeof(struct proxy_l));			\
		if (p==0){														\
			ser_error=E_OUT_OF_MEM;										\
			LM_ERR("memory allocation failure\n");						\
			goto error;													\
		}																\
		memset(p,0,sizeof(struct proxy_l));								\
		p->name=*name;													\
		p->port=port;													\
																		\
		LM_DBG("doing DNS lookup...\n");								\
		proto=protocol;													\
		he=sip_resolvehost(name, &(p->port), &proto);					\
		if (he==0){														\
			ser_error=E_BAD_ADDRESS;									\
			LM_CRIT("could not resolve hostname:"						\
				" \"%.*s\"\n", name->len, name->s);						\
			p_free(p);													\
			goto error;													\
		}																\
		if (he_cpy(&(p->host), he)!=0){									\
			p_free(p);													\
			goto error;													\
		}																\
		p->proto=proto;													\
		p->ok=1;														\
		return p;														\
	error:																\
		return 0;														\
																		\
	} while(0)

/* same as add_proxy, but it doesn't add the proxy to the list
 * uses also SRV if possible & port==0 (quick hack) */

struct proxy_l* mk_proxy(str* name, unsigned short port, int protocol)
{
	MK_PROXY(name, port, protocol, pkg_malloc, pkg_free, hostent_cpy);
}


struct proxy_l* mk_shm_proxy(str* name, unsigned short port, int protocol)
{
	MK_PROXY(name, port, protocol, shm_malloc, shm_free, hostent_shm_cpy);
}



/* same as mk_proxy, but get the host as an ip*/
struct proxy_l* mk_proxy_from_ip(struct ip_addr* ip, unsigned short port,
									int proto)
{
	struct proxy_l* p;

	p=(struct proxy_l*) pkg_malloc(sizeof(struct proxy_l));
	if (p==0){
		LM_CRIT("memory allocation failure\n");
		goto error;
	}
	memset(p,0,sizeof(struct proxy_l));

	p->port=port;
	p->proto=proto;
	p->host.h_addrtype=ip->af;
	p->host.h_length=ip->len;
	p->host.h_addr_list=pkg_malloc(2*sizeof(char*));
	if (p->host.h_addr_list==0) goto error;
	p->host.h_addr_list[1]=0;
	p->host.h_addr_list[0]=pkg_malloc(ip->len+1);
	if (p->host.h_addr_list[0]==0){
		pkg_free(p->host.h_addr_list);
		goto error;
	}

	memcpy(p->host.h_addr_list[0], ip->u.addr, ip->len);
	p->host.h_addr_list[0][ip->len]=0;

	return p;

error:
	return 0;
}




void free_proxy(struct proxy_l* p)
{
	if (p) free_hostent(&p->host);
}


void free_shm_proxy(struct proxy_l* p)
{
	if (p) free_shm_hostent(&p->host);
}

/*
 * $Id$
 *
 * proxy list & assoc. functions
 *
 */


#include "proxy.h"
#include "error.h"
#include "dprint.h"

#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

#ifdef DNS_IP_HACK
#include "ut.h"
#endif

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

struct proxy_l* proxies=0;



/* searches for the proxy named 'name', on port 'port'
   returns: pointer to proxy_l on success or 0 if not found */ 
static struct proxy_l* find_proxy(char *name, unsigned short port)
{
	struct proxy_l* t;
	for(t=proxies; t; t=t->next)
		if ((strcasecmp(t->name, name)==0) && (t->port==port))
			break;
	return t;
}



/* copies a hostent structure*, returns 0 on success, <0 on error*/
static int hostent_cpy(struct hostent *dst, struct hostent* src)
{
	unsigned len,len2;
	int r,ret,i;

	/* start copying the host entry.. */
	/* copy h_name */
	len=strlen(src->h_name)+1;
	dst->h_name=(char*)malloc(sizeof(char) * len);
	if (dst->h_name) strncpy(dst->h_name,src->h_name, len);
	else{
		ret=E_OUT_OF_MEM;
		goto error;
	}

	/* copy h_aliases */
	for (len=0;src->h_aliases[len];len++);
	dst->h_aliases=(char**)malloc(sizeof(char*)*(len+1));
	if (dst->h_aliases==0){
		ret=E_OUT_OF_MEM;
		free(dst->h_name);
		goto error;
	}
	memset((void*)dst->h_aliases, 0, sizeof(char*) * (len+1) );
	for (i=0;i<len;i++){
		len2=strlen(src->h_aliases[i])+1;
		dst->h_aliases[i]=(char*)malloc(sizeof(char)*len2);
		if (dst->h_aliases==0){
			ret=E_OUT_OF_MEM;
			free(dst->h_name);
			for(r=0; r<i; r++)	free(dst->h_aliases[r]);
			free(dst->h_aliases);
			goto error;
		}
		strncpy(dst->h_aliases[i], src->h_aliases[i], len2);
	}
	/* copy h_addr_list */
	for (len=0;src->h_addr_list[len];len++);
	dst->h_addr_list=(char**)malloc(sizeof(char*)*(len+1));
	if (dst->h_addr_list==0){
		ret=E_OUT_OF_MEM;
		free(dst->h_name);
		for(r=0; dst->h_aliases[r]; r++)	free(dst->h_aliases[r]);
		free(dst->h_aliases[r]);
		free(dst->h_aliases);
		goto error;
	}
	memset((void*)dst->h_addr_list, 0, sizeof(char*) * (len+1) );
	for (i=0;i<len;i++){
		dst->h_addr_list[i]=(char*)malloc(sizeof(char)*src->h_length);
		if (dst->h_addr_list[i]==0){
			ret=E_OUT_OF_MEM;
			free(dst->h_name);
			for(r=0; dst->h_aliases[r]; r++)	free(dst->h_aliases[r]);
			free(dst->h_aliases[r]);
			free(dst->h_aliases);
			for (r=0; r<i;r++) free(dst->h_addr_list[r]);
			free(dst->h_addr_list);
			goto error;
		}
		memcpy(dst->h_addr_list[i], src->h_addr_list[i], src->h_length);
	}

	/* copy h_addr_type & length */
	dst->h_addrtype=src->h_addrtype;
	dst->h_length=src->h_length;
	/*finished hostent copy */
	
	return 0;

error:
	LOG(L_CRIT, "ERROR: hostent_cpy: memory allocation failure\n");
	return ret;
}



void free_hostent(struct hostent *dst)
{
	int r;
	if (dst->h_name) free(dst->h_name);
	if (dst->h_aliases){
		for(r=0; dst->h_aliases[r]; r++)	free(dst->h_aliases[r]);
		free(dst->h_aliases[r]);
		free(dst->h_aliases);
	}
	if (dst->h_addr_list){
		for (r=0; dst->h_addr_list[r];r++) free(dst->h_addr_list[r]);
		free(dst->h_addr_list[r]);
		free(dst->h_addr_list);
	}
}




struct proxy_l* add_proxy(char* name, unsigned short port)
{
	struct proxy_l* p;
	struct hostent* he;
	
	if ((p=find_proxy(name, port))!=0) return p;
	if ((p=mk_proxy(name, port))==0) goto error;
	/* add p to the proxy list */
	p->next=proxies;
	proxies=p;
	return p;

error:
	return 0;
}




/* same as add_proxy, but it doesn't add the proxy to the list*/
struct proxy_l* mk_proxy(char* name, unsigned short port)
{
	struct proxy_l* p;
	struct hostent* he;
#ifdef DNS_IP_HACK
	int err;
	unsigned int ip;
	int len;
#endif

	p=(struct proxy_l*) malloc(sizeof(struct proxy_l));
	if (p==0){
		LOG(L_CRIT, "ERROR: mk_proxy: memory allocation failure\n");
		goto error;
	}
	memset(p,0,sizeof(struct proxy_l));
	p->name=name;
	p->port=port;
#ifdef DNS_IP_HACK
	len=strlen(name);
	ip=str2ip(name, len, &err);
	if (err==0){
		p->host.h_name=malloc(len+1);
		if (p->host.h_name==0) goto error;
		memcpy(p->host.h_name, name, len);
		p->host.h_aliases=malloc(sizeof(char*));
		if (p->host.h_aliases==0) {
			free(p->host.h_name);
			goto error;
		}
		p->host.h_aliases[0]=0;
		p->host.h_addrtype=AF_INET;
		p->host.h_length=4;
		p->host.h_addr_list=malloc(2*sizeof(char*));
		if (p->host.h_addr_list==0){
			free(p->host.h_name);
			free(p->host.h_aliases);
			goto error;
		}
		p->host.h_addr_list[1]=0;
		p->host.h_addr_list[0]=malloc(5);
		if (p->host.h_addr_list[0]==0){
			free(p->host.h_name);
			free(p->host.h_aliases);
			free(p->host.h_addr_list);
			goto error;
		}
		memcpy(p->host.h_addr_list[0], (char*)&ip, 4);
		p->host.h_addr_list[0][4]=0;

		return p;
	}
#endif
	/* fail over to normal lookup */

	he=gethostbyname(name);
	if (he==0){
		LOG(L_CRIT, "ERROR: mk_proxy: could not resolve hostname:"
					" \"%s\"\n", name);
		free(p);
		goto error;
	}
	if (hostent_cpy(&(p->host), he)!=0){
		free(p);
		goto error;
	}
	p->ok=1;
	return p;
error:
	return 0;
}



/* same as mk_proxy, but get the host as an ip*/
struct proxy_l* mk_proxy_from_ip(unsigned int ip, unsigned short port)
{
	struct proxy_l* p;
	struct hostent* he;
	int err;

	p=(struct proxy_l*) malloc(sizeof(struct proxy_l));
	if (p==0){
		LOG(L_CRIT, "ERROR: mk_proxy_from_ip: memory allocation failure\n");
		goto error;
	}
	memset(p,0,sizeof(struct proxy_l));

	p->port=port;
	p->host.h_addrtype=AF_INET;
	p->host.h_length=4;
	p->host.h_addr_list=malloc(2*sizeof(char*));
	if (p->host.h_addr_list==0) goto error;
	p->host.h_addr_list[1]=0;
	p->host.h_addr_list[0]=malloc(5);
	if (p->host.h_addr_list[0]==0){
		free(p->host.h_addr_list);
		goto error;
	}

	memcpy(p->host.h_addr_list[0], (char*)&ip, 4);
	p->host.h_addr_list[0][4]=0;

	return p;

error:
	return 0;
}




void free_proxy(struct proxy_l* p)
{
	if (p) free_hostent(&p->host);
}

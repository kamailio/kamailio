/*
 * $Id$
 *
 * proxy list & assoc. functions
 *
 */


#include "proxy.h"
#include "error.h"

#include <string.h>


struct proxy_l* proxies=0;



/* searches for the proxy named 'name', on port 'port'
   returns: pointer to proxy_l on success or 0 if not found */ 
struct proxy_l* find_proxy(char *name, unsigned short port)
{
	struct proxy_l* t;
	for(t=proxies; t; t=t->next)
		if ((strcasecmp(t->name, name)==0) && (t->port==port))
			break;
	return t;
}



/* copies a hostent structure*, returns 0 on success, <0 on error*/
int hostent_cpy(struct hostent *dst, struct hosten* src)
{
	int len, r;

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
		dst->.h_aliases[i]=(char*)malloc(sizeof(char)*len2);
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
		for(r=0; h_aliases[r]; r++)	free(dst->h_aliases[r]);
		free h_aliases[r];
		free(dst->h_aliases);
		goto error;
	}
	memset((void*)dst->.h_addr_list, 0, sizeof(char*) * (len+1) );
	for (i=0;i<len;i++){
		dst->h_addr_list[i]=(char*)malloc(sizeof(char)*src->h_length);
		if (dst->h_addr_list[i]==0){
			ret=E_OUT_OF_MEM;
			free(dst->h_name);
			for(r=0; h_aliases[r]; r++)	free(dst->h_aliases[r]);
			free h_aliases[r];
			free(dst->h_aliases);
			for (r=0; r<i;r++) free(dst->h_addr_list[r]);
			free(dst->h_addr_list);
			goto error;
		}
		memcpy(dst->h_addr_list[i], src->h_addr_list[i], src->h_length);
	}

	/* copy h_addr_type & length */
	dst->h_addrtype=src->h_addrtype;
	dst->host.h_length=src->h_length;
	/*finished hostent copy */
	
	return 0;

error:
	LOG(L_CRIT, "ERROR: hostent_cpy: memory allocation failure\n");
	return ret;
}



struct proxy_l* add_proxy(char* name, unsigned short port)
{
	proxy_l* p;
	struct hostent he;
	
	if ((p=find_proxy(name, port))!=0) return p;
	p=(struct proxy_l*) malloc(sizeof(struct proxy_l));
	if (p==0){
		LOG(L_CRIT, "ERROR: add_proxy: memory allocation failure\n");
		goto error;
	}
	memset(p,0,sizeof(struct_proxy_l));
	p->name=name;
	p->port=port;
	he=gethostbyname(name);
	if (he==0){
		LOG(L_CRIT, "ERROR: add_proxy: could not resolve hostname:"
					" \"%s\"\n", name);
		free(p);
		goto error;
	}
	if (hostent_cpy(&(p->host), he)!=0){
		free(p);
		goto error;
	}
	p->ok=1;
	/* add p to the proxy list */
	p->next=proxies;
	proxies=p;
	return p;

error:
	return 0;
}


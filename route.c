/*
 * $Id$
 *
 * SIP routing engine
 *
 */
 
#include <sys/types.h>
#include <regex.h>
#include <netdb.h>
#include <string.h>

#include "route.h"
#include "cfg_parser.h"
#include "dprint.h"

/* main routing list */
struct route_elem* rlist=0;



void free_re(struct route_elem* r)
{
	int i;
	if (r){
			regfree(&(r->method));
			regfree(&(r->uri));
			
			if (r->host.h_name)      free(r->host.h_name);
			if (r->host.h_aliases){
				for (i=0; r->host.h_aliases[i]; i++)
					free(r->host.h_aliases[i]);
				free(r->host.h_aliases);
			}
			if (r->host.h_addr_list){
				for (i=0; r->host.h_addr_list[i]; i++)
					free(r->host.h_addr_list[i]);
				free(r->host.h_addr_list);
			}
			free(r);
	}
}



struct route_elem* init_re()
{
	struct route_elem* r;
	r=(struct route_elem *) malloc(sizeof(struct route_elem));
	if (r==0) return 0;
	memset((void*)r, 0, sizeof (struct route_elem));
	return r;
}



void push(struct route_elem* re, struct route_elem** head)
{
	struct route_elem *t;
	re->next=0;
	if (*head==0){
		*head=re;
		return;
	}
	for (t=*head; t->next;t=t->next);
	t->next=re;
}



void clear_rlist(struct route_elem** rl)
{
	struct route_elem *t, *u;

	if (*rl==0) return;
	u=0;
	for (t=*rl; t; u=t, t=t->next){
		if (u) free_re(u);
	}
	*rl=0;
}



int add_rule(struct cfg_line* cl, struct route_elem** head)
{
	
	struct route_elem* re;
	struct hostent * he;
	int ret;
	int i,len, len2;


	re=init_re();
	if (re==0) return E_OUT_OF_MEM;

	if (regcomp(&(re->method), cl->method, REG_EXTENDED|REG_NOSUB|REG_ICASE)){
		DPrint("ERROR: bad re \"%s\"\n", cl->method);
		ret=E_BAD_RE;
		goto error;
	}
	if (regcomp(&(re->uri), cl->uri, REG_EXTENDED|REG_NOSUB|REG_ICASE) ){
		DPrint("ERROR: bad re \"%s\"\n", cl->uri);
		ret=E_BAD_RE;
		goto error;
	}

	
	he=gethostbyname(cl->address);
	if (he==0){
		DPrint("ERROR: cannot resolve \"%s\"\n", cl->address);
		ret=E_BAD_ADDRESS;
		goto error;
	}
	
	/* start copying the host entry.. */
	/* copy h_name */
	len=strlen(he->h_name)+1;
	re->host.h_name=(char*)malloc(len);
	if (re->host.h_name) strncpy(re->host.h_name, he->h_name, len);
	else{
		ret=E_OUT_OF_MEM;
		goto error;
	}

	/* copy h_aliases */
	for (len=0;he->h_aliases[len];len++);
	re->host.h_aliases=(char**)malloc(len+1);
	if (re->host.h_aliases==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memset((void*)re->host.h_aliases, 0, sizeof(char*) * (len+1) );
	for (i=0;i<len;i++){
		len2=strlen(he->h_aliases[i])+1;
		re->host.h_aliases[i]=(char*)malloc(len2);
		if (re->host.h_aliases==0){
			ret=E_OUT_OF_MEM;
			goto error;
		}
		strncpy(re->host.h_aliases[i], he->h_aliases[i], len2);
	}
	/* copy h_addr_list */
	for (len=0;he->h_addr_list[len];len++);
	re->host.h_addr_list=(char**)malloc(len+1);
	if (re->host.h_addr_list==0){
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memset((void*)re->host.h_addr_list, 0, sizeof(char*) * (len+1) );
	for (i=0;i<len;i++){
		re->host.h_addr_list[i]=(char*)malloc(he->h_length+1);
		if (re->host.h_addr_list==0){
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(re->host.h_addr_list[i], he->h_addr_list[i], he->h_length+1);
	}
	/* copy h_addr_type & length */
	re->host.h_addrtype=he->h_addrtype;
	re->host.h_length=he->h_length;
	/*finished hostent copy */

	
	
	re->current_addr_idx=0;
	re->ok=1;

	push(re,head);
	return 0;
	
error:
		free_re(re);
		return ret;
}



struct route_elem* route_match(char* method, char* uri, struct route_elem** rl)
{
	struct route_elem* t;
	if (*rl==0){
		DPrint("WARNING: empty routing table\n");
		return 0;
	}
	for (t=*rl; t; t=t->next){
		if (regexec(&(t->method), method, 0, 0, 0)==0){
			/* we have a method mach !!! */
			if (regexec(&(t->uri), uri, 0, 0, 0)==0){
				/* we have a full match */
				return t;
			}
		}
	}
	/* no match :( */
	return 0;
}



/* debug function, prints main routing table */
void print_rl()
{
	struct route_elem* t;
	int i,j;

	if (rlist==0){
		DPrint("the routing table is emty\n");
		return;
	}
	
	for (t=rlist,i=0; t; i++, t=t->next){
		DPrint("%2d.to=%s ; route ok=%d\n", i,
				t->host.h_name, t->ok);
		DPrint("   ips: ");
		for (j=0; t->host.h_addr_list[j]; j++)
			DPrint("%d.%d.%d.%d ",
				(unsigned char) t->host.h_addr_list[j][0],
				(unsigned char) t->host.h_addr_list[j][1],
			    (unsigned char) t->host.h_addr_list[j][2],
				(unsigned char) t->host.h_addr_list[j][3]
				  );
				
		DPrint("\n   Statistics: tx=%d, errors=%d, tx_bytes=%d, idx=%d\n",
				t->tx, t->errors, t->tx_bytes, t->current_addr_idx);
	}

}


